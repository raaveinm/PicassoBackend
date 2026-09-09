//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <map>
#include <memory>
#include <string>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "dto/Rest.hpp"
#include "service/AuthService.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::http {
    /*
     * Steam OpenID login. Both endpoints currently reach a stub that throws, so they
     * answer 501 - the URL shape is fixed and the client can be written against it,
     * but nothing authenticates yet (roadmap step 3).
     */
    class AuthController : public oatpp::web::server::api::ApiController {
    public:
        AuthController(const std::shared_ptr<ObjectMapper>& objectMapper,
                       std::shared_ptr<service::AuthService> auth)
            : ApiController(objectMapper), auth_(std::move(auth)) {}

        ENDPOINT("GET", "/auth/steam/begin", authBegin) {
            const auto response = createResponse(Status::CODE_302, "");
            /* oat++ 1.3.0's Header has no LOCATION constant - only CONTENT_TYPE and AUTHORIZATION. */
            response->putHeader("Location", auth_->beginLoginUrl().c_str());
            return response;
        }

        /*
         * Steam redirects the user back here with the assertion in the query string.
         * Every openid.* parameter has to be forwarded verbatim to check_authentication,
         * so they are collected rather than picked apart.
         */
        ENDPOINT("GET", "/auth/steam/return", authReturn,
                 REQUEST(std::shared_ptr<IncomingRequest>, request)) {
            std::map<std::string, std::string> params;
            for (const auto& [name, value] : request->getQueryParameters().getAll()) {
                params.emplace(*name.toString(), *value.toString());
            }

            const auto issued = auth_->completeLogin(params);
            OATPP_ASSERT_HTTP(issued.has_value(), Status::CODE_401, "steam assertion rejected")

            auto body = dto::AuthTokenDto::createShared();
            body->token = issued->token.c_str();
            body->steamId = std::to_string(issued->steamId.value()).c_str();
            body->expiresAt = issued->expiresAtEpochMs;

            return createDtoResponse(Status::CODE_200, body);
        }

    private:
        std::shared_ptr<service::AuthService> auth_;
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
