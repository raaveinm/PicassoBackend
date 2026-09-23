# Implements domain ports; nothing above it links this target except the
# composition root in app/, which needs makeRepositories to build them once.
#
# oatpp-postgresql is PRIVATE, not PUBLIC: the driver and its DbClient stop at this
# target. If a consumer above ever needs it on its include path, something has
# reached past the repository interfaces and the layering is broken.

add_library(picasso_storage STATIC
        PicassoDatabaseClient.hpp
        Repositories.cpp
        Repositories.hpp
        Rows.hpp
        PgChatRepository.cpp
        PgChatRepository.hpp
        PgConversationRepository.cpp
        PgConversationRepository.hpp
        PgSessionRepository.cpp
        PgSessionRepository.hpp)

target_link_libraries(picasso_storage
        PUBLIC picasso_base picasso_domain
        PRIVATE oatpp::oatpp-postgresql)

target_compile_definitions(picasso_storage PRIVATE
        DATABASE_MIGRATIONS="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
)