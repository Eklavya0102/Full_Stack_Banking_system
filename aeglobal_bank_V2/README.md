# AE Global Bank — Banking Management System

A full-stack banking web application with an Admin Portal and Staff Portal. Built with a C++ HTTP backend (cpp-httplib + SQLite) and a pure HTML/JS/Tailwind CSS frontend.

---

## Project Structure

```
aeglobal_bank/
├── backend/
│   ├── include/
│   │   ├── Account.hpp          # Account model (with address field)
│   │   ├── AccountRequest.hpp   # Account request model
│   │   ├── Admin.hpp            # Admin model
│   │   ├── Card.hpp             # Card model (XXXX XXXX XXXX XXXX format)
│   │   ├── Database.hpp         # Database interface declarations
│   │   ├── Staff.hpp            # Staff model
│   │   └── User.hpp             # User model
│   ├── src/
│   │   └── Database.cpp         # All DB logic (SQLite)
│   └── main.cpp                 # HTTP server + all API endpoints
├── frontend/
│   ├── index.html               # Login page (Admin & Staff)
│   ├── js/
│   │   └── api.js               # Shared API client
│   ├── admin/
│   │   └── admin_dashboard.html # Admin Portal (single-page app)
│   └── staff/
│       └── staff_dashboard.html # Staff Portal (single-page app)
├── vendor/
│   └── httplib.h                # cpp-httplib header-only library
├── CMakeLists.txt
├── build.sh
└── README.md
```

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| GCC or Clang | 11+ / 13+ | Must support C++17 |
| CMake | 3.16+ | For CMake build method |
| SQLite3 | Any recent | libsqlite3-dev on Ubuntu |
| Browser | Chrome/Firefox/Edge | For the frontend |

### Install dependencies — Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential cmake libsqlite3-dev
```

### Install dependencies — macOS
```bash
brew install cmake sqlite3
```

---

## Build & Run

use MSYS2 UCRT62
### Option 1 — build.sh (Recommended)
```bash
bash build.sh
```

### Option 2 — CMake manually
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./aeglobal_bank_server
```

### Option 3 — Direct g++ compile
```bash
g++ -std=c++17 -O2 \
    backend/main.cpp backend/src/Database.cpp \
    -Ibackend/include -Ivendor \
    -lsqlite3 -lpthread \
    -o aeglobal_bank_server

./aeglobal_bank_server
```

The server starts on **port 8080** by default.

---

## Accessing the Application

| URL | Description |
|-----|-------------|
| http://localhost:8080/ | Login page |
| http://localhost:8080/admin/admin_dashboard.html | Admin Portal |
| http://localhost:8080/staff/staff_dashboard.html | Staff Portal |

---

## Default Credentials

A default admin is seeded on first run:

| Role | Email | Password |
|------|-------|----------|
| Admin | admin@aeglobal.com | admin123 |
  staff   staff@aeglobal.com   staff123
Staff accounts are created by the Admin inside the portal.

---

## Features

### Admin Portal
- **Dashboard** — Stats, recent 6 transactions (scrollable), quick actions
- **Create Staff** — Add staff with branch assignment
- **Staff List** — View all staff, delete staff with confirmation
- **Approve Account Requests** — Review, approve, or reject pending requests
- **Search Account** — Search by name, phone, email, ID, account number, or card number
- **View Balance & Card** — Lookup by account number OR card number; shows full holder details + card
- **Transactions** — Full history with account filter and staff name column
- **Close Account** — Verify balance and close account

### Staff Portal
- **Dashboard** — Quick action tiles, recent 6 transactions (scrollable)
- **Open Account** — Submit account requests for admin approval
- **Deposit / Withdrawal** — Perform transactions on active accounts
- **Search Account** — Search by any field including card number
- **Check Balance** — Lookup by account number OR card number
- **Transaction History** — Full history with staff name column
- **Close Account** — Submit closure requests

---

## API Reference

### Auth
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | /api/auth/login | Login, returns role + token |
| POST | /api/auth/logout | Logout |

### Admin
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | /api/admin/staff | Create staff account |
| GET | /api/admin/staff | List all staff |
| DELETE | /api/admin/staff/:id | Remove a staff member |
| GET | /api/admin/stats | Dashboard statistics |

### Account Requests
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | /api/account/request | Submit account opening request |
| GET | /api/account/requests?filter= | List requests (pending/all) |
| POST | /api/account/approve | Approve → creates account + card |
| POST | /api/account/reject | Reject with reason |

### Accounts
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | /api/account/search?q= | Search (name/phone/email/ID/acc no/card no) |
| GET | /api/account/balance?account_number= | Balance by account number |
| GET | /api/account/balance?card_number= | Balance by card number |
| GET | /api/account/card?account_number= | Card details |
| POST | /api/account/close | Close account |

### Transactions
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | /api/transaction/deposit | Deposit funds |
| POST | /api/transaction/withdraw | Withdraw funds |
| GET | /api/transactions?account_number= | Transaction history |

---

## Database

SQLite database file: **banking.db** (auto-created in working directory on first run).

<<<<<<< HEAD
### Tables
| Table | Description |
|-------|-------------|
| users | Admin and staff accounts |
| accounts | Customer accounts (holder name, phone, email, ID, address, balance) |
| account_requests | Pending/approved/rejected account opening requests |
| cards | Debit cards (format: XXXX XXXX XXXX XXXX), linked to accounts |
| transactions | All deposit/withdrawal records with staff email + resolved name |

> **Important:** Delete banking.db before first run when upgrading from an older version to avoid schema conflicts.

---

## Bug Fixes Applied

| # | Fix | Description |
|---|-----|-------------|
| 1 | SQLITE_TRANSIENT | All getter().c_str() bindings changed from SQLITE_STATIC to SQLITE_TRANSIENT — prevents dangling pointer UB that caused null/garbage data saved to DB |
| 2 | Card number format | Cards generated and stored as XXXX XXXX XXXX XXXX |
| 3 | Staff delete | Admin can remove staff with confirmation dialog |
| 4 | Address persisted | Address stored in accounts table on approval |
| 5 | Full data on approval | All holder fields (phone, email, ID, address) stored and shown everywhere |
| 6 | Staff name in transactions | performed_by email resolved to full name via JOIN on users table |
| 7 | Card number search | Search and balance lookup both accept card number input |
| 8 | Sidebar fixed | Sidebar is now sticky with fixed viewport height — does not stretch |
| 9 | Dashboard tx limit | Dashboard shows max 6 transactions; section is vertically scrollable |

---

## UI Layout Notes

- **Sidebar**: `sticky top-16 h-[calc(100vh-64px)] overflow-y-auto` — fixed to viewport, scrolls independently
- **Main content**: `h-[calc(100vh-64px)] overflow-y-auto` — scrolls independently of sidebar
- **Dashboard tx section**: `max-height: 360px; overflow-y: auto` — capped at ~6 rows, scrollable
- **Header**: `sticky top-0 z-50` — always visible at top
=======

LICENSE:

MIT License

Copyright (c) [year] [your name]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software...

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
>>>>>>> f0ca142d5778bd7be38c0b03a3e19d6901464f17
