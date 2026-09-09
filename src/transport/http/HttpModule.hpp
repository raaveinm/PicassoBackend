//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "oatpp/core/data/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include "service/Services.hpp"

namespace picasso::transport::http {
    /*
     * Module entry point. Adding a controller means editing this one function -
     * main() and the composition root stay unaware of how many there are.
     */
    void registerControllers(const std::shared_ptr<oatpp::web::server::HttpRouter>& router,
                             const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper,
                             const service::Services& services);
} // namespace picasso::transport::http
