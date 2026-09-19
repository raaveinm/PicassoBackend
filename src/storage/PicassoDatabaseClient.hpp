//
// Created by raaveinm on 9/19/26.
//

#ifndef PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP
#define PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP

#include "oatpp/orm/SchemaMigration.hpp"
#include "oatpp/orm/DbClient.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DbClient)

namespace picasso::storage {
    const std::string DATABASE_NAME = "picasso_database";
    const std::string TAG = "database_client";

    class PicassoDatabaseClient : public oatpp::orm::DbClient {
    public:
        explicit PicassoDatabaseClient(const std::shared_ptr<oatpp::orm::Executor>& executor)
            : DbClient(executor)
        {
            oatpp::orm::SchemaMigration migration(executor, DATABASE_NAME);
            migration.addFile(/*ver*/ 1, /*filename*/DATABASE_MIGRATIONS "/0001_init.sql" );
            migration.migrate();

            OATPP_LOGD(TAG, "Database client initialized, schema version: %ld", executor->getSchemaVersion(DATABASE_NAME));
        }

        QUERY(createUser,
            "INSERT INTO users VALUES (:steam_id);",
            PARAM(oatpp::Int64, steam_id))

    };

#include OATPP_CODEGEN_END(DbClient)
}

#endif //PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP
