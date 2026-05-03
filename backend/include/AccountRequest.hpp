#pragma once
#include <string>

class AccountRequest {
private:
    int id_;
    std::string holderName_;
    std::string phone_;
    std::string email_;
    std::string idType_;
    std::string idNumber_;
    std::string accountType_;
    std::string address_;
    double initialDeposit_;
    std::string status_;       // pending, approved, rejected
    std::string requestedBy_;  // staff email
    std::string createdAt_;
    std::string reviewedAt_;
    std::string reviewedBy_;
    std::string rejectionReason_;

public:
    AccountRequest()
        : id_(0), initialDeposit_(0.0), status_("pending") {}

    int getId() const { return id_; }
    std::string getHolderName() const { return holderName_; }
    std::string getPhone() const { return phone_; }
    std::string getEmail() const { return email_; }
    std::string getIdType() const { return idType_; }
    std::string getIdNumber() const { return idNumber_; }
    std::string getAccountType() const { return accountType_; }
    std::string getAddress() const { return address_; }
    double getInitialDeposit() const { return initialDeposit_; }
    std::string getStatus() const { return status_; }
    std::string getRequestedBy() const { return requestedBy_; }
    std::string getCreatedAt() const { return createdAt_; }
    std::string getReviewedAt() const { return reviewedAt_; }
    std::string getReviewedBy() const { return reviewedBy_; }
    std::string getRejectionReason() const { return rejectionReason_; }

    void setId(int v) { id_ = v; }
    void setHolderName(const std::string& v) { holderName_ = v; }
    void setPhone(const std::string& v) { phone_ = v; }
    void setEmail(const std::string& v) { email_ = v; }
    void setIdType(const std::string& v) { idType_ = v; }
    void setIdNumber(const std::string& v) { idNumber_ = v; }
    void setAccountType(const std::string& v) { accountType_ = v; }
    void setAddress(const std::string& v) { address_ = v; }
    void setInitialDeposit(double v) { initialDeposit_ = v; }
    void setStatus(const std::string& v) { status_ = v; }
    void setRequestedBy(const std::string& v) { requestedBy_ = v; }
    void setCreatedAt(const std::string& v) { createdAt_ = v; }
    void setReviewedAt(const std::string& v) { reviewedAt_ = v; }
    void setReviewedBy(const std::string& v) { reviewedBy_ = v; }
    void setRejectionReason(const std::string& v) { rejectionReason_ = v; }
};
