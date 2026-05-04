#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
VENDOR_DIR="${SCRIPT_DIR}/vendor"

echo "+======================================================+"
echo "|   AE Global Bank -- MSYS2 UCRT64 Build              |"
echo "+======================================================+"

echo "[1/4] Installing packages..."
pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-sqlite3 \
    mingw-w64-ucrt-x86_64-nlohmann-json
echo "[+] Packages installed."

echo "[2/4] Checking httplib.h..."
mkdir -p "${VENDOR_DIR}"
if [ ! -s "${VENDOR_DIR}/httplib.h" ]; then
    echo "    Downloading httplib.h from GitHub..."
    curl -fsSL "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h" \
         -o "${VENDOR_DIR}/httplib.h"
    echo "[+] Downloaded: $(du -sh ${VENDOR_DIR}/httplib.h | cut -f1)"
else
    echo "[+] httplib.h already present ($(du -sh ${VENDOR_DIR}/httplib.h | cut -f1))"
fi

echo "[3/4] Building..."
rm -rf "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j$(nproc)
echo "[+] Build complete."

echo "[4/4] Copying DLLs..."
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libsqlite3-0.dll; do
    if [ -f "/ucrt64/bin/${dll}" ]; then
        cp "/ucrt64/bin/${dll}" "${BUILD_DIR}/"
        echo "  + ${dll}"
    fi
done

echo ""
echo "+======================================================+"
echo "|  SUCCESS!                                            |"
echo "|  Run:  cd build && ./banking_server.exe              |"
echo "|  Open: build/frontend/index.html in browser         |"
echo "|  Admin: admin@aeglobal.com / admin123                |"
echo "|  Staff: staff@aeglobal.com / staff123                |"
echo "+======================================================+"
