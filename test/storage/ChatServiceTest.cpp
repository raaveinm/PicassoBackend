//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "storage/StorageTestDatabase.hpp"

using picasso::domain::ConnectionId;
using picasso::domain::ConversationId;
using picasso::domain::MessageId;
using picasso::domain::SteamId;
using picasso::service::ChatService;
using picasso::service::RejectCode;
using picasso::test::StorageFixture;

namespace {
    const ConnectionId PHONE(1);
    const ConnectionId DESKTOP(2);

    bool contains(const std::vector<SteamId>& ids, const SteamId id) {
        return std::ranges::find(ids, id) != ids.end();
    }
} // namespace

BOOST_FIXTURE_TEST_SUITE(
    chat_service,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    ///////////////////////////////////////////////
    /// Sending
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(a_send_is_stored_and_delivered_to_everyone_but_the_sending_connection) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        events().take();

        const auto result = chatService().submit(alice, conversation_id, "client-1", "hello", PHONE);

        BOOST_TEST(result.status == ChatService::SubmitStatus::Stored);
        BOOST_TEST_REQUIRE(result.message.has_value());
        BOOST_TEST(result.message->sender_steam_id == alice);        // from the argument, never from the payload
        BOOST_TEST(result.message->client_message_id == "client-1");
        BOOST_TEST(result.message->text_message == "hello");

        const auto published = events().take();
        BOOST_TEST_REQUIRE(published.size() == 1U);
        const auto* delivered = std::get_if<picasso::domain::ChatDelivered>(&published[0]);
        BOOST_TEST_REQUIRE(delivered != nullptr);
        BOOST_TEST(delivered->message.id == result.message->id);
        // both members are recipients - alice's OTHER devices must hear about it too...
        BOOST_TEST(contains(delivered->to, alice));
        BOOST_TEST(contains(delivered->to, bob));
        // ...but the connection that sent it is excluded: it is answered with the ack instead
        BOOST_TEST_REQUIRE(delivered->except.has_value());
        BOOST_TEST(*delivered->except == PHONE);
    }

    BOOST_AUTO_TEST_CASE(a_retry_is_acked_again_but_delivered_only_once) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        const auto first = chatService().submit(alice, conversation_id, "client-1", "hello", PHONE);
        events().take();

        const auto retry = chatService().submit(alice, conversation_id, "client-1", "hello", PHONE);

        BOOST_TEST(retry.status == ChatService::SubmitStatus::Duplicate);
        BOOST_TEST(retry.message->id == first.message->id);          // the SAME message, so the client resolves its row
        BOOST_TEST(events().take().empty());                         // and nobody is told twice
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(a_stranger_cannot_write_into_a_conversation) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto mallory = user(76561198000000099ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        events().take();

        const auto result = chatService().submit(mallory, conversation_id, "x", "let me in", PHONE);
        const auto missing = chatService().submit(mallory, ConversationId(424242), "x", "nowhere", PHONE);

        BOOST_TEST(result.status == ChatService::SubmitStatus::Rejected);
        BOOST_TEST(result.code == RejectCode::NotMember);
        BOOST_TEST(missing.code == RejectCode::NotMember);           // "no such conversation" and "not yours" are one answer
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).empty());
        BOOST_TEST(events().take().empty());
    }

    BOOST_AUTO_TEST_CASE(a_dm_stops_accepting_messages_the_moment_the_pair_stops_being_allies) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        BOOST_TEST(chatService().submit(alice, conversation_id, "c1", "before", PHONE).status == ChatService::SubmitStatus::Stored);

        contactService().removeOrUnblock(bob, alice);                // a block takes effect on the very next message

        const auto after = chatService().submit(alice, conversation_id, "c2", "after", PHONE);
        BOOST_TEST(after.status == ChatService::SubmitStatus::Rejected);
        BOOST_TEST(after.code == RejectCode::NotAllowed);
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).size() == 1U);   // history stays, readable
    }

    BOOST_AUTO_TEST_CASE(a_palette_does_not_consult_the_contact_graph) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto invited = user(76561198000000004ULL);
        const auto palette_id = palette({alice, bob, carol});
        // bob and carol have never been contacts of anyone; that must not matter inside a palette
        BOOST_TEST(chatService().submit(bob, palette_id, "c1", "hi", PHONE).status == ChatService::SubmitStatus::Stored);
        // a pending invitee is not a member
        BOOST_TEST(chatService().submit(invited, palette_id, "c2", "hi", PHONE).code == RejectCode::NotMember);
    }

    BOOST_AUTO_TEST_CASE(a_palette_message_reaches_the_members_and_nobody_else) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto outsider = user(76561198000000003ULL);
        const auto palette_id = palette({alice, bob});
        events().take();

        BOOST_TEST(chatService().submit(alice, palette_id, "c1", "squad", PHONE).status == ChatService::SubmitStatus::Stored);

        const auto published = events().take();         // kept alive: a reference into a temporary would dangle
        const auto& delivered = std::get<picasso::domain::ChatDelivered>(published.at(0));
        BOOST_TEST(delivered.to.size() == 2U);
        BOOST_TEST(!contains(delivered.to, outsider));
    }

    BOOST_AUTO_TEST_CASE(bad_input_is_rejected_before_anything_is_stored) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);

        BOOST_TEST(chatService().submit(alice, conversation_id, "c", "   \n\t ", PHONE).code == RejectCode::Invalid);
        BOOST_TEST(chatService().submit(alice, conversation_id, "", "hi", PHONE).code == RejectCode::Invalid);
        BOOST_TEST(chatService().submit(alice, conversation_id, std::string(65, 'k'), "hi", PHONE).code == RejectCode::Invalid);
        BOOST_TEST(chatService().submit(alice, conversation_id, "c", std::string(4097, 'x'), PHONE).code == RejectCode::TooLong);
        // the limit is in BYTES: this is 2049 two-byte characters
        BOOST_TEST(chatService().submit(alice, conversation_id, "c", std::string(2049, 'x') + std::string(2048, 'y'), PHONE).code
                   == RejectCode::TooLong);
        BOOST_TEST(chatService().submit(alice, conversation_id, "c", std::string(4096, 'x'), PHONE).status
                   == ChatService::SubmitStatus::Stored);                // exactly at the limit is fine
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(sending_is_rate_limited_per_user_and_recovers) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);

        for (int i = 0; i < 10; ++i) {                                // the burst
            BOOST_TEST_REQUIRE(chatService().submit(alice, conversation_id, "burst-" + std::to_string(i), "x", PHONE).status
                               == ChatService::SubmitStatus::Stored);
        }
        BOOST_TEST(chatService().submit(alice, conversation_id, "burst-10", "x", PHONE).code == RejectCode::RateLimited);
        // a limit on alice is not a limit on bob
        BOOST_TEST(chatService().submit(bob, conversation_id, "bob-1", "x", DESKTOP).status == ChatService::SubmitStatus::Stored);

        advanceClock(60'000);                                         // a minute buys back many tokens at 30/min
        BOOST_TEST(chatService().submit(alice, conversation_id, "after", "x", PHONE).status == ChatService::SubmitStatus::Stored);
    }

    BOOST_AUTO_TEST_CASE(a_rate_limited_send_is_retryable_not_stored) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        for (int i = 0; i < 10; ++i) {
            static_cast<void>(chatService().submit(alice, conversation_id, "b" + std::to_string(i), "x", PHONE));
        }

        const auto limited = chatService().submit(alice, conversation_id, "again", "x", PHONE);
        BOOST_TEST_REQUIRE(limited.status == ChatService::SubmitStatus::Rejected);
        advanceClock(60'000);
        // the very same key, retried: stored for the first time now - nothing was half-written before
        const auto retry = chatService().submit(alice, conversation_id, "again", "x", PHONE);
        BOOST_TEST(retry.status == ChatService::SubmitStatus::Stored);
    }

    ///////////////////////////////////////////////
    /// Ordering
    ///////////////////////////////////////////////

    // A client's cursor is "the highest id I hold". If id 11 could reach a client before id 10,
    // a disconnect in between would lose 10 forever. So deliveries must be published in id order
    // even when many senders hit one conversation at once.
    BOOST_AUTO_TEST_CASE(concurrent_senders_are_delivered_in_id_order) {
        constexpr int SENDERS = 8;
        constexpr int EACH = 10;
        std::vector<SteamId> members;
        for (int i = 0; i < SENDERS; ++i) {
            members.push_back(user(76561198000000100ULL + static_cast<std::uint64_t>(i)));
        }
        const auto palette_id = palette({members[0], members[1], members[2], members[3], members[4], members[5], members[6], members[7]});
        events().take();

        std::vector<std::thread> threads;
        for (int s = 0; s < SENDERS; ++s) {
            threads.emplace_back([&, s] {
                for (int m = 0; m < EACH; ++m) {
                    static_cast<void>(chatService().submit(
                        members[static_cast<std::size_t>(s)],
                        palette_id,
                        "t" + std::to_string(s) + "-" + std::to_string(m),
                        "x",
                        ConnectionId(static_cast<std::uint64_t>(s + 1))));
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }

        const auto published = events().take();
        BOOST_TEST_REQUIRE(published.size() == static_cast<std::size_t>(SENDERS * EACH));
        std::int64_t previous = 0;
        bool in_order = true;
        for (const auto& event : published) {
            const auto id = std::get<picasso::domain::ChatDelivered>(event).message.id.value();
            in_order = in_order && id > previous;
            previous = id;
        }
        BOOST_TEST(in_order);
    }

    ///////////////////////////////////////////////
    /// Catching up
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(a_first_sync_resets_every_conversation_to_its_newest_page) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto with_bob = dm(alice, bob);
        const auto squad = palette({alice, carol});
        ally(alice, bob);
        static_cast<void>(chatService().submit(bob, with_bob, "c1", "one", PHONE));
        static_cast<void>(chatService().submit(carol, squad, "c2", "two", PHONE));

        const auto synced = chatService().sync(alice, {}, std::nullopt, 100);

        // the server walks ALL of alice's conversations, not just the ones she named
        BOOST_TEST_REQUIRE(synced.conversations.size() == 2U);
        for (const auto& entry : synced.conversations) {
            BOOST_TEST(entry.reset);                                  // no cursor: a reset, not a delta
            BOOST_TEST(!entry.has_more_before);
            BOOST_TEST(entry.messages.size() == 1U);
            BOOST_TEST(entry.writable);
        }
        BOOST_TEST(synced.server_time > 0);
        BOOST_TEST(synced.deleted_cursor < synced.server_time);       // deliberately a little behind "now"
        BOOST_TEST(synced.deleted.empty());                           // no cursor, nothing to report
    }

    BOOST_AUTO_TEST_CASE(a_sync_with_a_cursor_returns_only_what_is_missing) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        const auto first = chatService().submit(bob, conversation_id, "c1", "seen", PHONE);
        static_cast<void>(chatService().submit(bob, conversation_id, "c2", "missed-1", PHONE));
        static_cast<void>(chatService().submit(bob, conversation_id, "c3", "missed-2", PHONE));

        const auto synced = chatService().sync(alice, {{conversation_id, first.message->id}}, std::nullopt, 100);

        BOOST_TEST_REQUIRE(synced.conversations.size() == 1U);
        const auto& entry = synced.conversations[0];
        BOOST_TEST(!entry.reset);                                     // a delta: append these
        BOOST_TEST_REQUIRE(entry.messages.size() == 2U);
        BOOST_TEST(entry.messages[0].text_message == "missed-1");
        BOOST_TEST(entry.messages[1].text_message == "missed-2");
    }

    // The one rule that keeps a cursor a plain maximum: the cached range of a conversation is
    // always one contiguous block ending at the newest message. When more than a page was
    // missed the server does not hand over "the first page after the cursor" (leaving a hole);
    // it replaces the range with the newest page.
    BOOST_AUTO_TEST_CASE(missing_more_than_a_page_becomes_a_reset_with_the_newest_page) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        const auto seen = chatService().submit(bob, conversation_id, "seen", "seen", PHONE);
        for (int i = 1; i <= 5; ++i) {
            chat().append(conversation_id, bob, "m" + std::to_string(i), "m" + std::to_string(i), 1);
        }

        const auto synced = chatService().sync(alice, {{conversation_id, seen.message->id}}, std::nullopt, 3);

        const auto& entry = synced.conversations.at(0);
        BOOST_TEST(entry.reset);
        BOOST_TEST(entry.has_more_before);
        BOOST_TEST_REQUIRE(entry.messages.size() == 3U);              // the newest three, oldest first
        BOOST_TEST(entry.messages[0].text_message == "m3");
        BOOST_TEST(entry.messages[2].text_message == "m5");
    }

    BOOST_AUTO_TEST_CASE(a_first_sync_flags_when_there_is_older_history) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        for (int i = 1; i <= 4; ++i) {
            chat().append(conversation_id, bob, "m" + std::to_string(i), std::to_string(i), 1);
        }

        const auto synced = chatService().sync(alice, {}, std::nullopt, 3);

        BOOST_TEST(synced.conversations.at(0).reset);
        BOOST_TEST(synced.conversations.at(0).has_more_before);
        BOOST_TEST(synced.conversations.at(0).messages.size() == 3U);
    }

    BOOST_AUTO_TEST_CASE(a_sync_reports_a_frozen_dm_as_not_writable_and_still_lists_it) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        dm(alice, bob);                                               // never allies: frozen from the start

        const auto synced = chatService().sync(alice, {}, std::nullopt, 100);

        BOOST_TEST_REQUIRE(synced.conversations.size() == 1U);
        BOOST_TEST(!synced.conversations[0].writable);                // readable, but sends are refused
    }

    BOOST_AUTO_TEST_CASE(a_sync_carries_deletions_since_the_cursor) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        const auto doomed = chatService().submit(bob, conversation_id, "c1", "oops", PHONE);
        static_cast<void>(chatService().remove(bob, conversation_id, doomed.message->id));

        const auto synced = chatService().sync(alice, {}, 0, 100);

        BOOST_TEST_REQUIRE(synced.deleted.size() == 1U);
        BOOST_TEST(synced.deleted[0].message_id == doomed.message->id);
        BOOST_TEST(synced.conversations.at(0).messages.empty());      // and the deleted message is not listed either
    }

    BOOST_AUTO_TEST_CASE(history_pages_backwards_and_says_when_it_runs_out) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        MessageId newest;
        for (int i = 1; i <= 5; ++i) {
            newest = chat().append(conversation_id, bob, "m" + std::to_string(i), "m" + std::to_string(i), 1).message.id;
        }

        const auto page = chatService().history(alice, conversation_id, newest, 3);
        BOOST_TEST(page.member);
        BOOST_TEST_REQUIRE(page.messages.size() == 3U);
        BOOST_TEST(page.messages[0].text_message == "m2");
        BOOST_TEST(page.has_more_before);

        const auto last = chatService().history(alice, conversation_id, page.messages[0].id, 3);
        BOOST_TEST(last.messages.size() == 1U);
        BOOST_TEST(!last.has_more_before);                            // the client can stop asking
    }

    BOOST_AUTO_TEST_CASE(history_is_not_readable_by_outsiders) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto mallory = user(76561198000000099ULL);
        const auto conversation_id = dm(alice, bob);
        chat().append(conversation_id, alice, "c1", "private", 1);

        const auto stolen = chatService().history(mallory, conversation_id, MessageId(1'000'000), 100);
        const auto missing = chatService().history(mallory, ConversationId(424242), MessageId(1'000'000), 100);

        BOOST_TEST(!stolen.member);
        BOOST_TEST(stolen.messages.empty());
        BOOST_TEST(!missing.member);                                  // indistinguishable from a conversation that isn't there
    }

    ///////////////////////////////////////////////
    /// Deleting
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(deleting_tells_every_device_of_every_member_exactly_once) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        const auto sent = chatService().submit(alice, conversation_id, "c1", "oops", PHONE);
        events().take();

        BOOST_TEST(chatService().remove(alice, conversation_id, sent.message->id) == ChatService::RemoveOutcome::Removed);
        BOOST_TEST(chatService().remove(alice, conversation_id, sent.message->id) == ChatService::RemoveOutcome::Removed);   // idempotent

        const auto published = events().take();
        BOOST_TEST_REQUIRE(published.size() == 1U);                   // the repeat said nothing
        const auto& removed = std::get<picasso::domain::MessageRemoved>(published[0]);
        BOOST_TEST(removed.message_id == sent.message->id);
        BOOST_TEST(contains(removed.to, alice));                      // the deleter's other devices must drop it too
        BOOST_TEST(contains(removed.to, bob));
    }

    BOOST_AUTO_TEST_CASE(only_the_sender_can_delete) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto conversation_id = dm(alice, bob);
        ally(alice, bob);
        const auto sent = chatService().submit(alice, conversation_id, "c1", "mine", PHONE);
        events().take();

        BOOST_TEST(chatService().remove(bob, conversation_id, sent.message->id) == ChatService::RemoveOutcome::NotFound);

        BOOST_TEST(events().take().empty());
        BOOST_TEST(chat().newest(conversation_id, MessageId(0), 100).size() == 1U);
    }

BOOST_AUTO_TEST_SUITE_END()
