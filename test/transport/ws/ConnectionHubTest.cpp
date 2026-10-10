//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include <memory>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "transport/ws/ConnectionHub.hpp"

using picasso::domain::ConnectionId;
using picasso::domain::SteamId;
using picasso::transport::ws::ConnectionHub;
using picasso::transport::ws::Outbound;

namespace {
    /* A connection that just remembers what it was sent. */
    class FakeConnection final : public Outbound {
    public:
        explicit FakeConnection(const std::uint64_t id) : id_(id) {}

        void send(const std::string& frame) override { received.push_back(frame); }

        ConnectionId connectionId() const override { return id_; }

        std::vector<std::string> received;

    private:
        ConnectionId id_;
    };
} // namespace

// "Everyone except the connection that sent it" is what keeps a sender from seeing its own
// message twice (once as the ack, once as a delivery) while its OTHER devices still get it.
BOOST_AUTO_TEST_SUITE(connection_hub)

    BOOST_AUTO_TEST_CASE(an_except_send_skips_only_the_named_connection) {
        ConnectionHub hub;
        const SteamId alice(1);
        const SteamId bob(2);
        const auto alice_phone = std::make_shared<FakeConnection>(10);
        const auto alice_desktop = std::make_shared<FakeConnection>(11);
        const auto bob_phone = std::make_shared<FakeConnection>(12);
        hub.add(alice, alice_phone);
        hub.add(alice, alice_desktop);
        hub.add(bob, bob_phone);

        hub.sendToAllExcept({alice, bob}, "frame", ConnectionId(10));

        BOOST_TEST(alice_phone->received.empty());                       // the sender: answered by an ack instead
        BOOST_TEST(alice_desktop->received.size() == 1U);                // her other device still gets it
        BOOST_TEST(bob_phone->received.size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(an_offline_member_is_simply_skipped) {
        ConnectionHub hub;
        const auto bob_phone = std::make_shared<FakeConnection>(1);
        hub.add(SteamId(2), bob_phone);

        hub.sendToAllExcept({SteamId(1), SteamId(2)}, "frame", ConnectionId(99));   // SteamId(1) has no socket

        BOOST_TEST(bob_phone->received.size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(a_removed_connection_receives_nothing_more) {
        ConnectionHub hub;
        const SteamId alice(1);
        const auto phone = std::make_shared<FakeConnection>(1);
        hub.add(alice, phone);
        hub.remove(alice, phone.get());

        hub.sendToAll({alice}, "frame");

        BOOST_TEST(phone->received.empty());
        BOOST_TEST(!hub.isOnline(alice));
    }

BOOST_AUTO_TEST_SUITE_END()
