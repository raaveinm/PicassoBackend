//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include "service/ContactService.hpp"

#include <utility>

#include "domain/Clock.hpp"

namespace picasso::service {
    ContactService::ContactService(
        std::shared_ptr<domain::ContactRepository> contacts,
        std::shared_ptr<domain::ConversationRepository> conversations,
        std::shared_ptr<domain::EventSink> events)
    : contacts_(std::move(contacts)),
    conversations_(std::move(conversations)),
    events_(std::move(events)) {
    }

    void ContactService::publishPair(const domain::SteamId first, const domain::SteamId second) const {
        events_->publish(domain::ContactChanged{.to = first, .other = second, .level = contacts_->levelOf(first, second)});
        events_->publish(domain::ContactChanged{.to = second, .other = first, .level = contacts_->levelOf(second, first)});
    }

    ///////////////////////////////////////////////
    /// Queries
    ///////////////////////////////////////////////

    domain::ContactsSnapshot ContactService::snapshot(const domain::SteamId me) const {
        auto snapshot = contacts_->snapshot(me);
        snapshot.palette_invites = conversations_->pendingInvitesOf(me);
        return snapshot;
    }

    ///////////////////////////////////////////////
    /// Requests
    ///////////////////////////////////////////////

    ContactService::RequestOutcome ContactService::request(
        const domain::SteamId me,
        const domain::SteamId target
    ) const {
        if (me == target) { return RequestOutcome::Self; }

        const std::int64_t now = domain::nowEpochMs();

        const auto mine = contacts_->levelOf(me, target);
        if (mine == domain::ContactLevel::Imposter) { return RequestOutcome::UnblockFirst; }
        if (contacts_->canCommunicate(me, target)) { return RequestOutcome::AlreadyContacts; }

        if (contacts_->countPendingFrom(me) >= domain::MAX_PENDING_REQUESTS ||
            contacts_->countCreatedSince(me, now - domain::RATE_WINDOW_MS) >= domain::MAX_REQUESTS_PER_DAY) {
            return RequestOutcome::RateLimited;
        }

        contacts_->ensureUser(target);

        if (contacts_->levelOf(target, me) == domain::ContactLevel::Imposter) { return RequestOutcome::Pending; }

        if (const auto theirs = contacts_->requestState(target, me)) {
            const bool live = !theirs->declined_at_epoch_ms.has_value() &&
                now - theirs->created_at_epoch_ms < domain::CONTACT_REQUEST_TTL_MS;
            if (live && contacts_->accept(target, me, now)) {
                publishPair(me, target);
                return RequestOutcome::Accepted;
            }
        }

        if (const auto ours = contacts_->requestState(me, target)) {
            if (ours->declined_at_epoch_ms) {
                if (now - *ours->declined_at_epoch_ms < domain::DECLINE_COOLDOWN_MS) { return RequestOutcome::Pending; }
            } else if (now - ours->created_at_epoch_ms < domain::CONTACT_REQUEST_TTL_MS) {
                return RequestOutcome::Pending;
            }
            // else: an expired request or a decline past its cooldown - re-arm below.
        }

        contacts_->putRequest(me, target, now);
        events_->publish(domain::ContactRequested{.to = target, .from = me, .created_at_epoch_ms = now});
        return RequestOutcome::Pending;
    }

    bool ContactService::accept(const domain::SteamId me, const domain::SteamId from) const {
        if (!contacts_->accept(from, me, domain::nowEpochMs())) { return false; }
        publishPair(me, from);
        return true;
    }

    void ContactService::declineOrWithdraw(const domain::SteamId me, const domain::SteamId other) const {
        contacts_->decline(other, me, domain::nowEpochMs());
        contacts_->withdraw(me, other);
    }

    ///////////////////////////////////////////////
    /// Contact rows
    ///////////////////////////////////////////////

    ContactService::LevelOutcome ContactService::setLevel(const domain::SteamId me,
                                                          const domain::SteamId other,
                                                          const domain::ContactLevel level) const {
        if (me == other) { return LevelOutcome::Self; }

        if (level == domain::ContactLevel::Imposter) {
            contacts_->block(me, other, domain::nowEpochMs());
            // The blocker sees imposter; the blocked person sees their row vanish, as for a removal.
            events_->publish(domain::ContactChanged{me, other, domain::ContactLevel::Imposter});
            events_->publish(domain::ContactChanged{other, me, std::nullopt});
            return LevelOutcome::Ok;
        }

        if (!contacts_->setTier(me, other, level)) { return LevelOutcome::NotAContact; }
        // A tier is what the OWNER grants, so only the owner's other devices need to hear about it.
        events_->publish(domain::ContactChanged{me, other, level});
        return LevelOutcome::Ok;
    }

    void ContactService::removeOrUnblock(const domain::SteamId me, const domain::SteamId other) const {
        const auto level = contacts_->levelOf(me, other);
        if (!level) { return; }

        if (*level == domain::ContactLevel::Imposter) {
            if (contacts_->unblock(me, other)) {
                events_->publish(domain::ContactChanged{me, other, std::nullopt});
            }
        } else if (contacts_->remove(me, other)) {
            events_->publish(domain::ContactChanged{me, other, std::nullopt});
            events_->publish(domain::ContactChanged{other, me, std::nullopt});
        }
    }
} // namespace picasso::service
