# The composition root, and the only module allowed to know about all the others
add_library(picasso_app STATIC
        App.cpp
        App.hpp
        Config.cpp
        Config.hpp
        Components.hpp
        ServerRunner.cpp
        ServerRunner.hpp)

target_link_libraries(picasso_app
        PUBLIC picasso_base
        picasso_transport_http
        picasso_transport_ws
        picasso_service
        picasso_storage
        picasso_steam
        oatpp::oatpp)
