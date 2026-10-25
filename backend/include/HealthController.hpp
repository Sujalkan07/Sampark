#pragma once

#include <httplib.h>

namespace connectx {

class HealthController {
public:
    static void handleHealth(const httplib::Request& req, httplib::Response& res);
};

} // namespace connectx
