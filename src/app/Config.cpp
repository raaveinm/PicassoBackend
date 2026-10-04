//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "app/Config.hpp"

#include <cctype>
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

    std::string redactDsn(const std::string& dsn) {
        const std::string mask{"***"};
        const std::string passwordKey{"password="};

        // URI form: everything between the first ':' of the authority and the '@' that ends the userinfo.
        const std::size_t schemeEnd = dsn.find("://");
        if (schemeEnd != std::string::npos) {
            const std::size_t authorityStart = schemeEnd + 3;
            const std::size_t slash = dsn.find('/', authorityStart);
            const std::size_t authorityEnd = (slash == std::string::npos) ? dsn.size() : slash;
            const std::size_t at = dsn.rfind('@', authorityEnd);
            const std::size_t colon = dsn.find(':', authorityStart);
            if (at != std::string::npos && at >= authorityStart && colon != std::string::npos && colon < at) {
                return dsn.substr(0, colon + 1) + mask + dsn.substr(at);
            }
            return dsn;
        }

        // Keyword/value form: a "password=" at the start of a token; the value ends at whitespace, or at the
        // closing quote when it is single-quoted (a backslash escapes the next character inside quotes).
        std::string result;
        std::size_t index = 0;
        while (index < dsn.size()) {
            const bool atTokenStart =
                index == 0 || std::isspace(static_cast<unsigned char>(dsn[index - 1])) != 0;
            if (atTokenStart && dsn.compare(index, passwordKey.size(), passwordKey) == 0) {
                result += passwordKey + mask;
                index += passwordKey.size();
                if (index < dsn.size() && dsn[index] == '\'') {
                    ++index;
                    while (index < dsn.size() && dsn[index] != '\'') {
                        if (dsn[index] == '\\' && index + 1 < dsn.size()) {
                            ++index;
                        }
                        ++index;
                    }
                    if (index < dsn.size()) {
                        ++index; // closing quote
                    }
                } else {
                    while (index < dsn.size() && std::isspace(static_cast<unsigned char>(dsn[index])) == 0) {
                        ++index;
                    }
                }
            } else {
                result += dsn[index];
                ++index;
            }
        }

        return result;
    }

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
