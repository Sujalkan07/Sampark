#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <httplib.h>

namespace connectx {

inline std::string getClientIp(const httplib::Request& req) {
    std::string xff = req.get_header_value("X-Forwarded-For");
    if (!xff.empty()) {
        size_t comma = xff.find(',');
        if (comma != std::string::npos) {
            return xff.substr(0, comma);
        }
        return xff;
    }
    std::string xRealIp = req.get_header_value("X-Real-IP");
    if (!xRealIp.empty()) return xRealIp;
    if (!req.remote_addr.empty()) return req.remote_addr;
    return "127.0.0.1";
}

class RateLimiter {
public:
    static RateLimiter& getInstance() {
        static RateLimiter instance;
        return instance;
    }

    bool allowRequest(const std::string& key, int maxRequests, int windowSeconds) {
        if (maxRequests <= 0 || windowSeconds <= 0) return true;

        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto windowDuration = std::chrono::seconds(windowSeconds);

        // Periodically purge stale entries to ensure memory remains strictly bounded
        if (history_.size() > 5000) {
            cleanup(now);
        }

        auto& timestamps = history_[key];
        while (!timestamps.empty() && (now - timestamps.front()) > windowDuration) {
            timestamps.erase(timestamps.begin());
        }

        if (static_cast<int>(timestamps.size()) >= maxRequests) {
            return false;
        }

        timestamps.push_back(now);
        return true;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
    }

private:
    RateLimiter() = default;

    void cleanup(std::chrono::steady_clock::time_point now) {
        for (auto it = history_.begin(); it != history_.end();) {
            auto& timestamps = it->second;
            while (!timestamps.empty() && (now - timestamps.front()) > std::chrono::seconds(300)) {
                timestamps.erase(timestamps.begin());
            }
            if (timestamps.empty()) {
                it = history_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> history_;
};

} // namespace connectx
