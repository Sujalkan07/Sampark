#include "MessagingController.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

namespace connectx {

using json = nlohmann::json;

MessagingController::MessagingController(std::shared_ptr<IMessagingService> messagingService)
    : messagingService_(std::move(messagingService)) {}

std::string MessagingController::extractSessionToken(const httplib::Request& req) {
    // 1. Check Cookie header
    std::string cookieHeader = req.get_header_value("Cookie");
    if (!cookieHeader.empty()) {
        std::istringstream stream(cookieHeader);
        std::string part;
        while (std::getline(stream, part, ';')) {
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

void MessagingController::sendError(httplib::Response& res, int statusCode, const std::string& message) {
    json body = {
        {"error", message},
        {"statusCode", statusCode}
    };
    res.status = statusCode;
    res.set_content(body.dump(), "application/json");
}

void MessagingController::handleCreateOrGetConversation(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        json payload = json::parse(req.body);
        if (!payload.contains("participantId") || !payload["participantId"].is_number()) {
            sendError(res, 400, "Field 'participantId' is required and must be an integer.");
            return;
        }

        int64_t participantId = payload["participantId"].get<int64_t>();
        auto result = messagingService_->createOrGetConversation(token, participantId);

        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"conversation", result.conversation->toJson()}
        };
        res.status = result.statusCode;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON in request body.");
    } catch (const std::exception& e) {
        std::cerr << "[MessagingController] Exception in handleCreateOrGetConversation: " << e.what() << std::endl;
        sendError(res, 500, "An internal server error occurred.");
    }
}

void MessagingController::handleListConversations(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    auto result = messagingService_->listConversations(token);
    if (!result.success) {
        sendError(res, result.statusCode, result.message);
        return;
    }

    json convsArray = json::array();
    for (const auto& c : result.conversations) {
        convsArray.push_back(c.toJson());
    }

    json responseBody = {
        {"conversations", convsArray},
        {"count", result.conversations.size()}
    };
    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

void MessagingController::handleGetConversation(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t conversationId = std::stoll(req.matches[1].str());
        auto result = messagingService_->getConversation(token, conversationId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"conversation", result.conversation->toJson()}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const std::exception& e) {
        std::cerr << "[MessagingController] Exception in handleGetConversation: " << e.what() << std::endl;
        sendError(res, 400, "Invalid conversation ID parameter.");
    }
}

void MessagingController::handleGetMessages(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t conversationId = std::stoll(req.matches[1].str());

        int limit = 50;
        if (req.has_param("limit")) {
            limit = std::stoi(req.get_param_value("limit"));
        }

        int64_t beforeId = 0;
        if (req.has_param("before")) {
            beforeId = std::stoll(req.get_param_value("before"));
        }

        int64_t afterId = 0;
        if (req.has_param("after")) {
            afterId = std::stoll(req.get_param_value("after"));
        }

        auto result = messagingService_->getMessages(token, conversationId, limit, beforeId, afterId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json msgsArray = json::array();
        for (const auto& m : result.messages) {
            msgsArray.push_back(m.toJson());
        }

        json responseBody = {
            {"messages", msgsArray},
            {"count", result.messages.size()}
        };
        if (result.conversation.has_value()) {
            responseBody["conversation"] = result.conversation->toJson();
        }

        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const std::exception& e) {
        std::cerr << "[MessagingController] Exception in handleGetMessages: " << e.what() << std::endl;
        sendError(res, 400, "Invalid request parameters.");
    }
}

void MessagingController::handleSendMessage(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t conversationId = std::stoll(req.matches[1].str());

        json payload = json::parse(req.body);
        std::string body = payload.value("body", "");

        auto result = messagingService_->sendMessage(token, conversationId, body);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"message", result.chatMessage->toJson()}
        };
        res.status = 201;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON in request body.");
    } catch (const std::exception& e) {
        std::cerr << "[MessagingController] Exception in handleSendMessage: " << e.what() << std::endl;
        sendError(res, 400, "Invalid request parameters.");
    }
}

void MessagingController::handleMarkAsRead(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t conversationId = std::stoll(req.matches[1].str());
        auto result = messagingService_->markAsRead(token, conversationId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"success", true},
            {"message", result.message}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const std::exception& e) {
        std::cerr << "[MessagingController] Exception in handleMarkAsRead: " << e.what() << std::endl;
        sendError(res, 400, "Invalid conversation ID.");
    }
}

} // namespace connectx
