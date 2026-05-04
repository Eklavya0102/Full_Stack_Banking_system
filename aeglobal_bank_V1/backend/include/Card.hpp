#pragma once
#include <string>

class Card {
private:
    int id_;
    std::string accountNumber_;
    std::string cardNumber_;
    std::string expiryDate_;
    std::string cvvHash_;
    std::string cardType_;   // debit, credit
    std::string status_;     // active, blocked, expired
    std::string issuedAt_;

public:
    Card() : id_(0), status_("active") {}

    int getId() const { return id_; }
    std::string getAccountNumber() const { return accountNumber_; }
    std::string getCardNumber() const { return cardNumber_; }
    std::string getCardNumberMasked() const {
        // Card stored as "XXXX XXXX XXXX XXXX"
        // Return "**** **** **** XXXX"
        if (cardNumber_.size() < 4) return "**** **** **** ****";
        // Get last 4 chars (digits of last group)
        std::string last4 = cardNumber_.substr(cardNumber_.size() - 4);
        return "**** **** **** " + last4;
    }
    std::string getExpiryDate() const { return expiryDate_; }
    std::string getCvvHash() const { return cvvHash_; }
    std::string getCardType() const { return cardType_; }
    std::string getStatus() const { return status_; }
    std::string getIssuedAt() const { return issuedAt_; }

    void setId(int v) { id_ = v; }
    void setAccountNumber(const std::string& v) { accountNumber_ = v; }
    void setCardNumber(const std::string& v) { cardNumber_ = v; }
    void setExpiryDate(const std::string& v) { expiryDate_ = v; }
    void setCvvHash(const std::string& v) { cvvHash_ = v; }
    void setCardType(const std::string& v) { cardType_ = v; }
    void setStatus(const std::string& v) { status_ = v; }
    void setIssuedAt(const std::string& v) { issuedAt_ = v; }

    bool isValid() const { return status_ == "active"; }
};
