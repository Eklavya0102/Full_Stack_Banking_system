#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include "User.hpp"
#include "Account.hpp"
#include "Card.hpp"
#include "AccountRequest.hpp"
#include "Admin.hpp"
#include "Staff.hpp"

using json = nlohmann::json;

class Database {
private:
    sqlite3* db_;
    std::string dbPath_;

    bool exec(const std::string& sql);
    std::string generateAccountNumber();
    std::string generateCardNumber();
    std::string currentTimestamp();

public:
    explicit Database(const std::string& path = "banking.db");
    ~Database();

    bool open();
    void close();
    bool initSchema();
    bool seedData();

    // Auth
    std::shared_ptr<User> authenticate(const std::string& email,
                                       const std::string& password,
                                       const std::string& role);

    // Staff management
    bool createStaff(const std::string& name, const std::string& email,
                     const std::string& password, const std::string& branch,
                     const std::string& createdBy);
    std::vector<Staff> getAllStaff();
    bool deleteStaff(int id);

    // Account Requests
    int createAccountRequest(const AccountRequest& req);
    std::vector<AccountRequest> getPendingRequests();
    std::vector<AccountRequest> getAllRequests();
    bool approveRequest(int id, const std::string& reviewedBy);
    bool rejectRequest(int id, const std::string& reviewedBy,
                       const std::string& reason);

    // Accounts
    std::shared_ptr<Account> getAccountByNumber(const std::string& accountNum);
    std::vector<Account> searchAccounts(const std::string& query);
    bool updateBalance(const std::string& accountNum, double newBalance);
    bool closeAccount(const std::string& accountNum, const std::string& closedBy);

    // Transactions
    bool recordTransaction(const std::string& accountNum,
                           const std::string& type,
                           double amount, double balAfter,
                           const std::string& performedBy);
    std::vector<json> getRecentTransactions(int limit = 20);
    std::vector<json> getTransactionsByAccount(const std::string& accountNum);

    // Cards
    std::shared_ptr<Card> getCardByAccount(const std::string& accountNum);

    // Stats
    json getDashboardStats();
};
