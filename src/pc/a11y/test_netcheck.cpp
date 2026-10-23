/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The connection checker's pure parts: addresses as players type them, DHT
 * replies written by hand in bencode, the verdict from sets of observations,
 * and the probe packets' round trip through bytes. */
#include "netcheck_logic.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using netcheck::Endpoint;
using netcheck::Mapping;
using netcheck::Observation;

Endpoint ep(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) {
    return Endpoint{static_cast<uint32_t>(a) << 24 | static_cast<uint32_t>(b) << 16 |
                        static_cast<uint32_t>(c) << 8 | d,
        port};
}

std::string compact(Endpoint e) {
    std::string s;
    s += static_cast<char>(e.address >> 24);
    s += static_cast<char>(e.address >> 16);
    s += static_cast<char>(e.address >> 8);
    s += static_cast<char>(e.address);
    s += static_cast<char>(e.port >> 8);
    s += static_cast<char>(e.port);
    return s;
}

std::string bstring(const std::string& s) {
    return std::to_string(s.size()) + ":" + s;
}

std::optional<netcheck::KrpcReply> parse(const std::string& s) {
    return netcheck::parse_reply(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void addresses_parse_as_typed() {
    assert(netcheck::parse_endpoint("203.0.113.5:51234") == ep(203, 0, 113, 5, 51234));
    assert(netcheck::parse_endpoint("  203.0.113.5:51234 \r\n") == ep(203, 0, 113, 5, 51234));
    assert(netcheck::parse_endpoint("203.0.113.5:51234.") == ep(203, 0, 113, 5, 51234));
    assert(!netcheck::parse_endpoint("203.0.113.5:51234.."));
    assert(!netcheck::parse_endpoint("203.0.113.5"));
    assert(!netcheck::parse_endpoint("203.0.113:51234"));
    assert(!netcheck::parse_endpoint("203.0.113.256:51234"));
    assert(!netcheck::parse_endpoint("203.0.113.5:0"));
    assert(!netcheck::parse_endpoint("203.0.113.5:65536"));
    assert(!netcheck::parse_endpoint("203..113.5:80"));
    assert(!netcheck::parse_endpoint("melee#ABC:80"));
    assert(!netcheck::parse_endpoint(""));
    assert(netcheck::to_string(ep(203, 0, 113, 5, 51234)) == "203.0.113.5:51234");
}

void private_ranges_are_not_public() {
    assert(netcheck::is_public(ep(203, 0, 113, 5, 1).address));
    assert(!netcheck::is_public(ep(10, 1, 2, 3, 1).address));
    assert(!netcheck::is_public(ep(192, 168, 1, 1, 1).address));
    assert(!netcheck::is_public(ep(172, 16, 0, 1, 1).address));
    assert(!netcheck::is_public(ep(100, 64, 0, 1, 1).address));
    assert(!netcheck::is_public(ep(127, 0, 0, 1, 1).address));
    assert(!netcheck::is_public(ep(169, 254, 0, 1, 1).address));
    assert(!netcheck::is_public(ep(224, 0, 0, 251, 1).address));
}

void query_is_sorted_bencode() {
    netcheck::NodeId id{};
    netcheck::NodeId target{};
    id.fill('i');
    target.fill('t');
    const std::string q = netcheck::find_node_query(id, target, 0x0102);
    const std::string expected = "d1:ad2:id20:" + std::string(20, 'i') +
                                 "6:target20:" + std::string(20, 't') +
                                 "e1:q9:find_node1:t2:\x01\x02" + "1:y1:qe";
    assert(q == expected);
}

void reply_gives_seen_as_and_nodes() {
    const Endpoint seen = ep(198, 51, 100, 7, 40000);
    const Endpoint node_a = ep(203, 0, 113, 1, 6881);
    const Endpoint node_b = ep(192, 0, 2, 9, 51413);
    const std::string nodes =
        std::string(20, 'a') + compact(node_a) + std::string(20, 'b') + compact(node_b);
    const std::string reply = "d2:ip" + bstring(compact(seen)) + "1:rd2:id" +
                              bstring(std::string(20, 'n')) + "5:nodes" + bstring(nodes) +
                              "5:token" + bstring("xyz") + "e1:t2:\x7f\x01" + "1:v4:LT\x01\x02" +
                              "1:y1:re";
    const auto r = parse(reply);
    assert(r);
    assert(r->transaction == 0x7f01);
    assert(r->seen_as == seen);
    assert(r->nodes.size() == 2);
    assert(r->nodes[0] == node_a);
    assert(r->nodes[1] == node_b);
}

void reply_without_ip_still_counts() {
    const auto r = parse("d1:rd2:id" + bstring(std::string(20, 'n')) + "e1:t2:ab1:y1:re");
    assert(r);
    assert(!r->seen_as);
    assert(r->nodes.empty());
}

void queries_errors_and_junk_are_refused() {
    assert(!parse("d1:ad2:id" + bstring(std::string(20, 'n')) + "e1:q4:ping1:t2:ab1:y1:qe"));
    assert(!parse("d1:eli201e5:Erroree1:t2:ab1:y1:ee"));
    assert(!parse("d1:t2:ab1:y1:r"));   /* cut short */
    assert(!parse("d1:t3:abc1:y1:re")); /* not this checker's transaction */
    assert(!parse("d1:t2:ab1:y99:re")); /* a length past the end */
    assert(!parse(""));
    assert(!parse("MNC1"));
    /* Well formed, but nested past the depth limit. */
    std::string deep = "d1:t2:ab1:y1:r1:x";
    deep += std::string(64, 'l') + std::string(64, 'e') + "e";
    assert(!parse(deep));
}

Observation seen_by(uint8_t network, Endpoint seen_as) {
    return Observation{ep(203, 0, network, 1, 6881), seen_as};
}

void one_port_everywhere_is_consistent() {
    const Endpoint pub = ep(198, 51, 100, 7, 40000);
    const Endpoint local = ep(192, 168, 1, 5, 40000);
    const std::vector<Observation> obs = {seen_by(1, pub), seen_by(2, pub), seen_by(3, pub)};
    const auto v = netcheck::judge(obs, 5, local);
    assert(v.mapping == Mapping::Consistent);
    assert(v.public_endpoint == pub);
    assert(v.observers == 3);
    assert(!v.no_nat);
}

void ports_that_differ_vary() {
    const std::vector<Observation> obs = {seen_by(1, ep(198, 51, 100, 7, 40000)),
        seen_by(2, ep(198, 51, 100, 7, 40001)), seen_by(3, ep(198, 51, 100, 7, 40000))};
    const auto v = netcheck::judge(obs, 3, {});
    assert(v.mapping == Mapping::Varies);
    assert(v.seen_ports == 2);
    assert(v.seen_addresses == 1);
    assert(v.public_endpoint == ep(198, 51, 100, 7, 40000));
    assert(v.votes.size() == 2);
    assert(v.votes.at(ep(198, 51, 100, 7, 40000)) == 2);
    assert(v.votes.at(ep(198, 51, 100, 7, 40001)) == 1);
}

void addresses_that_differ_vary() {
    const std::vector<Observation> obs = {seen_by(1, ep(198, 51, 100, 7, 40000)),
        seen_by(2, ep(198, 51, 100, 8, 40000)), seen_by(3, ep(198, 51, 100, 7, 40000))};
    const auto v = netcheck::judge(obs, 3, {});
    assert(v.mapping == Mapping::Varies);
    assert(v.seen_addresses == 2);
}

void one_odd_answer_is_outvoted() {
    /* Seven networks agree and one disagrees: a sound router with one stale
     * node among the observers. The odd answer still has its vote. */
    const Endpoint pub = ep(198, 51, 100, 7, 40000);
    std::vector<Observation> obs;
    for (uint8_t n = 1; n <= 7; ++n) {
        obs.push_back(seen_by(n, pub));
    }
    obs.push_back(seen_by(8, ep(198, 51, 100, 7, 40001)));
    const auto v = netcheck::judge(obs, 8, {});
    assert(v.mapping == Mapping::Consistent);
    assert(v.public_endpoint == pub);
    assert(v.votes.size() == 2);
}

void a_bare_majority_still_varies() {
    /* Three of five agree: enough networks, but too many disagree. */
    const Endpoint pub = ep(198, 51, 100, 7, 40000);
    const std::vector<Observation> obs = {seen_by(1, pub), seen_by(2, pub), seen_by(3, pub),
        seen_by(4, ep(198, 51, 100, 7, 40001)), seen_by(5, ep(198, 51, 100, 7, 40002))};
    assert(netcheck::judge(obs, 5, {}).mapping == Mapping::Varies);
}

void one_network_has_one_voice() {
    /* Three answers from one network and a disagreeing one from another are
     * two observers, too few for a verdict. */
    const Endpoint pub = ep(198, 51, 100, 7, 40000);
    std::vector<Observation> obs = {seen_by(1, pub),
        Observation{ep(203, 0, 1, 2, 6881), ep(198, 51, 100, 7, 1)},
        Observation{ep(203, 0, 1, 3, 6881), ep(198, 51, 100, 7, 2)}, seen_by(2, pub)};
    const auto v = netcheck::judge(obs, 4, {});
    assert(v.observers == 2);
    assert(v.mapping == Mapping::Unknown);
    assert(v.public_endpoint == pub);
}

void a_nodes_mistake_is_left_out() {
    /* A "seen as" with port 0 or a private address is a node's error, as the
     * game's DHT treats it: it neither votes nor makes the verdict vary. */
    const Endpoint pub = ep(198, 51, 100, 7, 40000);
    const std::vector<Observation> obs = {seen_by(1, pub), seen_by(2, pub), seen_by(3, pub),
        seen_by(4, ep(198, 51, 100, 7, 0)), seen_by(5, ep(192, 168, 1, 5, 40000))};
    const auto v = netcheck::judge(obs, 5, {});
    assert(v.mapping == Mapping::Consistent);
    assert(v.observers == 3);
}

void silence_and_too_few() {
    assert(netcheck::judge({}, 0, {}).mapping == Mapping::Unreachable);
    assert(!netcheck::judge({}, 0, {}).public_endpoint);
    assert(netcheck::judge({}, 4, {}).mapping == Mapping::Unknown);
}

void own_public_address_means_no_nat() {
    const Endpoint pub = ep(198, 51, 100, 7, 40000);
    const std::vector<Observation> obs = {seen_by(1, pub), seen_by(2, pub), seen_by(3, pub)};
    assert(netcheck::judge(obs, 3, pub).no_nat);
}

void probes_round_trip() {
    netcheck::Probe p;
    p.kind = netcheck::ProbeKind::Reply;
    p.nonce = 0x0102030405060708u;
    p.echo_nonce = 0xF1F2F3F4F5F6F7F8u;
    p.sent_ms = 123456;
    p.echo_ms = 0xFFFFFFF0u;
    const auto bytes = netcheck::encode_probe(p);
    const auto back = netcheck::decode_probe(bytes.data(), bytes.size());
    assert(back);
    assert(back->kind == p.kind);
    assert(back->nonce == p.nonce);
    assert(back->echo_nonce == p.echo_nonce);
    assert(back->sent_ms == p.sent_ms);
    assert(back->echo_ms == p.echo_ms);

    assert(!netcheck::decode_probe(bytes.data(), bytes.size() - 1));
    auto wrong_magic = bytes;
    wrong_magic[0] = 'X';
    assert(!netcheck::decode_probe(wrong_magic.data(), wrong_magic.size()));
    auto wrong_kind = bytes;
    wrong_kind[4] = 'Q';
    assert(!netcheck::decode_probe(wrong_kind.data(), wrong_kind.size()));
}

}  // namespace

int main() {
    addresses_parse_as_typed();
    private_ranges_are_not_public();
    query_is_sorted_bencode();
    reply_gives_seen_as_and_nodes();
    reply_without_ip_still_counts();
    queries_errors_and_junk_are_refused();
    one_port_everywhere_is_consistent();
    ports_that_differ_vary();
    addresses_that_differ_vary();
    one_odd_answer_is_outvoted();
    a_bare_majority_still_varies();
    one_network_has_one_voice();
    a_nodes_mistake_is_left_out();
    silence_and_too_few();
    own_public_address_means_no_nat();
    probes_round_trip();
    std::cout << "netcheck: all tests passed\n";
    return 0;
}
