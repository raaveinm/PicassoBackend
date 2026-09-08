//
// Created by Kirill "Raaveinm" on 9/7/26.
//

module;

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

export module test;

#include OATPP_CODEGEN_BEGIN(ApiController) // define api controller

namespace endpoints {
    export class EndpointController : public oatpp::web::server::api::ApiController {
    public:
        explicit EndpointController(const std::shared_ptr<ObjectMapper>& objectMapper)
            : ApiController(objectMapper) {}

        ENDPOINT("GET", "/ping", ping) {
            auto response = createResponse(Status::CODE_200, "pong");
            response->putHeader(Header::CONTENT_TYPE, "text/plain; charset=utf-8");
            response->putHeader("X-Service-Status", "Healthy");
            return response;
        }

        ENDPOINT("GET", "/", root) {
            const auto file = oatpp::String::loadFromFile(PICASSO_STATIC_ROOT "/index.html");

            OATPP_ASSERT_HTTP(file != nullptr, Status::CODE_404, "index.html not found");

            const auto response = createResponse(Status::CODE_200, file);
            response->putHeader(Header::CONTENT_TYPE, "text/html; charset=utf-8");

            return response;
        }
    };
} // namespace endpoints

#include OATPP_CODEGEN_END(ApiController)
