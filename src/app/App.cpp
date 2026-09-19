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
    namespace {
        const std::string TAG = "oatpp-core-app";
    }

    Components buildComponents(const Config& config) {
        Components components;
        components.config = config;

        components.objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
        components.router = oatpp::web::server::HttpRouter::createShared();

        components.activityLogger = std::make_shared<logger::ActivityLogger>();

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
            const std::string config_message =
                "Config {\n  " + config.bindAddress + ":"
            + std::string{std::to_string(config.port)} +
            + "\n  databaseDsn :" + config.databaseDsn
            + "\n  publicUrl :" + config.publicUrl
            + "\n  logLevel :" + config.logLevel
            + "\n}";
            OATPP_LOGI(TAG, config_message.c_str());
            auto components = buildComponents(config);
            exitCode = serve(components);
        } catch (const std::exception& error) {
            OATPP_LOGE(TAG, "startup failed: %s", error.what());
            exitCode = 1;
        }

        if (const auto leaked = oatpp::base::Environment::getObjectsCount(); leaked != 0) {
            OATPP_LOGW(TAG, "%d oatpp objects still alive at shutdown", leaked);
        }

        oatpp::base::Environment::destroy();
        return exitCode;
    }
} // namespace picasso::app
