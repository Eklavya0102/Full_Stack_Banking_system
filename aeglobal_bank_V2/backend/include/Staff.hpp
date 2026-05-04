#pragma once
#include "User.hpp"

class Staff : public User {
private:
    std::string branch_;
    std::string createdBy_;

public:
    Staff() = default;
    Staff(int id, const std::string& name, const std::string& email,
          const std::string& password, const std::string& branch = "",
          const std::string& createdBy = "")
        : User(id, name, email, password, "staff"),
          branch_(branch), createdBy_(createdBy) {}

    std::string getPortalName() const override { return "Staff Portal"; }
    std::string getBranch() const { return branch_; }
    std::string getCreatedBy() const { return createdBy_; }
    void setBranch(const std::string& b) { branch_ = b; }
};
