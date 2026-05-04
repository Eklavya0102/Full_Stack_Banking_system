#pragma once
#include "User.hpp"

class Admin : public User {
public:
    Admin() = default;
    Admin(int id, const std::string& name, const std::string& email,
          const std::string& password)
        : User(id, name, email, password, "admin") {}

    std::string getPortalName() const override { return "Admin Portal"; }
    bool canApproveRequests() const override { return true; }
    bool canCreateStaff() const override { return true; }
};
