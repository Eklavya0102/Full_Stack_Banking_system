#include "Database.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <ctime>

using json = nlohmann::json;

Database::Database(const std::string& path) : db_(nullptr), dbPath_(path) {}

Database::~Database() { close(); }

bool Database::open() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open DB: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    // Enable WAL for better concurrency
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    return true;
}

void Database::close() {
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool Database::exec(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::string Database::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Database::generateAccountNumber() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    return "AE" + std::to_string(dis(gen));
}

std::string Database::generateCardNumber() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    // Format: XXXX XXXX XXXX XXXX (16 digits with spaces)
    return std::to_string(dis(gen)) + " " + std::to_string(dis(gen)) +
           " " + std::to_string(dis(gen)) + " " + std::to_string(dis(gen));
}

bool Database::initSchema() {
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            role TEXT NOT NULL CHECK(role IN ('admin','staff')),
            branch TEXT DEFAULT '',
            created_by TEXT DEFAULT '',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS accounts (
            account_number TEXT PRIMARY KEY,
            holder_name TEXT NOT NULL,
            phone TEXT NOT NULL,
            email TEXT NOT NULL,
            id_type TEXT NOT NULL,
            id_number TEXT NOT NULL,
            account_type TEXT NOT NULL DEFAULT 'savings',
            address TEXT DEFAULT '',
            balance REAL NOT NULL DEFAULT 0.0,
            status TEXT NOT NULL DEFAULT 'active',
            created_by TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS cards (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            account_number TEXT NOT NULL,
            card_number TEXT UNIQUE NOT NULL,
            expiry_date TEXT NOT NULL,
            cvv TEXT NOT NULL,
            card_type TEXT NOT NULL DEFAULT 'debit',
            status TEXT NOT NULL DEFAULT 'active',
            issued_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(account_number) REFERENCES accounts(account_number)
        );
        CREATE TABLE IF NOT EXISTS account_requests (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            holder_name TEXT NOT NULL,
            phone TEXT NOT NULL,
            email TEXT NOT NULL,
            id_type TEXT NOT NULL,
            id_number TEXT NOT NULL,
            account_type TEXT NOT NULL DEFAULT 'savings',
            address TEXT DEFAULT '',
            initial_deposit REAL NOT NULL DEFAULT 0.0,
            status TEXT NOT NULL DEFAULT 'pending',
            requested_by TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            reviewed_at TEXT DEFAULT '',
            reviewed_by TEXT DEFAULT '',
            rejection_reason TEXT DEFAULT ''
        );
        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            account_number TEXT NOT NULL,
            type TEXT NOT NULL,
            amount REAL NOT NULL,
            balance_after REAL NOT NULL,
            performed_by TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )";
    return exec(sql);
}

bool Database::seedData() {
    // Check if admin already exists
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_,
        "SELECT COUNT(*) FROM users WHERE role='admin'", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    sqlite3_step(stmt);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (count > 0) return true; // Already seeded

    // Only seed the two default login users — no fake accounts/transactions
    std::string sql = R"(
        INSERT INTO users (name, email, password, role, branch) VALUES
            ('Administrator', 'admin@aeglobal.com', 'admin123', 'admin', 'HQ'),
            ('Staff Member',  'staff@aeglobal.com', 'staff123', 'staff', 'Branch-01');
    )";
    return exec(sql);
}

// ─── Auth ───────────────────────────────────────────────────────────────────

std::shared_ptr<User> Database::authenticate(const std::string& email,
                                              const std::string& password,
                                              const std::string& role) {
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT id, name, email, password, role, branch FROM users "
        "WHERE email=? AND password=? AND role=?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_STATIC);

    std::shared_ptr<User> user;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string name = (const char*)sqlite3_column_text(stmt, 1);
        std::string em   = (const char*)sqlite3_column_text(stmt, 2);
        std::string pw   = (const char*)sqlite3_column_text(stmt, 3);
        std::string r    = (const char*)sqlite3_column_text(stmt, 4);
        if (r == "admin")
            user = std::make_shared<Admin>(id, name, em, pw);
        else
            user = std::make_shared<Staff>(id, name, em, pw);
    }
    sqlite3_finalize(stmt);
    return user;
}

// ─── Staff Management ────────────────────────────────────────────────────────

bool Database::createStaff(const std::string& name, const std::string& email,
                            const std::string& password,
                            const std::string& branch,
                            const std::string& createdBy) {
    sqlite3_stmt* stmt;
    std::string sql =
        "INSERT INTO users (name, email, password, role, branch, created_by) "
        "VALUES (?, ?, ?, 'staff', ?, ?)";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, branch.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, createdBy.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Staff> Database::getAllStaff() {
    std::vector<Staff> list;
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT id, name, email, password, branch, created_by "
        "FROM users WHERE role='staff' ORDER BY id DESC";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return list;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        auto text = [&](int c) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, c);
            return t ? t : "";
        };
        list.emplace_back(id, text(1), text(2), text(3), text(4), text(5));
    }
    sqlite3_finalize(stmt);
    return list;
}

bool Database::deleteStaff(int id) {
    sqlite3_stmt* stmt;
    std::string sql = "DELETE FROM users WHERE id=? AND role='staff'";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(stmt);
    return ok;
}

// ─── Account Requests ────────────────────────────────────────────────────────

int Database::createAccountRequest(const AccountRequest& req) {
    sqlite3_stmt* stmt;
    std::string sql =
        "INSERT INTO account_requests (holder_name, phone, email, id_type, "
        "id_number, account_type, address, initial_deposit, requested_by) "
        "VALUES (?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(stmt, 1, req.getHolderName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, req.getPhone().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, req.getEmail().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, req.getIdType().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, req.getIdNumber().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, req.getAccountType().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, req.getAddress().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 8, req.getInitialDeposit());
    sqlite3_bind_text(stmt, 9, req.getRequestedBy().c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok ? (int)sqlite3_last_insert_rowid(db_) : -1;
}

static AccountRequest rowToRequest(sqlite3_stmt* stmt) {
    AccountRequest r;
    auto text = [&](int c) -> std::string {
        const char* t = (const char*)sqlite3_column_text(stmt, c);
        return t ? t : "";
    };
    r.setId(sqlite3_column_int(stmt, 0));
    r.setHolderName(text(1));
    r.setPhone(text(2));
    r.setEmail(text(3));
    r.setIdType(text(4));
    r.setIdNumber(text(5));
    r.setAccountType(text(6));
    r.setAddress(text(7));
    r.setInitialDeposit(sqlite3_column_double(stmt, 8));
    r.setStatus(text(9));
    r.setRequestedBy(text(10));
    r.setCreatedAt(text(11));
    r.setReviewedAt(text(12));
    r.setReviewedBy(text(13));
    r.setRejectionReason(text(14));
    return r;
}

std::vector<AccountRequest> Database::getPendingRequests() {
    std::vector<AccountRequest> list;
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT id, holder_name, phone, email, id_type, id_number, "
        "account_type, address, initial_deposit, status, requested_by, "
        "created_at, reviewed_at, reviewed_by, rejection_reason "
        "FROM account_requests WHERE status='pending' ORDER BY id DESC";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return list;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        list.push_back(rowToRequest(stmt));
    sqlite3_finalize(stmt);
    return list;
}

std::vector<AccountRequest> Database::getAllRequests() {
    std::vector<AccountRequest> list;
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT id, holder_name, phone, email, id_type, id_number, "
        "account_type, address, initial_deposit, status, requested_by, "
        "created_at, reviewed_at, reviewed_by, rejection_reason "
        "FROM account_requests ORDER BY id DESC LIMIT 50";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return list;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        list.push_back(rowToRequest(stmt));
    sqlite3_finalize(stmt);
    return list;
}

bool Database::approveRequest(int id, const std::string& reviewedBy) {
    // Fetch request
    sqlite3_stmt* stmt;
    std::string sel =
        "SELECT id, holder_name, phone, email, id_type, id_number, "
        "account_type, address, initial_deposit, status, requested_by, "
        "created_at, reviewed_at, reviewed_by, rejection_reason "
        "FROM account_requests WHERE id=?";
    if (sqlite3_prepare_v2(db_, sel.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return false; }
    AccountRequest req = rowToRequest(stmt);
    sqlite3_finalize(stmt);

    std::string ts = currentTimestamp();
    std::string accNum = generateAccountNumber();
    std::string cardNum = generateCardNumber();

    // Create account
    sqlite3_stmt* ins;
    std::string accSql =
        "INSERT INTO accounts (account_number, holder_name, phone, email, "
        "id_type, id_number, account_type, address, balance, status, created_by) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, accSql.c_str(), -1, &ins, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(ins, 1, accNum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 2, req.getHolderName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 3, req.getPhone().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 4, req.getEmail().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 5, req.getIdType().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 6, req.getIdNumber().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 7, req.getAccountType().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 8, req.getAddress().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(ins, 9, req.getInitialDeposit());
    sqlite3_bind_text(ins, 10, "active", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins, 11, req.getRequestedBy().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(ins);
    sqlite3_finalize(ins);

    // Create card (expiry 3 years from now)
    std::string expiry = "12/28"; // simplified
    sqlite3_stmt* cins;
    std::string cardSql =
        "INSERT INTO cards (account_number, card_number, expiry_date, cvv, card_type) "
        "VALUES (?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, cardSql.c_str(), -1, &cins, nullptr) == SQLITE_OK) {
        std::string cvv = std::to_string(100 + id % 900);
        sqlite3_bind_text(cins, 1, accNum.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cins, 2, cardNum.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cins, 3, expiry.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(cins, 4, cvv.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cins, 5, "debit", -1, SQLITE_STATIC);
        sqlite3_step(cins);
        sqlite3_finalize(cins);
    }

    // Initial deposit transaction
    if (req.getInitialDeposit() > 0) {
        recordTransaction(accNum, "initial_deposit", req.getInitialDeposit(),
                          req.getInitialDeposit(), reviewedBy);
    }

    // Update request status
    sqlite3_stmt* upd;
    std::string updSql =
        "UPDATE account_requests SET status='approved', reviewed_at=?, "
        "reviewed_by=? WHERE id=?";
    if (sqlite3_prepare_v2(db_, updSql.c_str(), -1, &upd, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(upd, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(upd, 2, reviewedBy.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(upd, 3, id);
    bool ok = sqlite3_step(upd) == SQLITE_DONE;
    sqlite3_finalize(upd);
    return ok;
}

bool Database::rejectRequest(int id, const std::string& reviewedBy,
                              const std::string& reason) {
    sqlite3_stmt* stmt;
    std::string sql =
        "UPDATE account_requests SET status='rejected', reviewed_at=?, "
        "reviewed_by=?, rejection_reason=? WHERE id=?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    std::string ts = currentTimestamp();
    sqlite3_bind_text(stmt, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, reviewedBy.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, reason.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

// ─── Accounts ────────────────────────────────────────────────────────────────

std::shared_ptr<Account> Database::getAccountByNumber(const std::string& num) {
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT account_number, holder_name, phone, email, id_type, id_number, "
        "account_type, address, balance, status, created_by, created_at "
        "FROM accounts WHERE account_number=?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;
    sqlite3_bind_text(stmt, 1, num.c_str(), -1, SQLITE_STATIC);

    std::shared_ptr<Account> acc;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        acc = std::make_shared<Account>();
        auto text = [&](int c) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, c);
            return t ? t : "";
        };
        acc->setAccountNumber(text(0));
        acc->setHolderName(text(1));
        acc->setPhone(text(2));
        acc->setEmail(text(3));
        acc->setIdType(text(4));
        acc->setIdNumber(text(5));
        acc->setAccountType(text(6));
        acc->setAddress(text(7));
        acc->setBalance(sqlite3_column_double(stmt, 8));
        acc->setStatus(text(9));
        acc->setCreatedBy(text(10));
        acc->setCreatedAt(text(11));
    }
    sqlite3_finalize(stmt);
    return acc;
}

std::vector<Account> Database::searchAccounts(const std::string& query) {
    std::vector<Account> list;
    sqlite3_stmt* stmt;
    std::string pattern = "%" + query + "%";
    std::string sql =
        "SELECT account_number, holder_name, phone, email, id_type, id_number, "
        "account_type, address, balance, status, created_by, created_at "
        "FROM accounts WHERE "
        "holder_name LIKE ? OR phone LIKE ? OR email LIKE ? OR "
        "id_number LIKE ? OR account_number LIKE ? LIMIT 20";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return list;
    for (int i = 1; i <= 5; i++)
        sqlite3_bind_text(stmt, i, pattern.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Account acc;
        auto text = [&](int c) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, c);
            return t ? t : "";
        };
        acc.setAccountNumber(text(0));
        acc.setHolderName(text(1));
        acc.setPhone(text(2));
        acc.setEmail(text(3));
        acc.setIdType(text(4));
        acc.setIdNumber(text(5));
        acc.setAccountType(text(6));
        acc.setAddress(text(7));
        acc.setBalance(sqlite3_column_double(stmt, 8));
        acc.setStatus(text(9));
        acc.setCreatedBy(text(10));
        acc.setCreatedAt(text(11));
        list.push_back(acc);
    }
    sqlite3_finalize(stmt);
    return list;
}

bool Database::updateBalance(const std::string& accountNum, double newBalance) {
    sqlite3_stmt* stmt;
    std::string sql = "UPDATE accounts SET balance=? WHERE account_number=?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_double(stmt, 1, newBalance);
    sqlite3_bind_text(stmt, 2, accountNum.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::closeAccount(const std::string& accountNum,
                             const std::string& closedBy) {
    sqlite3_stmt* stmt;
    std::string sql =
        "UPDATE accounts SET status='closed' WHERE account_number=?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, accountNum.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (ok) {
        // Block card
        sqlite3_stmt* cs;
        std::string csql =
            "UPDATE cards SET status='blocked' WHERE account_number=?";
        if (sqlite3_prepare_v2(db_, csql.c_str(), -1, &cs, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(cs, 1, accountNum.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(cs);
            sqlite3_finalize(cs);
        }
    }
    return ok;
}

// ─── Transactions ─────────────────────────────────────────────────────────────

bool Database::recordTransaction(const std::string& accountNum,
                                  const std::string& type,
                                  double amount, double balAfter,
                                  const std::string& performedBy) {
    sqlite3_stmt* stmt;
    std::string sql =
        "INSERT INTO transactions (account_number, type, amount, "
        "balance_after, performed_by) VALUES (?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, accountNum.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, amount);
    sqlite3_bind_double(stmt, 4, balAfter);
    sqlite3_bind_text(stmt, 5, performedBy.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<json> Database::getRecentTransactions(int limit) {
    std::vector<json> list;
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT t.id, t.account_number, a.holder_name, t.type, "
        "t.amount, t.balance_after, t.performed_by, t.created_at "
        "FROM transactions t "
        "LEFT JOIN accounts a ON t.account_number = a.account_number "
        "ORDER BY t.id DESC LIMIT ?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return list;
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto text = [&](int c) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, c);
            return t ? t : "";
        };
        json tx;
        tx["id"] = sqlite3_column_int(stmt, 0);
        tx["account_number"] = text(1);
        tx["holder_name"] = text(2);
        tx["type"] = text(3);
        tx["amount"] = sqlite3_column_double(stmt, 4);
        tx["balance_after"] = sqlite3_column_double(stmt, 5);
        tx["performed_by"] = text(6);
        tx["created_at"] = text(7);
        list.push_back(tx);
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<json> Database::getTransactionsByAccount(
    const std::string& accountNum) {
    std::vector<json> list;
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT t.id, t.account_number, a.holder_name, t.type, t.amount, "
        "t.balance_after, t.performed_by, t.created_at "
        "FROM transactions t "
        "LEFT JOIN accounts a ON t.account_number = a.account_number "
        "WHERE t.account_number=? ORDER BY t.id DESC LIMIT 20";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return list;
    sqlite3_bind_text(stmt, 1, accountNum.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto text = [&](int c) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, c);
            return t ? t : "";
        };
        json tx;
        tx["id"] = sqlite3_column_int(stmt, 0);
        tx["account_number"] = text(1);
        tx["holder_name"] = text(2);
        tx["type"] = text(3);
        tx["amount"] = sqlite3_column_double(stmt, 4);
        tx["balance_after"] = sqlite3_column_double(stmt, 5);
        tx["performed_by"] = text(6);
        tx["created_at"] = text(7);
        list.push_back(tx);
    }
    sqlite3_finalize(stmt);
    return list;
}

// ─── Cards ───────────────────────────────────────────────────────────────────

std::shared_ptr<Card> Database::getCardByAccount(const std::string& accountNum) {
    sqlite3_stmt* stmt;
    std::string sql =
        "SELECT id, account_number, card_number, expiry_date, cvv, "
        "card_type, status, issued_at FROM cards WHERE account_number=?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;
    sqlite3_bind_text(stmt, 1, accountNum.c_str(), -1, SQLITE_STATIC);

    std::shared_ptr<Card> card;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        card = std::make_shared<Card>();
        auto text = [&](int c) -> std::string {
            const char* t = (const char*)sqlite3_column_text(stmt, c);
            return t ? t : "";
        };
        card->setId(sqlite3_column_int(stmt, 0));
        card->setAccountNumber(text(1));
        card->setCardNumber(text(2));
        card->setExpiryDate(text(3));
        card->setCvvHash(text(4));
        card->setCardType(text(5));
        card->setStatus(text(6));
        card->setIssuedAt(text(7));
    }
    sqlite3_finalize(stmt);
    return card;
}

// ─── Stats ───────────────────────────────────────────────────────────────────

json Database::getDashboardStats() {
    json stats;
    sqlite3_stmt* stmt;

    // Total accounts
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM accounts", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["total_accounts"] = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // Active accounts
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM accounts WHERE status='active'",
                       -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["active_accounts"] = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // Total transactions
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM transactions", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["total_transactions"] = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // Total deposits
    sqlite3_prepare_v2(db_,
        "SELECT COALESCE(SUM(amount),0) FROM transactions WHERE type='deposit' OR type='initial_deposit'",
        -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["total_deposits"] = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);

    // Total withdrawals
    sqlite3_prepare_v2(db_,
        "SELECT COALESCE(SUM(amount),0) FROM transactions WHERE type='withdraw'",
        -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["total_withdrawals"] = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);

    // Pending requests
    sqlite3_prepare_v2(db_,
        "SELECT COUNT(*) FROM account_requests WHERE status='pending'",
        -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["pending_requests"] = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // Staff count
    sqlite3_prepare_v2(db_,
        "SELECT COUNT(*) FROM users WHERE role='staff'",
        -1, &stmt, nullptr);
    sqlite3_step(stmt);
    stats["staff_count"] = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return stats;
}
