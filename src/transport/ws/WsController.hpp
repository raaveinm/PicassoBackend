//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp-websocket/ConnectionHandler.hpp"
#include "oatpp-websocket/Handshaker.hpp"

#include "service/Services.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::ws {
    /* Key under which the verified steamId is handed to the upgraded connection. */
    inline constexpr const char* kSteamIdParameter = "steamId";

    /*
     * The upgrade endpoint, and the single place a connection acquires an identity.
     * Authentication happens here, before the handshake response is returned - a
     * socket that reaches the session layer has already been vouched for.
     */
    class WsController : public oatpp::web::server::api::ApiController {
    public:
        WsController(const std::shared_ptr<ObjectMapper>& objectMapper,
                     std::shared_ptr<oatpp::websocket::ConnectionHandler> connectionHandler,
                     std::shared_ptr<service::AuthService> auth)
            : ApiController(objectMapper),
              connectionHandler_(std::move(connectionHandler)),
              auth_(std::move(auth)) {}

        ENDPOINT("GET", "/ws", ws, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
            /*
             * The client is a KMP app, not a browser, so it can set headers on the
             * upgrade request - no need for the query-param or ticket workarounds a
             * browser WebSocket would force.
             */
            const auto authorization = request->getHeader("Authorization");
            OATPP_ASSERT_HTTP(authorization, Status::CODE_401, "missing bearer token")

            const auto steamId = auth_->authenticate(*authorization);
            OATPP_ASSERT_HTTP(steamId.has_value(), Status::CODE_401, "invalid or expired token")

            const auto response =
                oatpp::websocket::Handshaker::serversideHandshake(request->getHeaders(), connectionHandler_);

            /*
             * The identity travels to the session through the upgrade parameters, not
             * through anything the client can send later.
             */
            auto parameters = std::make_shared<oatpp::network::ConnectionHandler::ParameterMap>();
            (*parameters)[kSteamIdParameter] = oatpp::String(std::to_string(steamId->value()).c_str());
            response->setConnectionUpgradeParameters(parameters);

            return response;
        }

    private:
        std::shared_ptr<oatpp::websocket::ConnectionHandler> connectionHandler_;
        std::shared_ptr<service::AuthService> auth_;
    };
} // namespace picasso::transport::ws

#include OATPP_CODEGEN_END(ApiController)
