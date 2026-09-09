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

# picasso_storage is PUBLIC only for the Repositories struct in the makeServices
# signature. If that ever turns into a real dependency on a Pg* class, the layering
# has been broken - services must stay behind the ports.
target_link_libraries(picasso_service
        PUBLIC picasso_base picasso_domain picasso_steam picasso_storage)
