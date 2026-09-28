//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "steam/OpenIdVerifier.hpp"

#include <sstream>

#include "oatpp/network/tcp/client/ConnectionProvider.hpp"
#include "oatpp/web/client/HttpRequestExecutor.hpp"
#include "oatpp/web/protocol/http/outgoing/BufferBody.hpp"
#include "oatpp-openssl/client/ConnectionProvider.hpp"
#include "oatpp-openssl/Config.hpp"

namespace picasso::steam {
    namespace {
        constexpr auto kSteamHost = "steamcommunity.com";
        constexpr v_uint16 kSteamPort = 443;
        constexpr auto kSteamLoginPath = "/openid/login";

        std::string percentEncode(const std::string& value) {
            static constexpr char kHex[] = "0123456789ABCDEF";
            std::string out;
            out.reserve(value.size());
            for (const unsigned char c : value) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    out.push_back(static_cast<char>(c));
                } else {
                    out.push_back('%');
                    out.push_back(kHex[c >> 4]);
                    out.push_back(kHex[c & 0x0F]);
                }
            }
            return out;
        }

        std::string buildFormBody(const std::map<std::string, std::string>& params) {
            std::ostringstream body;
            bool first = true;
            for (const auto& [key, value] : params) {
                if (!first) { body << '&'; }
                first = false;
                body << percentEncode(key) << '=' << percentEncode(value);
            }
            return body.str();
        }
    } // namespace

    std::string OpenIdVerifier::buildAuthUrl() const {
        const std::string returnTo = publicUrl_ + "/auth/steam/return";

        std::ostringstream url;
        url << "https://" << kSteamHost << kSteamLoginPath << "?"
            << "openid.ns=" << percentEncode(R"(https://specs.openid.net/auth/2.0)")
            << "&openid.mode=checkid_setup"
            << "&openid.return_to=" << percentEncode(returnTo)
            << "&openid.realm=" << percentEncode(publicUrl_)
            << "&openid.identity=" << percentEncode("http://specs.openid.net/auth/2.0/identifier_select")
            << "&openid.claimed_id=" << percentEncode("http://specs.openid.net/auth/2.0/identifier_select");

        return url.str();
    }

    std::optional<domain::SteamId> OpenIdVerifier::verify(const std::map<std::string, std::string>& params) {
        auto verifyParams = params;
        verifyParams["openid.mode"] = "check_authentication";

        const auto sslConfig = oatpp::openssl::Config::createDefaultClientConfigShared();
        const auto tcpProvider = oatpp::network::tcp::client::ConnectionProvider::createShared({kSteamHost, kSteamPort});
        const auto tlsProvider = oatpp::openssl::client::ConnectionProvider::createShared(sslConfig, tcpProvider);
        const auto executor = oatpp::web::client::HttpRequestExecutor::createShared(tlsProvider);

        oatpp::web::client::RequestExecutor::Headers headers;
        headers.put("Host", kSteamHost);

        const auto body = oatpp::web::protocol::http::outgoing::BufferBody::createShared(
            buildFormBody(verifyParams).c_str(),
            "application/x-www-form-urlencoded"
        );

        std::shared_ptr<oatpp::web::protocol::http::incoming::Response> response;
        try { response = executor->executeOnce("POST", kSteamLoginPath, headers, body);
        } catch (const std::exception&) {
            OATPP_LOGW(TAG_OPENID, "Token hasn't been verified");
            return std::nullopt;
        }

        if (response->getStatusCode() != 200) {
            OATPP_LOGE(TAG_OPENID, "External verification failed");
            return std::nullopt;
        }

        const auto text = response->readBodyToString();
        if (!text || text->find("is_valid:true") == std::string::npos) {
            OATPP_LOGW(TAG_OPENID, "validation failed");
            return std::nullopt;
        }

        const auto claimedIdIt = params.find("openid.claimed_id");
        if (claimedIdIt == params.end()) {
            return std::nullopt;
        }

        const auto& claimedId = claimedIdIt->second;
        const auto lastSlash = claimedId.find_last_of('/');
        if (lastSlash == std::string::npos) {
            return std::nullopt;
        }

        try {
            return domain::SteamId(std::stoull(claimedId.substr(lastSlash + 1)));
        } catch (const std::exception&) {
            OATPP_LOGW(TAG_OPENID, "Invalid claimed id");
            return std::nullopt;
        }
    }
} // namespace picasso::steam