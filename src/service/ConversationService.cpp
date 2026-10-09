//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include "service/ConversationService.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "domain/Clock.hpp"
#include "domain/Contact.hpp"

namespace picasso::service {
    namespace {
        std::string trimmed(const std::string& text) {
            const auto is_space = [](const char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
            std::size_t start = 0;
            while (start < text.size() && is_space(text[start])) { ++start; }
            std::size_t stop = text.size();
            while (stop > start && is_space(text[stop - 1])) { --stop; }
            return text.substr(start, stop - start);
        }
    } // namespace

    ConversationService::ConversationService(
        std::shared_ptr<domain::ConversationRepository> conversations,
        std::shared_ptr<domain::ContactRepository> contacts,
        std::shared_ptr<domain::EventSink> events)
        : conversations_(std::move(conversations)),
          contacts_(std::move(contacts)),
          events_(std::move(events)) {
    }

    ///////////////////////////////////////////////
    /// Creation
    ///////////////////////////////////////////////

    ConversationService::CreateResult ConversationService::createDm(const domain::SteamId me,
                                                                    const domain::SteamId peer) const {
        if (me == peer) { return {CreateOutcome::Self, std::nullopt}; }

        if (!contacts_->canCommunicate(me, peer)) { return {.outcome = CreateOutcome::NotAllowed, .conversation = std::nullopt}; }

        const auto dm = conversations_->getOrCreateDm(me, peer, domain::nowEpochMs());
        auto conversation = conversations_->get(dm.id);
        if (dm.created && conversation) {
            events_->publish(domain::ConversationAdded{peer, *conversation});
            events_->publish(domain::ConversationAdded{me, *conversation});
        }
        return {dm.created ? CreateOutcome::Created : CreateOutcome::Existing, std::move(conversation)};
    }

    ConversationService::CreateResult ConversationService::createPalette(
        const domain::SteamId me,
        const std::string& name,
        const std::vector<domain::SteamId>& invitees
    ) const {
        const std::string clean_name = trimmed(name);
        if (clean_name.empty() || clean_name.size() > domain::MAX_PALETTE_NAME_BYTES) {
            return {CreateOutcome::InvalidName, std::nullopt};
        }

        std::vector<domain::SteamId> unique_invitees;
        for (const auto& invitee : invitees) {
            if (invitee == me) continue;
            if (std::ranges::find(unique_invitees, invitee) == unique_invitees.end()) {
                unique_invitees.push_back(invitee);
            }
        }
        if (unique_invitees.size() > domain::MAX_INVITEES_PER_CREATE) {
            return {CreateOutcome::TooManyInvitees, std::nullopt};
        }

        // All-or-nothing: validate everyone before the first row is written.
        for (const auto& invitee : unique_invitees) {
            if (!contacts_->canCommunicate(me, invitee)) { return {CreateOutcome::NotAllowed, std::nullopt}; }
        }

        const std::int64_t now = domain::nowEpochMs();
        const auto palette_id = conversations_->createPalette(clean_name, me, now);
        for (const auto& invitee : unique_invitees) {
            const auto outcome = conversations_->invite(palette_id, me, invitee, now);
            if (outcome == domain::ConversationRepository::InviteOutcome::Created) {
                events_->publish(domain::PaletteInviteChanged{
                    .to = invitee,
                    .invite = {.conversation_id = palette_id, .name = clean_name, .inviter = me, .created_at_epoch_ms = now},
                    .pending = true,
                });
            }
        }
        auto conversation = conversations_->get(palette_id);
        if (conversation) { events_->publish(domain::ConversationAdded{me, *conversation}); }
        return {CreateOutcome::Created, std::move(conversation)};
    }

    ///////////////////////////////////////////////
    /// Invitations
    ///////////////////////////////////////////////

    ConversationService::InviteResult ConversationService::invite(
        const domain::SteamId me,
        const domain::ConversationId& palette,
        const domain::SteamId invitee) const {

        const auto conversation = conversations_->get(palette);
        if (!conversation || conversation->kind != domain::ConversationKind::Palette ||
            !conversations_->isMember(palette, me)) {
            return {InviteOutcome::NotAMember, std::nullopt};
        }
        if (me == invitee) { return {InviteOutcome::Self, std::nullopt}; }
        if (!contacts_->canCommunicate(me, invitee)) { return {InviteOutcome::NotAllowed, std::nullopt}; }

        const auto outcome = conversations_->invite(palette, me, invitee, domain::nowEpochMs());
        auto updated = conversations_->get(palette);

        if (outcome == domain::ConversationRepository::InviteOutcome::CooldownActive && updated) {
            updated->invited.push_back(invitee);
            std::ranges::sort(updated->invited);
        }

        const bool created = outcome == domain::ConversationRepository::InviteOutcome::Created;
        if (created && updated) {
            events_->publish(domain::PaletteInviteChanged{
                .to = invitee,
                .invite = {.conversation_id = palette,
                    .name = updated->name.value_or(std::string()),
                    .inviter = me,
                    .created_at_epoch_ms = domain::nowEpochMs()},
                .pending = true,
            });
        }
        return {created ? InviteOutcome::Invited : InviteOutcome::Unchanged, std::move(updated)};
    }

    std::optional<domain::Conversation> ConversationService::acceptInvite(
        const domain::SteamId me,
        const domain::ConversationId& palette
    ) const {
        if (!conversations_->acceptInvite(palette, me, domain::nowEpochMs())) { return std::nullopt; }
        auto joined = conversations_->get(palette);
        if (!joined) { return std::nullopt; }

        // The joiner's other devices: drop the invitation, add the palette.
        events_->publish(domain::PaletteInviteChanged{
            .to = me,
            .invite = {.conversation_id = palette, .name = joined->name.value_or(std::string()), .inviter = {}, .created_at_epoch_ms = 0},
            .pending = false,
        });
        events_->publish(domain::ConversationAdded{me, *joined});
        for (const auto& member : joined->members) {
            if (member != me) { events_->publish(domain::ConversationUpdated{member, *joined}); }
        }
        return joined;
    }

    void ConversationService::declineInvite(const domain::SteamId me, const domain::ConversationId& palette) const {
        if (!conversations_->declineInvite(palette, me, domain::nowEpochMs())) { return; }
        // Only the decliner's other devices hear about it; the inviter is deliberately not told.
        events_->publish(domain::PaletteInviteChanged{
            .to = me,
            .invite = {.conversation_id = palette, .name = {}, .inviter = {}, .created_at_epoch_ms = 0},
            .pending = false,
        });
    }
} // namespace picasso::service
