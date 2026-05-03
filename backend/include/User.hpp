#pragma once
#include <string>

class User {
protected:
    int id_;
    std::string name_;
    std::string email_;
    std::string password_;
    std::string role_;

public:
    User() : id_(0) {}
    User(int id, const std::string& name, const std::string& email,
         const std::string& password, const std::string& role)
        : id_(id), name_(name), email_(email), password_(password), role_(role) {}

    virtual ~User() = default;

    int getId() const { return id_; }
    std::string getName() const { return name_; }
    std::string getEmail() const { return email_; }
    std::string getPassword() const { return password_; }
    std::string getRole() const { return role_; }

    void setName(const std::string& n) { name_ = n; }
    void setEmail(const std::string& e) { email_ = e; }
    void setPassword(const std::string& p) { password_ = p; }

    virtual std::string getPortalName() const = 0;
    virtual bool canApproveRequests() const { return false; }
    virtual bool canCreateStaff() const { return false; }
};
