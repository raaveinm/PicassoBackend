//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "app/Config.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace picasso::app {
    namespace {
        std::string envOr(const char* name, const std::string& fallback) {
            const char* value = std::getenv(name);
            return (value != nullptr && *value != '\0') ? std::string(value) : fallback;
        }

        std::uint16_t envPortOr(const char* name, const std::uint16_t fallback) {
            const char* value = std::getenv(name);
            if (value == nullptr || *value == '\0') {
                return fallback;
            }

            const auto parsed = std::stoul(value);
            if (parsed == 0 || parsed > 65535) {
                throw std::invalid_argument(std::string(name) + " must be in 1..65535, got " + value);
            }

            return static_cast<std::uint16_t>(parsed);
        }
    } // namespace

    Config loadConfigFromEnvironment() {
        Config config;

        config.bindAddress = envOr("PICASSO_BIND", config.bindAddress);
        config.port = envPortOr("PICASSO_PORT", config.port);
        config.databaseDsn = envOr("PICASSO_DB_DSN", config.databaseDsn);
        config.publicUrl = envOr("PICASSO_PUBLIC_URL", config.publicUrl);
        config.logLevel = envOr("PICASSO_LOG_LEVEL", config.logLevel);

        return config;
    }
} // namespace picasso::app
