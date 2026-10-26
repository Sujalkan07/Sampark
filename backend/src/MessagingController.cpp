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
            const std::string samparkPrefix = "sampark_session=";
            if (part.rfind(samparkPrefix, 0) == 0) {
                return part.substr(samparkPrefix.length());
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
        if (!payload.is_object() || !payload.contains("participantId") || !payload["participantId"].is_number()) {
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
        std::string convIdStr = req.matches[1];
        int64_t convId = std::stoll(convIdStr);

        auto result = messagingService_->getConversation(token, convId);
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
        sendError(res, 400, "Invalid conversation ID format.");
    }
}

void MessagingController::handleGetMessages(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        std::string convIdStr = req.matches[1];
        int64_t convId = std::stoll(convIdStr);

        int limit = 50;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
        }

        int64_t beforeId = 0;
        if (req.has_param("before")) {
            try { beforeId = std::stoll(req.get_param_value("before")); } catch (...) {}
        }

        int64_t afterId = 0;
        if (req.has_param("after")) {
            try { afterId = std::stoll(req.get_param_value("after")); } catch (...) {}
        }

        auto result = messagingService_->getMessages(token, convId, limit, beforeId, afterId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json msgsArray = json::array();
        for (const auto& m : result.messages) {
            msgsArray.push_back(m.toJson());
        }

        json responseBody = {
            {"conversation", result.conversation->toJson()},
            {"messages", msgsArray},
            {"count", result.messages.size()}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid conversation ID parameter.");
    }
}

void MessagingController::handleSendMessage(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        std::string convIdStr = req.matches[1];
        int64_t convId = std::stoll(convIdStr);

        json payload = json::parse(req.body);
        std::string body = "";
        if (payload.contains("body") && payload["body"].is_string()) {
            body = payload["body"].get<std::string>();
        }

        std::optional<int64_t> replyToId;
        if (payload.contains("replyToId") && payload["replyToId"].is_number()) {
            replyToId = payload["replyToId"].get<int64_t>();
        }

        std::optional<int64_t> attachmentId;
        if (payload.contains("attachmentId") && payload["attachmentId"].is_number()) {
            attachmentId = payload["attachmentId"].get<int64_t>();
        }

        auto result = messagingService_->sendMessage(token, convId, body, replyToId, attachmentId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"message", result.chatMessage->toJson()}
        };
        res.status = result.statusCode;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON in request body.");
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid conversation ID or request format.");
    }
}

void MessagingController::handleMarkAsRead(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        std::string convIdStr = req.matches[1];
        int64_t convId = std::stoll(convIdStr);

        auto result = messagingService_->markAsRead(token, convId);
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
        sendError(res, 400, "Invalid conversation ID parameter.");
    }
}

void MessagingController::handleEditMessage(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        std::string msgIdStr = req.matches[1];
        int64_t msgId = std::stoll(msgIdStr);

        json payload = json::parse(req.body);
        if (!payload.contains("body") || !payload["body"].is_string()) {
            sendError(res, 400, "Field 'body' is required and must be a string.");
            return;
        }

        std::string newBody = payload["body"].get<std::string>();
        auto result = messagingService_->editMessage(token, msgId, newBody);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"success", true},
            {"message", result.chatMessage->toJson()}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON in request body.");
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid message ID parameter.");
    }
}

void MessagingController::handleDeleteMessage(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        std::string msgIdStr = req.matches[1];
        int64_t msgId = std::stoll(msgIdStr);

        auto result = messagingService_->deleteMessage(token, msgId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"success", true},
            {"messageId", msgId},
            {"status", "deleted"}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid message ID parameter.");
    }
}

void MessagingController::handleSearchMessages(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        std::string convIdStr = req.matches[1];
        int64_t convId = std::stoll(convIdStr);

        std::string query = req.get_param_value("q");
        if (query.empty()) {
            json emptyResp = {{"messages", json::array()}, {"count", 0}};
            res.status = 200;
            res.set_content(emptyResp.dump(), "application/json");
            return;
        }

        int limit = 50;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
        }

        auto result = messagingService_->searchConversationMessages(token, convId, query, limit);
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
            {"count", result.messages.size()},
            {"query", query}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid parameters for search.");
    }
}

void MessagingController::handleSearchGlobalMessages(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    std::string query = req.get_param_value("q");
    if (query.empty()) {
        json emptyResp = {{"messages", json::array()}, {"count", 0}};
        res.status = 200;
        res.set_content(emptyResp.dump(), "application/json");
        return;
    }

    int limit = 50;
    if (req.has_param("limit")) {
        try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
    }

    auto result = messagingService_->searchGlobalMessages(token, query, limit);
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
        {"count", result.messages.size()},
        {"query", query}
    };
    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

// Phase 4: Group Chat Handlers
void MessagingController::handleCreateGroup(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        json payload = json::parse(req.body);
        if (!payload.contains("name") || !payload["name"].is_string()) {
            sendError(res, 400, "Field 'name' is required.");
            return;
        }
        std::string name = payload["name"].get<std::string>();
        std::string description = payload.value("description", "");
        std::string avatarUrl = payload.value("avatarUrl", "");

        std::vector<int64_t> memberIds;
        if (payload.contains("memberIds") && payload["memberIds"].is_array()) {
            for (const auto& item : payload["memberIds"]) {
                if (item.is_number()) {
                    memberIds.push_back(item.get<int64_t>());
                }
            }
        }

        auto result = messagingService_->createGroup(token, name, description, avatarUrl, memberIds);
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
        sendError(res, 500, "An internal server error occurred.");
    }
}

void MessagingController::handleGetGroupMembers(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        auto result = messagingService_->getGroupMembers(token, convId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json memArray = json::array();
        for (const auto& m : result.members) {
            memArray.push_back(m.toJson());
        }

        json responseBody = {
            {"members", memArray},
            {"count", result.members.size()}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (...) {
        sendError(res, 400, "Invalid conversation ID format.");
    }
}

void MessagingController::handleAddGroupMember(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        json payload = json::parse(req.body);
        if (!payload.contains("userId") || !payload["userId"].is_number()) {
            sendError(res, 400, "Field 'userId' is required.");
            return;
        }
        int64_t targetUserId = payload["userId"].get<int64_t>();

        auto result = messagingService_->addGroupMember(token, convId, targetUserId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json memArray = json::array();
        for (const auto& m : result.members) {
            memArray.push_back(m.toJson());
        }

        json responseBody = {
            {"success", true},
            {"members", memArray}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON.");
    } catch (...) {
        sendError(res, 400, "Invalid request format.");
    }
}

void MessagingController::handleRemoveGroupMember(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        int64_t targetUserId = std::stoll(req.matches[2]);

        auto result = messagingService_->removeGroupMember(token, convId, targetUserId);
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
    } catch (...) {
        sendError(res, 400, "Invalid conversation or user ID.");
    }
}

void MessagingController::handleUpdateGroupMemberRole(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        int64_t targetUserId = std::stoll(req.matches[2]);

        json payload = json::parse(req.body);
        if (!payload.contains("role") || !payload["role"].is_string()) {
            sendError(res, 400, "Field 'role' is required.");
            return;
        }

        std::string role = payload["role"].get<std::string>();
        auto result = messagingService_->updateGroupMemberRole(token, convId, targetUserId, role);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json memArray = json::array();
        for (const auto& m : result.members) {
            memArray.push_back(m.toJson());
        }

        json responseBody = {
            {"success", true},
            {"message", result.message},
            {"members", memArray}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON.");
    } catch (...) {
        sendError(res, 400, "Invalid conversation or user ID.");
    }
}

void MessagingController::handleUpdateGroup(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        json payload = json::parse(req.body);

        std::string name = payload.value("name", "");
        std::string description = payload.value("description", "");
        std::string avatarUrl = payload.value("avatarUrl", "");

        auto result = messagingService_->updateGroup(token, convId, name, description, avatarUrl);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"success", true},
            {"conversation", result.conversation->toJson()}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON.");
    } catch (...) {
        sendError(res, 400, "Invalid request format.");
    }
}

void MessagingController::handleDeleteGroup(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        auto result = messagingService_->deleteGroup(token, convId);
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
    } catch (...) {
        sendError(res, 400, "Invalid conversation ID.");
    }
}

void MessagingController::handleLeaveGroup(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        auto result = messagingService_->leaveGroup(token, convId);
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
    } catch (...) {
        sendError(res, 400, "Invalid conversation ID.");
    }
}

// Phase 4: Reactions Handlers
void MessagingController::handleToggleReaction(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t msgId = std::stoll(req.matches[1]);
        json payload = json::parse(req.body);
        if (!payload.contains("emoji") || !payload["emoji"].is_string()) {
            sendError(res, 400, "Field 'emoji' is required.");
            return;
        }
        std::string emoji = payload["emoji"].get<std::string>();

        auto result = messagingService_->toggleReaction(token, msgId, emoji);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json rxArray = json::array();
        for (const auto& r : result.reactions) {
            rxArray.push_back(r.toJson());
        }

        json responseBody = {
            {"success", true},
            {"action", result.reactionAction},
            {"reactions", rxArray}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON.");
    } catch (...) {
        sendError(res, 400, "Invalid message ID.");
    }
}

// Phase 4: Attachment Handlers
void MessagingController::handleUploadAttachment(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t convId = std::stoll(req.matches[1]);
        std::string fileName;
        std::string fileContent;
        std::string mimeType;

        if (req.is_multipart_form_data()) {
            if (req.form.has_file("file")) {
                auto file = req.form.get_file("file");
                fileName = file.filename;
                fileContent = file.content;
                mimeType = file.content_type;
            } else {
                sendError(res, 400, "Missing 'file' field in multipart form upload.");
                return;
            }
        } else {
            // Support JSON upload format
            json payload = json::parse(req.body);
            fileName = payload.value("fileName", "attachment.dat");
            fileContent = payload.value("content", "");
            mimeType = payload.value("mimeType", "application/octet-stream");
        }

        auto result = messagingService_->uploadAttachment(token, convId, fileName, fileContent, mimeType);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"success", true},
            {"attachment", result.attachment->toJson()}
        };
        res.status = 201;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON payload.");
    } catch (...) {
        sendError(res, 400, "Invalid conversation ID or request payload.");
    }
}

void MessagingController::handleGetAttachment(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t attachmentId = std::stoll(req.matches[1]);
        auto result = messagingService_->getAttachment(token, attachmentId);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        std::string safeHeaderName;
        for (char c : result.fileName) {
            if (c != '"' && c != '\r' && c != '\n' && static_cast<unsigned char>(c) >= 32) {
                safeHeaderName.push_back(c);
            }
        }
        if (safeHeaderName.empty()) safeHeaderName = "attachment.dat";

        res.status = 200;
        res.set_header("Content-Type", result.fileMimeType.empty() ? "application/octet-stream" : result.fileMimeType);
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("Content-Disposition", "inline; filename=\"" + safeHeaderName + "\"");
        res.set_content(result.fileContent, result.fileMimeType);
    } catch (...) {
        sendError(res, 400, "Invalid attachment ID.");
    }
}

void MessagingController::handleDeleteAttachment(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        int64_t attachmentId = std::stoll(req.matches[1]);
        auto result = messagingService_->deleteAttachment(token, attachmentId);
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
    } catch (...) {
        sendError(res, 400, "Invalid attachment ID.");
    }
}

// Phase 4: Preferences Handlers
void MessagingController::handleGetPreferences(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    auto result = messagingService_->getUserPreferences(token);
    if (!result.success) {
        sendError(res, result.statusCode, result.message);
        return;
    }

    json responseBody = {
        {"preferences", result.preferences->toJson()}
    };
    res.status = 200;
    res.set_content(responseBody.dump(), "application/json");
}

void MessagingController::handleUpdatePreferences(const httplib::Request& req, httplib::Response& res) {
    std::string token = extractSessionToken(req);
    if (token.empty()) {
        sendError(res, 401, "Authentication required. Please log in.");
        return;
    }

    try {
        json payload = json::parse(req.body);
        auto existing = messagingService_->getUserPreferences(token);
        UserPreferences prefs;
        if (existing.success && existing.preferences) {
            prefs = *existing.preferences;
        }
        if (payload.contains("theme")) {
            if (!payload["theme"].is_string()) {
                sendError(res, 400, "Theme must be a string.");
                return;
            }
            std::string th = payload["theme"].get<std::string>();
            if (th == "light" || th == "dark" || th == "system") {
                prefs.theme = th;
            } else {
                sendError(res, 400, "Theme must be one of: 'system', 'light', 'dark'.");
                return;
            }
        }
        if (payload.contains("soundNotifications")) {
            if (!payload["soundNotifications"].is_boolean()) {
                sendError(res, 400, "Field 'soundNotifications' must be a boolean.");
                return;
            }
            prefs.soundNotifications = payload["soundNotifications"].get<bool>();
        } else if (payload.contains("sound_enabled")) {
            if (!payload["sound_enabled"].is_boolean()) {
                sendError(res, 400, "Field 'sound_enabled' must be a boolean.");
                return;
            }
            prefs.soundNotifications = payload["sound_enabled"].get<bool>();
        }
        if (payload.contains("desktopNotifications")) {
            if (!payload["desktopNotifications"].is_boolean()) {
                sendError(res, 400, "Field 'desktopNotifications' must be a boolean.");
                return;
            }
            prefs.desktopNotifications = payload["desktopNotifications"].get<bool>();
        } else if (payload.contains("desktop_notifications_enabled")) {
            if (!payload["desktop_notifications_enabled"].is_boolean()) {
                sendError(res, 400, "Field 'desktop_notifications_enabled' must be a boolean.");
                return;
            }
            prefs.desktopNotifications = payload["desktop_notifications_enabled"].get<bool>();
        }
        if (payload.contains("readReceiptsEnabled")) {
            if (!payload["readReceiptsEnabled"].is_boolean()) {
                sendError(res, 400, "Field 'readReceiptsEnabled' must be a boolean.");
                return;
            }
            prefs.readReceiptsEnabled = payload["readReceiptsEnabled"].get<bool>();
        } else if (payload.contains("read_receipts_enabled")) {
            if (!payload["read_receipts_enabled"].is_boolean()) {
                sendError(res, 400, "Field 'read_receipts_enabled' must be a boolean.");
                return;
            }
            prefs.readReceiptsEnabled = payload["read_receipts_enabled"].get<bool>();
        }

        auto result = messagingService_->updateUserPreferences(token, prefs);
        if (!result.success) {
            sendError(res, result.statusCode, result.message);
            return;
        }

        json responseBody = {
            {"success", true},
            {"preferences", result.preferences->toJson()}
        };
        res.status = 200;
        res.set_content(responseBody.dump(), "application/json");
    } catch (const json::parse_error&) {
        sendError(res, 400, "Malformed JSON.");
    } catch (const std::exception& e) {
        sendError(res, 500, "An internal server error occurred.");
    }
}

} // namespace connectx
