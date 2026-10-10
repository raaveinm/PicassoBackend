//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#include <cstdint>
#include <string>

#include <boost/test/unit_test.hpp>

#include "storage/StorageTestDatabase.hpp"

using picasso::domain::ChatRepository;
using picasso::domain::ConversationId;
using picasso::domain::MessageId;
using picasso::test::StorageFixture;

namespace {
    constexpr std::int64_t NOW = 1'790'000'000'000;
} // namespace

// The repository is the bottom of the sync contract: ids must grow within a conversation (a
// cursor is just "the highest id I hold"), a retry must find the original instead of
// inserting a second one, and a deleted message must vanish from every read yet keep
// blocking a duplicate. Each of those has its own cases below.
BOOST_FIXTURE_TEST_SUITE(
    chat_repository,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    ///////////////////////////////////////////////
    /// Appending
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(append_then_read_round_trips) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);

        const auto first = chat().append(conversation_id, alice, "c1", "hi bob", NOW);
        const auto second = chat().append(conversation_id, bob, "c2", "hi alice", NOW + 1);

        BOOST_TEST(!first.duplicate);
        BOOST_TEST(first.message.id < second.message.id);       // a cursor depends on ids growing

        const auto history = chat().newest(conversation_id, MessageId(0), 100);
        BOOST_TEST_REQUIRE(history.size() == 2U);
        BOOST_TEST(history[0].id == first.message.id);
        BOOST_TEST(history[0].conversation_id == conversation_id);
        BOOST_TEST(history[0].sender_steam_id == alice);
        BOOST_TEST(history[0].client_message_id == "c1");
        BOOST_TEST(history[0].text_message == "hi bob");
        BOOST_TEST(history[0].created_at_epoch_ms == NOW);
        BOOST_TEST(history[1].sender_steam_id == bob);
    }

    BOOST_AUTO_TEST_CASE(a_retry_returns_the_original_instead_of_a_second_row) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);

        const auto first = chat().append(conversation_id, alice, "same-key", "hello", NOW);
        const auto retry = chat().append(conversation_id, alice, "same-key", "hello", NOW + 5'000);

        BOOST_TEST(!first.duplicate);
        BOOST_TEST(retry.duplicate);
        BOOST_TEST(retry.message.id == first.message.id);
        BOOST_TEST(retry.message.created_at_epoch_ms == NOW);   // the original's timestamp, not the retry's
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(the_key_is_scoped_to_conversation_and_sender) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto alice_bob = dm(alice, bob);
        const auto alice_carol = dm(alice, carol);

        // the same UUID from another sender, or in another conversation, is a different message
        BOOST_TEST(!chat().append(alice_bob, alice, "k", "a", NOW).duplicate);
        BOOST_TEST(!chat().append(alice_bob, bob, "k", "b", NOW).duplicate);
        BOOST_TEST(!chat().append(alice_carol, alice, "k", "c", NOW).duplicate);
        BOOST_TEST(chat().newest(alice_bob, MessageId(0), 100).size() == 2U);
    }

    // Text is bound as a parameter, never spliced into SQL.
    BOOST_AUTO_TEST_CASE(text_is_stored_verbatim) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        const std::string hostile = "'); DROP TABLE message_data; -- \"quotes\" и юникод 🎮";

        chat().append(conversation_id, alice, "c", hostile, NOW);

        const auto history = chat().newest(conversation_id, MessageId(0), 100);
        BOOST_TEST_REQUIRE(history.size() == 1U);
        BOOST_TEST(history[0].text_message == hostile);
    }

    BOOST_AUTO_TEST_CASE(append_to_missing_conversation_throws) {
        const auto alice = user(76561198000000001ULL);

        BOOST_CHECK_THROW(
            chat().append(ConversationId(999999), alice, "c", "nowhere", NOW),
            std::runtime_error
        );
    }

    ///////////////////////////////////////////////
    /// Reading
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(newest_returns_the_tail_ascending) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        for (int i = 1; i <= 5; ++i) {
            chat().append(conversation_id, alice, "c" + std::to_string(i), std::to_string(i), NOW + i);
        }

        const auto tail = chat().newest(conversation_id, MessageId(0), 3);

        // the NEWEST three, oldest first - which is what a first load and a `reset` need
        BOOST_TEST_REQUIRE(tail.size() == 3U);
        BOOST_TEST(tail[0].text_message == "3");
        BOOST_TEST(tail[1].text_message == "4");
        BOOST_TEST(tail[2].text_message == "5");
    }

    BOOST_AUTO_TEST_CASE(newest_after_a_cursor_skips_what_the_client_holds) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        const auto first = chat().append(conversation_id, alice, "c1", "1", NOW);
        chat().append(conversation_id, alice, "c2", "2", NOW);
        chat().append(conversation_id, alice, "c3", "3", NOW);

        const auto after = chat().newest(conversation_id, first.message.id, 100);

        BOOST_TEST_REQUIRE(after.size() == 2U);
        BOOST_TEST(after[0].text_message == "2");
        BOOST_TEST(after[1].text_message == "3");
    }

    BOOST_AUTO_TEST_CASE(older_than_pages_backwards) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        MessageId fifth;
        for (int i = 1; i <= 6; ++i) {
            const auto stored = chat().append(conversation_id, alice, "c" + std::to_string(i), std::to_string(i), NOW);
            if (i == 5) fifth = stored.message.id;
        }

        const auto page = chat().olderThan(conversation_id, fifth, 2);

        // the two just below the fifth: 3 and 4, oldest first
        BOOST_TEST_REQUIRE(page.size() == 2U);
        BOOST_TEST(page[0].text_message == "3");
        BOOST_TEST(page[1].text_message == "4");
    }

    BOOST_AUTO_TEST_CASE(reads_are_scoped_to_the_conversation) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto alice_bob = dm(alice, bob);
        const auto alice_carol = dm(alice, carol);

        chat().append(alice_bob, alice, "c1", "for bob", NOW);
        chat().append(alice_carol, alice, "c2", "for carol", NOW);

        const auto history = chat().newest(alice_bob, MessageId(0), 100);
        BOOST_TEST_REQUIRE(history.size() == 1U);
        BOOST_TEST(history[0].text_message == "for bob");
    }

    ///////////////////////////////////////////////
    /// Deleting
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(a_deleted_message_disappears_from_every_read) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        const auto keep = chat().append(conversation_id, alice, "c1", "keep", NOW);
        const auto drop = chat().append(conversation_id, alice, "c2", "drop", NOW);

        BOOST_TEST(chat().softDelete(conversation_id, drop.message.id, alice, NOW + 1) == ChatRepository::DeleteOutcome::Deleted);

        const auto tail = chat().newest(conversation_id, MessageId(0), 100);
        BOOST_TEST_REQUIRE(tail.size() == 1U);
        BOOST_TEST(tail[0].id == keep.message.id);
        BOOST_TEST(chat().olderThan(conversation_id, MessageId(1'000'000), 100).size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(only_the_sender_can_delete_and_deleting_twice_is_fine) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        const auto stored = chat().append(conversation_id, alice, "c1", "mine", NOW);

        BOOST_TEST(chat().softDelete(conversation_id, stored.message.id, bob, NOW) == ChatRepository::DeleteOutcome::NotFound);
        BOOST_TEST(chat().softDelete(conversation_id, MessageId(424242), alice, NOW) == ChatRepository::DeleteOutcome::NotFound);
        BOOST_TEST(chat().softDelete(conversation_id, stored.message.id, alice, NOW) == ChatRepository::DeleteOutcome::Deleted);
        BOOST_TEST(chat().softDelete(conversation_id, stored.message.id, alice, NOW) == ChatRepository::DeleteOutcome::AlreadyDeleted);
        // and a stranger still cannot, even after the fact
        BOOST_TEST(chat().softDelete(conversation_id, stored.message.id, bob, NOW) == ChatRepository::DeleteOutcome::NotFound);
    }

    BOOST_AUTO_TEST_CASE(a_late_retry_of_a_deleted_message_does_not_resurrect_it) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        const auto stored = chat().append(conversation_id, alice, "c1", "oops", NOW);
        chat().softDelete(conversation_id, stored.message.id, alice, NOW + 1);

        const auto retry = chat().append(conversation_id, alice, "c1", "oops", NOW + 2);

        BOOST_TEST(retry.duplicate);
        BOOST_TEST(retry.message.id == stored.message.id);
        BOOST_TEST(retry.message.text_message.empty());         // the tombstone has no text
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).empty());
    }

    BOOST_AUTO_TEST_CASE(deleted_since_lists_only_tombstones_in_the_members_conversations) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto alice_bob = dm(alice, bob);
        const auto bob_carol = dm(bob, carol);
        const auto old_one = chat().append(alice_bob, alice, "c1", "old", NOW);
        const auto new_one = chat().append(alice_bob, alice, "c2", "new", NOW);
        const auto elsewhere = chat().append(bob_carol, bob, "c3", "private", NOW);
        chat().softDelete(alice_bob, old_one.message.id, alice, 1'000);
        chat().softDelete(alice_bob, new_one.message.id, alice, 9'000);
        chat().softDelete(bob_carol, elsewhere.message.id, bob, 9'000);

        const auto recent = chat().deletedSince(alice, 5'000);

        // only the one deleted after the cursor, and nothing from a conversation alice is not in
        BOOST_TEST_REQUIRE(recent.size() == 1U);
        BOOST_TEST(recent[0].conversation_id == alice_bob);
        BOOST_TEST(recent[0].message_id == new_one.message.id);
    }

BOOST_AUTO_TEST_SUITE_END()
