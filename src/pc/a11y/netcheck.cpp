/* SPDX-License-Identifier: GPL-3.0-or-later */
/* melee-netcheck: a console program that tells a player whether their
 * network suits the game's Direct Connect, before they try it with a friend.
 *
 * Direct Connect meets a friend through the public BitTorrent DHT and then
 * punches a hole: both games send to the address and port the DHT saw the
 * other at, and each router lets the other's packets in because it has just
 * sent to them (net_match.c, docs/netcode-plan.md section 7). That works
 * when a router keeps one public port for a socket whoever it talks to. The
 * check asks DHT nodes on several networks how they see this machine and
 * compares their answers; the test with a friend tries the hole punch itself
 * between two copies of this program.
 *
 * Standalone and outside the game, so it writes to the console rather than
 * pc_log_line. Every line is plain text a screen reader reads as it comes,
 * and every line but the prompts also goes to a result file, which a friend
 * can send back. */
#include "netcheck_logic.hpp"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

using netcheck::Endpoint;
using std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kNoSocket = INVALID_SOCKET;
void close_socket(NativeSocket s) {
    closesocket(s);
}
#else
using NativeSocket = int;
constexpr NativeSocket kNoSocket = -1;
void close_socket(NativeSocket s) {
    close(s);
}
#endif

/* The check stops when this many networks have said how they see us, or
 * when kCheckTime runs out; the bootstrap nodes are asked again every
 * kBootstrapRetry until one answers. */
constexpr int kEnoughObservers = 8;
constexpr milliseconds kCheckTime{12000};
constexpr milliseconds kBootstrapRetry{2000};
/* One new node asked this often while the check runs; when none is left to
 * ask, one that answered is asked for more at most this often. */
constexpr milliseconds kAskEvery{100};
constexpr milliseconds kRefillEvery{500};

/* The test with a friend: a probe this often, for at most kPunchTime, and
 * once a reply proves both directions, kAfterContact more of round trips. */
constexpr milliseconds kProbeEvery{200};
constexpr milliseconds kPunchTime{60000};
constexpr milliseconds kAfterContact{5000};
/* A router forgets an idle mapping after half a minute or more; while the
 * program waits for the friend's address, a DHT query this often keeps the
 * public port the check printed. */
constexpr milliseconds kKeepAlive{15000};

/* The DHT's well-known entry points, the same the game asks (resolve in
 * net_dht.c). */
struct Bootstrap {
    const char* host;
    const char* port;
};
constexpr Bootstrap kBootstraps[] = {
    {"dht.transmissionbt.com", "6881"},
    {"dht.libtorrent.org", "25401"},
    {"router.bittorrent.com", "6881"},
    {"router.utorrent.com", "6881"},
};

sockaddr_in to_sockaddr(Endpoint e) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(e.address);
    a.sin_port = htons(e.port);
    return a;
}

Endpoint from_sockaddr(const sockaddr_in& a) {
    return Endpoint{ntohl(a.sin_addr.s_addr), ntohs(a.sin_port)};
}

uint32_t network_of(Endpoint e) {
    return e.address & 0xFFFFFF00u;
}

/* One UDP socket for the whole run: the check and the test with a friend
 * must go out through the same router mapping, so the address the check
 * prints is the one the friend sends to. */
class UdpSocket {
public:
    UdpSocket() = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    ~UdpSocket() {
        if (m_socket != kNoSocket) {
            close_socket(m_socket);
        }
    }

    bool open(uint16_t port, bool loopback) {
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == kNoSocket) {
            return false;
        }
#ifdef _WIN32
        /* Windows reports an ICMP "port unreachable" from an earlier send
         * as an error on the next receive; a closed port on a DHT node or a
         * friend not started yet is ordinary here, so turn that off. */
        BOOL report = FALSE;
        DWORD returned = 0;
        WSAIoctl(m_socket, SIO_UDP_CONNRESET, &report, sizeof report, nullptr, 0, &returned,
            nullptr, nullptr);
#endif
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(loopback ? INADDR_LOOPBACK : INADDR_ANY);
        a.sin_port = htons(port);
        if (bind(m_socket, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
            return false;
        }
        socklen_t length = sizeof a;
        if (getsockname(m_socket, reinterpret_cast<sockaddr*>(&a), &length) != 0) {
            return false;
        }
        m_port = ntohs(a.sin_port);
        return true;
    }

    uint16_t port() const { return m_port; }

    void send(Endpoint to, const void* data, std::size_t size) {
        sockaddr_in a = to_sockaddr(to);
        sendto(m_socket, static_cast<const char*>(data), static_cast<int>(size), 0,
            reinterpret_cast<sockaddr*>(&a), sizeof a);
    }

    /* Waits up to timeout for one datagram; its size, or -1 when none came. */
    int receive(milliseconds timeout, uint8_t* buffer, std::size_t size, Endpoint& from) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(m_socket, &read_set);
        timeval tv{};
        tv.tv_sec = static_cast<decltype(tv.tv_sec)>(timeout.count() / 1000);
        tv.tv_usec = static_cast<decltype(tv.tv_usec)>(timeout.count() % 1000 * 1000);
        if (select(static_cast<int>(m_socket) + 1, &read_set, nullptr, nullptr, &tv) <= 0) {
            return -1;
        }
        sockaddr_in a{};
        socklen_t length = sizeof a;
        const int n = static_cast<int>(recvfrom(m_socket, reinterpret_cast<char*>(buffer),
            static_cast<int>(size), 0, reinterpret_cast<sockaddr*>(&a), &length));
        if (n < 0) {
            return -1;
        }
        from = from_sockaddr(a);
        return n;
    }

private:
    NativeSocket m_socket = kNoSocket;
    uint16_t m_port = 0;
};

std::vector<Endpoint> resolve_bootstraps() {
    std::vector<Endpoint> out;
    for (const Bootstrap& b : kBootstraps) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* list = nullptr;
        if (getaddrinfo(b.host, b.port, &hints, &list) != 0) {
            continue;
        }
        for (addrinfo* p = list; p != nullptr; p = p->ai_next) {
            Endpoint e = from_sockaddr(*reinterpret_cast<sockaddr_in*>(p->ai_addr));
            if (std::find(out.begin(), out.end(), e) == out.end()) {
                out.push_back(e);
            }
        }
        freeaddrinfo(list);
    }
    return out;
}

/* This machine's own address on the route to toward: connecting a UDP
 * socket sends nothing, but makes the system pick the interface. */
uint32_t local_address_toward(Endpoint toward) {
    NativeSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kNoSocket) {
        return 0;
    }
    sockaddr_in a = to_sockaddr(toward);
    uint32_t address = 0;
    if (connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0) {
        socklen_t length = sizeof a;
        if (getsockname(s, reinterpret_cast<sockaddr*>(&a), &length) == 0) {
            address = ntohl(a.sin_addr.s_addr);
        }
    }
    close_socket(s);
    return address;
}

/* Lines typed at the console, read on their own thread so the socket keeps
 * being served while the player types. The thread is left blocked in its
 * read when the program ends, so it shares its state rather than borrowing
 * this object's. */
class ConsoleInput {
public:
    ConsoleInput() : m_state(std::make_shared<State>()) {
        std::thread([state = m_state] {
            std::string line;
            while (std::getline(std::cin, line)) {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->lines.push_back(line);
            }
            std::lock_guard<std::mutex> lock(state->mutex);
            state->closed = true;
        }).detach();
    }

    /* A typed line, or nothing yet; closed is set once input has ended. */
    std::optional<std::string> take(bool& closed) {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        closed = m_state->closed && m_state->lines.empty();
        if (m_state->lines.empty()) {
            return std::nullopt;
        }
        std::string line = m_state->lines.front();
        m_state->lines.pop_front();
        return line;
    }

private:
    struct State {
        std::mutex mutex;
        std::deque<std::string> lines;
        bool closed = false;
    };
    std::shared_ptr<State> m_state;
};

/* Copies text to the clipboard, so the address can be pasted into a chat. */
bool copy_to_clipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    bool ok = false;
    EmptyClipboard();
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (memory != nullptr) {
        auto* wide = static_cast<wchar_t*>(GlobalLock(memory));
        for (std::size_t i = 0; i < text.size(); ++i) {
            wide[i] = static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
        }
        wide[text.size()] = 0;
        GlobalUnlock(memory);
        ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        if (!ok) {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
    return ok;
#else
    (void)text;
    return false;
#endif
}

/* Started by a double click, the console window closes with the program, so
 * it waits for Enter first; started from a command prompt, it does not. */
bool console_is_ours() {
#ifdef _WIN32
    DWORD processes[2];
    return GetConsoleProcessList(processes, 2) == 1;
#else
    return false;
#endif
}

constexpr const char* kResultFileName = "melee-netcheck-result.txt";

/* The result file, or null when it could not be opened. */
std::FILE* s_result = nullptr;

/* Beside the program, where a friend finds it; if that folder is not
 * writable, the current one. Rewritten on every run. Returns where it went. */
std::filesystem::path open_result_file() {
    std::vector<std::filesystem::path> places;
#ifdef _WIN32
    wchar_t exe[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        places.push_back(std::filesystem::path(exe).parent_path() / kResultFileName);
    }
#endif
    places.push_back(std::filesystem::path(kResultFileName));
    for (const std::filesystem::path& place : places) {
#ifdef _WIN32
        s_result = _wfopen(place.c_str(), L"w");
#else
        s_result = std::fopen(place.c_str(), "w");
#endif
        if (s_result != nullptr) {
            return place;
        }
    }
    return {};
}

/* Writes to the console and the result file. */
void print(const char* format, ...) __attribute__((format(printf, 1, 2)));
void print(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::fflush(stdout);
    if (s_result != nullptr) {
        va_start(args, format);
        std::vfprintf(s_result, format, args);
        va_end(args);
        std::fflush(s_result);
    }
}

void say(const char* text) {
    print("%s\n", text);
}

/* A prompt: the console alone, since it means nothing in the file. */
void ask(const char* text) {
    std::printf("%s\n", text);
    std::fflush(stdout);
}

class Checker {
public:
    Checker(UdpSocket& socket, std::mt19937_64& random) : m_socket(socket), m_random(random) {
        for (uint8_t& b : m_id) {
            b = static_cast<uint8_t>(m_random());
        }
    }

    /* Asks the DHT how it sees this machine; prints what it found and
     * returns the verdict. */
    netcheck::Verdict run() {
        say("Asking servers on the internet how they see this computer. This takes up to "
            "a quarter of a minute.");
        const std::vector<Endpoint> bootstraps = resolve_bootstraps();
        if (bootstraps.empty()) {
            say("");
            say("Result: no internet.");
            say("The program could not look up the servers' names. Check that this computer "
                "is online, then run it again.");
            return {};
        }
        m_local.address = local_address_toward(bootstraps.front());
        m_local.port = m_socket.port();

        const auto start = Clock::now();
        auto next_bootstrap = start;
        auto next_ask = start;
        while (Clock::now() - start < kCheckTime && observers() < kEnoughObservers) {
            const auto now = Clock::now();
            if (m_replies == 0 && now >= next_bootstrap) {
                for (Endpoint b : bootstraps) {
                    ask(b);
                }
                next_bootstrap = now + kBootstrapRetry;
            }
            if (now >= next_ask) {
                ask_next_node();
                next_ask = now + kAskEvery;
            }
            uint8_t buffer[2048];
            Endpoint from;
            const int n = m_socket.receive(milliseconds{20}, buffer, sizeof buffer, from);
            if (n > 0) {
                read_reply(buffer, static_cast<std::size_t>(n), from);
            }
        }
        const netcheck::Verdict v = netcheck::judge(m_observations, m_replies, m_local);
        report(v);
        return v;
    }

    /* A query to a node that answered, so the router keeps the mapping. */
    void keep_alive() {
        if (!m_observations.empty()) {
            ask(m_observations.front().observer);
        }
    }

private:
    int observers() const {
        std::set<uint32_t> networks;
        for (const netcheck::Observation& o : m_observations) {
            networks.insert(network_of(o.observer));
        }
        return static_cast<int>(networks.size());
    }

    void ask(Endpoint node) {
        netcheck::NodeId target;
        for (uint8_t& b : target) {
            b = static_cast<uint8_t>(m_random());
        }
        const uint16_t transaction = ++m_transaction;
        m_pending[transaction] = node;
        const std::string query = netcheck::find_node_query(m_id, target, transaction);
        m_socket.send(node, query.data(), query.size());
    }

    /* The next node from the replies so far, from a network not asked yet,
     * so the answers come from as many different places as possible. */
    void ask_next_node() {
        while (!m_queue.empty()) {
            const Endpoint node = m_queue.front();
            m_queue.pop_front();
            if (node.port != 0 && netcheck::is_public(node.address) &&
                m_asked_networks.insert(network_of(node)).second)
            {
                ask(node);
                return;
            }
        }
        /* Every node named so far is asked, and many never answer: a node
         * that did answer is asked again, for a random target each time, so
         * it names others. Without this a run whose first names were stale
         * ran dry with too few answers for a verdict. */
        const auto now = Clock::now();
        if (!m_responders.empty() && now >= m_next_refill) {
            ask(m_responders[m_refills++ % m_responders.size()]);
            m_next_refill = now + kRefillEvery;
        }
    }

    void read_reply(const uint8_t* data, std::size_t size, Endpoint from) {
        const auto reply = netcheck::parse_reply(data, size);
        if (!reply) {
            return;
        }
        const auto pending = m_pending.find(reply->transaction);
        if (pending == m_pending.end() || !(pending->second == from)) {
            return;
        }
        m_pending.erase(pending);
        ++m_replies;
        if (std::find(m_responders.begin(), m_responders.end(), from) == m_responders.end()) {
            m_responders.push_back(from);
        }
        if (reply->seen_as) {
            m_observations.push_back({from, *reply->seen_as});
        }
        for (Endpoint node : reply->nodes) {
            m_queue.push_back(node);
        }
    }

    void report(const netcheck::Verdict& v) {
        say("");
        switch (v.mapping) {
        case netcheck::Mapping::Unreachable:
            say("Result: blocked.");
            say("No server on the internet answered. Something on this network blocks the kind "
                "of traffic the game uses (UDP): a firewall, security software, or a school or "
                "work network. Online play will not work from here as things stand.");
            break;
        case netcheck::Mapping::Unknown:
            say("Result: not sure.");
            say("Servers answered, but too few of them said how they see this computer. Run "
                "the check again. If it says this again, try the test with a friend below: it "
                "is the real proof.");
            break;
        case netcheck::Mapping::Consistent:
            say("Result: good.");
            say("Every server saw this computer at the same address and port, so your router "
                "keeps one port for everyone the game talks to. Direct Connect should work "
                "between you and a friend whose result is also good.");
            break;
        case netcheck::Mapping::Varies:
            say("Result: likely trouble.");
            if (v.seen_addresses > 1) {
                say("Different servers saw this computer at different public addresses: its "
                    "traffic leaves through more than one internet connection, or through a "
                    "VPN that spreads it.");
            } else {
                say("Different servers saw this computer at different ports. Your router gives "
                    "every destination a port of its own (symmetric NAT), so the game cannot "
                    "tell your friend where to send.");
            }
            say("Direct Connect may still work with a friend whose network is more open. The "
                "test with a friend below shows whether your two networks reach each other.");
            break;
        }
        if (v.no_nat) {
            say("This computer has a public address of its own: no router translates its "
                "traffic.");
        }
        say("");
        print("Details: this computer is %s. Servers that answered: %d. Networks that said how "
              "they see it: %d.\n",
            netcheck::to_string(m_local).c_str(), m_replies, v.observers);
        if (v.mapping == netcheck::Mapping::Varies) {
            std::map<std::pair<uint32_t, uint16_t>, int> seen;
            std::set<uint32_t> counted;
            for (const netcheck::Observation& o : m_observations) {
                if (counted.insert(network_of(o.observer)).second) {
                    ++seen[{o.seen_as.address, o.seen_as.port}];
                }
            }
            for (const auto& [endpoint, count] : seen) {
                print("Seen as %s by %d.\n",
                    netcheck::to_string(Endpoint{endpoint.first, endpoint.second}).c_str(), count);
            }
        } else if (v.public_endpoint) {
            print("Seen as %s.\n", netcheck::to_string(*v.public_endpoint).c_str());
        }
    }

    UdpSocket& m_socket;
    std::mt19937_64& m_random;
    netcheck::NodeId m_id{};
    Endpoint m_local;
    uint16_t m_transaction = 0;
    std::map<uint16_t, Endpoint> m_pending;
    std::deque<Endpoint> m_queue;
    std::set<uint32_t> m_asked_networks;
    std::vector<netcheck::Observation> m_observations;
    std::vector<Endpoint> m_responders;
    std::size_t m_refills = 0;
    Clock::time_point m_next_refill;
    int m_replies = 0;
};

uint32_t now_ms(Clock::time_point epoch) {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<milliseconds>(Clock::now() - epoch).count());
}

/* Answers a probe from anyone: a friend may start before this side has the
 * friend's address typed in. Returns whether data was a probe. */
bool answer_probe(UdpSocket& socket, const netcheck::Probe& p, uint64_t own_nonce, Endpoint from) {
    if (p.kind != netcheck::ProbeKind::Probe || p.nonce == own_nonce) {
        return false;
    }
    netcheck::Probe reply;
    reply.kind = netcheck::ProbeKind::Reply;
    reply.nonce = own_nonce;
    reply.echo_nonce = p.nonce;
    reply.echo_ms = p.sent_ms;
    const auto bytes = netcheck::encode_probe(reply);
    socket.send(from, bytes.data(), bytes.size());
    return true;
}

/* The test with a friend: probes to friend until a reply proves both
 * directions, then a few more seconds of round trips. */
void punch(
    UdpSocket& socket, Endpoint friend_endpoint, uint64_t own_nonce, Clock::time_point epoch) {
    print("Trying to reach %s for up to a minute. Your friend types your address now "
          "too.\n",
        netcheck::to_string(friend_endpoint).c_str());
    const auto start = Clock::now();
    auto next_probe = start;
    auto next_progress = start + milliseconds{15000};
    std::optional<Clock::time_point> contact;
    std::optional<Endpoint> heard_from;
    std::vector<uint32_t> round_trips;
    while (true) {
        const auto now = Clock::now();
        if (contact ? now - *contact >= kAfterContact : now - start >= kPunchTime) {
            break;
        }
        if (now >= next_probe) {
            netcheck::Probe p;
            p.nonce = own_nonce;
            p.sent_ms = now_ms(epoch);
            const auto bytes = netcheck::encode_probe(p);
            socket.send(friend_endpoint, bytes.data(), bytes.size());
            /* Their router may send from another port than the one they
             * read out; answer where their packets really come from too. */
            if (heard_from && !(*heard_from == friend_endpoint)) {
                socket.send(*heard_from, bytes.data(), bytes.size());
            }
            next_probe = now + kProbeEvery;
        }
        if (!contact && now >= next_progress) {
            print("Still trying, %d seconds so far.\n",
                static_cast<int>(
                    std::chrono::duration_cast<std::chrono::seconds>(now - start).count()));
            next_progress = now + milliseconds{15000};
        }
        uint8_t buffer[512];
        Endpoint from;
        const int n = socket.receive(milliseconds{20}, buffer, sizeof buffer, from);
        const auto p =
            n > 0 ? netcheck::decode_probe(buffer, static_cast<std::size_t>(n)) : std::nullopt;
        if (!p) {
            continue;
        }
        if (answer_probe(socket, *p, own_nonce, from)) {
            if (!heard_from) {
                heard_from = from;
                if (from == friend_endpoint) {
                    say("Your friend's packets are reaching you.");
                } else {
                    print("Your friend's packets are reaching you, from %s: their router "
                          "changed the port.\n",
                        netcheck::to_string(from).c_str());
                }
            }
        } else if (p->kind == netcheck::ProbeKind::Reply && p->echo_nonce == own_nonce) {
            round_trips.push_back(now_ms(epoch) - p->echo_ms);
            if (!contact) {
                contact = Clock::now();
                say("Your friend's program answered yours: connected both ways. Measuring the "
                    "round trip for a few seconds.");
            }
        }
    }
    say("");
    if (contact) {
        std::sort(round_trips.begin(), round_trips.end());
        say("Result: connected.");
        print("Packets got through both ways. The round trip took %u milliseconds "
              "(the slowest of %d took %u). Direct Connect should be able to connect "
              "you two.\n",
            round_trips[round_trips.size() / 2], static_cast<int>(round_trips.size()),
            round_trips.back());
    } else if (heard_from) {
        say("Result: one way only.");
        say("Your friend's packets reached you, but your answers did not reach them. Their "
            "router or firewall stops them; if their check said likely trouble, that is why.");
    } else {
        say("Result: no contact.");
        say("Nothing got through either way. Check that you both typed the other's address "
            "right while both programs were running: the address changes every time the "
            "program starts. If it was right, your two networks cannot reach each other this "
            "way.");
    }
}

struct Options {
    uint16_t port = 0;
    bool loopback = false;
    bool check = true;
    std::optional<Endpoint> friend_endpoint;
    bool ok = true;
};

Options parse_options(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            const int port = std::atoi(argv[++i]);
            o.ok = o.ok && port > 0 && port < 65536;
            o.port = static_cast<uint16_t>(port);
        } else if (arg == "--loopback") {
            /* Developer test: two copies on one machine, bound to 127.0.0.1
             * so no firewall prompt appears; the check needs the internet,
             * so it is skipped. */
            o.loopback = true;
            o.check = false;
        } else if (arg == "--no-check") {
            o.check = false;
        } else if (auto e = netcheck::parse_endpoint(arg)) {
            o.friend_endpoint = e;
        } else {
            o.ok = false;
        }
    }
    return o;
}

/* Says where the results are saved, on the console alone. */
void tell_result_file(const std::filesystem::path& place) {
    if (place.empty()) {
        ask("The results could not be saved to a file.");
    } else if (!place.has_parent_path()) {
        ask("The results are also saved in melee-netcheck-result.txt, in the folder this was "
            "started from.");
    } else {
        ask("The results are also saved in melee-netcheck-result.txt, beside this program.");
    }
}

int finish(int code) {
    if (console_is_ours()) {
        ask("Press Enter to close.");
        std::string line;
        std::getline(std::cin, line);
    }
    return code;
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    const std::filesystem::path result_file = open_result_file();
    char date[32] = "";
    const std::time_t now = std::time(nullptr);
    std::strftime(date, sizeof date, "%Y-%m-%d %H:%M", std::localtime(&now));
    print("Melee connection check, %s.\n", date);
    if (!options.ok) {
        say("Usage: melee-netcheck [--port N] [--no-check] [--loopback] [friend's address]");
        say("With no friend's address, it checks this network, then asks for one.");
        return finish(2);
    }
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        say("Windows networking did not start.");
        return finish(1);
    }
#endif
    if (!options.loopback) {
        say("If Windows asks whether to allow this program on networks, choose Allow.");
    }
    UdpSocket socket;
    if (!socket.open(options.port, options.loopback)) {
        say("Could not open a network port. If the game is running with the same port, close "
            "it first.");
        return finish(1);
    }
    std::random_device seed;
    std::mt19937_64 random(static_cast<uint64_t>(seed()) << 32 | seed());
    const uint64_t own_nonce = random();
    const auto epoch = Clock::now();

    Checker checker(socket, random);
    std::optional<Endpoint> own_public;
    if (options.check) {
        const netcheck::Verdict v = checker.run();
        tell_result_file(result_file);
        if (v.mapping == netcheck::Mapping::Unreachable) {
            return finish(1);
        }
        own_public = v.public_endpoint;
    }
    say("");

    if (options.friend_endpoint) {
        punch(socket, *options.friend_endpoint, own_nonce, epoch);
        return finish(0);
    }

    say("Test with a friend: you both run this program, then each types the other's address.");
    if (own_public) {
        const std::string text = netcheck::to_string(*own_public);
        print("Your address is %s.%s Keep this window open: the address only holds while "
              "the program runs.\n",
            text.c_str(), copy_to_clipboard(text) ? " It is copied to the clipboard." : "");
    } else {
        print("This computer listens on port %u.\n", socket.port());
    }
    ConsoleInput input;
    while (true) {
        ask("Type your friend's address and press Enter, or press Enter alone to close.");
        std::optional<Endpoint> target;
        auto next_keep_alive = Clock::now() + kKeepAlive;
        while (!target) {
            bool closed = false;
            const auto line = input.take(closed);
            if (closed) {
                return 0;
            }
            if (line) {
                if (line->find_first_not_of(" \t\r") == std::string::npos) {
                    return 0;
                }
                target = netcheck::parse_endpoint(*line);
                if (!target) {
                    ask("That is not an address like 203.0.113.5:51234. Type it again, or press "
                        "Enter alone to close.");
                } else if (own_public && *target == *own_public) {
                    ask("That is your own address. Type your friend's.");
                    target.reset();
                }
            }
            if (Clock::now() >= next_keep_alive) {
                checker.keep_alive();
                next_keep_alive = Clock::now() + kKeepAlive;
            }
            uint8_t buffer[512];
            Endpoint from;
            const int n = socket.receive(milliseconds{50}, buffer, sizeof buffer, from);
            if (n > 0) {
                if (const auto p = netcheck::decode_probe(buffer, static_cast<std::size_t>(n))) {
                    answer_probe(socket, *p, own_nonce, from);
                }
            }
        }
        punch(socket, *target, own_nonce, epoch);
        say("");
        say("To try again, both of you type the address again.");
    }
}
