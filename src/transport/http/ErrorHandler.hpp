//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "oatpp/core/data/mapping/ObjectMapper.hpp"
#include "oatpp/web/protocol/http/outgoing/Response.hpp"
#include "oatpp/web/server/handler/ErrorHandler.hpp"

namespace picasso::transport::http {
    /*
     * One JSON shape for every failure, so the client never has to parse an oat++
     * default error page.
     *
     * Only the (status, message, headers) overload exists in oat++ 1.3.0 - the
     * processor catches exceptions before we see them and passes a status in. The
     * not-implemented stubs are recovered from the message prefix; see
     * picasso::kNotImplementedPrefix.
     */
    class ErrorHandler final : public oatpp::base::Countable,
                               public oatpp::web::server::handler::ErrorHandler {
    public:
        explicit ErrorHandler(std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper);

        std::shared_ptr<oatpp::web::protocol::http::outgoing::Response>
        handleError(const oatpp::web::protocol::http::Status& status,
                    const oatpp::String& message,
                    const Headers& headers) override;

    private:
        std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper_;
    };
} // namespace picasso::transport::http
