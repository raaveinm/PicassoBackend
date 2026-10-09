//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "oatpp/core/base/Environment.hpp"
#include "oatpp/orm/QueryResult.hpp"

#include "storage/Rows.hpp"

namespace picasso::storage {
    /*
     * Every repository method used to repeat "if (!isSuccess) log + throw". Returning
     * the result lets a call read `require(db_->x(...), TAG, "what")->fetch<...>()`.
     */
    inline const std::shared_ptr<oatpp::orm::QueryResult>& require(
        const std::shared_ptr<oatpp::orm::QueryResult>& result,
        const std::string& tag,
        const std::string& what
    ) {
        if (!result->isSuccess()) {
            const std::string message = what + ": " + *result->getErrorMessage();
            OATPP_LOGE(tag, "%s", message.c_str());
            throw std::runtime_error(message);
        }
        return result;
    }

    /* True when a RETURNING produced at least one row; only for queries that `RETURNING x AS value` a bigint. */
    inline bool returnedRow(const std::shared_ptr<oatpp::orm::QueryResult>& result) {
        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        return !rows->empty();
    }
} // namespace picasso::storage
