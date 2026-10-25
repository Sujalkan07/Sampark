#pragma once

#include "Database.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include "AuthService.hpp"
#include "AuthController.hpp"
#include "ConversationRepository.hpp"
#include "MessageRepository.hpp"
#include "MessagingService.hpp"
#include "MessagingController.hpp"
#include <httplib.h>
#include <memory>
#include <string>

namespace connectx {

class Server {
public:
    Server(Database& db,
           std::shared_ptr<IUserRepository> userRepo,
           std::shared_ptr<ISessionRepository> sessionRepo,
           std::shared_ptr<IAuthService> authService,
           std::shared_ptr<IConversationRepository> convRepo = nullptr,
           std::shared_ptr<IMessageRepository> msgRepo = nullptr,
           std::shared_ptr<IMessagingService> messagingService = nullptr);
    ~Server();

    void setupRoutes();
    bool listen(const std::string& host, int port);
    void stop();

    httplib::Server& getHttpServer() { return svr_; }

private:
    Database& db_;
    std::shared_ptr<IUserRepository> userRepo_;
    std::shared_ptr<ISessionRepository> sessionRepo_;
    std::shared_ptr<IAuthService> authService_;
    std::shared_ptr<IConversationRepository> convRepo_;
    std::shared_ptr<IMessageRepository> msgRepo_;
    std::shared_ptr<IMessagingService> messagingService_;

    std::unique_ptr<AuthController> authController_;
    std::unique_ptr<MessagingController> messagingController_;

    httplib::Server svr_;
    std::string allowedOrigins_;
};

} // namespace connectx
