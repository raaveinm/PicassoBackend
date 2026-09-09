//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "oatpp/core/data/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include "app/Config.hpp"
#include "service/Services.hpp"
#include "storage/Repositories.hpp"
#include "transport/ws/ConnectionHub.hpp"
#include "transport/ws/WsModule.hpp"

namespace picasso::app {

    struct Components {
        Config config;

        std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper;
        std::shared_ptr<oatpp::web::server::HttpRouter> router;

        storage::Repositories repositories;
        std::shared_ptr<transport::ws::ConnectionHub> hub;
        service::Services services;
        transport::ws::WsEndpoint wsEndpoint;
    };

    Components buildComponents(const Config& config);
} // namespace picasso::app
