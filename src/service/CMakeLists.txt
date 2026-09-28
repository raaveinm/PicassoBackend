# Use cases. Links domain (for the ports) and steam - deliberately NOT oat++ and
# NOT storage: the services are handed port implementations, they never construct
# them and never see a DbClient or a socket.
add_library(picasso_service STATIC
        Services.cpp
        Services.hpp
        ChatService.cpp
        ChatService.hpp
        CallSignalService.cpp
        CallSignalService.hpp
        AuthService.cpp
        AuthService.hpp)

target_link_libraries(picasso_service
        PUBLIC picasso_base picasso_domain picasso_steam picasso_storage
        PRIVATE OpenSSL::Crypto)
