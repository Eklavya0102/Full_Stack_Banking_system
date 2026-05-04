#pragma once
#include <string>

class Account {
private:
    std::string accountNumber_;
    std::string holderName_;
    std::string phone_;
    std::string email_;
    std::string idType_;
    std::string idNumber_;
    std::string accountType_;
    std::string address_;
    double balance_;
    std::string status_;   // active, closed, suspended
    std::string createdAt_;
    std::string createdBy_;

public:
    Account() : balance_(0.0), status_("active") {}

    // Getters
    std::string getAccountNumber() const { return accountNumber_; }
    std::string getHolderName() const { return holderName_; }
    std::string getPhone() const { return phone_; }
    std::string getEmail() const { return email_; }
    std::string getIdType() const { return idType_; }
    std::string getIdNumber() const { return idNumber_; }
    std::string getAccountType() const { return accountType_; }
    std::string getAddress() const { return address_; }
    double getBalance() const { return balance_; }
    std::string getStatus() const { return status_; }
    std::string getCreatedAt() const { return createdAt_; }
    std::string getCreatedBy() const { return createdBy_; }

    // Setters
    void setAccountNumber(const std::string& v) { accountNumber_ = v; }
    void setHolderName(const std::string& v) { holderName_ = v; }
    void setPhone(const std::string& v) { phone_ = v; }
    void setEmail(const std::string& v) { email_ = v; }
    void setIdType(const std::string& v) { idType_ = v; }
    void setIdNumber(const std::string& v) { idNumber_ = v; }
    void setAccountType(const std::string& v) { accountType_ = v; }
    void setAddress(const std::string& v) { address_ = v; }
    void setBalance(double v) { balance_ = v; }
    void setStatus(const std::string& v) { status_ = v; }
    void setCreatedAt(const std::string& v) { createdAt_ = v; }
    void setCreatedBy(const std::string& v) { createdBy_ = v; }

    bool deposit(double amount) {
        if (amount <= 0 || status_ != "active") return false;
        balance_ += amount;
        return true;
    }

    bool withdraw(double amount) {
        if (amount <= 0 || balance_ < amount || status_ != "active") return false;
        balance_ -= amount;
        return true;
    }
};
