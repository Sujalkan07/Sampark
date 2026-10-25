#pragma once

#include "AuthService.hpp"
#include <httplib.h>
#include <memory>

namespace connectx {

class AuthController {
public:
    explicit AuthController(std::shared_ptr<IAuthService> authService);

    void handleRegister(const httplib::Request& req, httplib::Response& res);
    void handleLogin(const httplib::Request& req, httplib::Response& res);
    void handleLogout(const httplib::Request& req, httplib::Response& res);
    void handleMe(const httplib::Request& req, httplib::Response& res);
    void handleSearchUsers(const httplib::Request& req, httplib::Response& res);
    void handleUpdateProfile(const httplib::Request& req, httplib::Response& res);
    void handleChangePassword(const httplib::Request& req, httplib::Response& res);
    void handleGetUserProfile(const httplib::Request& req, httplib::Response& res);

    std::string extractSessionToken(const httplib::Request& req);

private:
    std::shared_ptr<IAuthService> authService_;

    void setSessionCookie(httplib::Response& res, const std::string& token);
    void clearSessionCookie(httplib::Response& res);
    void sendError(httplib::Response& res, int statusCode, const std::string& message);
};

} // namespace connectx
