# REST surface: health, Steam auth, conversation history.
add_library(picasso_transport_http STATIC
        HttpModule.cpp
        HttpModule.hpp
        ErrorHandler.cpp
        ErrorHandler.hpp
        HealthController.hpp
        AuthController.hpp
        ConversationController.hpp)

target_link_libraries(picasso_transport_http
        PUBLIC picasso_base picasso_dto picasso_service oatpp::oatpp)

# HealthController serves static/index.html through this path. Compile-time, which
# will not survive containerisation - see CLAUDE.md "Known WIP / gaps".
target_compile_definitions(picasso_transport_http
        PRIVATE PICASSO_STATIC_ROOT="${CMAKE_SOURCE_DIR}/static")
