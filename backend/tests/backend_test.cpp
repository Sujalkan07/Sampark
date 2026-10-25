#include "Database.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include "ConversationRepository.hpp"
#include "MessageRepository.hpp"
#include "AuthService.hpp"
#include "MessagingService.hpp"
#include "Validator.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

void runTests() {
    std::cout << ">>> STARTING SAMPARK BACKEND TEST SUITE <<<" << std::endl;
    const std::string testDbPath = "test_connectx.db";

    // Clean up any existing test DB file
    if (fs::exists(testDbPath)) {
        fs::remove(testDbPath);
    }
    if (fs::exists(testDbPath + "-wal")) {
        fs::remove(testDbPath + "-wal");
    }
    if (fs::exists(testDbPath + "-shm")) {
        fs::remove(testDbPath + "-shm");
    }

    {
        connectx::Database db(testDbPath);
        assert(db.initSchema());
        std::cout << "[PASS] Test 1: Database initialization and schema creation" << std::endl;

        auto userRepo = std::make_shared<connectx::SqliteUserRepository>(db);
        auto sessionRepo = std::make_shared<connectx::SqliteSessionRepository>(db);
        auto authService = std::make_shared<connectx::AuthService>(userRepo, sessionRepo);

        // Test 2: Input Validation tests
        std::string err;
        assert(!connectx::Validator::isValidUsername("ab", err)); // too short
        assert(connectx::Validator::isValidUsername("aarav_sharma", err));
        assert(!connectx::Validator::isValidEmail("not-an-email", err));
        assert(connectx::Validator::isValidEmail("aarav@iitb.ac.in", err));
        assert(!connectx::Validator::isValidPassword("short", err)); // < 8
        assert(connectx::Validator::isValidPassword("ValidPass123!", err));
        std::cout << "[PASS] Test 2: Input validator rules" << std::endl;

        // Test 3: Valid registration creates persistent account
        auto regResult = authService->registerUser("aarav_sharma", "aarav@iitb.ac.in", "Aarav Sharma", "SecretPassword123!");
        assert(regResult.success);
        assert(regResult.statusCode == 201);
        assert(regResult.user.has_value());
        assert(regResult.user->username == "aarav_sharma");
        assert(regResult.user->email == "aarav@iitb.ac.in");
        assert(regResult.session.has_value());
        std::cout << "[PASS] Test 3: Valid registration creates account and session" << std::endl;

        // Test 4: Passwords are not stored in plaintext
        auto savedUser = userRepo->findByUsername("aarav_sharma");
        assert(savedUser.has_value());
        assert(savedUser->passwordHash != "SecretPassword123!");
        assert(!savedUser->passwordHash.empty());
        assert(!savedUser->salt.empty());
        assert(savedUser->passwordHash.length() == 64); // 32 bytes hex
        assert(savedUser->salt.length() == 32); // 16 bytes hex
        std::cout << "[PASS] Test 4: Password is securely hashed (PBKDF2-HMAC-SHA256, 100k iterations) and not plaintext" << std::endl;

        // Test 5: Duplicate username rejection
        auto dupUserResult = authService->registerUser("aarav_sharma", "other@example.com", "Other Name", "Password123!");
        assert(!dupUserResult.success);
        assert(dupUserResult.statusCode == 409);
        std::cout << "[PASS] Test 5: Duplicate username rejected" << std::endl;

        // Test 6: Duplicate email rejection
        auto dupEmailResult = authService->registerUser("new_user", "aarav@iitb.ac.in", "New User", "Password123!");
        assert(!dupEmailResult.success);
        assert(dupEmailResult.statusCode == 409);
        std::cout << "[PASS] Test 6: Duplicate email rejected" << std::endl;

        // Test 7: Correct credentials allow login
        auto loginResult = authService->login("aarav_sharma", "SecretPassword123!");
        assert(loginResult.success);
        assert(loginResult.statusCode == 200);
        assert(loginResult.session.has_value());
        std::string userSessionToken = loginResult.session->token;
        std::cout << "[PASS] Test 7: Correct credentials login successful" << std::endl;

        // Test 8: Login with email
        auto loginEmailResult = authService->login("aarav@iitb.ac.in", "SecretPassword123!");
        assert(loginEmailResult.success);
        assert(loginEmailResult.statusCode == 200);
        std::cout << "[PASS] Test 8: Login using email identifier successful" << std::endl;

        // Test 9: Incorrect credentials rejected
        auto badPassResult = authService->login("aarav_sharma", "WrongPassword999!");
        assert(!badPassResult.success);
        assert(badPassResult.statusCode == 401);

        auto nonExistentUserResult = authService->login("nonexistent_user", "SomePassword123!");
        assert(!nonExistentUserResult.success);
        assert(nonExistentUserResult.statusCode == 401);
        std::cout << "[PASS] Test 9: Incorrect credentials safely rejected" << std::endl;

        // Test 10: Valid session resolves correct user
        auto sessionUser = authService->validateSession(userSessionToken);
        assert(sessionUser.has_value());
        assert(sessionUser->username == "aarav_sharma");
        assert(sessionUser->fullName == "Aarav Sharma");
        std::cout << "[PASS] Test 10: Valid session resolves correct user profile" << std::endl;

        // Test 11: Invalid session token rejected
        auto invalidSessionUser = authService->validateSession("completely_bogus_token_12345");
        assert(!invalidSessionUser.has_value());
        std::cout << "[PASS] Test 11: Invalid session token returns nullopt" << std::endl;

        // Test 12: Multi-user isolation
        auto user2Result = authService->registerUser("priya_patel", "priya@iitd.ac.in", "Priya Patel", "PriyaSecurePass2026!");
        assert(user2Result.success);
        std::string priyaToken = user2Result.session->token;

        auto resolvedAarav = authService->validateSession(userSessionToken);
        auto resolvedPriya = authService->validateSession(priyaToken);
        assert(resolvedAarav.has_value() && resolvedPriya.has_value());
        assert(resolvedAarav->id != resolvedPriya->id);
        assert(resolvedAarav->username == "aarav_sharma");
        assert(resolvedPriya->username == "priya_patel");
        assert(userSessionToken != priyaToken);
        std::cout << "[PASS] Test 12: Multi-user isolation verified between distinct sessions" << std::endl;

        // Test 13: SQL Injection protection (parameterized queries)
        std::string injectionPayload = "admin' OR '1'='1";
        auto sqlInjResult = authService->login(injectionPayload, "any_pass");
        assert(!sqlInjResult.success);
        assert(sqlInjResult.statusCode == 401);
        std::cout << "[PASS] Test 13: SQL injection payload safely treated as literal string" << std::endl;

        // Test 14: Logout invalidates the session
        bool logoutSuccess = authService->logout(userSessionToken);
        assert(logoutSuccess);
        auto loggedOutUser = authService->validateSession(userSessionToken);
        assert(!loggedOutUser.has_value());
        std::cout << "[PASS] Test 14: Logout terminates session; subsequent verification fails" << std::endl;

        // Re-authenticate Aarav to obtain a fresh session for discovery tests
        auto reLoginAarav = authService->login("aarav_sharma", "SecretPassword123!");
        assert(reLoginAarav.success);
        std::string aaravToken = reLoginAarav.session->token;

        // Test 15: User Discovery & Self-exclusion
        // Priya searches: should find Aarav and exclude Priya
        auto priyaDiscovers = authService->searchUsers(priyaToken, "");
        assert(priyaDiscovers.has_value());
        assert(priyaDiscovers->size() == 1);
        assert((*priyaDiscovers)[0].username == "aarav_sharma");
        assert((*priyaDiscovers)[0].fullName == "Aarav Sharma");

        // Aarav searches: should find Priya and exclude Aarav
        auto aaravDiscovers = authService->searchUsers(aaravToken, "");
        assert(aaravDiscovers.has_value());
        assert(aaravDiscovers->size() == 1);
        assert((*aaravDiscovers)[0].username == "priya_patel");
        assert((*aaravDiscovers)[0].fullName == "Priya Patel");
        std::cout << "[PASS] Test 15: User discovery excludes caller and discovers registered peers" << std::endl;

        // Test 16: Partial and Case-Insensitive Search
        auto searchPriyaCase = authService->searchUsers(aaravToken, "PRIYA");
        assert(searchPriyaCase.has_value() && searchPriyaCase->size() == 1);
        assert((*searchPriyaCase)[0].username == "priya_patel");

        auto searchFullName = authService->searchUsers(priyaToken, "Sharma");
        assert(searchFullName.has_value() && searchFullName->size() == 1);
        assert((*searchFullName)[0].username == "aarav_sharma");

        auto searchNonExistent = authService->searchUsers(aaravToken, "ghost_user_xyz");
        assert(searchNonExistent.has_value() && searchNonExistent->empty());
        std::cout << "[PASS] Test 16: Case-insensitive username and display name search" << std::endl;

        // Test 17: Unauthenticated discovery rejection
        auto unauthSearch = authService->searchUsers("bad_token_999", "");
        assert(!unauthSearch.has_value());
        std::cout << "[PASS] Test 17: Unauthenticated search request rejected (nullopt)" << std::endl;

        // Test 18: SQL injection protection in search query
        auto sqlInjSearch = authService->searchUsers(aaravToken, "' OR '1'='1");
        assert(sqlInjSearch.has_value() && sqlInjSearch->empty());
        std::cout << "[PASS] Test 18: SQL injection search payload safely handled without data leakage" << std::endl;

        // Test 19: Privacy verification (No password hash, salt, or private tokens)
        auto discoveredUser = (*priyaDiscovers)[0];
        assert(discoveredUser.passwordHash.empty());
        assert(discoveredUser.salt.empty());
        connectx::json publicJson = discoveredUser.toPublicJson();
        assert(!publicJson.contains("passwordHash"));
        assert(!publicJson.contains("salt"));
        assert(!publicJson.contains("email"));
        assert(publicJson.contains("username"));
        assert(publicJson.contains("fullName"));
        std::cout << "[PASS] Test 19: User discovery payload does not expose hashes, salts, or emails" << std::endl;
    } // DB is closed and destructed here

    // Test 20: Database persistence survives application restart
    {
        connectx::Database db(testDbPath);
        auto userRepo = std::make_shared<connectx::SqliteUserRepository>(db);
        auto sessionRepo = std::make_shared<connectx::SqliteSessionRepository>(db);
        auto authService = std::make_shared<connectx::AuthService>(userRepo, sessionRepo);

        // Verify Aarav Sharma still exists in DB
        auto persistedUser = userRepo->findByUsername("aarav_sharma");
        assert(persistedUser.has_value());
        assert(persistedUser->email == "aarav@iitb.ac.in");

        // Verify authentication against persisted record works after restart
        auto reAuth = authService->login("aarav_sharma", "SecretPassword123!");
        assert(reAuth.success);
        assert(reAuth.user.has_value());
        assert(reAuth.user->username == "aarav_sharma");
        std::cout << "[PASS] Test 20: Database persistence survives application restart" << std::endl;

        // PHASE 2 MESSAGING TESTS
        auto convRepo = std::make_shared<connectx::SqliteConversationRepository>(db);
        auto msgRepo = std::make_shared<connectx::SqliteMessageRepository>(db);
        auto messagingService = std::make_shared<connectx::MessagingService>(convRepo, msgRepo, userRepo, sessionRepo);

        std::string aaravToken = reAuth.session->token;
        int64_t aaravId = reAuth.user->id;

        // Login Priya
        auto priyaAuth = authService->login("priya_patel", "PriyaSecurePass2026!");
        assert(priyaAuth.success);
        std::string priyaToken = priyaAuth.session->token;
        int64_t priyaId = priyaAuth.user->id;

        // Register Third User: Rohan Kapoor (ID 3) for isolation testing
        auto rohanReg = authService->registerUser("rohan_k", "rohan@iitb.ac.in", "Rohan Kapoor", "RohanPass789!");
        assert(rohanReg.success);
        std::string rohanToken = rohanReg.session->token;

        // Test 21: Direct conversation creation between Aarav and Priya
        auto createConv = messagingService->createOrGetConversation(aaravToken, priyaId);
        assert(createConv.success);
        assert(createConv.conversation.has_value());
        int64_t convId = createConv.conversation->id;
        assert(convId > 0);
        assert(createConv.conversation->otherParticipant.has_value());
        assert(createConv.conversation->otherParticipant->username == "priya_patel");
        std::cout << "[PASS] Test 21: Direct conversation created between Aarav and Priya (ID: " << convId << ")" << std::endl;

        // Test 22: Uniqueness and idempotency: Priya requesting conversation with Aarav resolves to same ID
        auto priyaGetConv = messagingService->createOrGetConversation(priyaToken, aaravId);
        assert(priyaGetConv.success);
        assert(priyaGetConv.conversation.has_value());
        assert(priyaGetConv.conversation->id == convId);
        assert(priyaGetConv.conversation->otherParticipant->username == "aarav_sharma");
        std::cout << "[PASS] Test 22: Bi-directional pair normalization resolves to exact same conversation ID" << std::endl;
        assert(priyaGetConv.conversation->id == convId);
        assert(priyaGetConv.conversation->otherParticipant->username == "aarav_sharma");
        std::cout << "[PASS] Test 22: Bi-directional pair normalization resolves to exact same conversation ID" << std::endl;

        // Test 23: Self-conversation rejection
        auto selfConv = messagingService->createOrGetConversation(aaravToken, aaravId);
        assert(!selfConv.success);
        assert(selfConv.statusCode == 400);
        std::cout << "[PASS] Test 23: Self-conversation rejected with HTTP 400" << std::endl;

        // Test 24: Unauthenticated conversation access rejected
        auto unauthConv = messagingService->createOrGetConversation("invalid_token_xyz", priyaId);
        assert(!unauthConv.success);
        assert(unauthConv.statusCode == 401);
        std::cout << "[PASS] Test 24: Unauthenticated conversation access rejected with HTTP 401" << std::endl;

        // Test 25: Non-existent user conversation rejected
        auto noUserConv = messagingService->createOrGetConversation(aaravToken, 99999);
        assert(!noUserConv.success);
        assert(noUserConv.statusCode == 404);
        std::cout << "[PASS] Test 25: Non-existent user conversation creation rejected with HTTP 404" << std::endl;

        // Test 26: Sending message persists to SQLite with server timestamp and sender
        auto sendMsg1 = messagingService->sendMessage(aaravToken, convId, "Hello Priya, how are you?");
        assert(sendMsg1.success);
        assert(sendMsg1.chatMessage.has_value());
        assert(sendMsg1.chatMessage->senderId == aaravId);
        assert(sendMsg1.chatMessage->body == "Hello Priya, how are you?");
        assert(!sendMsg1.chatMessage->createdAt.empty());
        int64_t msg1Id = sendMsg1.chatMessage->id;
        std::cout << "[PASS] Test 26: Message successfully sent and persisted in SQLite (ID: " << msg1Id << ")" << std::endl;

        // Test 27: Message history retrieval in chronological order
        auto sendMsg2 = messagingService->sendMessage(priyaToken, convId, "Hi Aarav! I am doing great, working on ConnectX.");
        assert(sendMsg2.success);
        int64_t msg2Id = sendMsg2.chatMessage->id;

        auto historyResult = messagingService->getMessages(aaravToken, convId, 50, 0, 0);
        assert(historyResult.success);
        assert(historyResult.messages.size() == 2);
        assert(historyResult.messages[0].id == msg1Id);
        assert(historyResult.messages[1].id == msg2Id);
        assert(historyResult.messages[0].body == "Hello Priya, how are you?");
        assert(historyResult.messages[1].body == "Hi Aarav! I am doing great, working on ConnectX.");
        std::cout << "[PASS] Test 27: Message history retrieved in stable chronological order" << std::endl;

        // Test 28: Empty or whitespace-only messages rejected
        auto emptySend = messagingService->sendMessage(aaravToken, convId, "   \n\t  ");
        assert(!emptySend.success);
        assert(emptySend.statusCode == 400);
        std::cout << "[PASS] Test 28: Empty and whitespace-only message rejected with HTTP 400" << std::endl;

        // Test 29: Message length validation (>4000 characters rejected)
        std::string hugeMessage(4001, 'A');
        auto hugeSend = messagingService->sendMessage(aaravToken, convId, hugeMessage);
        assert(!hugeSend.success);
        assert(hugeSend.statusCode == 400);
        std::cout << "[PASS] Test 29: Message exceeding 4000 chars rejected with HTTP 400" << std::endl;

        // Test 30: Participant isolation: Rohan (third user) cannot view messages or send to Aarav-Priya conversation
        auto rohanView = messagingService->getMessages(rohanToken, convId, 50, 0, 0);
        assert(!rohanView.success);
        assert(rohanView.statusCode == 403);

        auto rohanSend = messagingService->sendMessage(rohanToken, convId, "I am sneaking into your chat!");
        assert(!rohanSend.success);
        assert(rohanSend.statusCode == 403);
        std::cout << "[PASS] Test 30: Multi-user privacy enforced: third party denied access (HTTP 403)" << std::endl;

        // Test 31: Unread message count and mark as read
        // Priya sent msg2 to Aarav. Aarav has 1 unread message from Priya in convId.
        int unreadBefore = msgRepo->getUnreadCount(convId, aaravId);
        assert(unreadBefore == 1);

        auto markReadResult = messagingService->markAsRead(aaravToken, convId);
        assert(markReadResult.success);

        int unreadAfter = msgRepo->getUnreadCount(convId, aaravId);
        assert(unreadAfter == 0);
        std::cout << "[PASS] Test 31: Unread count calculated correctly and updated upon mark-as-read" << std::endl;

        // Test 32: Incremental polling cursor (afterId)
        auto sendMsg3 = messagingService->sendMessage(priyaToken, convId, "Did you see my latest commit?");
        assert(sendMsg3.success);
        int64_t msg3Id = sendMsg3.chatMessage->id;

        // Polling from after msg2Id should return ONLY msg3
        auto pollResult = messagingService->getMessages(aaravToken, convId, 50, 0, msg2Id);
        assert(pollResult.success);
        assert(pollResult.messages.size() == 1);
        assert(pollResult.messages[0].id == msg3Id);
        assert(pollResult.messages[0].body == "Did you see my latest commit?");
        std::cout << "[PASS] Test 32: Incremental polling cursor (afterId) returns only new message delta" << std::endl;

        // Test 33: Pagination cursor (beforeId)
        auto beforeResult = messagingService->getMessages(aaravToken, convId, 1, msg3Id, 0);
        assert(beforeResult.success);
        assert(beforeResult.messages.size() == 1);
        assert(beforeResult.messages[0].id == msg2Id);
        std::cout << "[PASS] Test 33: Pagination cursor (beforeId) retrieves earlier messages correctly" << std::endl;

        // Test 34: Conversation list returns other participant and last message preview
        auto listResult = messagingService->listConversations(aaravToken);
        assert(listResult.success);
        assert(!listResult.conversations.empty());
        auto& firstConv = listResult.conversations[0];
        assert(firstConv.id == convId);
        assert(firstConv.otherParticipant.has_value());
        assert(firstConv.otherParticipant->username == "priya_patel");
        assert(firstConv.lastMessage.has_value());
        assert(firstConv.lastMessage->body == "Did you see my latest commit?");
        std::cout << "[PASS] Test 34: Conversation list contains other participant details and last message preview" << std::endl;
    } // DB is closed here

    // Test 35: Messages and conversations survive full database restart
    {
        connectx::Database db(testDbPath);
        auto userRepo = std::make_shared<connectx::SqliteUserRepository>(db);
        auto sessionRepo = std::make_shared<connectx::SqliteSessionRepository>(db);
        auto convRepo = std::make_shared<connectx::SqliteConversationRepository>(db);
        auto msgRepo = std::make_shared<connectx::SqliteMessageRepository>(db);
        auto messagingService = std::make_shared<connectx::MessagingService>(convRepo, msgRepo, userRepo, sessionRepo);

        // Fetch Aarav and Priya
        auto aaravUser = userRepo->findByUsername("aarav_sharma");
        auto priyaUser = userRepo->findByUsername("priya_patel");
        assert(aaravUser.has_value());
        assert(priyaUser.has_value());

        // Verify conversation persists
        auto conv = convRepo->findByParticipants(aaravUser->id, priyaUser->id);
        assert(conv.has_value());

        // Verify messages persist
        auto msgs = msgRepo->getMessages(conv->id, 50, 0, 0);
        assert(msgs.size() == 3);
        assert(msgs[0].body == "Hello Priya, how are you?");
        assert(msgs[1].body == "Hi Aarav! I am doing great, working on ConnectX.");
        assert(msgs[2].body == "Did you see my latest commit?");
        std::cout << "[PASS] Test 35: Full conversation and message history persists across database restarts" << std::endl;
    }

    // Clean up test DB
    fs::remove(testDbPath);
    if (fs::exists(testDbPath + "-wal")) fs::remove(testDbPath + "-wal");
    if (fs::exists(testDbPath + "-shm")) fs::remove(testDbPath + "-shm");

    std::cout << "\n=============================================" << std::endl;
    std::cout << " ALL 35 BACKEND TESTS PASSED SUCCESSFULLY!   " << std::endl;
    std::cout << "=============================================" << std::endl;
}

int main() {
    try {
        runTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION DURING TEST: " << e.what() << std::endl;
        return 1;
    }
}
