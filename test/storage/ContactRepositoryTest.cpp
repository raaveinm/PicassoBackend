//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include <boost/test/unit_test.hpp>

#include "domain/Clock.hpp"
#include "storage/StorageTestDatabase.hpp"

using picasso::domain::ContactLevel;
using picasso::test::StorageFixture;

// canCommunicate is the single predicate behind DM access and palette invitations.
// A false positive here is an unsolicited-contact hole, so every way a pair can fail
// the rule gets its own case.
BOOST_FIXTURE_TEST_SUITE(
    contact_repository,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    BOOST_AUTO_TEST_CASE(strangers_cannot_communicate) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        BOOST_TEST(!contacts().canCommunicate(alice, bob));
        BOOST_TEST(!contacts().levelOf(alice, bob).has_value());
    }

    BOOST_AUTO_TEST_CASE(accept_writes_both_rows) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto now = picasso::domain::nowEpochMs();

        contacts().putRequest(alice, bob, now);
        BOOST_TEST(!contacts().canCommunicate(alice, bob));     // a pending request is not a contact

        BOOST_TEST(contacts().accept(alice, bob, now));
        BOOST_TEST(contacts().canCommunicate(alice, bob));
        BOOST_TEST(contacts().canCommunicate(bob, alice));      // symmetric
        BOOST_TEST(contacts().levelOf(alice, bob).value() == ContactLevel::Ally);
        BOOST_TEST(contacts().levelOf(bob, alice).value() == ContactLevel::Ally);
        BOOST_TEST(!contacts().requestState(alice, bob).has_value());   // consumed by the accept
    }

    BOOST_AUTO_TEST_CASE(accept_without_a_request_changes_nothing) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        BOOST_TEST(!contacts().accept(alice, bob, picasso::domain::nowEpochMs()));
        BOOST_TEST(!contacts().canCommunicate(alice, bob));
    }

    BOOST_AUTO_TEST_CASE(expired_request_cannot_be_accepted) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto now = picasso::domain::nowEpochMs();

        contacts().putRequest(alice, bob, now - picasso::domain::CONTACT_REQUEST_TTL_MS - 1000);

        BOOST_TEST(!contacts().accept(alice, bob, now));
        BOOST_TEST(!contacts().canCommunicate(alice, bob));
        BOOST_TEST(contacts().snapshot(bob).incoming.empty());  // and it no longer shows up
    }

    BOOST_AUTO_TEST_CASE(accept_never_downgrades_a_tier) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        BOOST_TEST(contacts().setTier(alice, bob, ContactLevel::Friend));

        // A stray second request resolving into accept must not flatten alice's row back to ally.
        contacts().putRequest(alice, bob, picasso::domain::nowEpochMs());
        BOOST_TEST(contacts().accept(alice, bob, picasso::domain::nowEpochMs()));

        BOOST_TEST(contacts().levelOf(alice, bob).value() == ContactLevel::Friend);
    }

    BOOST_AUTO_TEST_CASE(decline_keeps_a_cooldown_marker_and_hides_the_request) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto now = picasso::domain::nowEpochMs();

        contacts().putRequest(alice, bob, now);
        BOOST_TEST(contacts().decline(alice, bob, now));

        const auto state = contacts().requestState(alice, bob);
        BOOST_REQUIRE(state.has_value());
        BOOST_TEST(state->declined_at_epoch_ms.has_value());
        BOOST_TEST(contacts().snapshot(bob).incoming.empty());
        BOOST_TEST(contacts().snapshot(alice).outgoing.empty());    // the sender is not shown the decline
        BOOST_TEST(!contacts().accept(alice, bob, now));            // a declined request cannot be accepted
    }

    BOOST_AUTO_TEST_CASE(remove_is_symmetric_and_one_sided_in_who_asks) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);

        BOOST_TEST(contacts().remove(bob, alice));              // either side can end it
        BOOST_TEST(!contacts().levelOf(alice, bob).has_value());
        BOOST_TEST(!contacts().levelOf(bob, alice).has_value());
        BOOST_TEST(!contacts().remove(alice, bob));             // nothing left to remove
    }

    BOOST_AUTO_TEST_CASE(block_deletes_the_other_sides_row) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);

        contacts().block(alice, bob, picasso::domain::nowEpochMs());

        BOOST_TEST(contacts().levelOf(alice, bob).value() == ContactLevel::Imposter);
        // From bob's side a block must look exactly like a removal: no row at all.
        BOOST_TEST(!contacts().levelOf(bob, alice).has_value());
        BOOST_TEST(!contacts().canCommunicate(alice, bob));
        BOOST_TEST(!contacts().canCommunicate(bob, alice));
    }

    BOOST_AUTO_TEST_CASE(block_cancels_pending_requests_both_ways) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto now = picasso::domain::nowEpochMs();
        contacts().putRequest(alice, bob, now);
        contacts().putRequest(bob, alice, now);

        contacts().block(alice, bob, now);

        BOOST_TEST(!contacts().requestState(alice, bob).has_value());
        BOOST_TEST(!contacts().requestState(bob, alice).has_value());
    }

    BOOST_AUTO_TEST_CASE(blocking_a_user_who_never_logged_in_still_works) {
        const auto alice = user(76561198000000001ULL);
        const picasso::domain::SteamId ghost(76561198000000099ULL);   // no users row yet

        contacts().block(alice, ghost, picasso::domain::nowEpochMs());

        BOOST_TEST(contacts().levelOf(alice, ghost).value() == ContactLevel::Imposter);
    }

    BOOST_AUTO_TEST_CASE(remove_does_not_touch_a_block) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        contacts().block(alice, bob, picasso::domain::nowEpochMs());

        BOOST_TEST(!contacts().remove(alice, bob));             // an imposter row is a block, not a connection
        BOOST_TEST(contacts().levelOf(alice, bob).value() == ContactLevel::Imposter);
    }

    BOOST_AUTO_TEST_CASE(unblock_returns_to_stranger_not_ally) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        ally(alice, bob);
        contacts().block(alice, bob, picasso::domain::nowEpochMs());

        BOOST_TEST(contacts().unblock(alice, bob));

        BOOST_TEST(!contacts().levelOf(alice, bob).has_value());
        BOOST_TEST(!contacts().canCommunicate(alice, bob));
        BOOST_TEST(!contacts().unblock(alice, bob));            // idempotent: nothing left to unblock
    }

    BOOST_AUTO_TEST_CASE(set_tier_needs_an_existing_connection) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);

        BOOST_TEST(!contacts().setTier(alice, bob, ContactLevel::Friend));      // stranger: nothing to change

        ally(alice, bob);
        BOOST_TEST(contacts().setTier(alice, bob, ContactLevel::Friend));
        BOOST_TEST(contacts().levelOf(alice, bob).value() == ContactLevel::Friend);
        BOOST_TEST(contacts().levelOf(bob, alice).value() == ContactLevel::Ally);   // the tier is one-sided
        BOOST_TEST(contacts().canCommunicate(alice, bob));                       // friend still communicates
        BOOST_TEST(!contacts().setTier(alice, bob, ContactLevel::Imposter));    // a block goes through block()
    }

    BOOST_AUTO_TEST_CASE(snapshot_is_the_callers_own_view) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto dave = user(76561198000000004ULL);
        const auto now = picasso::domain::nowEpochMs();
        ally(alice, bob);
        contacts().putRequest(carol, alice, now);       // incoming for alice
        contacts().putRequest(alice, dave, now);        // outgoing for alice

        const auto snapshot = contacts().snapshot(alice);

        BOOST_REQUIRE_EQUAL(snapshot.contacts.size(), 1U);
        BOOST_TEST(snapshot.contacts[0].other == bob);
        BOOST_REQUIRE_EQUAL(snapshot.incoming.size(), 1U);
        BOOST_TEST(snapshot.incoming[0].other == carol);
        BOOST_REQUIRE_EQUAL(snapshot.outgoing.size(), 1U);
        BOOST_TEST(snapshot.outgoing[0].other == dave);
    }

BOOST_AUTO_TEST_SUITE_END()
