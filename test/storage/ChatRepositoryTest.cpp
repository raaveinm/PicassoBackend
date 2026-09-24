//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#include <cstdint>
#include <string>

#include <boost/test/unit_test.hpp>

#include "storage/StorageTestDatabase.hpp"

using picasso::domain::ConversationId;
using picasso::domain::MessageId;
using picasso::test::StorageFixture;

BOOST_FIXTURE_TEST_SUITE(
    chat_repository,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    BOOST_AUTO_TEST_CASE(append_then_history_round_trips) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);

        const auto first_id = chat().append(conversation_id, alice, "hi bob");
        const auto second_id = chat().append(conversation_id, bob, "hi alice");

        // ?after={id} sync depends on ids growing strictly within a conversation.
        BOOST_TEST(first_id < second_id);

        const auto history = chat().historyAfter(conversation_id, MessageId(0), 100);
        BOOST_TEST_REQUIRE(history.size() == 2U);

        BOOST_TEST(history[0].id == first_id);
        BOOST_TEST(history[0].conversation_id == conversation_id);
        BOOST_TEST(history[0].sender_steam_id == alice);
        BOOST_TEST(history[0].text_message == "hi bob");
        BOOST_TEST(history[0].created_at_epoch_ms > static_cast<std::int64_t>(0));

        BOOST_TEST(history[1].id == second_id);
        BOOST_TEST(history[1].sender_steam_id == bob);
    }

    BOOST_AUTO_TEST_CASE(history_after_skips_seen_and_respects_limit) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);

        const auto first_id = chat().append(conversation_id, alice, "1");
        const auto second_id = chat().append(conversation_id, alice, "2");
        chat().append(conversation_id, alice, "3");

        const auto after_first = chat().historyAfter(conversation_id, first_id, 1);
        BOOST_TEST_REQUIRE(after_first.size() == 1U);
        BOOST_TEST(after_first[0].id == second_id);

        const auto latest = chat().historyAfter(conversation_id, second_id, 100);
        BOOST_TEST_REQUIRE(latest.size() == 1U);
        BOOST_TEST(latest[0].text_message == "3");
    }

    BOOST_AUTO_TEST_CASE(history_is_scoped_to_conversation) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto alice_bob = dm(alice, bob);
        const auto alice_carol = dm(alice, carol);

        chat().append(alice_bob, alice, "for bob");
        chat().append(alice_carol, alice, "for carol");

        const auto history = chat().historyAfter(alice_bob, MessageId(0), 100);
        BOOST_TEST_REQUIRE(history.size() == 1U);
        BOOST_TEST(history[0].text_message == "for bob");
    }

    // Text is bound as a parameter, never spliced into SQL.
    BOOST_AUTO_TEST_CASE(text_is_stored_verbatim) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        const std::string hostile = "'); DROP TABLE message_data; -- \"quotes\" и юникод 🎮";

        chat().append(conversation_id, alice, hostile);

        const auto history = chat().historyAfter(conversation_id, MessageId(0), 100);
        BOOST_TEST_REQUIRE(history.size() == 1U);
        BOOST_TEST(history[0].text_message == hostile);
    }

    BOOST_AUTO_TEST_CASE(append_to_missing_conversation_throws) {
        const auto alice = user(76561198000000001ULL);

        BOOST_CHECK_THROW(
            chat().append(ConversationId(999999), alice, "nowhere"),
            std::runtime_error
        );
    }

BOOST_AUTO_TEST_SUITE_END()
