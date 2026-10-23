/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "netcheck_logic.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

namespace netcheck {

namespace {

/* "MNC1": the checker's own magic, so a game packet or a stray datagram on
 * the port is never read as a probe. A change to the layout changes it. */
constexpr std::array<uint8_t, 4> kProbeMagic{'M', 'N', 'C', '1'};

/* Deep enough for any KRPC reply; a deeper value is not one. */
constexpr int kMaxDepth = 8;

struct Cursor {
    const uint8_t* p;
    const uint8_t* end;

    bool at(char c) const { return p < end && *p == static_cast<uint8_t>(c); }
};

std::optional<std::string_view> read_string(Cursor& c) {
    std::size_t length = 0;
    const uint8_t* start = c.p;
    while (c.p < c.end && *c.p >= '0' && *c.p <= '9') {
        length = length * 10 + static_cast<std::size_t>(*c.p - '0');
        if (length > static_cast<std::size_t>(c.end - start)) {
            return std::nullopt;
        }
        ++c.p;
    }
    if (c.p == start || !c.at(':')) {
        return std::nullopt;
    }
    ++c.p;
    if (static_cast<std::size_t>(c.end - c.p) < length) {
        return std::nullopt;
    }
    std::string_view s(reinterpret_cast<const char*>(c.p), length);
    c.p += length;
    return s;
}

bool skip_value(Cursor& c, int depth) {
    if (depth > kMaxDepth || c.p >= c.end) {
        return false;
    }
    if (c.at('i')) {
        ++c.p;
        while (c.p < c.end && !c.at('e')) {
            ++c.p;
        }
        if (!c.at('e')) {
            return false;
        }
        ++c.p;
        return true;
    }
    if (c.at('l') || c.at('d')) {
        const bool dict = c.at('d');
        ++c.p;
        while (!c.at('e')) {
            if (dict && !read_string(c)) {
                return false;
            }
            if (!skip_value(c, depth + 1)) {
                return false;
            }
        }
        ++c.p;
        return true;
    }
    return read_string(c).has_value();
}

uint32_t get32(const uint8_t* b) {
    return static_cast<uint32_t>(b[0]) << 24 | static_cast<uint32_t>(b[1]) << 16 |
           static_cast<uint32_t>(b[2]) << 8 | b[3];
}

Endpoint compact_endpoint(const uint8_t* b) {
    return Endpoint{get32(b), static_cast<uint16_t>(b[4] << 8 | b[5])};
}

/* The "r" dictionary: only its compact node list matters here. */
bool read_response(Cursor& c, KrpcReply& reply) {
    if (!c.at('d')) {
        return false;
    }
    ++c.p;
    while (!c.at('e')) {
        auto key = read_string(c);
        if (!key) {
            return false;
        }
        if (*key == "nodes" && c.p < c.end && *c.p >= '0' && *c.p <= '9') {
            auto nodes = read_string(c);
            if (!nodes) {
                return false;
            }
            /* 26 bytes each: a 20-byte node id, then the compact endpoint. */
            const auto* b = reinterpret_cast<const uint8_t*>(nodes->data());
            for (std::size_t i = 0; i + 26 <= nodes->size(); i += 26) {
                reply.nodes.push_back(compact_endpoint(b + i + 20));
            }
        } else if (!skip_value(c, 1)) {
            return false;
        }
    }
    ++c.p;
    return true;
}

void put32(uint8_t* b, uint32_t v) {
    b[0] = static_cast<uint8_t>(v >> 24);
    b[1] = static_cast<uint8_t>(v >> 16);
    b[2] = static_cast<uint8_t>(v >> 8);
    b[3] = static_cast<uint8_t>(v);
}

void put64(uint8_t* b, uint64_t v) {
    put32(b, static_cast<uint32_t>(v >> 32));
    put32(b + 4, static_cast<uint32_t>(v));
}

uint64_t get64(const uint8_t* b) {
    return static_cast<uint64_t>(get32(b)) << 32 | get32(b + 4);
}

}  // namespace

std::string to_string(Endpoint e) {
    char text[32];
    std::snprintf(text, sizeof text, "%u.%u.%u.%u:%u", e.address >> 24, (e.address >> 16) & 0xFF,
        (e.address >> 8) & 0xFF, e.address & 0xFF, e.port);
    return text;
}

std::optional<Endpoint> parse_endpoint(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' ||
                                text.back() == '\n'))
    {
        text.remove_suffix(1);
    }
    if (!text.empty() && text.back() == '.') {
        text.remove_suffix(1);
    }
    uint32_t parts[5] = {};
    int part = 0;
    int digits = 0;
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            parts[part] = parts[part] * 10 + static_cast<uint32_t>(ch - '0');
            if (++digits > 5) {
                return std::nullopt;
            }
        } else if ((ch == '.' && part < 3) || (ch == ':' && part == 3)) {
            if (digits == 0) {
                return std::nullopt;
            }
            ++part;
            digits = 0;
        } else {
            return std::nullopt;
        }
    }
    if (part != 4 || digits == 0 || parts[4] == 0 || parts[4] > 65535) {
        return std::nullopt;
    }
    Endpoint e;
    for (int i = 0; i < 4; ++i) {
        if (parts[i] > 255) {
            return std::nullopt;
        }
        e.address = e.address << 8 | parts[i];
    }
    e.port = static_cast<uint16_t>(parts[4]);
    return e;
}

uint32_t network_of(Endpoint e) {
    return e.address & 0xFFFFFF00u;
}

bool is_public(uint32_t ip) {
    return (ip >> 24) != 0 && (ip >> 24) != 10 && (ip >> 24) != 127 && (ip >> 28) < 14 &&
           (ip >> 16) != 0xa9fe && (ip >> 20) != 0xac1 && (ip >> 16) != 0xc0a8 &&
           (ip >> 22) != 0x191;
}

std::string find_node_query(const NodeId& id, const NodeId& target, uint16_t transaction) {
    /* Bencoded dictionaries keep their keys sorted: a, q, t, y. */
    std::string q = "d1:ad2:id20:";
    q.append(reinterpret_cast<const char*>(id.data()), id.size());
    q += "6:target20:";
    q.append(reinterpret_cast<const char*>(target.data()), target.size());
    q += "e1:q9:find_node1:t2:";
    q += static_cast<char>(transaction >> 8);
    q += static_cast<char>(transaction & 0xFF);
    q += "1:y1:qe";
    return q;
}

std::optional<KrpcReply> parse_reply(const uint8_t* data, std::size_t size) {
    Cursor c{data, data + size};
    if (!c.at('d')) {
        return std::nullopt;
    }
    ++c.p;
    KrpcReply reply;
    bool have_transaction = false;
    bool is_response = false;
    while (!c.at('e')) {
        auto key = read_string(c);
        if (!key) {
            return std::nullopt;
        }
        if (*key == "r") {
            if (!read_response(c, reply)) {
                return std::nullopt;
            }
            continue;
        }
        if (*key != "t" && *key != "y" && *key != "ip") {
            if (!skip_value(c, 1)) {
                return std::nullopt;
            }
            continue;
        }
        auto value = read_string(c);
        if (!value) {
            return std::nullopt;
        }
        if (*key == "t" && value->size() == 2) {
            reply.transaction = static_cast<uint16_t>(
                static_cast<uint8_t>((*value)[0]) << 8 | static_cast<uint8_t>((*value)[1]));
            have_transaction = true;
        } else if (*key == "y") {
            is_response = *value == "r";
        } else if (*key == "ip" && value->size() == 6) {
            reply.seen_as = compact_endpoint(reinterpret_cast<const uint8_t*>(value->data()));
        }
    }
    if (!have_transaction || !is_response) {
        return std::nullopt;
    }
    return reply;
}

Verdict judge(const std::vector<Observation>& observations, int replies, Endpoint local) {
    Verdict v;
    if (replies == 0) {
        return v;
    }
    /* The rules of the game's own vote (observe and pc_dht_external_endpoint
     * in net_dht.c): a "seen as" with port 0 or an address that is not
     * public is a node's mistake and left out, and each network has one
     * voice, so several nodes behind one address cannot outvote the rest. */
    std::set<uint32_t> networks;
    std::set<uint16_t> ports;
    std::set<uint32_t> addresses;
    for (const Observation& o : observations) {
        if (o.seen_as.port == 0 || !is_public(o.seen_as.address) ||
            !networks.insert(network_of(o.observer)).second)
        {
            continue;
        }
        ++v.votes[o.seen_as];
        ports.insert(o.seen_as.port);
        addresses.insert(o.seen_as.address);
    }
    v.observers = static_cast<int>(networks.size());
    v.seen_ports = static_cast<int>(ports.size());
    v.seen_addresses = static_cast<int>(addresses.size());
    int agreeing = 0;
    if (!v.votes.empty()) {
        auto best = std::max_element(v.votes.begin(), v.votes.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        v.public_endpoint = best->first;
        v.no_nat = local.address != 0 && v.public_endpoint->address == local.address;
        agreeing = best->second;
    }
    /* One node with a stale or wrong answer must not make a sound router
     * look bad, so the most common answer stands when enough networks give
     * it; a router that gives each destination a port of its own disagrees
     * almost everywhere. */
    if (v.observers < kMinObservers) {
        v.mapping = Mapping::Unknown;
    } else if (agreeing >= kMinObservers &&
               agreeing * kAgreeShare.out_of >= v.observers * kAgreeShare.agree)
    {
        v.mapping = Mapping::Consistent;
    } else {
        v.mapping = Mapping::Varies;
    }
    return v;
}

std::array<uint8_t, kProbeBytes> encode_probe(const Probe& p) {
    std::array<uint8_t, kProbeBytes> b{};
    std::copy(kProbeMagic.begin(), kProbeMagic.end(), b.begin());
    b[4] = static_cast<uint8_t>(p.kind);
    put64(&b[8], p.nonce);
    put64(&b[16], p.echo_nonce);
    put32(&b[24], p.sent_ms);
    put32(&b[28], p.echo_ms);
    return b;
}

std::optional<Probe> decode_probe(const uint8_t* data, std::size_t size) {
    if (size != kProbeBytes || !std::equal(kProbeMagic.begin(), kProbeMagic.end(), data)) {
        return std::nullopt;
    }
    if (data[4] != static_cast<uint8_t>(ProbeKind::Probe) &&
        data[4] != static_cast<uint8_t>(ProbeKind::Reply))
    {
        return std::nullopt;
    }
    Probe p;
    p.kind = static_cast<ProbeKind>(data[4]);
    p.nonce = get64(data + 8);
    p.echo_nonce = get64(data + 16);
    p.sent_ms = get32(data + 24);
    p.echo_ms = get32(data + 28);
    return p;
}

}  // namespace netcheck
