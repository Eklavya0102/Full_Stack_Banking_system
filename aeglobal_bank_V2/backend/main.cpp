// ── Windows defines MUST be first — before any header pulls in windows.h ──
#ifdef _WIN32
  #define _WIN32_WINNT  0x0A00   // Windows 10+ (required by cpp-httplib)
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif

// cpp-httplib: single-header, uses WinSock2 directly — no ASIO, no IPv6 issues
#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <httplib.h>

#include <nlohmann/json.hpp>
#include "Database.hpp"
#include "Account.hpp"
#include "AccountRequest.hpp"
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

using json = nlohmann::json;

// ─── Global DB ────────────────────────────────────────────────────────────────
std::unique_ptr<Database> db;

// ─── Helpers ──────────────────────────────────────────────────────────────────
void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers",
                   "Content-Type,Authorization,X-User-Email,X-User-Role");
    res.set_header("Access-Control-Allow-Methods",
                   "GET,POST,PUT,DELETE,OPTIONS");
}

void sendJSON(httplib::Response& res, int status, const json& body) {
    setCORS(res);
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

void ok(httplib::Response& res, const json& body)  { sendJSON(res, 200, body); }
void bad(httplib::Response& res, int code, const std::string& msg) {
    sendJSON(res, code, {{"error", msg}});
}

std::string hdr(const httplib::Request& req, const std::string& key) {
    return req.has_header(key) ? req.get_header_value(key) : "";
}
std::string param(const httplib::Request& req, const std::string& key) {
    return req.has_param(key) ? req.get_param_value(key) : "";
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    db = std::make_unique<Database>("banking.db");
    if (!db->open())      { std::cerr << "DB open failed\n";   return 1; }
    if (!db->initSchema()) { std::cerr << "Schema init failed\n"; return 1; }
    db->seedData();

    httplib::Server svr;

    // ── CORS preflight for every route ───────────────────────────────────────
    svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.status = 204;
    });

    // ── POST /api/login ───────────────────────────────────────────────────────
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body     = json::parse(req.body);
            std::string email    = body.value("email", "");
            std::string password = body.value("password", "");
            std::string role     = body.value("role", "");
            if (email.empty()||password.empty()||role.empty()) {
                bad(res,400,"Email, password, and role are required"); return;
            }
            auto user = db->authenticate(email, password, role);
            if (!user) { bad(res,401,"Invalid credentials"); return; }
            json r;
            r["success"] = true;
            r["user"]["id"]    = user->getId();
            r["user"]["name"]  = user->getName();
            r["user"]["email"] = user->getEmail();
            r["user"]["role"]  = user->getRole();
            r["token"] = "ae-" + std::to_string(user->getId()) + "-" + role;
            ok(res, r);
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── GET /api/stats ────────────────────────────────────────────────────────
    svr.Get("/api/stats", [](const httplib::Request&, httplib::Response& res) {
        ok(res, {{"success",true},{"stats", db->getDashboardStats()}});
    });

    // ── GET /api/admin/staff ──────────────────────────────────────────────────
    svr.Get("/api/admin/staff", [](const httplib::Request& req, httplib::Response& res) {
        if (hdr(req,"X-User-Role")!="admin") { bad(res,403,"Admin only"); return; }
        auto staff = db->getAllStaff();
        json arr = json::array();
        for (auto& s : staff)
            arr.push_back({{"id",s.getId()},{"name",s.getName()},
                           {"email",s.getEmail()},{"branch",s.getBranch()},
                           {"created_by",s.getCreatedBy()}});
        ok(res, {{"success",true},{"staff",arr}});
    });

    // ── POST /api/admin/staff ─────────────────────────────────────────────────
    svr.Post("/api/admin/staff", [](const httplib::Request& req, httplib::Response& res) {
        if (hdr(req,"X-User-Role")!="admin") { bad(res,403,"Admin only"); return; }
        try {
            auto body = json::parse(req.body);
            std::string name   = body.value("name","");
            std::string email  = body.value("email","");
            std::string pw     = body.value("password","");
            std::string branch = body.value("branch","");
            if (name.empty()||email.empty()||pw.empty()) {
                bad(res,400,"Name, email and password required"); return;
            }
            if (!db->createStaff(name,email,pw,branch,hdr(req,"X-User-Email")))
                { bad(res,409,"Email already exists"); return; }
            ok(res, {{"success",true},{"message","Staff created"}});
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── GET /api/admin/requests ───────────────────────────────────────────────
    svr.Get("/api/admin/requests", [](const httplib::Request& req, httplib::Response& res) {
        if (hdr(req,"X-User-Role")!="admin") { bad(res,403,"Admin only"); return; }
        std::string filter = param(req,"filter");
        if (filter.empty()) filter = "pending";
        auto reqs = (filter=="all") ? db->getAllRequests() : db->getPendingRequests();
        json arr = json::array();
        for (auto& r : reqs)
            arr.push_back({{"id",r.getId()},{"holder_name",r.getHolderName()},
                {"phone",r.getPhone()},{"email",r.getEmail()},
                {"id_type",r.getIdType()},{"id_number",r.getIdNumber()},
                {"account_type",r.getAccountType()},{"address",r.getAddress()},
                {"initial_deposit",r.getInitialDeposit()},{"status",r.getStatus()},
                {"requested_by",r.getRequestedBy()},{"created_at",r.getCreatedAt()},
                {"reviewed_at",r.getReviewedAt()},{"reviewed_by",r.getReviewedBy()},
                {"rejection_reason",r.getRejectionReason()}});
        ok(res, {{"success",true},{"requests",arr}});
    });

    // ── POST /api/admin/approve/:id ───────────────────────────────────────────
    svr.Post(R"(/api/admin/approve/(\d+))",
    [](const httplib::Request& req, httplib::Response& res) {
        if (hdr(req,"X-User-Role")!="admin") { bad(res,403,"Admin only"); return; }
        int id = std::stoi(req.matches[1].str());
        if (!db->approveRequest(id, hdr(req,"X-User-Email")))
            { bad(res,400,"Could not approve"); return; }
        ok(res, {{"success",true},{"message","Approved — account & card created"}});
    });

    // ── POST /api/admin/reject/:id ────────────────────────────────────────────
    svr.Post(R"(/api/admin/reject/(\d+))",
    [](const httplib::Request& req, httplib::Response& res) {
        if (hdr(req,"X-User-Role")!="admin") { bad(res,403,"Admin only"); return; }
        int id = std::stoi(req.matches[1].str());
        try {
            auto body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string reason = body.value("reason","No reason provided");
            if (!db->rejectRequest(id, hdr(req,"X-User-Email"), reason))
                { bad(res,400,"Could not reject"); return; }
            ok(res, {{"success",true},{"message","Request rejected"}});
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── DELETE /api/admin/staff/:id ───────────────────────────────────────────
    svr.Delete(R"(/api/admin/staff/(\d+))",
    [](const httplib::Request& req, httplib::Response& res) {
        if (hdr(req,"X-User-Role")!="admin") { bad(res,403,"Admin only"); return; }
        int id = std::stoi(req.matches[1].str());
        if (!db->deleteStaff(id))
            { bad(res,404,"Staff not found or could not be deleted"); return; }
        ok(res, {{"success",true},{"message","Staff member removed"}});
    });

    // ── POST /api/account/request ─────────────────────────────────────────────
    svr.Post("/api/account/request",
    [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            AccountRequest r;
            r.setHolderName(body.value("holder_name",""));
            r.setPhone(body.value("phone",""));
            r.setEmail(body.value("email",""));
            r.setIdType(body.value("id_type",""));
            r.setIdNumber(body.value("id_number",""));
            r.setAccountType(body.value("account_type","savings"));
            r.setAddress(body.value("address",""));
            r.setInitialDeposit(body.value("initial_deposit",0.0));
            r.setRequestedBy(hdr(req,"X-User-Email"));
            if (r.getHolderName().empty()||r.getPhone().empty()||
                r.getIdType().empty()||r.getIdNumber().empty())
                { bad(res,400,"Required fields missing"); return; }
            int id = db->createAccountRequest(r);
            if (id<0) { bad(res,500,"DB error"); return; }
            ok(res, {{"success",true},{"request_id",id},
                     {"message","Request submitted for admin approval"}});
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── GET /api/account/search ───────────────────────────────────────────────
    svr.Get("/api/account/search",
    [](const httplib::Request& req, httplib::Response& res) {
        std::string q = param(req,"q");
        if (q.empty()) { bad(res,400,"Query required"); return; }
        auto accs = db->searchAccounts(q);
        json arr = json::array();
        for (auto& a : accs)
            arr.push_back({{"account_number",a.getAccountNumber()},
                {"holder_name",a.getHolderName()},{"phone",a.getPhone()},
                {"email",a.getEmail()},{"id_type",a.getIdType()},
                {"id_number",a.getIdNumber()},{"account_type",a.getAccountType()},
                {"address",a.getAddress()},
                {"balance",a.getBalance()},{"status",a.getStatus()},
                {"created_at",a.getCreatedAt()}});
        ok(res, {{"success",true},{"accounts",arr},{"count",(int)arr.size()}});
    });

    // ── GET /api/account/balance ──────────────────────────────────────────────
    svr.Get("/api/account/balance",
    [](const httplib::Request& req, httplib::Response& res) {
        std::string num     = param(req,"account_number");
        std::string cardNum = param(req,"card_number");
        std::shared_ptr<Account> acc;
        if (!num.empty()) {
            acc = db->getAccountByNumber(num);
        } else if (!cardNum.empty()) {
            acc = db->getAccountByCardNumber(cardNum);
        } else {
            bad(res,400,"Account number or card number required"); return;
        }
        if (!acc) { bad(res,404,"Account not found"); return; }
        auto card = db->getCardByAccount(acc->getAccountNumber());
        json r;
        r["success"] = true;
        r["account"]["account_number"] = acc->getAccountNumber();
        r["account"]["holder_name"]    = acc->getHolderName();
        r["account"]["phone"]          = acc->getPhone();
        r["account"]["email"]          = acc->getEmail();
        r["account"]["id_type"]        = acc->getIdType();
        r["account"]["id_number"]      = acc->getIdNumber();
        r["account"]["address"]        = acc->getAddress();
        r["account"]["balance"]        = acc->getBalance();
        r["account"]["status"]         = acc->getStatus();
        r["account"]["account_type"]   = acc->getAccountType();
        if (card) {
            r["card"]["card_number"]   = card->getCardNumber();
            r["card"]["masked_number"] = card->getCardNumberMasked();
            r["card"]["expiry"]        = card->getExpiryDate();
            r["card"]["type"]          = card->getCardType();
            r["card"]["status"]        = card->getStatus();
            r["card"]["valid"]         = card->isValid();
        } else { r["card"] = nullptr; }
        ok(res, r);
    });

    // ── POST /api/transaction/deposit ─────────────────────────────────────────
    svr.Post("/api/transaction/deposit",
    [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string num  = body.value("account_number","");
            double      amt  = body.value("amount",0.0);
            std::string by   = hdr(req,"X-User-Email");
            if (num.empty())  { bad(res,400,"Account number required"); return; }
            if (amt<=0)       { bad(res,400,"Amount must be > 0");      return; }
            auto acc = db->getAccountByNumber(num);
            if (!acc)                      { bad(res,404,"Not found");       return; }
            if (acc->getStatus()!="active"){ bad(res,400,"Account inactive"); return; }
            double nb = acc->getBalance()+amt;
            db->updateBalance(num,nb);
            db->recordTransaction(num,"deposit",amt,nb,by);
            ok(res,{{"success",true},{"message","Deposit successful"},
                    {"new_balance",nb},{"account_number",num}});
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── POST /api/transaction/withdraw ────────────────────────────────────────
    svr.Post("/api/transaction/withdraw",
    [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string num  = body.value("account_number","");
            double      amt  = body.value("amount",0.0);
            std::string by   = hdr(req,"X-User-Email");
            if (num.empty()) { bad(res,400,"Account number required"); return; }
            if (amt<=0)      { bad(res,400,"Amount must be > 0");      return; }
            auto acc = db->getAccountByNumber(num);
            if (!acc)                      { bad(res,404,"Not found");          return; }
            if (acc->getStatus()!="active"){ bad(res,400,"Account inactive");   return; }
            if (acc->getBalance()<amt)     { bad(res,400,"Insufficient funds"); return; }
            double nb = acc->getBalance()-amt;
            db->updateBalance(num,nb);
            db->recordTransaction(num,"withdraw",amt,nb,by);
            ok(res,{{"success",true},{"message","Withdrawal successful"},
                    {"new_balance",nb},{"account_number",num}});
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── GET /api/transactions ─────────────────────────────────────────────────
    svr.Get("/api/transactions",
    [](const httplib::Request& req, httplib::Response& res) {
        std::string num = param(req,"account_number");
        json txs = num.empty() ? db->getRecentTransactions(20)
                               : db->getTransactionsByAccount(num);
        ok(res, {{"success",true},{"transactions",txs}});
    });

    // ── POST /api/account/close ───────────────────────────────────────────────
    svr.Post("/api/account/close",
    [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string num = body.value("account_number","");
            if (num.empty()) { bad(res,400,"Account number required"); return; }
            auto acc = db->getAccountByNumber(num);
            if (!acc)                       { bad(res,404,"Not found");         return; }
            if (acc->getStatus()=="closed") { bad(res,400,"Already closed");    return; }
            if (!db->closeAccount(num, hdr(req,"X-User-Email")))
                { bad(res,500,"Failed to close"); return; }
            ok(res,{{"success",true},{"message","Account closed"}});
        } catch(...) { bad(res,400,"Invalid JSON"); }
    });

    // ── GET /api/account/card ─────────────────────────────────────────────────
    svr.Get("/api/account/card",
    [](const httplib::Request& req, httplib::Response& res) {
        std::string num = param(req,"account_number");
        if (num.empty()) { bad(res,400,"Account number required"); return; }
        auto card = db->getCardByAccount(num);
        if (!card) { bad(res,404,"No card found"); return; }
        ok(res,{{"success",true},{"card",{
            {"card_number",card->getCardNumber()},
            {"masked_number",card->getCardNumberMasked()},
            {"expiry",card->getExpiryDate()},
            {"type",card->getCardType()},
            {"status",card->getStatus()},
            {"valid",card->isValid()}}}});
    });

    // ── Start server ──────────────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << "+========================================+\n";
    std::cout << "|   AE Global Bank — India (INR)         |\n";
    std::cout << "|   Powered by cpp-httplib (WinSock2)    |\n";
    std::cout << "|   http://localhost:<port>              |\n";
    std::cout << "+========================================+\n";
    std::cout << "  Press Ctrl+C to stop.\n\n";

    // Try ports in order until one is free
    std::vector<int> ports = {8080, 8081, 8082, 3000};
    int bound_port = -1;

    for (int port : ports) {
        std::cout << "  Trying port " << port << "...\n";
        // Test if port is free by attempting to bind
        svr.set_pre_routing_handler(nullptr); // no-op reset
        // cpp-httplib listen() is blocking, so we use a non-blocking check first
        SOCKET test_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (test_sock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons((u_short)port);
            addr.sin_addr.s_addr = INADDR_ANY;
            int opt = 1;
            setsockopt(test_sock, SOL_SOCKET, SO_REUSEADDR,
                       (const char*)&opt, sizeof(opt));
            bool available = (bind(test_sock,
                                   (sockaddr*)&addr, sizeof(addr)) == 0);
            closesocket(test_sock);
            if (available) {
                bound_port = port;
                break;
            }
            std::cout << "  Port " << port << " is busy, trying next...\n";
        }
    }

    if (bound_port == -1) {
        std::cerr << "[ERROR] All ports (8080, 8081, 8082, 3000) are in use.\n";
        std::cerr << "        Free up one of these ports and try again.\n";
        return 1;
    }

    std::cout << "\n[+] Server starting on port " << bound_port << "\n";
    std::cout << "[+] Open in browser: build/frontend/index.html\n";
    std::cout << "[+] API base will be: http://localhost:" << bound_port << "/api\n\n";

    // Write the chosen port to a file so the frontend can read it
    {
        std::ofstream pf("port.txt");
        pf << bound_port;
    }

    if (!svr.listen("0.0.0.0", bound_port)) {
        std::cerr << "[ERROR] Failed to start server on port " << bound_port << "\n";
        return 1;
    }

    return 0;
}
