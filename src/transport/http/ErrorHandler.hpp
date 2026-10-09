//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "oatpp/core/data/mapping/ObjectMapper.hpp"
#include "oatpp/web/protocol/http/outgoing/Response.hpp"
#include "oatpp/web/server/handler/ErrorHandler.hpp"

namespace picasso::transport::http {

    inline constexpr std::string_view JSON_ERROR_PREFIX = "json-error: ";

    inline oatpp::String jsonError(const std::string& message) {
        return oatpp::String(std::string(JSON_ERROR_PREFIX) + message); // NOLINT(*-return-braced-init-list)
    }

    class ErrorHandler final : public oatpp::base::Countable,
                               public oatpp::web::server::handler::ErrorHandler {
    public:
        explicit ErrorHandler(std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper);

        [[deprecated]] std::shared_ptr<oatpp::web::protocol::http::outgoing::Response>
        handleError(const oatpp::web::protocol::http::Status& status,
                    const oatpp::String& message,
                    const Headers& headers) override;

    private:
        std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper_;
    };
} // namespace picasso::transport::http
