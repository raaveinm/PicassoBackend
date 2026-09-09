//
// Created by Kirill "Raaveinm" on 9/9/26.
//

/*
 * Anchor translation unit. domain/ is header-only by design - it holds types and
 * abstract ports, no behaviour - but a CMake static library needs at least one
 * source file. Keeping it empty is the point: anything with a definition worth
 * compiling probably belongs in service/ instead.
 */

#include "domain/Ids.hpp"

namespace picasso::domain {
    namespace {
        /* Compile-time check that the id types really are mutually unassignable. */
        static_assert(!std::is_convertible_v<SteamId, MessageId>);
        static_assert(!std::is_constructible_v<ConversationId, SteamId>);
    } // namespace
} // namespace picasso::domain
