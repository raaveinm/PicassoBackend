//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "app/App.hpp"

#include <exception>

#include "oatpp/core/base/Environment.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "app/Components.hpp"
#include "app/Config.hpp"
#include "app/ServerRunner.hpp"
#include "transport/http/HttpModule.hpp"

namespace picasso::app {
    Components buildComponents(const Config& config) {
        Components components;
        components.config = config;

        components.objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
        components.router = oatpp::web::server::HttpRouter::createShared();

        components.repositories = storage::makeRepositories(config.databaseDsn);

        components.hub = std::make_shared<transport::ws::ConnectionHub>();

        components.services = service::makeServices(
            components.repositories,
            components.hub,
            components.hub,
            std::make_shared<steam::OpenIdVerifier>(config.publicUrl));

        transport::http::registerControllers(components.router, components.objectMapper, components.services);
        components.wsEndpoint = transport::ws::registerWsEndpoint(
            components.router, components.objectMapper, components.hub, components.services);

        return components;
    }

    int run() {
        oatpp::base::Environment::init();

        int exitCode;
        try {
            const auto config = loadConfigFromEnvironment();
            auto components = buildComponents(config);
            exitCode = serve(components);
        } catch (const std::exception& error) {
            OATPP_LOGE("App", "startup failed: %s", error.what());
            exitCode = 1;
        }

        const auto leaked = oatpp::base::Environment::getObjectsCount();
        if (leaked != 0) {
            OATPP_LOGW("App", "%d oatpp objects still alive at shutdown", leaked);
        }

        oatpp::base::Environment::destroy();
        return exitCode;
    }
} // namespace picasso::app
