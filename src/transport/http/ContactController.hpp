//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <memory>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "dto/Rest.hpp"
#include "service/Services.hpp"
#include "transport/http/HttpSupport.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::http {
    /**
     * Picasso's own social graph. Every handler
     * takes the caller's identity from the session token and nowhere else.
     *
     * Several answers are intentionally identical across cases the caller must not be
     * able to tell apart - see ContactService::request.
     */
    class ContactController : public oatpp::web::server::api::ApiController {
    public:
        ContactController(const std::shared_ptr<ObjectMapper>& objectMapper, service::Services services)
            : ApiController(objectMapper), services_(std::move(services)) {}

        ENDPOINT("GET", "/contacts", contacts,
                 REQUEST(std::shared_ptr<IncomingRequest>, request)) {
            const auto me = requireIdentity(*services_.auth, request);
            return createDtoResponse(Status::CODE_200, toDto(services_.contacts->snapshot(me)));
        }

        ENDPOINT("POST", "/contacts/requests", sendRequest,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 BODY_DTO(Object<dto::SteamIdBodyDto>, body)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto target = parseSteamId(body->steamId);
            OATPP_ASSERT_HTTP(target.has_value(), Status::CODE_422, "invalid steamId")

            using Outcome = service::ContactService::RequestOutcome;
            switch (services_.contacts->request(me, *target)) {
                case Outcome::Pending:
                    return createResponse(Status::CODE_202, "");
                case Outcome::Accepted:
                case Outcome::AlreadyContacts:
                    return createResponse(Status::CODE_200, "");
                case Outcome::Self:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_422, "cannot request yourself");
                case Outcome::UnblockFirst:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_409, "unblock_first");
                case Outcome::RateLimited:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_429, "too many contact requests");
            }
            throw oatpp::web::protocol::http::HttpError(Status::CODE_500, "unreachable");
        }

        ENDPOINT("POST", "/contacts/requests/{steamId}/accept", acceptRequest,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, steamId)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto from = parseSteamId(steamId);
            OATPP_ASSERT_HTTP(from.has_value(), Status::CODE_404, jsonError("no such request"))
            OATPP_ASSERT_HTTP(services_.contacts->accept(me, *from), Status::CODE_404, jsonError("no such request"))
            return createResponse(Status::CODE_200, "");
        }

        /* Decline when we are the recipient, withdraw when we are the sender; silent either way. */
        ENDPOINT("DELETE", "/contacts/requests/{steamId}", declineRequest,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, steamId)) {
            const auto me = requireIdentity(*services_.auth, request);
            if (const auto other = parseSteamId(steamId)) {
                services_.contacts->declineOrWithdraw(me, *other);
            }
            return createResponse(Status::CODE_204, "");
        }

        ENDPOINT("PUT", "/contacts/{steamId}", setLevel,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, steamId),
                 BODY_DTO(Object<dto::ContactLevelBodyDto>, body)) {
            const auto me = requireIdentity(*services_.auth, request);
            const auto other = parseSteamId(steamId);
            OATPP_ASSERT_HTTP(other.has_value(), Status::CODE_422, "invalid steamId")
            const auto level = body->level ? domain::parseContactLevel(*body->level) : std::nullopt;
            OATPP_ASSERT_HTTP(level.has_value(), Status::CODE_422, "level must be ally, friend or imposter")

            using Outcome = service::ContactService::LevelOutcome;
            switch (services_.contacts->setLevel(me, *other, *level)) {
                case Outcome::Ok:
                    return createResponse(Status::CODE_204, "");
                case Outcome::Self:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_422, "cannot set a level on yourself");
                case Outcome::NotAContact:
                    throw oatpp::web::protocol::http::HttpError(Status::CODE_404, jsonError("not a contact"));
            }
            throw oatpp::web::protocol::http::HttpError(Status::CODE_500, "unreachable");
        }

        /* Removal if ally/friend, unblock if imposter, otherwise nothing - idempotent. */
        ENDPOINT("DELETE", "/contacts/{steamId}", removeContact,
                 REQUEST(std::shared_ptr<IncomingRequest>, request),
                 PATH(String, steamId)) {
            const auto me = requireIdentity(*services_.auth, request);
            if (const auto other = parseSteamId(steamId)) {
                services_.contacts->removeOrUnblock(me, *other);
            }
            return createResponse(Status::CODE_204, "");
        }

    private:
        service::Services services_;
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
