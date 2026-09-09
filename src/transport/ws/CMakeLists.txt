# The chat + RTC signaling transport. The only target that links oatpp-websocket.
#
# ConnectionHub is deliberately free of oat++ types even though it lives here: it
# talks to sessions through the Outbound interface, so the presence/fan-out logic
# can be tested without standing up a socket.
add_library(picasso_transport_ws STATIC
        WsModule.cpp
        WsModule.hpp
        ConnectionHub.cpp
        ConnectionHub.hpp
        EnvelopeCodec.cpp
        EnvelopeCodec.hpp
        WsSession.cpp
        WsSession.hpp
        WsController.hpp
        Outbound.hpp)

target_link_libraries(picasso_transport_ws
        PUBLIC picasso_base picasso_dto picasso_service
        oatpp::oatpp oatpp::oatpp-websocket)
