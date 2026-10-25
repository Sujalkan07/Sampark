#pragma once

#include <string>

namespace connectx {

class Validator {
public:
    static std::string trim(const std::string& str);
    static std::string toLower(const std::string& str);

    static bool isValidUsername(const std::string& username, std::string& error);
    static bool isValidEmail(const std::string& email, std::string& error);
    static bool isValidFullName(const std::string& fullName, std::string& error);
    static bool isValidPassword(const std::string& password, std::string& error);
    static bool isValidBio(const std::string& bio, std::string& error);
};

} // namespace connectx
