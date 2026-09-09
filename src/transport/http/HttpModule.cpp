//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/http/HttpModule.hpp"

#include "transport/http/AuthController.hpp"
#include "transport/http/ConversationController.hpp"
#include "transport/http/HealthController.hpp"

namespace picasso::transport::http {
    void registerControllers(const std::shared_ptr<oatpp::web::server::HttpRouter>& router,
                             const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper,
                             const service::Services& services) {
        router->addController(std::make_shared<HealthController>(objectMapper));
        router->addController(std::make_shared<AuthController>(objectMapper, services.auth));
        router->addController(std::make_shared<ConversationController>(objectMapper, services));
    }
} // namespace picasso::transport::http
