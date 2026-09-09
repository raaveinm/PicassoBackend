# The core. Links picasso_base and nothing else - no oat++, no driver, no sockets.
# If this target ever grows a dependency, the layering has been broken.
#
# Domain.cpp exists only to give the static library a translation unit; the module
# is otherwise header-only.
add_library(picasso_domain STATIC
        Domain.cpp
        Ids.hpp
        Errors.hpp
        Message.hpp
        Conversation.hpp
        Session.hpp
        ports/ChatRepository.hpp
        ports/ConversationRepository.hpp
        ports/SessionRepository.hpp
        ports/PresenceRegistry.hpp
        ports/SignalTransport.hpp)

target_link_libraries(picasso_domain PUBLIC picasso_base)
