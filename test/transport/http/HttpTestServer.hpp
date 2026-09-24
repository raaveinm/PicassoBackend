//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#pragma once

#include <memory>
#include <string>
#include <thread>

#include "oatpp/network/Server.hpp"
#include "oatpp/network/virtual_/Interface.hpp"
#include "oatpp/network/virtual_/client/ConnectionProvider.hpp"
#include "oatpp/network/virtual_/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/client/HttpRequestExecutor.hpp"
#include "oatpp/web/protocol/http/incoming/Response.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include "service/Services.hpp"
#include "transport/http/ErrorHandler.hpp"
#include "transport/http/HttpModule.hpp"

namespace picasso::test {

    class HttpTestServer {
    public:
        using Response = oatpp::web::protocol::http::incoming::Response;

        HttpTestServer() { start(); }

        ~HttpTestServer() {
            server_->stop();
            serverProvider_->stop();
            connectionHandler_->stop();
            if (thread_.joinable()) {
                thread_.join();
            }
        }

        HttpTestServer(const HttpTestServer&) = delete;
        HttpTestServer& operator=(const HttpTestServer&) = delete;

        /* Sends one request and returns the parsed response - status, headers, body. */
        [[nodiscard]] std::shared_ptr<Response> request(
            const std::string& method,
            const std::string& path,
            const oatpp::web::protocol::http::Headers& headers = {}
        ) const {
            return executor_->execute(method, path, headers, nullptr, nullptr);
        }

        [[nodiscard]] std::shared_ptr<Response> get(const std::string& path) const { return request("GET", path); }

    protected:
        service::Services services{};

    private:
        void start() {
            objectMapper_ = oatpp::parser::json::mapping::ObjectMapper::createShared();
            router_ = oatpp::web::server::HttpRouter::createShared();
            transport::http::registerControllers(router_, objectMapper_, services);

            connectionHandler_ = oatpp::web::server::HttpConnectionHandler::createShared(router_);
            connectionHandler_->setErrorHandler(
                std::make_shared<transport::http::ErrorHandler>(objectMapper_));

            // Name only has to be unique among live interfaces in this process.
            interface_ = oatpp::network::virtual_::Interface::obtainShared("picasso-http-test");
            serverProvider_ = oatpp::network::virtual_::server::ConnectionProvider::createShared(interface_);
            server_ = std::make_shared<oatpp::network::Server>(serverProvider_, connectionHandler_);
            thread_ = std::thread([server = server_] { server->run(); });

            executor_ = oatpp::web::client::HttpRequestExecutor::createShared(
                oatpp::network::virtual_::client::ConnectionProvider::createShared(interface_));
        }

        std::shared_ptr<oatpp::data::mapping::ObjectMapper> objectMapper_;
        std::shared_ptr<oatpp::web::server::HttpRouter> router_;
        std::shared_ptr<oatpp::web::server::HttpConnectionHandler> connectionHandler_;
        std::shared_ptr<oatpp::network::virtual_::Interface> interface_;
        std::shared_ptr<oatpp::network::virtual_::server::ConnectionProvider> serverProvider_;
        std::shared_ptr<oatpp::network::Server> server_;
        std::shared_ptr<oatpp::web::client::HttpRequestExecutor> executor_;
        std::thread thread_;
    };
} // namespace picasso::test
