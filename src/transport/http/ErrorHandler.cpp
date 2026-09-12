//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/http/ErrorHandler.hpp"

#include <string>
#include <utility>

#include "oatpp/web/protocol/http/outgoing/ResponseFactory.hpp"

#include "domain/Errors.hpp"
#include "dto/Rest.hpp"

namespace picasso::transport::http {
    namespace {
        using ResponseFactory = oatpp::web::protocol::http::outgoing::ResponseFactory;
        using Status = oatpp::web::protocol::http::Status;
        using Header = oatpp::web::protocol::http::Header;
    } // namespace

    ErrorHandler::ErrorHandler(std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper)
        : objectMapper_(std::move(objectMapper)) {}

    [[deprecated]] std::shared_ptr<oatpp::web::protocol::http::outgoing::Response>
    ErrorHandler::handleError(const oatpp::web::protocol::http::Status& status,
                              const oatpp::String& message,
                              const Headers& headers) {
        auto effectiveStatus = status;
        std::string text = message ? *message : std::string{};

        /*
         * A stub reached the wire. It arrives as a 500 because that is what the
         * processor makes of any std::exception; the prefix is what says otherwise.
         */
        if (text.rfind(std::string(kNotImplementedPrefix), 0) == 0) {
            effectiveStatus = Status::CODE_501;
            text.erase(0, kNotImplementedPrefix.size());
        }

        if (effectiveStatus == Status::CODE_404) {
            const auto file = oatpp::String::loadFromFile(PICASSO_STATIC_ROOT "/not_found.html");
            auto response = ResponseFactory::createResponse(Status::CODE_404, file);
            response->putHeader(Header::CONTENT_TYPE, "text/html; charset=utf-8");
            for (const auto& [name, value] : headers.getAll()) {
                response->putHeader(name.toString(), value.toString());
            }
            return response;
        }

        const auto body = dto::ErrorDto::createShared();
        body->status = effectiveStatus.code;
        body->code = effectiveStatus.description;
        body->message = text;

        auto response = ResponseFactory::createResponse(effectiveStatus, body, objectMapper_);
        for (const auto& [name, value] : headers.getAll()) {
            response->putHeader(name.toString(), value.toString());
        }

        return response;
    }
} // namespace picasso::transport::http
