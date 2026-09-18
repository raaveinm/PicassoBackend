//
// Created by Kirill "Raaveinm" on 9/7/26.
//

#pragma once

#include <cctype>
#include <string>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::http {
    class HealthController : public oatpp::web::server::api::ApiController {
    public:
        explicit HealthController(const std::shared_ptr<ObjectMapper>& objectMapper)
            : ApiController(objectMapper) {}

        ENDPOINT("GET", "/ping", ping) {
            auto response = createResponse(Status::CODE_200, "pong");
            response->putHeader(Header::CONTENT_TYPE, "text/plain; charset=utf-8");
            response->putHeader("X-Service-Status", "Healthy");
            return response;
        }

        ENDPOINT("GET", "/", root) {
            const auto file = oatpp::String::loadFromFile(PICASSO_STATIC_ROOT "/index.html");

            OATPP_ASSERT_HTTP(file != nullptr, Status::CODE_404, "index.html not found")

            const auto response = createResponse(Status::CODE_200, file);
            response->putHeader(Header::CONTENT_TYPE, "text/html; charset=utf-8");

            return response;
        }

        ENDPOINT("GET", "/docs", docs) {
            const auto file = oatpp::String::loadFromFile(PICASSO_STATIC_ROOT "/docs.html");

            OATPP_ASSERT_HTTP(file != nullptr, Status::CODE_404, "docs.html not found")

            const auto response = createResponse(Status::CODE_200, file);
            response->putHeader(Header::CONTENT_TYPE, "text/html; charset=utf-8");

            return response;
        }

        ENDPOINT("GET", "/static/css/{name}", staticCss, PATH(String, name)) {
            return serveStaticAsset(name, "css", ".css", "text/css; charset=utf-8");
        }

        ENDPOINT("GET", "/static/js/{name}", staticJs, PATH(String, name)) {
            return serveStaticAsset(name, "js", ".js", "text/javascript; charset=utf-8");
        }

    private:
        /*
         * {name} comes straight from the URL. It must never reach a filesystem path
         * unvalidated - reject anything but a plain "word.extension" segment before
         * it is concatenated into PICASSO_STATIC_ROOT below.
         */
        static bool isSafeAssetName(const std::string& fileName, const std::string& extension) {
            if (fileName.size() <= extension.size()) {
                return false;
            }
            if (fileName.compare(fileName.size() - extension.size(), extension.size(), extension) != 0) {
                return false;
            }
            for (const char c : fileName) {
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) {
                    return false;
                }
            }
            return fileName.find("..") == std::string::npos;
        }

        std::shared_ptr<OutgoingResponse> serveStaticAsset(const oatpp::String& name,
                                                            const std::string& subdir,
                                                            const std::string& extension,
                                                            const std::string& contentType) {
            OATPP_ASSERT_HTTP(name != nullptr && isSafeAssetName(*name, extension),
                              Status::CODE_404, "not found")

            const std::string path = std::string(PICASSO_STATIC_ROOT) + "/" + subdir + "/" + *name;
            const auto file = oatpp::String::loadFromFile(path.c_str());

            OATPP_ASSERT_HTTP(file != nullptr, Status::CODE_404, "not found")

            const auto response = createResponse(Status::CODE_200, file);
            response->putHeader(Header::CONTENT_TYPE, contentType.c_str());

            return response;
        }
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
