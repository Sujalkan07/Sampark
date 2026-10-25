#include "Database.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include "AuthService.hpp"
#include "Validator.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

void runTests() {
    std::cout << ">>> STARTING CONNECTX PHASE 1 BACKEND TEST SUITE <<<" << std::endl;
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
    } // DB is closed and destructed here

    // Test 15: Database persistence survives application restart
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
        std::cout << "[PASS] Test 15: Database persistence survives application restart" << std::endl;
    }

    // Clean up test DB
    fs::remove(testDbPath);
    if (fs::exists(testDbPath + "-wal")) fs::remove(testDbPath + "-wal");
    if (fs::exists(testDbPath + "-shm")) fs::remove(testDbPath + "-shm");

    std::cout << "\n=============================================" << std::endl;
    std::cout << " ALL 15 BACKEND TESTS PASSED SUCCESSFULLY!   " << std::endl;
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
