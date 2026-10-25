#include "Database.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include "AuthService.hpp"
#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "   Sampark Backend Server              " << std::endl;
    std::cout << "========================================" << std::endl;

    std::string host = "0.0.0.0";
    int port = 8080;
    std::string dbPath = "connectx.db";

    if (const char* envHost = std::getenv("HOST")) {
        host = envHost;
    }
    if (const char* envPort = std::getenv("PORT")) {
        port = std::atoi(envPort);
    }
    if (const char* envDb = std::getenv("DATABASE_PATH")) {
        dbPath = envDb;
    }

    // Allow command-line overrides: ./server [host] [port] [dbPath]
    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::atoi(argv[2]);
    if (argc > 3) dbPath = argv[3];

    // If the database path is connectx.db and it doesn't exist in the current working directory,
    // automatically check if it exists in the backend/ subfolder (e.g. running from project root)
    if (!std::getenv("DATABASE_PATH") && dbPath == "connectx.db") {
        if (!std::filesystem::exists("connectx.db") && std::filesystem::exists("backend/connectx.db")) {
            dbPath = "backend/connectx.db";
        }
    }

    std::cout << "[Config] Database Path: " << dbPath
              << " (" << std::filesystem::absolute(dbPath).string() << ")" << std::endl;
    std::cout << "[Config] Host: " << host << ", Port: " << port << std::endl;

    connectx::Database db(dbPath);
    if (!db.initSchema()) {
        std::cerr << "[Fatal] Failed to initialize database schema." << std::endl;
        return 1;
    }
    std::cout << "[Database] Schema verified and ready." << std::endl;

    auto userRepo = std::make_shared<connectx::SqliteUserRepository>(db);
    auto sessionRepo = std::make_shared<connectx::SqliteSessionRepository>(db);
    auto authService = std::make_shared<connectx::AuthService>(userRepo, sessionRepo);

    // Clean expired sessions on boot
    sessionRepo->cleanExpiredSessions();

    connectx::Server server(db, userRepo, sessionRepo, authService);

    std::cout << "[Server] Ready to accept connections." << std::endl;
    if (!server.listen(host, port)) {
        std::cerr << "[Fatal] Server failed to start or stopped unexpectedly." << std::endl;
        return 1;
    }

    return 0;
}
