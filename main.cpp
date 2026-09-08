#include "oatpp/web/server/HttpConnectionHandler.hpp"

#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"


import test;


void run() {
    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    /* Create Router for HTTP requests routing */
    const auto router = oatpp::web::server::HttpRouter::createShared();
    // ROUTERS
    router->addController(std::make_shared<endpoints::EndpointController>(objectMapper));
    /* Create HTTP connection handler with router */
    const auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
    /* Create TCP connection provider */
    const auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"localhost", 8000, oatpp::network::Address::IP_4});
    /* Create server which takes provided TCP connections and passes them to HTTP connection handler */
    oatpp::network::Server server(connectionProvider, connectionHandler);
    /* Print info about server port */
    OATPP_LOGI("Picasso-Test", "Server running on port %p", connectionProvider->getProperty("port").getData());

    /* Run server */
    server.run();
}

int main() {
    oatpp::base::Environment::init();
    run();
    oatpp::base::Environment::destroy();
    return 0;
}