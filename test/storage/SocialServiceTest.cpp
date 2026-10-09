//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include <algorithm>
#include <variant>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "domain/Clock.hpp"
#include "storage/StorageTestDatabase.hpp"

using picasso::domain::ContactLevel;
using picasso::domain::ConversationKind;
using picasso::domain::SteamId;
using picasso::service::ContactService;
using picasso::service::ConversationService;
using picasso::test::StorageFixture;

namespace {
    bool contains(const std::vector<SteamId>& ids, const SteamId id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }
} // namespace

// The services over a real Postgres. What matters here is not that each call
// works but that cases the caller must not be able to tell apart really do answer
// identically: a block, a recent decline and a never-seen steamId all look like a
// normal pending request.
BOOST_FIXTURE_TEST_SUITE(
    social_service,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    ///////////////////////////////////////////////
    /// Contact requests
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(request_then_accept_makes_allies) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::Pending);
        BOOST_TEST(contactService().snapshot(bob).incoming.size() == 1U);
        BOOST_TEST(contactService().snapshot(alice).outgoing.size() == 1U);

        BOOST_TEST(contactService().accept(bob, alice));
        BOOST_TEST(contacts().canCommunicate(alice, bob));
    }

    BOOST_AUTO_TEST_CASE(requesting_yourself_is_rejected) {
        const auto alice = user(76561198000000001ULL);

        BOOST_TEST(contactService().request(alice, alice) == ContactService::RequestOutcome::Self);
    }

    BOOST_AUTO_TEST_CASE(request_to_an_unknown_steam_id_waits_for_them) {
        const auto alice = user(76561198000000001ULL);
        const SteamId stranger(76561198000000042ULL);       // has never logged in here

        // Same answer as a request to a real user, and the target now exists as a stub...
        BOOST_TEST(contactService().request(alice, stranger) == ContactService::RequestOutcome::Pending);
        BOOST_TEST(contacts().userExists(stranger));
        // ...with the request waiting for them.
        BOOST_TEST(contactService().snapshot(stranger).incoming.size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(crossing_requests_resolve_into_a_contact) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::Pending);
        BOOST_TEST(contactService().request(bob, alice) == ContactService::RequestOutcome::Accepted);

        BOOST_TEST(contacts().canCommunicate(alice, bob));
        BOOST_TEST(contactService().snapshot(alice).outgoing.empty());
        BOOST_TEST(contactService().snapshot(bob).incoming.empty());
    }

    BOOST_AUTO_TEST_CASE(repeating_a_request_is_idempotent) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        contactService().request(alice, bob);
        const auto first = contactService().snapshot(bob).incoming.at(0).created_at_epoch_ms;
        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::Pending);

        const auto incoming = contactService().snapshot(bob).incoming;
        BOOST_REQUIRE_EQUAL(incoming.size(), 1U);
        BOOST_TEST(incoming[0].created_at_epoch_ms == first);   // a repeat must not refresh the expiry
    }

    BOOST_AUTO_TEST_CASE(requesting_an_existing_contact_is_a_no_op) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);

        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::AlreadyContacts);
    }

    BOOST_AUTO_TEST_CASE(a_blocked_requester_cannot_tell_it_was_dropped) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contactService().setLevel(bob, alice, ContactLevel::Imposter);

        // alice is told "pending", exactly as for a normal request...
        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::Pending);
        // ...but nothing reaches bob.
        BOOST_TEST(contactService().snapshot(bob).incoming.empty());
        BOOST_TEST(!contacts().requestState(alice, bob).has_value());
    }

    BOOST_AUTO_TEST_CASE(requesting_someone_you_blocked_asks_you_to_unblock_first) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contactService().setLevel(alice, bob, ContactLevel::Imposter);

        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::UnblockFirst);
    }

    BOOST_AUTO_TEST_CASE(a_declined_request_is_dropped_silently_during_the_cooldown) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contactService().request(alice, bob);
        contactService().declineOrWithdraw(bob, alice);

        BOOST_TEST(contactService().request(alice, bob) == ContactService::RequestOutcome::Pending);
        BOOST_TEST(contactService().snapshot(bob).incoming.empty());    // still nothing for bob
    }

    BOOST_AUTO_TEST_CASE(a_declined_request_can_be_resent_after_the_cooldown) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto long_ago = picasso::domain::nowEpochMs() - picasso::domain::DECLINE_COOLDOWN_MS - 1000;
        contacts().putRequest(alice, bob, long_ago);
        contacts().decline(alice, bob, long_ago);

        contactService().request(alice, bob);

        BOOST_TEST(contactService().snapshot(bob).incoming.size() == 1U);
    }

    BOOST_AUTO_TEST_CASE(requests_are_rate_limited) {
        const auto alice = user(76561198000000001ULL);
        for (int i = 0; i < picasso::domain::MAX_REQUESTS_PER_DAY; ++i) {
            const SteamId target(76561198100000000ULL + static_cast<std::uint64_t>(i));
            BOOST_REQUIRE(contactService().request(alice, target) == ContactService::RequestOutcome::Pending);
        }

        const SteamId one_too_many(76561198200000000ULL);
        BOOST_TEST(contactService().request(alice, one_too_many) == ContactService::RequestOutcome::RateLimited);
    }

    BOOST_AUTO_TEST_CASE(withdrawing_removes_it_from_the_recipient) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contactService().request(alice, bob);

        contactService().declineOrWithdraw(alice, bob);     // alice is the sender, so this withdraws

        BOOST_TEST(contactService().snapshot(bob).incoming.empty());
        BOOST_TEST(!contactService().accept(bob, alice));
    }

    ///////////////////////////////////////////////
    /// Contact rows
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(tier_changes_need_a_connection) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        BOOST_TEST(contactService().setLevel(alice, bob, ContactLevel::Friend)
                   == ContactService::LevelOutcome::NotAContact);
        ally(alice, bob);
        BOOST_TEST(contactService().setLevel(alice, bob, ContactLevel::Friend) == ContactService::LevelOutcome::Ok);
        BOOST_TEST(contactService().setLevel(alice, alice, ContactLevel::Friend) == ContactService::LevelOutcome::Self);
    }

    BOOST_AUTO_TEST_CASE(delete_removes_a_connection_and_unblocks_a_block) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        ally(alice, bob);
        contactService().setLevel(alice, carol, ContactLevel::Imposter);

        contactService().removeOrUnblock(alice, bob);
        contactService().removeOrUnblock(alice, carol);

        BOOST_TEST(!contacts().levelOf(alice, bob).has_value());
        BOOST_TEST(!contacts().levelOf(bob, alice).has_value());
        BOOST_TEST(!contacts().levelOf(alice, carol).has_value());
        contactService().removeOrUnblock(alice, carol);     // idempotent
    }

    ///////////////////////////////////////////////
    /// Direct messages
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(a_dm_needs_allies) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        const auto refused = conversationService().createDm(alice, bob);
        BOOST_TEST(refused.outcome == ConversationService::CreateOutcome::NotAllowed);
        BOOST_TEST(!refused.conversation.has_value());
        BOOST_TEST(conversations().conversationsOf(alice).empty());     // nothing was created
    }

    BOOST_AUTO_TEST_CASE(an_unknown_user_is_refused_like_a_stranger) {
        const auto alice = user(76561198000000001ULL);
        const SteamId nobody(76561198000000077ULL);

        BOOST_TEST(conversationService().createDm(alice, nobody).outcome
                   == ConversationService::CreateOutcome::NotAllowed);
    }

    BOOST_AUTO_TEST_CASE(a_dm_is_get_or_create_from_either_side) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);

        const auto first = conversationService().createDm(bob, alice);
        BOOST_REQUIRE(first.outcome == ConversationService::CreateOutcome::Created);
        const auto second = conversationService().createDm(alice, bob);

        BOOST_TEST(second.outcome == ConversationService::CreateOutcome::Existing);
        BOOST_REQUIRE(second.conversation.has_value());
        BOOST_TEST(second.conversation->id == first.conversation->id);
        BOOST_TEST(second.conversation->kind == ConversationKind::Dm);
        BOOST_TEST(!second.conversation->name.has_value());
        BOOST_TEST(second.conversation->members.size() == 2U);
        BOOST_TEST(second.conversation->invited.empty());
        BOOST_TEST(conversations().isMember(first.conversation->id, alice));
        BOOST_TEST(conversations().isMember(first.conversation->id, bob));
    }

    BOOST_AUTO_TEST_CASE(a_dm_with_yourself_is_rejected) {
        const auto alice = user(76561198000000001ULL);

        BOOST_TEST(conversationService().createDm(alice, alice).outcome == ConversationService::CreateOutcome::Self);
    }

    ///////////////////////////////////////////////
    /// Palettes
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(palette_creator_is_the_only_member_and_invitees_are_pending) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        ally(alice, bob);
        ally(alice, carol);

        const auto created = conversationService().createPalette(alice, "  Friday squad ", {bob, carol, bob, alice});

        BOOST_REQUIRE(created.outcome == ConversationService::CreateOutcome::Created);
        const auto& palette = *created.conversation;
        BOOST_TEST(palette.kind == ConversationKind::Palette);
        BOOST_TEST(palette.name.value() == "Friday squad");     // trimmed
        BOOST_TEST(palette.members.size() == 1U);
        BOOST_TEST(contains(palette.members, alice));
        BOOST_TEST(palette.invited.size() == 2U);               // deduplicated, self ignored
        BOOST_TEST(contains(palette.invited, bob));
        BOOST_TEST(contains(palette.invited, carol));
        // Pending is structurally not a member.
        BOOST_TEST(!conversations().isMember(palette.id, bob));
        BOOST_TEST(conversations().conversationsOf(bob).empty());
    }

    BOOST_AUTO_TEST_CASE(palette_creation_is_all_or_nothing) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        ally(alice, bob);                                       // carol is a stranger

        const auto created = conversationService().createPalette(alice, "squad", {bob, carol});

        BOOST_TEST(created.outcome == ConversationService::CreateOutcome::NotAllowed);
        BOOST_TEST(conversations().conversationsOf(alice).empty());     // not even the palette itself
        BOOST_TEST(contactService().snapshot(bob).palette_invites.empty());
    }

    BOOST_AUTO_TEST_CASE(palette_names_are_validated) {
        const auto alice = user(76561198000000001ULL);

        BOOST_TEST(conversationService().createPalette(alice, "   ", {}).outcome
                   == ConversationService::CreateOutcome::InvalidName);
        BOOST_TEST(conversationService().createPalette(alice, std::string(65, 'x'), {}).outcome
                   == ConversationService::CreateOutcome::InvalidName);
        BOOST_TEST(conversationService().createPalette(alice, "solo", {}).outcome
                   == ConversationService::CreateOutcome::Created);
    }

    BOOST_AUTO_TEST_CASE(accepting_an_invite_joins_and_declining_is_silent) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        ally(alice, bob);
        ally(alice, carol);
        const auto palette = conversationService().createPalette(alice, "squad", {bob, carol}).conversation->id;

        const auto invites = contactService().snapshot(bob).palette_invites;
        BOOST_REQUIRE_EQUAL(invites.size(), 1U);
        BOOST_TEST(invites[0].conversation_id == palette);
        BOOST_TEST(invites[0].inviter == alice);
        BOOST_TEST(invites[0].name == "squad");

        const auto joined = conversationService().acceptInvite(bob, palette);
        BOOST_REQUIRE(joined.has_value());
        BOOST_TEST(contains(joined->members, bob));
        BOOST_TEST(!contains(joined->invited, bob));
        BOOST_TEST(conversations().isMember(palette, bob));
        BOOST_TEST(!conversationService().acceptInvite(bob, palette).has_value());      // already consumed

        conversationService().declineInvite(carol, palette);
        BOOST_TEST(!conversations().isMember(palette, carol));
        BOOST_TEST(contactService().snapshot(carol).palette_invites.empty());
    }

    BOOST_AUTO_TEST_CASE(a_declined_invite_cannot_be_told_apart_by_the_inviter) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        const auto palette = conversationService().createPalette(alice, "squad", {bob}).conversation->id;
        conversationService().declineInvite(bob, palette);

        const auto again = conversationService().invite(alice, palette, bob);

        // Dropped during the cooldown, yet alice's answer still shows a normal pending invite.
        BOOST_TEST(again.outcome == ConversationService::InviteOutcome::Unchanged);
        BOOST_REQUIRE(again.conversation.has_value());
        BOOST_TEST(contains(again.conversation->invited, bob));
        BOOST_TEST(contactService().snapshot(bob).palette_invites.empty());     // bob really is not bothered again
    }

    BOOST_AUTO_TEST_CASE(any_member_can_invite_but_only_allies_of_theirs) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto dave = user(76561198000000004ULL);
        ally(alice, bob);
        ally(bob, carol);                                       // dave knows nobody
        const auto palette = conversationService().createPalette(alice, "squad", {bob}).conversation->id;
        static_cast<void>(conversationService().acceptInvite(bob, palette));

        // bob is a member now; he can invite his own ally even though alice does not know carol.
        BOOST_TEST(conversationService().invite(bob, palette, carol).outcome
                   == ConversationService::InviteOutcome::Invited);
        BOOST_TEST(conversationService().invite(bob, palette, dave).outcome
                   == ConversationService::InviteOutcome::NotAllowed);
        // A non-member cannot invite, and cannot learn the palette exists.
        BOOST_TEST(conversationService().invite(dave, palette, carol).outcome
                   == ConversationService::InviteOutcome::NotAMember);
    }

    BOOST_AUTO_TEST_CASE(inviting_a_member_or_an_invitee_changes_nothing) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        const auto palette = conversationService().createPalette(alice, "squad", {bob}).conversation->id;

        BOOST_TEST(conversationService().invite(alice, palette, bob).outcome
                   == ConversationService::InviteOutcome::Unchanged);       // already invited
        static_cast<void>(conversationService().acceptInvite(bob, palette));
        BOOST_TEST(conversationService().invite(alice, palette, bob).outcome
                   == ConversationService::InviteOutcome::Unchanged);       // already a member
    }

    BOOST_AUTO_TEST_CASE(removing_a_contact_cancels_pending_invites_but_not_membership) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        ally(alice, bob);
        ally(alice, carol);
        const auto palette = conversationService().createPalette(alice, "squad", {bob, carol}).conversation->id;
        static_cast<void>(conversationService().acceptInvite(carol, palette));

        contactService().removeOrUnblock(alice, bob);           // bob's invite is still pending
        contactService().removeOrUnblock(alice, carol);         // carol already joined

        BOOST_TEST(contactService().snapshot(bob).palette_invites.empty());
        BOOST_TEST(!conversationService().acceptInvite(bob, palette).has_value());
        BOOST_TEST(conversations().isMember(palette, carol));   // the graph does not reach inside a palette
    }

    BOOST_AUTO_TEST_CASE(a_frozen_dm_keeps_its_conversation_and_thaws_on_reunion) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        const auto dm = conversationService().createDm(alice, bob).conversation->id;

        contactService().removeOrUnblock(bob, alice);
        BOOST_TEST(conversations().isMember(dm, alice));        // the conversation is never deleted
        BOOST_TEST(conversationService().createDm(alice, bob).outcome
                   == ConversationService::CreateOutcome::NotAllowed);

        ally(alice, bob);                                       // the same pair becomes allies again
        const auto thawed = conversationService().createDm(alice, bob);
        BOOST_TEST(thawed.outcome == ConversationService::CreateOutcome::Existing);
        BOOST_TEST(thawed.conversation->id == dm);
    }

    ///////////////////////////////////////////////
    /// Pushed events
    ///////////////////////////////////////////////

    BOOST_AUTO_TEST_CASE(a_request_tells_only_the_target) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        events().take();

        contactService().request(alice, bob);

        const auto published = events().take();
        BOOST_REQUIRE_EQUAL(published.size(), 1U);
        const auto* requested = std::get_if<picasso::domain::ContactRequested>(&published[0]);
        BOOST_REQUIRE(requested != nullptr);
        BOOST_TEST(requested->to == bob);
        BOOST_TEST(requested->from == alice);
    }

    BOOST_AUTO_TEST_CASE(a_dropped_request_publishes_nothing) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contactService().setLevel(bob, alice, ContactLevel::Imposter);
        events().take();

        contactService().request(alice, bob);

        BOOST_TEST(events().take().empty());                    // bob must not even be pinged
    }

    BOOST_AUTO_TEST_CASE(accepting_tells_both_sides_their_own_row) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contactService().request(alice, bob);
        events().take();

        contactService().accept(bob, alice);

        const auto published = events().take();
        BOOST_REQUIRE_EQUAL(published.size(), 2U);
        const auto& for_bob = std::get<picasso::domain::ContactChanged>(published[0]);
        const auto& for_alice = std::get<picasso::domain::ContactChanged>(published[1]);
        BOOST_TEST(for_bob.to == bob);
        BOOST_TEST(for_bob.other == alice);
        BOOST_TEST(for_bob.level.value() == ContactLevel::Ally);
        BOOST_TEST(for_alice.to == alice);
        BOOST_TEST(for_alice.other == bob);
        BOOST_TEST(for_alice.level.value() == ContactLevel::Ally);
    }

    BOOST_AUTO_TEST_CASE(a_block_reaches_the_blocked_person_as_a_removal) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        events().take();

        contactService().setLevel(alice, bob, ContactLevel::Imposter);

        const auto published = events().take();
        BOOST_REQUIRE_EQUAL(published.size(), 2U);
        const auto& blocker = std::get<picasso::domain::ContactChanged>(published[0]);
        const auto& blocked = std::get<picasso::domain::ContactChanged>(published[1]);
        BOOST_TEST(blocker.to == alice);
        BOOST_TEST(blocker.level.value() == ContactLevel::Imposter);
        BOOST_TEST(blocked.to == bob);
        BOOST_TEST(blocked.other == alice);
        BOOST_TEST(!blocked.level.has_value());                 // indistinguishable from a removal
    }

    BOOST_AUTO_TEST_CASE(a_tier_change_is_only_the_owners_business) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        events().take();

        contactService().setLevel(alice, bob, ContactLevel::Friend);

        const auto published = events().take();
        BOOST_REQUIRE_EQUAL(published.size(), 1U);
        BOOST_TEST(std::get<picasso::domain::ContactChanged>(published[0]).to == alice);
    }

    BOOST_AUTO_TEST_CASE(a_new_dm_is_announced_to_both_parties_once) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        events().take();

        static_cast<void>(conversationService().createDm(alice, bob));
        BOOST_TEST(events().take().size() == 2U);
        static_cast<void>(conversationService().createDm(alice, bob));             // already there: nothing to announce
        BOOST_TEST(events().take().empty());
    }

    BOOST_AUTO_TEST_CASE(invites_and_joins_are_announced_to_the_right_people) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        const auto palette = conversationService().createPalette(alice, "squad", {bob}).conversation->id;
        events().take();

        static_cast<void>(conversationService().acceptInvite(bob, palette));

        const auto published = events().take();
        BOOST_REQUIRE_EQUAL(published.size(), 3U);
        // bob's other devices: the invite is resolved, and the palette is theirs now.
        const auto& resolved = std::get<picasso::domain::PaletteInviteChanged>(published[0]);
        BOOST_TEST(resolved.to == bob);
        BOOST_TEST(!resolved.pending);
        BOOST_TEST(std::get<picasso::domain::ConversationAdded>(published[1]).to == bob);
        // alice, already in it, learns about the new member.
        BOOST_TEST(std::get<picasso::domain::ConversationUpdated>(published[2]).to == alice);
    }

    BOOST_AUTO_TEST_CASE(a_declined_invite_is_not_announced_to_the_inviter) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        const auto palette = conversationService().createPalette(alice, "squad", {bob}).conversation->id;
        events().take();

        conversationService().declineInvite(bob, palette);

        const auto published = events().take();
        BOOST_REQUIRE_EQUAL(published.size(), 1U);
        BOOST_TEST(std::get<picasso::domain::PaletteInviteChanged>(published[0]).to == bob);
    }

BOOST_AUTO_TEST_SUITE_END()
