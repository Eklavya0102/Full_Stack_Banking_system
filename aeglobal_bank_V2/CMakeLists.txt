cmake_minimum_required(VERSION 3.16)
project(AEGlobalBank VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# nlohmann/json — from pacman
find_path(JSON_INCLUDE_DIR nlohmann/json.hpp
    HINTS
        "${CMAKE_SOURCE_DIR}/vendor"
        "/ucrt64/include"
        "C:/msys64/ucrt64/include"
)

# cpp-httplib — single header downloaded by build.sh into vendor/
# Check vendor/ first, then system include paths as fallback
find_path(HTTPLIB_INCLUDE_DIR httplib.h
    HINTS
        "${CMAKE_SOURCE_DIR}/vendor"
        "/ucrt64/include"
        "C:/msys64/ucrt64/include"
)

if(NOT HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR
        "\n[ERROR] httplib.h not found!\n"
        "Run build.sh — it downloads httplib.h into vendor/ automatically.\n"
        "Or manually: curl -fsSL https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -o vendor/httplib.h\n")
endif()

message(STATUS "  httplib.h : ${HTTPLIB_INCLUDE_DIR}/httplib.h")
message(STATUS "  JSON      : ${JSON_INCLUDE_DIR}")
message(STATUS "  SQLite3   : ${SQLite3_INCLUDE_DIRS}")

add_executable(banking_server
    backend/main.cpp
    backend/src/Database.cpp
)

target_include_directories(banking_server PRIVATE
    backend/include
    ${SQLite3_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
)

target_link_libraries(banking_server PRIVATE
    Threads::Threads
    SQLite3::SQLite3
    ws2_32
    wsock32
    mswsock
)

target_compile_definitions(banking_server PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_custom_command(TARGET banking_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/frontend"
        "$<TARGET_FILE_DIR:banking_server>/frontend"
    COMMENT "Copying frontend files..."
)
