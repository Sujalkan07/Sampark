#include "Server.hpp"
#include "HealthController.hpp"
#include <iostream>

namespace connectx {

Server::Server(Database& db,
               std::shared_ptr<IUserRepository> userRepo,
               std::shared_ptr<ISessionRepository> sessionRepo,
               std::shared_ptr<IAuthService> authService)
    : db_(db),
      userRepo_(std::move(userRepo)),
      sessionRepo_(std::move(sessionRepo)),
      authService_(std::move(authService)),
      authController_(std::make_unique<AuthController>(authService_)) {
    setupRoutes();
}

Server::~Server() {
    stop();
}

void Server::setupRoutes() {
    // 1. CORS Pre-routing Handler
    svr_.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (origin.empty()) {
            origin = "*";
        }
        res.set_header("Access-Control-Allow-Origin", origin);
        res.set_header("Access-Control-Allow-Credentials", "true");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Cookie, Accept, X-Requested-With");

        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 2. CORS Post-routing Handler (ensure CORS headers present on all responses)
    svr_.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (!origin.empty()) {
            res.set_header("Access-Control-Allow-Origin", origin);
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Cookie, Accept, X-Requested-With");
        }
    });

    // 3. Health Endpoint
    svr_.Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
        HealthController::handleHealth(req, res);
    });

    // 4. Authentication Endpoints
    svr_.Post("/api/auth/register", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleRegister(req, res);
    });

    svr_.Post("/api/auth/login", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleLogin(req, res);
    });

    svr_.Post("/api/auth/logout", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleLogout(req, res);
    });

    svr_.Get("/api/auth/me", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleMe(req, res);
    });
}

bool Server::listen(const std::string& host, int port) {
    std::cout << "[ConnectX Server] Starting on http://" << host << ":" << port << std::endl;
    return svr_.listen(host, port);
}

void Server::stop() {
    if (svr_.is_running()) {
        svr_.stop();
    }
}

} // namespace connectx
