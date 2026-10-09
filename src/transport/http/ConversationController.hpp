//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
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
     * being SSOT - history. The WS connection carries live traffic; history is how a
     * client that was offline catches up.
     *
     * History answers 501 until the chat step lands (roadmap step 2 of the contract).
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
        /// History
        ///////////////////////////////////////////////

        /*
         * `after` is the last message id the client already has; the response's
         * nextAfter is what it should pass next time. Absent `after` means "from the
         * beginning of what this server holds".
         */
        ENDPOINT("GET", "/conversations/{conversationId}/messages", messages,
                 PATH(String, conversationId),
                 QUERY(String, after, "after", ""),
                 QUERY(Int32, limit, "limit", 100)) {
            (void) conversationId;
            (void) after;
            (void) limit;

            notImplemented("transport: conversation history", "2: chat");
        }

    private:
        service::Services services_;
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
