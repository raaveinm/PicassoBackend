//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "dto/Rest.hpp"
#include "service/AuthService.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::http {
    class AuthController : public oatpp::web::server::api::ApiController {
    public:
        AuthController(const std::shared_ptr<ObjectMapper>& objectMapper,
                       std::shared_ptr<service::AuthService> auth)
            : ApiController(objectMapper), auth_(std::move(auth)) {}

        ENDPOINT("GET", "/auth/steam/begin", authBegin, QUERY(String, state, "state", "")) {
            const auto response = createResponse(Status::CODE_302, "");
            /* oat++ 1.3.0's Header has no LOCATION constant - only CONTENT_TYPE and AUTHORIZATION. */
            response->putHeader("Location", auth_->beginLoginUrl(decodeParam(state)).c_str());
            return response;
        }

        /*
         * Steam redirects the user back here with the assertion in the query string.
         * Every openid.* parameter has to be forwarded verbatim to check_authentication,
         * so they are collected rather than picked apart.
         */
        ENDPOINT("GET", "/auth/steam/return",
            authReturn,
            REQUEST(std::shared_ptr<IncomingRequest>,
            request))
        {
            std::map<std::string, std::string> params;
            for (const auto& [name, value] : request->getQueryParameters().getAll()) {
                params.emplace(decodeParam(name.toString()), decodeParam(value.toString()));
            }

            const auto issued = auth_->completeLogin(params);
            OATPP_ASSERT_HTTP(issued.has_value(), Status::CODE_401, "steam assertion rejected")

            const auto stateIt = params.find("state");
            if (stateIt != params.end() && !stateIt->second.empty()) {
                auth_->parkLogin(stateIt->second, *issued);

                const auto page = createResponse(Status::CODE_200, LOGIN_COMPLETE_PAGE);
                page->putHeader(Header::CONTENT_TYPE, "text/html; charset=utf-8");
                return page;
            }

            /* No nonce: a bare browser login with nothing waiting to claim it. */
            return createDtoResponse(Status::CODE_200, toTokenDto(*issued));
        }

        ENDPOINT("GET", "/auth/steam/poll", authPoll, QUERY(String, state, "state")) {
            const auto issued = auth_->claimLogin(decodeParam(state));
            if (!issued) { return createResponse(Status::CODE_204, ""); }

            return createDtoResponse(Status::CODE_200, toTokenDto(*issued));
        }

        /* Revokes the presented session. Idempotent, so an already-dead token is still a 204. */
        ENDPOINT("POST", "/auth/logout", authLogout,
                 HEADER(String, authorization, "Authorization")) {
            auth_->logout(authorization->c_str());
            return createResponse(Status::CODE_204, "");
        }

    private:
        static std::string decodeParam(const oatpp::String& raw) {
            if (!raw) return {};

            const std::string_view in{raw->c_str(), raw->size()};
            std::string out;
            out.reserve(in.size());

            for (std::size_t i = 0; i < in.size(); ++i) {
                if (in[i] == '%' && i + 2 < in.size() &&
                    std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
                    std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
                    out.push_back(static_cast<char>(std::stoi(std::string{in.substr(i + 1, 2)}, nullptr, 16)));
                    i += 2;
                } else if (in[i] == '+') {
                    /* Form-urlencoded: a literal '+' in a query string is a space. */
                    out.push_back(' ');
                } else {
                    out.push_back(in[i]);
                }
            }
            return out;
        }

        static oatpp::Object<dto::AuthTokenDto> toTokenDto(const service::IssuedToken& issued) {
            const auto body = dto::AuthTokenDto::createShared();
            body->token = issued.token.c_str();
            /* int64 over the wire would lose precision in a JS/JSON client; steamIds are sent as strings. */
            body->steamId = std::to_string(issued.steamId.value()).c_str();
            body->expiresAt = issued.expiresAtEpochMs;
            return body;
        }

        static constexpr auto LOGIN_COMPLETE_PAGE =
            "<!doctype html><meta charset=utf-8>"
            "<title>Signed in</title>"
            "<style>body{font-family:system-ui,sans-serif;display:grid;place-items:center;"
            "height:100vh;margin:0;background:#16181d;color:#e6e6e6}</style>"
            "<main><h1>Signed in</h1><p>You can close this tab and return to PickUsAll.</p></main>";

        std::shared_ptr<service::AuthService> auth_;
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
