//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "domain/Errors.hpp"
#include "dto/Rest.hpp"
#include "service/Services.hpp"
#include "transport/http/HttpSupport.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::http {
    /**
     * Creating conversations, inviting into palettes, and - the cache-sync side of
     * being SSOT - sync and history. The WS connection carries live traffic; these are how a
     * client that was offline catches up.
     */
    class ConversationController : public oatpp::web::server::api::ApiController {
    public:
        ConversationController(const std::shared_ptr<ObjectMapper>& objectMapper,
                               service::Services services)
            : ApiController(objectMapper), services_(std::move(services)) {}

        ///////////////////////////////////////////////
        /// Creation
        ///////////////////////////////////////////////

        /**
         * A dm is get-or-create (200 if it existed, 201 if new) and needs a mutual
         * ally; a palette always creates (201) and everyone in inviteSteamIds becomes a
         * pending invitation. Every refusal on contacts is the same 403 on purpose.
         */
        ENDPOINT("POST", "/conversations", createConversation,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 BODY_DTO(Object<dto::CreateConversationDto>, body)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto kind = body->kind ? domain::parseConversationKind(*body->kind) : std::nullopt;
            OATPP_ASSERT_HTTP(kind.has_value(), Status::CODE_422, "kind must be dm or palette")

            service::ConversationService::CreateResult result;
            if (*kind == domain::ConversationKind::Dm) {
                const auto peer = parseSteamId(body->peerSteamId);
                OATPP_ASSERT_HTTP(peer.has_value(), Status::CODE_422, "peerSteamId is required for a dm")
                result = services_.conversations->createDm(me, *peer);
            } else {
                std::vector<domain::SteamId> invitees;
                if (body->inviteSteamIds) {
                    for (const auto& raw : *body->inviteSteamIds) {
                        const auto invitee = parseSteamId(raw);
                        OATPP_ASSERT_HTTP(invitee.has_value(), Status::CODE_422, "invalid steamId in inviteSteamIds")
                        invitees.push_back(*invitee);
                    }
                }
                result = services_.conversations->createPalette(
                    me, body->name ? *body->name : std::string(), invitees);
            }

            using Outcome = service::ConversationService::CreateOutcome;
            switch (result.outcome) {
                case Outcome::Created:
                    return createDtoResponse(Status::CODE_201, toDto(*result.conversation));
                case Outcome::Existing:
                    return createDtoResponse(Status::CODE_200, toDto(*result.conversation));
                case Outcome::NotAllowed:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_403, "not_allowed");
                case Outcome::Self:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_422, "cannot start a conversation with yourself");
                case Outcome::InvalidName:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_422, "palette name must be 1-64 bytes");
                case Outcome::TooManyInvitees:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_422, "too many invitees");
            }
            throw oatpp::web::protocol::http::HttpError(Status::CODE_500, "unreachable");
        }

        ///////////////////////////////////////////////
        /// Palette invitations
        ///////////////////////////////////////////////

        ENDPOINT("POST", "/conversations/{conversationId}/invites", invite,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, conversationId),
                 BODY_DTO(Object<dto::SteamIdBodyDto>, body)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto palette = parseConversationId(conversationId);
            OATPP_ASSERT_HTTP(palette.has_value(), Status::CODE_404, jsonError("no such conversation"))
            const auto invitee = parseSteamId(body->steamId);
            OATPP_ASSERT_HTTP(invitee.has_value(), Status::CODE_422, "invalid steamId")

            const auto result = services_.conversations->invite(me, *palette, *invitee);

            using Outcome = service::ConversationService::InviteOutcome;
            switch (result.outcome) {
                case Outcome::Invited:
                    return createDtoResponse(Status::CODE_201, toDto(*result.conversation));
                case Outcome::Unchanged:
                    return createDtoResponse(Status::CODE_200, toDto(*result.conversation));
                case Outcome::NotAMember:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_404, jsonError("no such conversation"));
                case Outcome::NotAllowed:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_403, "not_allowed");
                case Outcome::Self:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_422, "cannot invite yourself");
            }
            throw oatpp::web::protocol::http::HttpError(Status::CODE_500, "unreachable");
        }

        ENDPOINT("POST", "/palette-invites/{conversationId}/accept", acceptInvite,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, conversationId)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto palette = parseConversationId(conversationId);
            OATPP_ASSERT_HTTP(palette.has_value(), Status::CODE_404, jsonError("no such invitation"))

            const auto joined = services_.conversations->acceptInvite(me, *palette);
            OATPP_ASSERT_HTTP(joined.has_value(), Status::CODE_404, jsonError("no such invitation"))
            return createDtoResponse(Status::CODE_200, toDto(*joined));
        }

        /* Silent: the inviter is not told. Idempotent, so an unknown invitation is still a 204. */
        ENDPOINT("DELETE", "/palette-invites/{conversationId}", declineInvite,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, conversationId)) {
            const auto me = requireIdentity(*services_.auth, request);
            if (const auto palette = parseConversationId(conversationId)) {
                services_.conversations->declineInvite(me, *palette);
            }
            return createResponse(Status::CODE_204, "");
        }

        ///////////////////////////////////////////////
        /// Catching up
        ///////////////////////////////////////////////

        /*
         * One batched catch-up for every conversation. The server walks ALL of the caller's
         * conversations, not only the ones named in `cursors`, so this also discovers new ones
         * and (by their absence) removed ones. See chat-sync-contract.md, section 4.2.
         */
        ENDPOINT("POST", "/sync", sync,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 BODY_DTO(Object<dto::SyncRequestDto>, body)) {
            const auto me = requireIdentity(*services_.auth, request);

            std::vector<service::ChatService::SyncCursor> cursors;
            if (body->cursors) {
                for (const auto& cursor : *body->cursors) {
                    const auto conversation = cursor ? parseConversationId(cursor->conversationId) : std::nullopt;
                    const auto after = cursor ? parseUnsigned(cursor->after ? *cursor->after : std::string()) : std::nullopt;
                    OATPP_ASSERT_HTTP(
                        conversation.has_value() && after.has_value() && *after <= static_cast<std::uint64_t>(INT64_MAX),
                        Status::CODE_422,
                        "invalid cursor")
                    cursors.push_back({.conversation_id = *conversation, .after = domain::MessageId(static_cast<std::int64_t>(*after))});
                }
            }

            std::optional<std::int64_t> deleted_since;
            if (body->deletedSince && !body->deletedSince->empty()) {
                const auto since = parseUnsigned(*body->deletedSince);
                OATPP_ASSERT_HTTP(since.has_value(), Status::CODE_422, "invalid deletedSince")
                deleted_since = static_cast<std::int64_t>(*since);
            }

            const auto [server_time, deleted_cursor, conversations, deleted]
            = services_.chat->sync(me, cursors, deleted_since, body->limit ? *body->limit : 0);

            const auto response = dto::SyncResponseDto::createShared();
            response->serverTime = oatpp::String(std::to_string(server_time));
            response->deletedCursor = oatpp::String(std::to_string(deleted_cursor));
            response->conversations = oatpp::List<oatpp::Object<dto::SyncConversationDto>>::createShared();
            for (const auto& entry : conversations) {
                auto row = dto::SyncConversationDto::createShared();
                row->conversation = toDto(entry.conversation, entry.writable);
                row->mode = entry.reset ? "reset" : "delta";
                row->hasMoreBefore = entry.has_more_before;
                row->messages = oatpp::List<oatpp::Object<dto::MessageDto>>::createShared();
                for (const auto& message : entry.messages) {
                    row->messages->push_back(toDto(message));
                }
                response->conversations->push_back(row);
            }
            response->deleted = oatpp::List<oatpp::Object<dto::MessageDeletedDto>>::createShared();
            for (const auto& [conversation_id, message_id] : deleted) {
                auto row = dto::MessageDeletedDto::createShared();
                row->conversationId = oatpp::String(std::to_string(conversation_id.value()));
                row->messageId = oatpp::String(std::to_string(message_id.value()));
                response->deleted->push_back(row);
            }
            return createDtoResponse(Status::CODE_200, response);
        }

        ///////////////////////////////////////////////
        /// History and deletion
        ///////////////////////////////////////////////

        /*
         * Scroll-up: the newest `limit` messages older than `before`, ascending. `before` is
         * the smallest message id the client already holds. Deleted messages are never listed.
         */
        ENDPOINT("GET", "/conversations/{conversationId}/messages", messages,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, conversationId),
                 QUERY(String, before, "before", ""),
                 QUERY(Int32, limit, "limit", 100)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto conversation = parseConversationId(conversationId);
            OATPP_ASSERT_HTTP(conversation.has_value(), Status::CODE_404, jsonError("no such conversation"))
            const auto before_id = parseUnsigned(before ? *before : std::string());
            OATPP_ASSERT_HTTP(
                before_id.has_value() && *before_id <= static_cast<std::uint64_t>(INT64_MAX),
                Status::CODE_422,
                "before is required")

            const auto [member, messages, has_more_before] = services_.chat->history(
                me,
                *conversation,
                domain::MessageId(static_cast<std::int64_t>(*before_id)),
                limit ? *limit : 0);
            OATPP_ASSERT_HTTP(member, Status::CODE_404, jsonError("no such conversation"))

            const auto body = dto::MessagePageDto::createShared();
            body->messages = oatpp::List<oatpp::Object<dto::MessageDto>>::createShared();
            for (const auto& message : messages) {
                body->messages->push_back(toDto(message));
            }
            body->hasMoreBefore = has_more_before;
            return createDtoResponse(Status::CODE_200, body);
        }

        ENDPOINT("DELETE", "/conversations/{conversationId}/messages/{messageId}", removeMessage,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, conversationId),
                 PATH(String, messageId)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto conversation = parseConversationId(conversationId);
            const auto message = parseUnsigned(messageId ? *messageId : std::string());
            OATPP_ASSERT_HTTP(
                conversation.has_value() && message.has_value() && *message <= static_cast<std::uint64_t>(INT64_MAX),
                Status::CODE_404,
                jsonError("no such message"))

            const auto outcome = services_.chat->remove(
                me,
                *conversation,
                domain::MessageId(static_cast<std::int64_t>(*message)));
            OATPP_ASSERT_HTTP(
                outcome == service::ChatService::RemoveOutcome::Removed,
                Status::CODE_404,
                jsonError("no such message"))
            return createResponse(Status::CODE_204, "");
        }

    private:
        service::Services services_;
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
