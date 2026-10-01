//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "steam/OpenIdVerifier.hpp"

#include <algorithm>
#include <sstream>
#include <string>

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

    std::string OpenIdVerifier::buildAuthUrl(const std::string& state) const {

        const std::string returnTo = state.empty()
            ? publicUrl_ + "/auth/steam/return"
            : publicUrl_ + "/auth/steam/return?state=" + percentEncode(state);

        std::ostringstream url;
        url << "https://" << kSteamHost << kSteamLoginPath << "?"
            << "openid.ns=" << percentEncode(R"(http://specs.openid.net/auth/2.0)")
            << "&openid.mode=checkid_setup"
            << "&openid.return_to=" << percentEncode(returnTo)
            << "&openid.realm=" << percentEncode(publicUrl_)
            << "&openid.identity=" << percentEncode("http://specs.openid.net/auth/2.0/identifier_select")
            << "&openid.claimed_id=" << percentEncode("http://specs.openid.net/auth/2.0/identifier_select");

        return url.str();
    }

    std::optional<domain::SteamId> OpenIdVerifier::verify(const std::map<std::string, std::string>& params) {
        if (const auto modeIt = params.find("openid.mode");
            modeIt != params.end() && modeIt->second == "error") {
            const auto errIt = params.find("openid.error");
            OATPP_LOGE(TAG_OPENID, "steam refused the auth request: %s",
                       errIt != params.end() ? errIt->second.c_str() : "<no openid.error>");
            return std::nullopt;
        }

        std::map<std::string, std::string> verifyParams;
        for (const auto& [key, value] : params) {
            if (key.starts_with("openid.")) { verifyParams.emplace(key, value); }
        }
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
        try { response = executor->execute("POST", kSteamLoginPath, headers, body, nullptr);
        } catch (const std::exception& e) {
            OATPP_LOGE(TAG_OPENID, "check_authentication POST failed: %s", e.what());
            return std::nullopt;
        }

        if (response->getStatusCode() != 200) {
            OATPP_LOGE(TAG_OPENID, "External verification failed: HTTP %d", response->getStatusCode());
            return std::nullopt;
        }

        const auto text = response->readBodyToString();
        if (!text || text->find("is_valid:true") == std::string::npos) {
            std::string flat = text ? std::string{text->c_str()} : std::string{"<empty body>"};
            std::replace(flat.begin(), flat.end(), '\n', ' ');
            OATPP_LOGW(TAG_OPENID, "validation failed: steam said '%s'", flat.c_str());
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