//
// Created by Kirill "Raaveinm" on 9/22/26.
//

#ifndef PICKUSALLBACKEND_ROWS_HPP
#define PICKUSALLBACKEND_ROWS_HPP

/**
 *
 * DTO Private fields
 *
 * @ScalarInt64Row used as unified pass for id's like
 * steam_id / chat_id (every object with @Int64 type)
 *
 */

#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

namespace picasso::storage {

    class ScalarInt64Row : public oatpp::DTO {
        DTO_INIT(ScalarInt64Row, DTO)
        DTO_FIELD(Int64, value);
    };

    class MessageRow : public oatpp::DTO {
        DTO_INIT(MessageRow, DTO)
        DTO_FIELD(Int64, id);
        DTO_FIELD(Int64, conversation_id);
        DTO_FIELD(Int64, sender_steam_id);
        DTO_FIELD(String, text_message);
        DTO_FIELD(Int64, sent_at);
    };

    class ExistsRow : public oatpp::DTO {
        DTO_INIT(ExistsRow, DTO)
        DTO_FIELD(Boolean, is_member);
    };

    class SessionRow : public oatpp::DTO {
        DTO_INIT(SessionRow, DTO)
        DTO_FIELD(String, token_hash);
        DTO_FIELD(Int64, steam_id);
        DTO_FIELD(Int64, created_at);
        DTO_FIELD(Int64, expires_at);
        DTO_FIELD(Int64, revoked_at);
    };
}



#endif //PICKUSALLBACKEND_ROWS_HPP
