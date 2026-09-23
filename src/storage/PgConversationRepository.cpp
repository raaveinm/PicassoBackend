//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgConversationRepository.hpp"

#include <stdexcept>
#include <utility>

#include "storage/Rows.hpp"

namespace picasso::storage {

    bool PgConversationRepository::isMember(
        const domain::ConversationId& conversation_id,
        const domain::SteamId steam_id
    ) {
        const auto result = db_->isMember(
            oatpp::Int64(conversation_id.value()),
            oatpp::Int64(static_cast<v_int64>(steam_id.value())));

        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgConversationRepository::isMember: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgConversationRepository::isMember: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ExistsRow>>>();
        return !rows->empty() && *rows->at(0)->is_member;
    }

    std::vector<domain::SteamId> PgConversationRepository::members(const domain::ConversationId& conversation_id) {
        const auto result = db_->selectMembers(oatpp::Int64(conversation_id.value()));
        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgConversationRepository::members: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgConversationRepository::members: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        std::vector<domain::SteamId> steamIds;
        steamIds.reserve(rows->size());
        for (const auto& row : *rows) {
            steamIds.emplace_back(static_cast<std::uint64_t>(*row->value));
        }
        return steamIds;
    }

    std::vector<domain::ConversationId> PgConversationRepository::conversationsOf(const domain::SteamId steamId) {
        const auto result = db_->selectConversationsOf(oatpp::Int64(static_cast<v_int64>(steamId.value())));
        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgConversationRepository::conversationsOf: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgConversationRepository::conversationsOf: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        std::vector<domain::ConversationId> ids;
        ids.reserve(rows->size());
        for (const auto& row : *rows) {
            ids.emplace_back(*row->value);
        }
        return ids;
    }
} // namespace picasso::storage
