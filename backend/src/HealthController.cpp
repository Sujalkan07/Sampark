#include "HealthController.hpp"
#include <nlohmann/json.hpp>

namespace connectx {

using json = nlohmann::json;

void HealthController::handleHealth(const httplib::Request&, httplib::Response& res) {
    json responseBody = {
        {"status", "ok"},
        {"service", "sampark-backend"},
        {"timestamp", std::time(nullptr)}
    };

    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

} // namespace connectx
