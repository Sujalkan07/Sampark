#pragma once

#include "MessagingService.hpp"
#include <httplib.h>
#include <memory>
#include <string>

namespace connectx {

class MessagingController {
public:
    explicit MessagingController(std::shared_ptr<IMessagingService> messagingService);

    void handleCreateOrGetConversation(const httplib::Request& req, httplib::Response& res);
    void handleListConversations(const httplib::Request& req, httplib::Response& res);
    void handleGetConversation(const httplib::Request& req, httplib::Response& res);
    void handleGetMessages(const httplib::Request& req, httplib::Response& res);
    void handleSendMessage(const httplib::Request& req, httplib::Response& res);
    void handleMarkAsRead(const httplib::Request& req, httplib::Response& res);
    void handleEditMessage(const httplib::Request& req, httplib::Response& res);
    void handleDeleteMessage(const httplib::Request& req, httplib::Response& res);
    void handleSearchMessages(const httplib::Request& req, httplib::Response& res);
    void handleSearchGlobalMessages(const httplib::Request& req, httplib::Response& res);

    static std::string extractSessionToken(const httplib::Request& req);

private:
    std::shared_ptr<IMessagingService> messagingService_;
    static void sendError(httplib::Response& res, int statusCode, const std::string& message);
};

} // namespace connectx
