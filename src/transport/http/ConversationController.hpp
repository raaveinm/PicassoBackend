//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "domain/Errors.hpp"
#include "dto/Rest.hpp"
#include "service/Services.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace picasso::transport::http {
    /*
     * The cache-sync side of being SSOT. The WS connection carries live traffic;
     * this is how a client that was offline catches up.
     *
     * Answers 501 until storage lands (roadmap step 4).
     */
    class ConversationController : public oatpp::web::server::api::ApiController {
    public:
        ConversationController(const std::shared_ptr<ObjectMapper>& objectMapper,
                               service::Services services)
            : ApiController(objectMapper), services_(std::move(services)) {}

        /*
         * `after` is the last message id the client already has; the response's
         * nextAfter is what it should pass next time. Absent `after` means "from the
         * beginning of what this server holds".
         */
        ENDPOINT("GET", "/conversations/{conversationId}/messages", messages,
                 PATH(String, conversationId),
                 QUERY(String, after, "after", ""),
                 QUERY(Int32, limit, "limit", 100)) {
            (void) conversationId;
            (void) after;
            (void) limit;

            notImplemented("transport: conversation history", "4: storage");
        }

    private:
        service::Services services_;
    };
} // namespace picasso::transport::http

#include OATPP_CODEGEN_END(ApiController)
