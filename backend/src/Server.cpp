#include "Server.hpp"
#include "HealthController.hpp"
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cstdlib>

namespace connectx {

bool isOriginAllowed(const std::string& origin) {
    if (origin.empty()) return false;

    static const std::unordered_set<std::string> defaultAllowedOrigins = {
        "http://localhost:5173",
        "http://127.0.0.1:5173",
        "http://localhost:3000",
        "http://127.0.0.1:3000",
        "http://localhost:4173",
        "http://127.0.0.1:4173",
        "http://localhost:8080",
        "http://127.0.0.1:8080"
    };

    if (defaultAllowedOrigins.find(origin) != defaultAllowedOrigins.end()) {
        return true;
    }

    // Check environment variable SAMPARK_ALLOWED_ORIGINS (comma-separated)
    const char* envAllowed = std::getenv("SAMPARK_ALLOWED_ORIGINS");
    if (!envAllowed) {
        envAllowed = std::getenv("CONNECTX_ALLOWED_ORIGINS");
    }
    if (envAllowed) {
        std::stringstream ss(envAllowed);
        std::string item;
        while (std::getline(ss, item, ',')) {
            size_t start = item.find_first_not_of(" \t\r\n");
            size_t end = item.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                if (item.substr(start, end - start + 1) == origin) {
                    return true;
                }
            }
        }
    }

    return false;
}

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

        if (!origin.empty()) {
            if (isOriginAllowed(origin)) {
                res.set_header("Access-Control-Allow-Origin", origin);
                res.set_header("Access-Control-Allow-Credentials", "true");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Cookie, Accept, X-Requested-With");
                res.set_header("Vary", "Origin");

                if (req.method == "OPTIONS") {
                    res.status = 204;
                    return httplib::Server::HandlerResponse::Handled;
                }
            } else {
                // Untrusted cross-origin request
                if (req.method == "OPTIONS") {
                    res.status = 403;
                    res.set_content("CORS origin not allowed.", "text/plain");
                    return httplib::Server::HandlerResponse::Handled;
                }
                // For non-OPTIONS from untrusted origins, do NOT set Access-Control-Allow-Origin.
                // Standard browser SOP will block response reading.
            }
        } else if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 2. CORS Post-routing Handler
    svr_.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (!origin.empty() && isOriginAllowed(origin)) {
            if (!res.has_header("Access-Control-Allow-Origin")) {
                res.set_header("Access-Control-Allow-Origin", origin);
                res.set_header("Access-Control-Allow-Credentials", "true");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Cookie, Accept, X-Requested-With");
                res.set_header("Vary", "Origin");
            }
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

    // 5. User Discovery & Profile Endpoints
    svr_.Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleSearchUsers(req, res);
    });

    svr_.Get("/api/users/search", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleSearchUsers(req, res);
    });

    svr_.Put("/api/users/profile", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleUpdateProfile(req, res);
    });

    svr_.Put("/api/auth/password", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleChangePassword(req, res);
    });

    svr_.Get(R"(/api/users/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        authController_->handleGetUserProfile(req, res);
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

    svr_.Get(R"(/api/conversations/(\d+)/messages/search)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleSearchMessages(req, res);
    });

    svr_.Get("/api/messages/search", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleSearchGlobalMessages(req, res);
    });

    svr_.Post(R"(/api/conversations/(\d+)/messages)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleSendMessage(req, res);
    });

    svr_.Put(R"(/api/messages/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleEditMessage(req, res);
    });

    svr_.Delete(R"(/api/messages/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleDeleteMessage(req, res);
    });

    svr_.Post(R"(/api/conversations/(\d+)/read)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleMarkAsRead(req, res);
    });

    // 8. Phase 4: Group Endpoints
    svr_.Post("/api/groups", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleCreateGroup(req, res);
    });

    svr_.Get(R"(/api/conversations/(\d+)/members)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleGetGroupMembers(req, res);
    });

    svr_.Post(R"(/api/conversations/(\d+)/members)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleAddGroupMember(req, res);
    });

    svr_.Delete(R"(/api/conversations/(\d+)/members/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleRemoveGroupMember(req, res);
    });

    svr_.Put(R"(/api/conversations/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleUpdateGroup(req, res);
    });

    svr_.Delete(R"(/api/conversations/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleDeleteGroup(req, res);
    });

    svr_.Post(R"(/api/conversations/(\d+)/leave)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleLeaveGroup(req, res);
    });

    // 9. Phase 4: Reaction Endpoints
    svr_.Post(R"(/api/messages/(\d+)/reactions)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleToggleReaction(req, res);
    });

    // 10. Phase 4: Attachment Endpoints
    svr_.Post(R"(/api/conversations/(\d+)/attachments)", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleUploadAttachment(req, res);
    });

    svr_.Get(R"(/api/attachments/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleGetAttachment(req, res);
    });

    // 11. Phase 4: Preferences Endpoints
    svr_.Get("/api/users/preferences", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleGetPreferences(req, res);
    });

    svr_.Put("/api/users/preferences", [this](const httplib::Request& req, httplib::Response& res) {
        messagingController_->handleUpdatePreferences(req, res);
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
