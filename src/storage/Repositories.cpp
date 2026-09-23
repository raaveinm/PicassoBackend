//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/Repositories.hpp"

#include <chrono>
#include <stdexcept>

#include <oatpp/core/base/Environment.hpp>
#include "oatpp-postgresql/ConnectionProvider.hpp"
#include "oatpp-postgresql/Executor.hpp"

#include "PicassoDatabaseClient.hpp"
#include "storage/PgChatRepository.hpp"
#include "storage/PgConversationRepository.hpp"
#include "storage/PgSessionRepository.hpp"

namespace picasso::storage {

    namespace {
        constexpr v_int64 max_connections = 10;
        constexpr  std::chrono::minutes connection_ttl{ 2 };
    }

    static Repositories makeRepositories(const std::string& dsn) {
        if (dsn.empty()) {
            OATPP_LOGE(DATABASE_TAG, "DATABASE FATAL ERR -> dsn is not set");
            throw std::invalid_argument("dsn is not set");
        }

        const auto connection_provider = std::make_shared<oatpp::postgresql::ConnectionProvider>(dsn);
        const auto pool = oatpp::postgresql::ConnectionPool::createShared(
            connection_provider,
            max_connections,
            connection_ttl
        );
        auto executor = std::make_shared<oatpp::postgresql::Executor>(pool);
        auto db = std::make_shared<PicassoDatabaseClient>(executor);

        return Repositories{
            .chat = std::make_shared<PgChatRepository>(db),
            .conversations = std::make_shared<PgConversationRepository>(db),
            .sessions = std::make_shared<PgSessionRepository>(db),
        };
    }
} // namespace picasso::storage
