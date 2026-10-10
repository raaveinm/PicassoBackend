//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/ChatService.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "../../../../../.conan2/p/b/oatpp40d967da452fd/p/include/oatpp-1.3.0/oatpp/oatpp/core/base/Environment.hpp"
#include "domain/Clock.hpp"

namespace picasso::service {
    namespace {
        bool isBlank(const std::string& text) {
            return std::ranges::all_of(text, [](const char c) {
                return std::isspace(static_cast<unsigned char>(c)) != 0;
            });
        }

        int clampLimit(const int requested) {
            if (requested <= 0) {
                return domain::DEFAULT_PAGE_SIZE;
            }
            return std::min(requested, domain::MAX_PAGE_SIZE);
        }
    } // namespace

    ChatService::ChatService(
        std::shared_ptr<domain::ChatRepository> chat,
        std::shared_ptr<domain::ConversationRepository> conversations,
        std::shared_ptr<domain::ContactRepository> contacts,
        std::shared_ptr<domain::EventSink> events,
        Clock clock
    )
        : chat_(std::move(chat)),
          conversations_(std::move(conversations)),
          contacts_(std::move(contacts)),
          events_(std::move(events)),
          clock_(clock ? std::move(clock) : Clock(&domain::nowEpochMs)) {
    }

    std::mutex& ChatService::lockFor(const domain::ConversationId& conversation_id) {
        const auto stripe = static_cast<std::size_t>(conversation_id.value()) % LOCK_STRIPES;
        return locks_[stripe];
    }

    bool ChatService::takeToken(const domain::SteamId sender, const std::int64_t now_epoch_ms) {
        const std::lock_guard lock(rate_mutex_);

        auto [entry, is_new] = buckets_.try_emplace(sender, Bucket{domain::CHAT_RATE_BURST, now_epoch_ms});
        auto& [tokens, refilled_at_epoch_ms] = entry->second;
        if (!is_new) {
            const double minutes = static_cast<double>(now_epoch_ms - refilled_at_epoch_ms) / 60'000.0;
            tokens = std::min(domain::CHAT_RATE_BURST, tokens + minutes * domain::CHAT_RATE_PER_MINUTE);
            refilled_at_epoch_ms = now_epoch_ms;
        }

        if (tokens < 1.0) {
            return false;
        }
        tokens -= 1.0;
        return true;
    }

    ///////////////////////////////////////////////
    /// Sending
    ///////////////////////////////////////////////

    ChatService::SubmitResult ChatService::submit(
        const domain::SteamId sender,
        const domain::ConversationId& conversation_id,
        const std::string& client_message_id,
        const std::string& body,
        const domain::ConnectionId origin
    ) {
        const auto rejected = [](const RejectCode code) {
            return SubmitResult{.status = SubmitStatus::Rejected, .message = std::nullopt, .code = code};
        };

        // Cheap checks first, before the database or the lock is touched.
        if (client_message_id.empty() || client_message_id.size() > domain::MAX_CLIENT_MESSAGE_ID_BYTES ||
            isBlank(body)) {
            return rejected(RejectCode::Invalid);
        }
        if (body.size() > domain::MAX_BODY_BYTES) {
            return rejected(RejectCode::TooLong);
        }
        const std::int64_t now = clock_();
        if (!takeToken(sender, now)) {
            return rejected(RejectCode::RateLimited);
        }

        try {
            const std::lock_guard lock(lockFor(conversation_id));

            const auto conversation = conversations_->get(conversation_id);
            if (!conversation ||
                std::ranges::find(conversation->members, sender) ==
                    conversation->members.end()) {
                return rejected(RejectCode::NotMember);
            }

            // A dm is open only while the two are allies
            if (conversation->kind == domain::ConversationKind::Dm) {
                const auto peer = std::ranges::find_if(conversation->members,
                    [sender](const domain::SteamId member) { return member != sender; });
                if (peer == conversation->members.end() || !contacts_->canCommunicate(sender, *peer)) {
                    return rejected(RejectCode::NotAllowed);
                }
            }

            const auto [message, duplicate] = chat_->append(conversation_id, sender, client_message_id, body, now);
            if (duplicate) {
                // A retry of a message already stored and already delivered: ack it, tell nobody.
                return SubmitResult{
                    .status = SubmitStatus::Duplicate,
                    .message = message,
                    .code = RejectCode::Internal};
            }

            events_->publish(domain::ChatDelivered{
                .to = conversation->members,
                .message = message,
                .except = origin,
            });
            return SubmitResult{
                .status = SubmitStatus::Stored,
                .message = message,
                .code = RejectCode::Internal};
        } catch (const std::exception& e) {
            OATPP_LOGW(CHAT_SERVICE_TAG, "[ChatService::submit]", e.what());
            return rejected(RejectCode::Internal);
        }
    }

    ///////////////////////////////////////////////
    /// Catching up
    ///////////////////////////////////////////////

    ChatService::SyncResult ChatService::sync(
        const domain::SteamId me,
        const std::vector<SyncCursor>& cursors,
        const std::optional<std::int64_t> deleted_since,
        const int limit_requested
    ) {
        const int limit = clampLimit(limit_requested);
        SyncResult result;
        result.server_time = clock_();
        result.deleted_cursor = std::max<std::int64_t>(0, result.server_time - domain::DELETED_CURSOR_OVERLAP_MS);

        auto ids = conversations_->conversationsOf(me);
        std::ranges::sort(ids);

        for (const auto& id : ids) {
            auto conversation = conversations_->get(id);
            if (!conversation) { continue; }

            SyncedConversation synced;
            synced.writable = true;
            if (conversation->kind == domain::ConversationKind::Dm) {
                const auto peer = std::ranges::find_if(conversation->members,
                    [me](const domain::SteamId member) { return member != me; });
                synced.writable = peer != conversation->members.end() && contacts_->canCommunicate(me, *peer);
            }
            synced.conversation = std::move(*conversation);

            const auto cursor = std::find_if(
                cursors.begin(),
                cursors.end(),
                [&id](const SyncCursor& candidate) { return candidate.conversation_id == id; });
            const bool has_cursor = cursor != cursors.end();

            auto rows = chat_->newest(id, has_cursor ? cursor->after : domain::MessageId(0), limit + 1);
            const bool more_than_a_page = static_cast<int>(rows.size()) > limit;
            if (more_than_a_page) {
                rows.erase(rows.begin(), rows.end() - limit);       // keep the newest `limit`
            }

            synced.reset = !has_cursor || more_than_a_page;
            synced.has_more_before = synced.reset && more_than_a_page;
            synced.messages = std::move(rows);
            result.conversations.push_back(std::move(synced));
        }

        if (deleted_since) {
            result.deleted = chat_->deletedSince(me, *deleted_since);
        }
        return result;
    }

    ChatService::HistoryPage ChatService::history(
        const domain::SteamId me,
        const domain::ConversationId& conversation_id,
        const domain::MessageId before,
        const int limit_requested
    ) {
        HistoryPage page;
        if (!conversations_->isMember(conversation_id, me)) {
            return page;
        }
        page.member = true;

        const int limit = clampLimit(limit_requested);
        auto rows = chat_->olderThan(conversation_id, before, limit + 1);
        if (static_cast<int>(rows.size()) > limit) {
            rows.erase(rows.begin(), rows.end() - limit);
            page.has_more_before = true;
        }
        page.messages = std::move(rows);
        return page;
    }

    ///////////////////////////////////////////////
    /// Deleting
    ///////////////////////////////////////////////

    ChatService::RemoveOutcome ChatService::remove(
        const domain::SteamId me,
        const domain::ConversationId& conversation_id,
        const domain::MessageId message_id
    ) {
        // Same lock as a send, so a delete frame can never overtake the message frame it deletes.
        const std::lock_guard lock(lockFor(conversation_id));

        const auto outcome = chat_->softDelete(conversation_id, message_id, me, clock_());
        if (outcome == domain::ChatRepository::DeleteOutcome::NotFound) {
            return RemoveOutcome::NotFound;
        }
        if (outcome == domain::ChatRepository::DeleteOutcome::Deleted) {
            events_->publish(domain::MessageRemoved{
                .to = conversations_->members(conversation_id),
                .conversation_id = conversation_id,
                .message_id = message_id,
            });
        }
        return RemoveOutcome::Removed;
    }
} // namespace picasso::service
