//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "oatpp/core/data/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp-websocket/ConnectionHandler.hpp"

#include "service/Services.hpp"
#include "transport/ws/ConnectionHub.hpp"

namespace picasso::transport::ws {
    /*
     * Handles owned by the caller for the process lifetime. The connection handler
     * and the instance listener must outlive every socket they created, and nothing
     * else in the wiring holds them.
     */
    struct WsEndpoint {
        std::shared_ptr<oatpp::websocket::ConnectionHandler> connectionHandler;
        std::shared_ptr<oatpp::websocket::ConnectionHandler::SocketInstanceListener> instanceListener;
    };

    /*
     * Module entry point. Note the argument order this forces on the composition
     * root: the hub has to exist before the services, because the hub *is* their
     * PresenceRegistry and SignalTransport, and the services have to exist before
     * this call, because sessions dispatch into them. Two-phase by necessity.
     */
    WsEndpoint registerWsEndpoint(const std::shared_ptr<oatpp::web::server::HttpRouter>& router,
                                  const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper,
                                  const std::shared_ptr<ConnectionHub>& hub,
                                  const service::Services& services);
} // namespace picasso::transport::ws
