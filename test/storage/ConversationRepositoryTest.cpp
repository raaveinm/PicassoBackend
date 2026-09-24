//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#include <algorithm>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "storage/StorageTestDatabase.hpp"

using picasso::domain::ConversationId;
using picasso::domain::SteamId;
using picasso::test::StorageFixture;

namespace {
    template <typename T>
    std::vector<T> sorted(std::vector<T> values) {
        std::sort(values.begin(), values.end());
        return values;
    }
} // namespace

// isMember is what will close the RTC open relay - a false positive here lets
// anyone push SDP/ICE at anyone, so outsiders get as many cases as members.
BOOST_FIXTURE_TEST_SUITE(
    conversation_repository,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    BOOST_AUTO_TEST_CASE(dm_membership_is_symmetric) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto conversation_id = dm(bob, alice);

        BOOST_TEST(conversations().isMember(conversation_id, alice));
        BOOST_TEST(conversations().isMember(conversation_id, bob));
        BOOST_TEST(!conversations().isMember(conversation_id, carol));
    }

    BOOST_AUTO_TEST_CASE(palette_membership) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto dave = user(76561198000000004ULL);
        const auto conversation_id = palette({alice, bob, carol});

        BOOST_TEST(conversations().isMember(conversation_id, carol));
        BOOST_TEST(!conversations().isMember(conversation_id, dave));
    }

    BOOST_AUTO_TEST_CASE(unknown_conversation_has_no_members) {
        const auto alice = user(76561198000000001ULL);
        const ConversationId missing(999999);

        BOOST_TEST(!conversations().isMember(missing, alice));
        BOOST_TEST(conversations().members(missing).empty());
    }

    // A DMs id must not be confused with a palette's: membership is checked per table.
    BOOST_AUTO_TEST_CASE(dm_member_is_not_member_of_other_palette) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        // ReSharper disable once CppExpressionWithoutSideEffects
        dm(alice, bob);
        const auto palette_id = palette({carol});

        BOOST_TEST(!conversations().isMember(palette_id, alice));
    }

    BOOST_AUTO_TEST_CASE(members_lists_everyone) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto dm_id = dm(alice, bob);
        const auto palette_id = palette({alice, bob, carol});

        BOOST_TEST(sorted(conversations().members(dm_id)) == (std::vector{alice, bob}));
        BOOST_TEST(sorted(conversations().members(palette_id)) == (std::vector{alice, bob, carol}));
    }

    BOOST_AUTO_TEST_CASE(conversations_of_spans_dm_and_palette) {
        const auto alice = user(76561198000000001ULL);
        const auto bob = user(76561198000000002ULL);
        const auto carol = user(76561198000000003ULL);
        const auto alice_bob = dm(alice, bob);
        const auto bob_carol = dm(bob, carol);
        const auto group = palette({alice, carol});

        BOOST_TEST(sorted(conversations().conversationsOf(alice)) == (std::vector{alice_bob, group}));
        BOOST_TEST(sorted(conversations().conversationsOf(bob)) == (std::vector{alice_bob, bob_carol}));
        BOOST_TEST(conversations().conversationsOf(SteamId(1)).empty());
    }

BOOST_AUTO_TEST_SUITE_END()
