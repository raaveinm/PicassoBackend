//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/CallSignalService.hpp"

#include <utility>

#include "domain/Errors.hpp"

namespace picasso::service {
    CallSignalService::CallSignalService(std::shared_ptr<domain::ConversationRepository> conversations,
                                         std::shared_ptr<domain::PresenceRegistry> presence,
                                         std::shared_ptr<domain::SignalTransport> transport)
        : conversations_(std::move(conversations)),
          presence_(std::move(presence)),
          transport_(std::move(transport)) {}

    void CallSignalService::invite(const domain::SteamId, const domain::ConversationId&) {
        notImplemented("service: CallSignalService::invite", "5: rtc signaling");
    }

    void CallSignalService::relayToPeer(const domain::SteamId,
                                        const domain::ConversationId&,
                                        const domain::SteamId,
                                        const std::string&) {
        /*
         * Shape once implemented (roadmap step 5): reject unless BOTH `from` and
         * `to` are members of conversationId, then transport_->sendTo(to, frame).
         * The membership answer comes off the session's cached set, not a query -
         * ICE candidates arrive in bursts.
         */
        notImplemented("service: CallSignalService::relayToPeer", "5: rtc signaling");
    }
} // namespace picasso::service
