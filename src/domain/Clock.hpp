//
// Created by Kirill "Raaveinm" on 9/22/26.
//

#ifndef PICKUSALLBACKEND_CLOCK_HPP
#define PICKUSALLBACKEND_CLOCK_HPP

#pragma once

#include <chrono>

namespace picasso::domain {
    inline std::int64_t nowEpochMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
} // namespace picasso::domain

#endif //PICKUSALLBACKEND_CLOCK_HPP
