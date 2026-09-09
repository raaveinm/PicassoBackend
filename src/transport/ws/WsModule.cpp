//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/ws/WsModule.hpp"

#include <string>
#include <unordered_map>

#include "oatpp/core/base/Environment.hpp"

#include "transport/ws/EnvelopeCodec.hpp"
#include "transport/ws/WsController.hpp"
#include "transport/ws/WsSession.hpp"

namespace picasso::transport::ws {
    namespace {
        /*
         * Bridges oat++'s socket lifecycle to ours: builds a session when a socket
         * appears, and unregisters it from the hub when it goes away.
         */
        class SessionFactory final : public oatpp::websocket::ConnectionHandler::SocketInstanceListener {
        public:
            SessionFactory(std::shared_ptr<ConnectionHub> hub,
                           std::shared_ptr<EnvelopeCodec> codec,
                           service::Services services)
                : hub_(std::move(hub)), codec_(std::move(codec)), services_(std::move(services)) {}

            void onAfterCreate(const WebSocket& socket,
                               const std::shared_ptr<const ParameterMap>& params) override {
                const auto entry = params->find(kSteamIdParameter);
                if (entry == params->end() || !entry->second) {
                    /*
                     * Only reachable if someone routes a socket here without going
                     * through WsController. Refuse rather than invent an identity.
                     */
                    OATPP_LOGE("WsModule", "upgraded socket carries no steamId - closing");
                    socket.sendClose();
                    return;
                }

                const auto steamId = domain::SteamId(std::stoull(*entry->second));
                auto session = std::make_shared<WsSession>(steamId, &socket, codec_, services_);

                {
                    const std::lock_guard lock(mutex_);
                    sessions_[&socket] = session;
                }

                hub_->add(steamId, session);
                socket.setListener(session);

                OATPP_LOGI("WsModule", "session opened for %lu", steamId.value());
            }

            void onBeforeDestroy(const WebSocket& socket) override {
                std::shared_ptr<WsSession> session;

                {
                    const std::lock_guard lock(mutex_);
                    const auto entry = sessions_.find(&socket);
                    if (entry == sessions_.end()) {
                        return;
                    }
                    session = entry->second;
                    sessions_.erase(entry);
                }

                hub_->remove(session->steamId(), session.get());
                OATPP_LOGI("WsModule", "session closed for %lu", session->steamId().value());
            }

        private:
            std::shared_ptr<ConnectionHub> hub_;
            std::shared_ptr<EnvelopeCodec> codec_;
            service::Services services_;

            std::mutex mutex_;
            /* Keeps each session alive between onAfterCreate and onBeforeDestroy. */
            std::unordered_map<const WebSocket*, std::shared_ptr<WsSession>> sessions_;
        };
    } // namespace

    WsEndpoint registerWsEndpoint(const std::shared_ptr<oatpp::web::server::HttpRouter>& router,
                                  const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper,
                                  const std::shared_ptr<ConnectionHub>& hub,
                                  const service::Services& services) {
        auto connectionHandler = oatpp::websocket::ConnectionHandler::createShared();

        auto factory = std::make_shared<SessionFactory>(hub,
                                                        std::make_shared<EnvelopeCodec>(objectMapper),
                                                        services);
        connectionHandler->setSocketInstanceListener(factory);

        router->addController(std::make_shared<WsController>(objectMapper, connectionHandler, services.auth));

        return WsEndpoint{connectionHandler, factory};
    }
} // namespace picasso::transport::ws
