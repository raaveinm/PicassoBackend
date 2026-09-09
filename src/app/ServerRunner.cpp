//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "app/ServerRunner.hpp"

#include <exception>

#include "oatpp/core/base/Environment.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"

#include "transport/http/ErrorHandler.hpp"

namespace picasso::app {
    int serve(Components& components) {
        const auto& config = components.config;

        std::shared_ptr<oatpp::network::tcp::server::ConnectionProvider> connectionProvider;
        try {
            connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared(
                {config.bindAddress, config.port, oatpp::network::Address::IP_4});
        } catch (const std::exception& error) {
            /*
             * The common case is "port already taken". oat++ throws a runtime_error
             * out of the provider constructor; letting it propagate out of main
             * terminates the process through std::terminate with no usable message.
             */
            OATPP_LOGE("ServerRunner", "cannot bind %s:%d - %s",
                       config.bindAddress.c_str(), config.port, error.what());
            return 1;
        }

        const auto connectionHandler =
            oatpp::web::server::HttpConnectionHandler::createShared(components.router);
        connectionHandler->setErrorHandler(
            std::make_shared<transport::http::ErrorHandler>(components.objectMapper));

        oatpp::network::Server server(connectionProvider, connectionHandler);

        OATPP_LOGI("ServerRunner", "listening on %s:%d", config.bindAddress.c_str(), config.port);

        /*
         * Blocking API: one thread per connection, which with long-lived WS
         * connections is one thread per client. Sized for a friend group, not a
         * public room - see CLAUDE.md "Architecture decisions".
         */
        server.run();

        return 0;
    }
} // namespace picasso::app
