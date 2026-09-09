//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include "app/Components.hpp"
#include "app/Config.hpp"

namespace picasso::app {
    /*
     * Binds and serves. Returns a process exit code rather than letting a bind
     * failure escape as an uncaught exception - the old behaviour was a std::abort
     * with a stack trace, which tells a self-hosting user nothing actionable.
     */
    int serve(Components& components);
} // namespace picasso::app
