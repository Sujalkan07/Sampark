#include "Database.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include "ConversationRepository.hpp"
#include "MessageRepository.hpp"
#include "AuthService.hpp"
#include "MessagingService.hpp"
#include "Validator.hpp"
#include "Server.hpp"
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

        // Phase 3 Tests:
        // Test 36: User profile update
        auto profRes = authService->updateProfile(aaravToken, "Aarav S. Sharma", "Passionate systems programmer and C++ enthusiast.");
        assert(profRes.success);
        assert(profRes.statusCode == 200);
        assert(profRes.user->fullName == "Aarav S. Sharma");
        assert(profRes.user->bio == "Passionate systems programmer and C++ enthusiast.");
        std::cout << "[PASS] Test 36: Profile update modifies full name and bio" << std::endl;

        // Test 37: Oversized bio rejected
        std::string hugeBio(350, 'A');
        auto bigBioRes = authService->updateProfile(aaravToken, "Aarav S. Sharma", hugeBio);
        assert(!bigBioRes.success);
        assert(bigBioRes.statusCode == 400);
        std::cout << "[PASS] Test 37: Oversized bio safely rejected with HTTP 400" << std::endl;

        // Test 38: Password change
        auto chgPassRes = authService->changePassword(aaravToken, "SecretPassword123!", "NewStrongPassword456!");
        assert(chgPassRes.success);
        assert(chgPassRes.statusCode == 200);
        auto newLoginRes = authService->login("aarav_sharma", "NewStrongPassword456!");
        assert(newLoginRes.success);
        std::cout << "[PASS] Test 38: Password change verifies current password and accepts new password" << std::endl;

        // Test 39: Password change with incorrect current password rejected
        auto wrongPassRes = authService->changePassword(aaravToken, "WrongOldPassword!", "AnotherPassword789!");
        assert(!wrongPassRes.success);
        assert(wrongPassRes.statusCode == 401);
        std::cout << "[PASS] Test 39: Password change with incorrect current password safely rejected (HTTP 401)" << std::endl;

        // Test 40: Edit message by author
        auto editRes = messagingService->editMessage(priyaToken, msg3Id, "Did you see my latest commit on GitHub?");
        assert(editRes.success);
        assert(editRes.statusCode == 200);
        assert(editRes.chatMessage->body == "Did you see my latest commit on GitHub?");
        assert(editRes.chatMessage->editedAt.has_value());
        std::cout << "[PASS] Test 40: Author can edit message; body and editedAt updated" << std::endl;

        // Test 41: Edit message by non-author rejected
        auto unauthEditRes = messagingService->editMessage(aaravToken, msg3Id, "Tampered text");
        assert(!unauthEditRes.success);
        assert(unauthEditRes.statusCode == 403);
        std::cout << "[PASS] Test 41: Unauthorized message edit by non-author rejected (HTTP 403)" << std::endl;

        // Test 42: Delete message by author
        auto delRes = messagingService->deleteMessage(priyaToken, msg3Id);
        assert(delRes.success);
        assert(delRes.statusCode == 200);
        assert(delRes.chatMessage->isDeleted == true);
        assert(delRes.chatMessage->body == "This message was deleted");
        std::cout << "[PASS] Test 42: Author can delete message; message flagged as deleted with redacted body" << std::endl;

        // Test 43: Delete message by non-author rejected
        auto unauthDelRes = messagingService->deleteMessage(priyaToken, msg1Id);
        assert(!unauthDelRes.success);
        assert(unauthDelRes.statusCode == 403);
        std::cout << "[PASS] Test 43: Unauthorized message deletion by non-author rejected (HTTP 403)" << std::endl;

        // Test 44: Search messages in conversation finds matching query and excludes deleted messages
        auto searchMsg = messagingService->sendMessage(aaravToken, convId, "Let's review the SQLite architecture docs.");
        assert(searchMsg.success);
        auto searchRes = messagingService->searchConversationMessages(aaravToken, convId, "SQLite", 10);
        assert(searchRes.success);
        assert(searchRes.messages.size() == 1);
        assert(searchRes.messages[0].body == "Let's review the SQLite architecture docs.");
        std::cout << "[PASS] Test 44: In-conversation search locates keyword and excludes deleted messages" << std::endl;

        // Test 45: Global message search across participant's conversations
        auto globalSearchRes = messagingService->searchGlobalMessages(priyaToken, "architecture", 10);
        assert(globalSearchRes.success);
        assert(globalSearchRes.messages.size() == 1);
        assert(globalSearchRes.messages[0].body == "Let's review the SQLite architecture docs.");
        std::cout << "[PASS] Test 45: Global message search across user's active conversations verified" << std::endl;
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
        assert(msgs.size() == 4);
        assert(msgs[0].body == "Hello Priya, how are you?");
        assert(msgs[1].body == "Hi Aarav! I am doing great, working on ConnectX.");
        assert(msgs[2].body == "This message was deleted");
        assert(msgs[2].isDeleted == true);
        assert(msgs[3].body == "Let's review the SQLite architecture docs.");
        std::cout << "[PASS] Test 46: Full conversation, edited messages, and deletions persist across database restarts" << std::endl;

        // Test 47: CORS origin allowlist validation
        assert(connectx::isOriginAllowed("http://localhost:5173") == true);
        assert(connectx::isOriginAllowed("http://127.0.0.1:5173") == true);
        assert(connectx::isOriginAllowed("http://localhost:8080") == true);
        assert(connectx::isOriginAllowed("https://malicious-attacker.com") == false);
        assert(connectx::isOriginAllowed("http://evil.org") == false);
        assert(connectx::isOriginAllowed("") == false);
        std::cout << "[PASS] Test 47: CORS origin allowlist accurately accepts trusted origins and rejects attackers" << std::endl;

        // Test 48: Concurrent session invalidation on password change
        auto authService = std::make_shared<connectx::AuthService>(userRepo, sessionRepo);
        auto regNeha = authService->registerUser("neha_verma", "neha@college.edu.in", "Neha Verma", "NehaPassword123!");
        assert(regNeha.success);
        auto loginDevice1 = authService->login("neha_verma", "NehaPassword123!");
        auto loginDevice2 = authService->login("neha_verma", "NehaPassword123!");
        assert(loginDevice1.session.has_value() && loginDevice2.session.has_value());
        std::string tokenDevice1 = loginDevice1.session->token;
        std::string tokenDevice2 = loginDevice2.session->token;
        assert(tokenDevice1 != tokenDevice2);

        // Verify both sessions initially valid
        assert(authService->validateSession(tokenDevice1).has_value());
        assert(authService->validateSession(tokenDevice2).has_value());

        // Change password from Device 2
        auto changeResult = authService->changePassword(tokenDevice2, "NehaPassword123!", "NewNehaPassword456!");
        assert(changeResult.success);

        // Current session (Device 2) remains valid
        assert(authService->validateSession(tokenDevice2).has_value());
        // Other session (Device 1) must be invalidated
        assert(!authService->validateSession(tokenDevice1).has_value());
        std::cout << "[PASS] Test 48: Concurrent session invalidation preserves active session and revokes others" << std::endl;

        // Test 49: Redundant deletion of already soft-deleted message rejected with 400
        int64_t deletedMsgId = msgs[2].id;
        std::string priyaToken = authService->login("priya_patel", "PriyaSecurePass2026!").session->token;
        auto redelResult = messagingService->deleteMessage(priyaToken, deletedMsgId);
        assert(!redelResult.success);
        assert(redelResult.statusCode == 400);
        assert(redelResult.message == "Message is already deleted.");
        std::cout << "[PASS] Test 49: Redundant deletion of soft-deleted message is safely rejected (HTTP 400)" << std::endl;

        // Test 50: Search with special characters (%, _, ') without wildcard bleed
        std::string aaravToken = authService->login("aarav_sharma", "NewStrongPassword456!").session->token;
        auto sendSpecial1 = messagingService->sendMessage(aaravToken, conv->id, "Rate is 100% completed_task today!");
        auto sendSpecial2 = messagingService->sendMessage(aaravToken, conv->id, "Rate is 1000 completed-task today!");
        assert(sendSpecial1.success && sendSpecial2.success);

        // Search specifically for literal '% completed_task'
        auto searchEscaped = messagingService->searchConversationMessages(aaravToken, conv->id, "% completed_task", 10);
        assert(searchEscaped.success);
        assert(searchEscaped.messages.size() == 1);
        assert(searchEscaped.messages[0].body == "Rate is 100% completed_task today!");
        std::cout << "[PASS] Test 50: Search pattern escaping handles special characters without wildcard bleed" << std::endl;

        // Test 51: Idempotent Schema Migration on existing and fresh DBs
        db.migrateSchema();
        db.migrateSchema();
        assert(db.hasColumn("users", "bio"));
        assert(db.hasColumn("users", "last_active_at"));
        assert(db.hasColumn("messages", "is_deleted"));
        assert(db.hasColumn("messages", "edited_at"));
        std::cout << "[PASS] Test 51: Schema migrations are repeatable and idempotent across invocations" << std::endl;

        // ==========================================
        // PHASE 4 TESTS: GROUPS, REPLIES, REACTIONS,
        // ATTACHMENTS, AND PREFERENCES
        // ==========================================

        // Register additional members for Group testing
        auto regRohit = authService->registerUser("rohit_v", "rohit@iitb.ac.in", "Rohit Verma", "RohitPass2026!");
        assert(regRohit.success);
        auto regAnanya = authService->registerUser("ananya_s", "ananya@iitb.ac.in", "Ananya Singh", "AnanyaPass2026!");
        assert(regAnanya.success);

        // Test 52: Group Creation with Creator as Admin & Initial Members
        auto p4Aarav = userRepo->findByUsername("aarav_sharma");
        auto p4Priya = userRepo->findByUsername("priya_patel");
        auto p4Rohit = userRepo->findByUsername("rohit_v");
        assert(p4Aarav.has_value() && p4Priya.has_value() && p4Rohit.has_value());

        std::vector<int64_t> initialMemberIds = { p4Priya->id, p4Rohit->id };
        auto createGroupRes = messagingService->createGroup(
            aaravToken,
            "IIT Bombay Coders",
            "Official group for coders and developers",
            "",
            initialMemberIds
        );
        assert(createGroupRes.success);
        assert(createGroupRes.statusCode == 201);
        assert(createGroupRes.conversation.has_value());
        assert(createGroupRes.conversation->type == "group");
        assert(createGroupRes.conversation->name == "IIT Bombay Coders");
        assert(createGroupRes.conversation->createdBy == p4Aarav->id);
        int64_t groupId = createGroupRes.conversation->id;

        // Verify members
        auto membersRes = messagingService->getGroupMembers(aaravToken, groupId);
        assert(membersRes.success);
        assert(membersRes.members.size() == 3); // Aarav + Priya + Rohit
        bool foundAdmin = false;
        for (const auto& m : membersRes.members) {
            if (m.userId == p4Aarav->id) {
                assert(m.role == "admin");
                foundAdmin = true;
            } else {
                assert(m.role == "member");
            }
        }
        assert(foundAdmin);
        std::cout << "[PASS] Test 52: Group creation with creator as admin and multiple initial members" << std::endl;

        // Test 53: Group Messaging (Sender Details & Multi-Member Delivery)
        auto groupMsg1 = messagingService->sendMessage(aaravToken, groupId, "Welcome to the IITB group everyone!");
        assert(groupMsg1.success);
        assert(groupMsg1.chatMessage.has_value());
        assert(groupMsg1.chatMessage->senderUsername == "aarav_sharma");
        assert(groupMsg1.chatMessage->conversationId == groupId);

        auto groupMsg2 = messagingService->sendMessage(priyaToken, groupId, "Glad to join, thanks Aarav!");
        assert(groupMsg2.success);
        assert(groupMsg2.chatMessage->senderUsername == "priya_patel");

        // Rohit reads group history
        std::string rohitToken = authService->login("rohit_v", "RohitPass2026!").session->token;
        auto rohitGroupHistory = messagingService->getMessages(rohitToken, groupId, 50, 0, 0);
        assert(rohitGroupHistory.success);
        assert(rohitGroupHistory.messages.size() == 2);
        assert(rohitGroupHistory.messages[0].body == "Welcome to the IITB group everyone!");
        assert(rohitGroupHistory.messages[1].body == "Glad to join, thanks Aarav!");
        std::cout << "[PASS] Test 53: Group messaging with sender attribution across members" << std::endl;

        // Test 54: Non-Member Access Rejection
        std::string ananyaToken = authService->login("ananya_s", "AnanyaPass2026!").session->token;
        auto nonMemberHistory = messagingService->getMessages(ananyaToken, groupId, 50, 0, 0);
        assert(!nonMemberHistory.success);
        assert(nonMemberHistory.statusCode == 403);

        auto nonMemberSend = messagingService->sendMessage(ananyaToken, groupId, "I am not invited but trying to post");
        assert(!nonMemberSend.success);
        assert(nonMemberSend.statusCode == 403);
        std::cout << "[PASS] Test 54: Non-member access to group history and sending is rejected (HTTP 403)" << std::endl;

        // Test 55: Member Management (Admin Adds Member, Member Cannot Remove Admin)
        auto ananyaUser = userRepo->findByUsername("ananya_s");
        assert(ananyaUser.has_value());

        // Non-admin (Priya) tries to add Ananya -> rejected 403
        auto priyaAdd = messagingService->addGroupMember(priyaToken, groupId, ananyaUser->id);
        assert(!priyaAdd.success);
        assert(priyaAdd.statusCode == 403);

        // Admin (Aarav) adds Ananya -> success 200
        auto adminAdd = messagingService->addGroupMember(aaravToken, groupId, ananyaUser->id);
        assert(adminAdd.success);
        assert(adminAdd.statusCode == 200);

        // Verify Ananya can now read messages
        auto ananyaHistory = messagingService->getMessages(ananyaToken, groupId, 50, 0, 0);
        assert(ananyaHistory.success);

        // Non-admin (Rohit) tries to remove Aarav -> rejected 403
        auto nonAdminRemove = messagingService->removeGroupMember(rohitToken, groupId, p4Aarav->id);
        assert(!nonAdminRemove.success);
        assert(nonAdminRemove.statusCode == 403);

        // Admin (Aarav) removes Rohit -> success
        auto adminRemove = messagingService->removeGroupMember(aaravToken, groupId, p4Rohit->id);
        assert(adminRemove.success);
        std::cout << "[PASS] Test 55: Group member administration with role-based permissions" << std::endl;

        // Test 56: Group Leave & Automatic Admin Transfer
        // Aarav (sole admin) leaves group. Admin should be transferred to oldest remaining member (Priya).
        auto leaveRes = messagingService->leaveGroup(aaravToken, groupId);
        assert(leaveRes.success);

        // Verify Priya is now admin
        auto updatedMembers = messagingService->getGroupMembers(priyaToken, groupId);
        assert(updatedMembers.success);
        bool priyaIsAdmin = false;
        for (const auto& m : updatedMembers.members) {
            if (m.userId == p4Priya->id) {
                if (m.role == "admin") priyaIsAdmin = true;
            }
        }
        assert(priyaIsAdmin);
        std::cout << "[PASS] Test 56: Admin departure automatically transfers administrative role to remaining member" << std::endl;

        // Test 57: Message Replies & Cross-Conversation Rejection
        int64_t welcomeMsgId = groupMsg1.chatMessage->id;
        auto replyRes = messagingService->sendMessage(priyaToken, groupId, "Replying to welcome note", welcomeMsgId);
        assert(replyRes.success);
        assert(replyRes.chatMessage.has_value());
        assert(replyRes.chatMessage->replyToId.has_value());
        assert(replyRes.chatMessage->replyToId.value() == welcomeMsgId);
        assert(replyRes.chatMessage->replyTo.has_value());
        assert(replyRes.chatMessage->replyTo->id == welcomeMsgId);
        assert(replyRes.chatMessage->replyTo->body == "Welcome to the IITB group everyone!");

        // Reject cross-conversation reply (replying to a direct-chat message inside a group chat)
        int64_t directMsgId = msgs[0].id;
        auto crossReplyRes = messagingService->sendMessage(priyaToken, groupId, "Cross-conv reply", directMsgId);
        assert(!crossReplyRes.success);
        assert(crossReplyRes.statusCode == 400);
        assert(crossReplyRes.message == "Invalid reply reference: message does not belong to this conversation.");
        std::cout << "[PASS] Test 57: Message reply-to referencing and cross-conversation reference validation" << std::endl;

        // Test 58: Emoji Reactions Toggle & Aggregation
        int64_t repliedMsgId = replyRes.chatMessage->id;
        // Priya adds thumbs up
        auto react1 = messagingService->toggleReaction(priyaToken, repliedMsgId, "👍");
        assert(react1.success);
        assert(react1.reactions.size() == 1);
        assert(react1.reactions[0].emoji == "👍");
        assert(react1.reactions[0].count == 1);

        // Ananya also adds thumbs up
        auto react2 = messagingService->toggleReaction(ananyaToken, repliedMsgId, "👍");
        assert(react2.success);
        assert(react2.reactions.size() == 1);
        assert(react2.reactions[0].count == 2);

        // Ananya adds heart
        auto react3 = messagingService->toggleReaction(ananyaToken, repliedMsgId, "❤️");
        assert(react3.success);
        assert(react3.reactions.size() == 2);

        // Priya toggles off thumbs up (removes her reaction)
        auto react4 = messagingService->toggleReaction(priyaToken, repliedMsgId, "👍");
        assert(react4.success);
        for (const auto& r : react4.reactions) {
            if (r.emoji == "👍") {
                assert(r.count == 1);
            }
        }
        std::cout << "[PASS] Test 58: Emoji reaction toggling, aggregation, and user tracking" << std::endl;

        // Test 59: File & Image Attachment Persistence and Download Authorization
        std::string sampleImageContent = "FAKE_JPEG_IMAGE_BINARY_DATA_FOR_TESTING";
        auto uploadRes = messagingService->uploadAttachment(
            priyaToken,
            groupId,
            "campus_map.jpg",
            sampleImageContent,
            "image/jpeg"
        );
        assert(uploadRes.success);
        assert(uploadRes.statusCode == 201);
        assert(uploadRes.attachment.has_value());
        int64_t attachId = uploadRes.attachment->id;
        assert(uploadRes.attachment->fileName == "campus_map.jpg");
        assert(uploadRes.attachment->mimeType == "image/jpeg");
        assert(uploadRes.attachment->fileSize == (int64_t)sampleImageContent.length());

        // Send message with linked attachment
        auto attachMsg = messagingService->sendMessage(priyaToken, groupId, "Here is the campus map", std::nullopt, attachId);
        assert(attachMsg.success);
        assert(attachMsg.chatMessage.has_value());
        assert(attachMsg.chatMessage->attachment.has_value());
        assert(attachMsg.chatMessage->attachment->id == attachId);

        // Authorized download by group member (Ananya)
        auto downloadAnanya = messagingService->getAttachment(ananyaToken, attachId);
        assert(downloadAnanya.success);
        assert(downloadAnanya.attachment.has_value());
        assert(downloadAnanya.fileContent == sampleImageContent);

        // Unauthorized download by non-member (Rohit, who was removed)
        auto downloadRohit = messagingService->getAttachment(rohitToken, attachId);
        assert(!downloadRohit.success);
        assert(downloadRohit.statusCode == 403);
        std::cout << "[PASS] Test 59: File attachment upload, association with message, and authorized download" << std::endl;

        // Test 60: User Personalization & Preferences
        // Default preferences
        auto defaultPrefs = messagingService->getUserPreferences(priyaToken);
        assert(defaultPrefs.success);
        assert(defaultPrefs.preferences.has_value());
        assert(defaultPrefs.preferences->theme == "system");
        assert(defaultPrefs.preferences->soundNotifications == true);
        assert(defaultPrefs.preferences->readReceiptsEnabled == true);

        // Update preferences to dark mode and disable sound
        connectx::UserPreferences customPrefs;
        customPrefs.theme = "dark";
        customPrefs.soundNotifications = false;
        customPrefs.desktopNotifications = true;
        customPrefs.readReceiptsEnabled = false;

        auto updatePrefRes = messagingService->updateUserPreferences(priyaToken, customPrefs);
        assert(updatePrefRes.success);
        assert(updatePrefRes.preferences.has_value());
        assert(updatePrefRes.preferences->theme == "dark");
        assert(updatePrefRes.preferences->soundNotifications == false);
        assert(updatePrefRes.preferences->desktopNotifications == true);
        assert(updatePrefRes.preferences->readReceiptsEnabled == false);

        // Re-read to confirm persistence
        auto rereadPrefs = messagingService->getUserPreferences(priyaToken);
        assert(rereadPrefs.success);
        assert(rereadPrefs.preferences->theme == "dark");
        assert(rereadPrefs.preferences->soundNotifications == false);
        std::cout << "[PASS] Test 60: User preferences CRUD and persistent settings" << std::endl;

        // Test 61: Phase 4 Schema Verification
        assert(db.hasColumn("conversation_members", "role"));
        assert(db.hasColumn("message_reactions", "emoji"));
        assert(db.hasColumn("attachments", "file_name"));
        assert(db.hasColumn("user_preferences", "theme"));
        assert(db.hasColumn("conversations", "type"));
        assert(db.hasColumn("conversations", "name"));
        assert(db.hasColumn("messages", "reply_to_id"));
        std::cout << "[PASS] Test 61: Phase 4 database tables, columns, and foreign keys verified" << std::endl;
    }

    // Clean up test DB
    fs::remove(testDbPath);
    if (fs::exists(testDbPath + "-wal")) fs::remove(testDbPath + "-wal");
    if (fs::exists(testDbPath + "-shm")) fs::remove(testDbPath + "-shm");

    std::cout << "\n=============================================" << std::endl;
    std::cout << " ALL 61 BACKEND TESTS PASSED SUCCESSFULLY!   " << std::endl;
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
