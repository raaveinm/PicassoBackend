//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace picasso {
    /*
     * Marker prefix on the exception message. oat++ 1.3.0's HttpProcessor catches
     * std::exception itself and hands the error handler a ready-made 500 - the
     * exception type never reaches us, only what(). A prefix is therefore the only
     * channel left for telling "planned, not built" apart from "broke", and
     * transport/http/ErrorHandler turns it into a 501.
     *
     * (The 1.3.0.latest branch has a handleError(std::exception_ptr) overload that
     * would make this unnecessary, but oatpp-postgresql pins plain 1.3.0 - see
     * conanfile.txt.)
     */
    inline constexpr std::string_view kNotImplementedPrefix = "not-implemented: ";

    /*
     * Used by every stub below transport/. Throwing beats returning an empty value:
     * an empty history would read as "no messages" and quietly corrupt the client's
     * cache, and a nullopt from authenticate() would look like a working auth check
     * that denies everyone.
     */
    [[noreturn]] inline void notImplemented(const std::string& what, const std::string& roadmapStep) {
        throw std::logic_error(std::string(kNotImplementedPrefix) + what +
                               " is not implemented yet (roadmap step " + roadmapStep + ")");
    }
} // namespace picasso
