/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* The connection checker's pure parts: the DHT messages it sends and reads,
 * the verdict on how this machine's router maps its ports, and the probe
 * packets of the two-person test. No sockets here, so the unit test drives
 * them with hand-made bytes. netcheck.cpp holds the program around them. */

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace netcheck {

/* An IPv4 address and UDP port, both in host byte order. */
struct Endpoint {
    uint32_t address = 0;
    uint16_t port = 0;

    auto operator<=>(const Endpoint&) const = default;
};

std::string to_string(Endpoint e);
/* "a.b.c.d:port", spaces around it allowed, and a full stop after it, as
 * when the sentence that printed it is copied whole; nothing else. */
std::optional<Endpoint> parse_endpoint(std::string_view text);
/* The /24 an address belongs to: answers from one network count once. */
uint32_t network_of(Endpoint e);
/* Not one of the private, loopback, link-local, shared (carrier-grade NAT)
 * or multicast ranges: the same test as the game's DHT (public_ip in
 * net_dht.c), so a node the game would not talk to is not asked here. */
bool is_public(uint32_t address);

using NodeId = std::array<uint8_t, 20>;

/* A KRPC find_node query (BEP 5) under the two-byte transaction id. A reply
 * carries the nodes nearest the target, and, from most nodes, the address
 * and port the reply's sender saw this query come from (BEP 42's "ip"). */
std::string find_node_query(const NodeId& id, const NodeId& target, uint16_t transaction);

struct KrpcReply {
    uint16_t transaction = 0;
    std::optional<Endpoint> seen_as;
    std::vector<Endpoint> nodes;
};

/* A KRPC reply ("y" is "r") with a two-byte transaction id; anything else,
 * a query or an error or bytes that are not bencode, gives nothing. */
std::optional<KrpcReply> parse_reply(const uint8_t* data, std::size_t size);

/* One DHT node's answer: who answered, and how it saw this machine. */
struct Observation {
    Endpoint observer;
    Endpoint seen_as;
};

enum class Mapping {
    Unreachable, /* no DHT node answered at all */
    Unknown,     /* answers, but too few said how they saw us */
    Consistent,  /* nearly every node saw one address and port */
    Varies,      /* the port, or the address, differed between nodes */
};

struct Verdict {
    Mapping mapping = Mapping::Unreachable;
    /* The address and port most nodes saw; set once any node said. */
    std::optional<Endpoint> public_endpoint;
    /* Each address and port seen, with how many networks saw it. */
    std::map<Endpoint, int> votes;
    int observers = 0;  /* distinct networks that said how they saw us */
    int seen_ports = 0; /* distinct ports among them */
    int seen_addresses = 0;
    bool no_nat = false; /* the public address is this machine's own */
};

/* Fewest observers, each from its own network, that a verdict trusts; as
 * many must agree on one answer for Consistent, and at least this share of
 * all of them. */
constexpr int kMinObservers = 3;
struct Share {
    int agree;
    int out_of;
};
constexpr Share kAgreeShare{3, 4};

/* replies counts every answer, with or without a "seen as"; local is the
 * socket's own address and port, its address 0 when it is not known. */
Verdict judge(const std::vector<Observation>& observations, int replies, Endpoint local);

/* The two-person test's packets: a probe goes to the friend every so often,
 * and a reply answers each probe at once, echoing the probe's nonce and
 * clock, so a reply proves both directions and times the round trip. */
enum class ProbeKind : uint8_t {
    Probe = 'P',
    Reply = 'R',
};

struct Probe {
    ProbeKind kind = ProbeKind::Probe;
    uint64_t nonce = 0;      /* the sender's, for this run */
    uint64_t echo_nonce = 0; /* a reply: the probe's nonce */
    uint32_t sent_ms = 0;    /* a probe: the sender's clock */
    uint32_t echo_ms = 0;    /* a reply: the probe's sent_ms */
};

constexpr std::size_t kProbeBytes = 32;

std::array<uint8_t, kProbeBytes> encode_probe(const Probe& p);
std::optional<Probe> decode_probe(const uint8_t* data, std::size_t size);

}  // namespace netcheck
