//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#include <cstdint>
#include <stdexcept>

#include <boost/test/unit_test.hpp>

#include "domain/Clock.hpp"
#include "domain/Session.hpp"
#include "storage/StorageTestDatabase.hpp"

using picasso::domain::Session;
using picasso::domain::SteamId;
using picasso::test::StorageFixture;

namespace {
    constexpr std::int64_t ONE_HOUR_MS = 60 * 60 * 1000;

    Session makeSession(const std::string& token_hash, const SteamId steam_id) {
        const std::int64_t now = picasso::domain::nowEpochMs();
        return Session{
            .token_hash = token_hash,
            .steam_id = steam_id,
            .created_at_epoch_ms = now,
            .expires_at_epoch_ms = now + ONE_HOUR_MS,
            .revoke_at_epoch_ms = std::nullopt,
        };
    }
} // namespace

BOOST_FIXTURE_TEST_SUITE(
    session_repository,
    StorageFixture,
    *boost::unit_test::precondition(picasso::test::hasTestDatabase)
)

    BOOST_AUTO_TEST_CASE(store_then_find_round_trips) {
        const auto alice = user(76561198000000001ULL);
        const auto stored = makeSession("hash-alice", alice);

        sessions().store(stored);
        const auto found = sessions().findByTokenHash("hash-alice");

        BOOST_TEST_REQUIRE(found.has_value());
        BOOST_TEST(found->token_hash == stored.token_hash);
        BOOST_TEST(found->steam_id == alice);
        BOOST_TEST(found->created_at_epoch_ms == stored.created_at_epoch_ms);
        BOOST_TEST(found->expires_at_epoch_ms == stored.expires_at_epoch_ms);
        BOOST_TEST(!found->revoke_at_epoch_ms.has_value());
        BOOST_TEST(found->isUsableAt(picasso::domain::nowEpochMs()));
    }

    BOOST_AUTO_TEST_CASE(unknown_hash_is_not_found) {
        BOOST_TEST(!sessions().findByTokenHash("no-such-hash").has_value());
    }

    BOOST_AUTO_TEST_CASE(revoke_makes_session_unusable) {
        const auto alice = user(76561198000000001ULL);
        sessions().store(makeSession("hash-alice", alice));

        sessions().revoke("hash-alice");
        const auto found = sessions().findByTokenHash("hash-alice");

        BOOST_TEST_REQUIRE(found.has_value());
        BOOST_TEST(found->revoke_at_epoch_ms.has_value());
        BOOST_TEST(!found->isUsableAt(picasso::domain::nowEpochMs()));
    }

    // One user may hold several sessions (phone + desktop); revoking one keeps the other.
    BOOST_AUTO_TEST_CASE(revoke_touches_only_its_own_session) {
        const auto alice = user(76561198000000001ULL);
        sessions().store(makeSession("hash-phone", alice));
        sessions().store(makeSession("hash-desktop", alice));

        sessions().revoke("hash-phone");

        const auto desktop = sessions().findByTokenHash("hash-desktop");
        BOOST_TEST_REQUIRE(desktop.has_value());
        BOOST_TEST(!desktop->revoke_at_epoch_ms.has_value());
    }

    BOOST_AUTO_TEST_CASE(duplicate_hash_is_rejected) {
        const auto alice = user(76561198000000001ULL);
        sessions().store(makeSession("hash-alice", alice));

        BOOST_CHECK_THROW(sessions().store(makeSession("hash-alice", alice)), std::runtime_error);
    }

    BOOST_AUTO_TEST_CASE(session_for_unknown_user_is_rejected) {
        BOOST_CHECK_THROW(
            sessions().store(makeSession("hash-ghost", SteamId(static_cast<std::uint64_t>(42)))),
            std::runtime_error
        );
    }

BOOST_AUTO_TEST_SUITE_END()
