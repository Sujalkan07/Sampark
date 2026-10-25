#include "Server.hpp"
#include "HealthController.hpp"
#include <iostream>

namespace connectx {

Server::Server(Database& db,
               std::shared_ptr<IUserRepository> userRepo,
               std::shared_ptr<ISessionRepository> sessionRepo,
               std::shared_ptr<IAuthService> authService,
               std::shared_ptr<IConversationRepository> convRepo,
               std::shared_ptr<IMessageRepository> msgRepo,
               std::shared_ptr<IMessagingService> messagingService)
    : db_(db),
      userRepo_(std::move(userRepo)),
      sessionRepo_(std::move(sessionRepo)),
      authService_(std::move(authService)),
      convRepo_(std::move(convRepo)),
      msgRepo_(std::move(msgRepo)),
      messagingService_(std::move(messagingService)),
      authController_(std::make_unique<AuthController>(authService_)) {

    // Lazy instantiate messaging components if not provided
    if (!convRepo_) {
        convRepo_ = std::make_shared<SqliteConversationRepository>(db_);
    }
    if (!msgRepo_) {
        msgRepo_ = std::make_shared<SqliteMessageRepository>(db_);
    }
    if (!messagingService_) {
        messagingService_ = std::make_shared<MessagingService>(convRepo_, msgRepo_, userRepo_, sessionRepo_);
    }

    messagingController_ = std::make_unique<MessagingController>(messagingService_);

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

    // 2. CORS Post-routing Handler
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

    // 5. User Discovery Endpoints
    svr_.Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleSearchUsers(req, res);
    });

    svr_.Get("/api/users/search", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleSearchUsers(req, res);
    });

    // 6. Conversation Endpoints
    svr_.Post("/api/conversations", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleCreateOrGetConversation(req, res);
    });

    svr_.Get("/api/conversations", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleListConversations(req, res);
    });

    svr_.Get(R"(/api/conversations/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleGetConversation(req, res);
    });

    // 7. Message Endpoints
    svr_.Get(R"(/api/conversations/(\d+)/messages)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleGetMessages(req, res);
    });

    svr_.Post(R"(/api/conversations/(\d+)/messages)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleSendMessage(req, res);
    });

    svr_.Post(R"(/api/conversations/(\d+)/read)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleMarkAsRead(req, res);
    });
}

bool Server::listen(const std::string& host, int port) {
    std::cout << "[Sampark Server] Starting on http://" << host << ":" << port << std::endl;
    return svr_.listen(host, port);
}

void Server::stop() {
    if (svr_.is_running()) {
        svr_.stop();
    }
}

} // namespace connectx
