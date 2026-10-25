#include "AuthController.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

namespace connectx {

using json = nlohmann::json;

AuthController::AuthController(std::shared_ptr<IAuthService> authService)
    : authService_(std::move(authService)) {}

std::string AuthController::extractSessionToken(const httplib::Request& req) {
    // 1. Check Cookie header
    std::string cookieHeader = req.get_header_value("Cookie");
    if (!cookieHeader.empty()) {
        std::istringstream stream(cookieHeader);
        std::string part;
        while (std::getline(stream, part, ';')) {
            // Trim leading spaces
            size_t start = part.find_first_not_of(" \t");
            if (start != std::string::npos) {
                part = part.substr(start);
            }
            const std::string cookiePrefix = "connectx_session=";
            if (part.rfind(cookiePrefix, 0) == 0) {
                return part.substr(cookiePrefix.length());
            }
        }
    }

    // 2. Fallback to Authorization: Bearer <token>
    std::string authHeader = req.get_header_value("Authorization");
    if (!authHeader.empty()) {
        const std::string bearerPrefix = "Bearer ";
        if (authHeader.rfind(bearerPrefix, 0) == 0) {
            return authHeader.substr(bearerPrefix.length());
        }
    }

    return "";
}

void AuthController::setSessionCookie(httplib::Response& res, const std::string& token) {
    // 7 days expiration = 604800 seconds
    std::string cookieVal = "connectx_session=" + token +
                            "; Path=/; Max-Age=604800; HttpOnly; SameSite=Lax";
    res.set_header("Set-Cookie", cookieVal);
}

void AuthController::clearSessionCookie(httplib::Response& res) {
    std::string cookieVal = "connectx_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax";
    res.set_header("Set-Cookie", cookieVal);
}

void AuthController::sendError(httplib::Response& res, int statusCode, const std::string& message) {
    json body = {
        {"error", message},
        {"statusCode", statusCode}
    };
    res.status = statusCode;
    res.set_content(body.dump(), "application/json");
}

void AuthController::handleRegister(const httplib::Request& req, httplib::Response& res) {
    try {
        json payload = json::parse(req.body);

        std::string username = payload.value("username", "");
        std::string email = payload.value("email", "");
        std::string fullName = payload.value("fullName", "");
        std::string password = payload.value("password", "");

        AuthResult result = authService_->registerUser(username, email, fullName, password);

        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        if (result.session) {
            setSessionCookie(res, result.session->token);
        }

        json responseBody = {
            {"message", result.message},
            {"user", result.user->toJson()}
        };
        if (result.session) {
            responseBody["sessionToken"] = result.session->token;
        }

        res.status = result.statusCode;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON in request body.");
    } catch (const std::exception& e) {
        std::cerr << "[AuthController] Exception in handleRegister: " << e.what() << std::endl;
        sendError(res, 500, "An internal server error occurred.");
    }
}

void AuthController::handleLogin(const httplib::Request& req, httplib::Response& res) {
    try {
        json payload = json::parse(req.body);

        std::string identifier = payload.value("identifier", "");
        std::string password = payload.value("password", "");

        AuthResult result = authService_->login(identifier, password);

        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        if (result.session) {
            setSessionCookie(res, result.session->token);
        }

        json responseBody = {
            {"message", result.message},
            {"user", result.user->toJson()}
        };
        if (result.session) {
            responseBody["sessionToken"] = result.session->token;
        }

        res.status = result.statusCode;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON in request body.");
    } catch (const std::exception& e) {
        std::cerr << "[AuthController] Exception in handleLogin: " << e.what() << std::endl;
        sendError(res, 500, "An internal server error occurred.");
    }
}

void AuthController::handleLogout(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (!token.empty()) {
        authService_->logout(token);
    }

    clearSessionCookie(res);

    json responseBody = {
        {"message", "Logged out successfully."}
    };
    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

void AuthController::handleMe(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. No session provided.");
        return;
    }

    auto user = authService_->validateSession(token);
    if (!user) {
        clearSessionCookie(res);
        sendError(res, 401, "Session expired or invalid. Please log in again.");
        return;
    }

    json responseBody = {
        {"user", user->toJson()}
    };
    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

void AuthController::handleSearchUsers(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    std::string query = req.get_param_value("q");
    int limit = 50;
    if (req.has_param("limit")) {
        try {
            limit = std::stoi(req.get_param_value("limit"));
        } catch (...) {
            limit = 50;
        }
    }

    auto usersOpt = authService_->searchUsers(token, query, limit);
    if (!usersOpt) {
        clearSessionCookie(res);
        sendError(res, 401, "Session expired or invalid. Please log in again.");
        return;
    }

    json userArray = json::array();
    for (const auto& u : *usersOpt) {
        userArray.push_back(u.toPublicJson());
    }

    json responseBody = {
        {"users", userArray},
        {"count", userArray.size()}
    };
    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

} // namespace connectx
