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
                                        const domain::SteamId to,
                                        const std::string& frame) {
        /*
         * TODO(roadmap step 4/5): reject unless BOTH `from` and `to` are members of
         * conversationId, once ConversationRepository is real. Until storage lands,
         * this is an open relay by steamId - the membership check the class-level
         * comment above promises does not exist yet.
         */
        transport_->sendTo(to, frame);
    }
} // namespace picasso::service
