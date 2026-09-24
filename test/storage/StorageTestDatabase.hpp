//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

#include <boost/test/unit_test.hpp>

#include "oatpp-postgresql/orm.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/orm/DbClient.hpp"

#include "domain/Ids.hpp"
#include "storage/Repositories.hpp"
#include "storage/Rows.hpp"

namespace picasso::domain {
    /* Lets BOOST_TEST print ids on failure. Found by ADL; test-only, domain/ stays stream-free. */
    template <typename Tag, typename T>
    std::ostream& boost_test_print_type(std::ostream& stream, const Id<Tag, T>& id) {
        return stream << id.value();
    }
} // namespace picasso::domain

namespace picasso::test {
    inline const auto TEST_DB_DSN_ENV = "PICASSO_TEST_DB_DSN";

    inline std::string testDsn() {
        const char* const dsn = std::getenv(TEST_DB_DSN_ENV);
        return dsn != nullptr ? std::string(dsn) : std::string();
    }

    /* Boost.Test precondition: suites that need Postgres are reported as skipped without it. */
    inline boost::test_tools::assertion_result hasTestDatabase(boost::unit_test::test_unit_id) {
        boost::test_tools::assertion_result result(!testDsn().empty());
        result.message() << TEST_DB_DSN_ENV << " is not set";
        return result;
    }

#include OATPP_CODEGEN_BEGIN(DbClient)

    /**
     * Rows the repositories read but have no port to write yet (users, dm/palette
     * membership). Lives in test/, not storage/, so none of it becomes production
     * API by accident.
     */
    class SeedDbClient : public oatpp::orm::DbClient {
    public:
        explicit SeedDbClient(const std::shared_ptr<oatpp::orm::Executor>& executor)
            : DbClient(executor) {}

        QUERY(truncateAll,
            "TRUNCATE users, sessions, conversations, chat, palette, members, message_data, game_queue "
            "RESTART IDENTITY CASCADE;")

        QUERY(insertUser,
            "INSERT INTO users (steam_id) VALUES (:steam_id);",
            PARAM(oatpp::Int64, steam_id))

        QUERY(insertConversation,
            "INSERT INTO conversations (kind, created_at) VALUES (:kind, 0) RETURNING id AS value;",
            PARAM(oatpp::String, kind))

        QUERY(insertChat,
            "INSERT INTO chat (conversation_id, member_a, member_b) VALUES (:conversation_id, :member_a, :member_b);",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, member_a),
            PARAM(oatpp::Int64, member_b))

        QUERY(insertPalette,
            "INSERT INTO palette (conversation_id, name) VALUES (:conversation_id, :name);",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::String, name))

        QUERY(insertMember,
            "INSERT INTO members (palette_id, user_id, joined_at) VALUES (:palette_id, :user_id, 0);",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, user_id))
    };

#include OATPP_CODEGEN_END(DbClient)

    /**
     * One connection pool per process, built on first use and deliberately never
     * destroyed: oatpp's pool keeps itself alive through a detached cleanup thread,
     * so tearing it down per test case would leak a pool and a thread each time.
     */
    class TestDatabase {
    public:
        static TestDatabase& instance() {
            static auto* const INSTANCE = new TestDatabase();
            return *INSTANCE;
        }

        storage::Repositories repositories;
        std::shared_ptr<SeedDbClient> seed;

    private:
        TestDatabase()
            // makeRepositories runs the real migration, so the schema under test is 0001_init.sql.
            : repositories(storage::makeRepositories(testDsn())) {
            const auto connection_provider = std::make_shared<oatpp::postgresql::ConnectionProvider>(
                oatpp::String(testDsn()));
            seed = std::make_shared<SeedDbClient>(
                std::make_shared<oatpp::postgresql::Executor>(connection_provider));
        }
    };

    /**
     * Per-test-case fixture: empty tables, identities restarted, plus helpers to
     * seed the rows a test needs. Throws on a failed seed so a broken fixture is
     * never mistaken for a repository bug.
     */
    class StorageFixture {
    public:
        StorageFixture() : db_(TestDatabase::instance()) {
            check(db_.seed->truncateAll(), "truncateAll");
        }

        [[nodiscard]] domain::ChatRepository& chat() const { return *db_.repositories.chat; }
        [[nodiscard]] domain::ConversationRepository& conversations() const { return *db_.repositories.conversations; }
        [[nodiscard]] domain::SessionRepository& sessions() const { return *db_.repositories.sessions; }

        [[nodiscard]] domain::SteamId user(const std::uint64_t steam_id) const {
            check(db_.seed->insertUser(oatpp::Int64(static_cast<v_int64>(steam_id))), "insertUser");
            return domain::SteamId(steam_id);
        }

        /* A dm between two existing users. Orders them itself - the schema demands member_a < member_b. */
        [[nodiscard]] domain::ConversationId dm(const domain::SteamId first, const domain::SteamId second) const {
            const auto conversation_id = conversation("dm");
            const auto [low, high] = std::minmax(first.value(), second.value());
            check(
                db_.seed->insertChat(
                    oatpp::Int64(conversation_id.value()),
                    oatpp::Int64(static_cast<v_int64>(low)),
                    oatpp::Int64(static_cast<v_int64>(high))
                ),
                "insertChat"
            );
            return conversation_id;
        }

        [[nodiscard]] domain::ConversationId palette(const std::initializer_list<domain::SteamId> members) const {
            const auto conversation_id = conversation("palette");
            check(
                db_.seed->insertPalette(oatpp::Int64(conversation_id.value()), oatpp::String("test palette")),
                "insertPalette"
            );
            for (const auto& member : members) {
                check(
                    db_.seed->insertMember(
                        oatpp::Int64(conversation_id.value()),
                        oatpp::Int64(static_cast<v_int64>(member.value()))
                    ),
                    "insertMember"
                );
            }
            return conversation_id;
        }

    private:
        domain::ConversationId conversation(const std::string& kind) const {
            const auto result = db_.seed->insertConversation(oatpp::String(kind));
            check(result, "insertConversation");
            const auto rows = result->fetch<oatpp::Vector<oatpp::Object<storage::ScalarInt64Row>>>();
            return domain::ConversationId(*rows->at(0)->value);
        }

        static void check(const std::shared_ptr<oatpp::orm::QueryResult>& result, const std::string& what) {
            if (!result->isSuccess()) {
                throw std::runtime_error("seed " + what + " failed: " + *result->getErrorMessage());
            }
        }

        TestDatabase& db_;
    };
} // namespace picasso::test
