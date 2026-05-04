# AE Global Bank — Full Stack Banking System

C++ backend · cpp-httplib · SQLite3 · Tailwind CSS frontend

---

## Why cpp-httplib instead of Crow?

Crow uses ASIO for networking. On Windows, ASIO tries to open a dual-stack
IPv4+IPv6 socket; if IPv6 is disabled at the OS level the acceptor crashes with
"local_endpoint: An invalid argument was supplied." cpp-httplib calls WinSock2
directly — no ASIO, no dual-stack, no crash.

---

## Project Structure

```
aeglobal_bank/
├── backend/
│   ├── include/
│   │   ├── User.hpp            Base class (OOP)
│   │   ├── Admin.hpp           Admin : User
│   │   ├── Staff.hpp           Staff : User
│   │   ├── Account.hpp         Account model
│   │   ├── Card.hpp            Card model
│   │   ├── AccountRequest.hpp  Request workflow
│   │   └── Database.hpp        SQLite3 interface
│   ├── src/
│   │   └── Database.cpp        Full SQLite3 CRUD
│   └── main.cpp                HTTP server + all routes
├── frontend/
│   ├── js/api.js               Shared fetch wrapper
│   ├── index.html              Login page
│   ├── admin/admin_dashboard.html
│   └── staff/staff_dashboard.html
├── CMakeLists.txt
├── build.sh                    One-command build
└── README.md
```

---

## Build Instructions (MSYS2 UCRT64)

Open the MSYS2 UCRT64 terminal (not MinGW64, not plain MSYS):

```bash
cd /d/Projects/aeglobal_bank     # adjust path to yours
bash build.sh
```

The script does everything:
1. Installs gcc, cmake, ninja, sqlite3, nlohmann-json, cpp-httplib via pacman
2. Cleans any old build directory
3. Configures and compiles
4. Copies runtime DLLs next to the exe

### Manual steps (if you prefer)

```bash
# Install packages
pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-sqlite3 \
    mingw-w64-ucrt-x86_64-nlohmann-json \
    mingw-w64-ucrt-x86_64-cpp-httplib

# Build
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Copy DLLs
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libsqlite3-0.dll; do
    cp /ucrt64/bin/$dll build/
done
```

---

## Running

```bash
cd build
./banking_server.exe
```

Expected output:
```
+========================================+
|   AE Global Bank Server v1.0           |
|   Powered by cpp-httplib (WinSock2)    |
|   http://localhost:8080                |
+========================================+
  Press Ctrl+C to stop.
```

Then open `build/frontend/index.html` in your browser.

---

## Default Credentials

| Role  | Email                  | Password |
|-------|------------------------|----------|
| Admin | admin@aeglobal.com     | admin123 |
| Staff | staff@aeglobal.com     | staff123 |

Five demo accounts (AE100001–AE100005) and three pending requests are
pre-seeded in the database on first run.

---

## API Routes

| Method | Path                        | Description                     |
|--------|-----------------------------|---------------------------------|
| POST   | /api/login                  | Authenticate, returns token     |
| GET    | /api/stats                  | Dashboard statistics            |
| GET    | /api/admin/staff            | List all staff                  |
| POST   | /api/admin/staff            | Create staff account            |
| GET    | /api/admin/requests         | Account opening requests        |
| POST   | /api/admin/approve/:id      | Approve request → create account|
| POST   | /api/admin/reject/:id       | Reject with reason              |
| POST   | /api/account/request        | Staff submits account request   |
| GET    | /api/account/search?q=      | Search by name/phone/email/ID   |
| GET    | /api/account/balance        | Balance + card status           |
| GET    | /api/account/card           | Card details                    |
| POST   | /api/transaction/deposit    | Credit account                  |
| POST   | /api/transaction/withdraw   | Debit account                   |
| GET    | /api/transactions           | Transaction history             |
| POST   | /api/account/close          | Close account + block card      |

---

## OOP Design

```
User  (abstract)
├── Admin  — canApproveRequests(), canCreateStaff()
└── Staff  — getBranch(), getCreatedBy()

Account        — deposit(), withdraw(), status management
Card           — getCardNumberMasked(), isValid()
AccountRequest — pending → approved / rejected
Database       — SQLite3 CRUD, seeding, stats
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `httplib.h not found` | `pacman -S mingw-w64-ucrt-x86_64-cpp-httplib` |
| Port 8080 already in use | `netstat -ano \| findstr 8080` in CMD, kill the PID |
| CORS error in browser | Open via `file://` not a live server, or run `npx serve build/` |
| DLL not found on double-click | Rerun build.sh — it copies the DLLs |
| `nlohmann/json.hpp not found` | `pacman -S mingw-w64-ucrt-x86_64-nlohmann-json` |
| `sqlite3.h not found` | `pacman -S mingw-w64-ucrt-x86_64-sqlite3` |


LICENSE:

MIT License

Copyright (c) [year] [your name]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software...

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
