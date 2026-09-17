//
// Created by raaveinm on 9/17/26.
//

#include "logger/ActivityLogger.hpp"

#include <cstdio>
#include <ctime>
#include <string_view>

namespace picasso::logger {
    namespace {
        constexpr std::string_view kFilePrefix = "log_file";
        constexpr std::string_view kFileSuffix = ".csv";
        constexpr std::string_view kCsvHeader = "timestamp,level,component,message\n";
    } // namespace

    ActivityLogger::ActivityLogger(std::filesystem::path directory, const int retentionDays)
        : directory_(std::move(directory)), retentionDays_(retentionDays) {
        std::filesystem::create_directories(directory_);
    }

    std::chrono::year_month_day ActivityLogger::todayLocal() {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        return std::chrono::year_month_day{
            std::chrono::year{local.tm_year + 1900},
            std::chrono::month{static_cast<unsigned>(local.tm_mon + 1)},
            std::chrono::day{static_cast<unsigned>(local.tm_mday)}};
    }

    std::string ActivityLogger::dateStamp(const std::chrono::year_month_day& day) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02u-%02u-%04d",
                      static_cast<unsigned>(day.month()),
                      static_cast<unsigned>(day.day()),
                      static_cast<int>(day.year()));
        return buf;
    }

    std::string ActivityLogger::timestamp() {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
        return buf;
    }

    std::string ActivityLogger::csvEscape(const std::string& field) {
        if (field.find_first_of(",\"\n") == std::string::npos) {
            return field;
        }
        std::string escaped = "\"";
        for (const char c : field) {
            if (c == '"') {
                escaped += "\"\"";
            } else {
                escaped += c;
            }
        }
        escaped += '\"';
        return escaped;
    }

    std::optional<std::chrono::year_month_day> ActivityLogger::parseDateStamp(const std::string& stamp) {
        unsigned month = 0;
        unsigned day = 0;
        int year = 0;
        if (std::sscanf(stamp.c_str(), "%2u-%2u-%4d", &month, &day, &year) != 3) { // NOLINT(*-err34-c)
            return std::nullopt;
        }
        const std::chrono::year_month_day parsed{
            std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
        if (!parsed.ok()) {
            return std::nullopt;
        }
        return parsed;
    }

    void ActivityLogger::purgeOldFiles(const std::chrono::year_month_day& today) const {
        const auto todaySysDays = std::chrono::sys_days{today};

        for (std::error_code ec; const auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const std::string name = entry.path().filename().string();
            if (!name.starts_with(kFilePrefix) || !name.ends_with(kFileSuffix) ||
                name.size() <= kFilePrefix.size() + kFileSuffix.size()) {
                continue;
            }

            const std::string stamp = name.substr(
                kFilePrefix.size(), name.size() - kFilePrefix.size() - kFileSuffix.size());
            const auto parsed = parseDateStamp(stamp);
            if (!parsed) {
                continue;
            }

            const auto fileSysDays = std::chrono::sys_days{*parsed};
            const auto ageDays = (todaySysDays - fileSysDays).count();
            if (ageDays > retentionDays_) {
                std::filesystem::remove(entry.path(), ec);
            }
        }
    }

    void ActivityLogger::rotateIfNeeded() {
        const auto today = todayLocal();
        const std::string stamp = dateStamp(today);
        if (stamp == currentStamp_ && file_.is_open()) {
            return;
        }

        if (file_.is_open()) {
            file_.close();
        }

        const auto path = directory_ / (std::string(kFilePrefix) + stamp + std::string(kFileSuffix));
        const bool isNewFile = !std::filesystem::exists(path);

        file_.open(path, std::ios::app);
        if (isNewFile) {
            file_ << kCsvHeader;
        }

        currentStamp_ = stamp;
        purgeOldFiles(today);
    }

    void ActivityLogger::log(const std::string& level, const std::string& component, const std::string& message) {
        const std::lock_guard lock(mutex_);
        rotateIfNeeded();

        file_ << csvEscape(timestamp()) << ','
              << csvEscape(level) << ','
              << csvEscape(component) << ','
              << csvEscape(message) << '\n';
        file_.flush();
    }
} // namespace picasso::logger
