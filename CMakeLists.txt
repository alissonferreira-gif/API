cmake_minimum_required(VERSION 3.22)
project(AlissonAsk VERSION 0.7 LANGUAGES CXX ASM_NASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    -O2
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_FLAGS "-f elf64")

set(SOURCES
    main.cpp
    chatbot_engine.cpp
    chatbot_engine.hpp
    conversation_manager.cpp
    conversation_manager.hpp
    gemini_client.cpp
    gemini_client.hpp
    asm_utils.hpp
    intent_classifier.hpp
    sentiment_analyzer.hpp
    rate_limiter.hpp
    thread_pool.hpp
    gamification.hpp
    geolocation.hpp
    sqlite_database.hpp
    notification_scheduler.hpp
    idatabase.hpp
    response_cache.hpp
    api_key_manager.hpp
)

set(ASM_SOURCES
    asm_utils.asm
)

add_executable(AlissonAsk
    ${SOURCES}
    ${ASM_SOURCES}
)

target_include_directories(AlissonAsk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)


find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
target_include_directories(AlissonAsk PRIVATE ${SQLITE3_INCLUDE_DIRS})
target_link_libraries(AlissonAsk PRIVATE ${SQLITE3_LIBRARIES})

pkg_check_modules(HTTPLIB REQUIRED cpp-httplib)
target_compile_options(AlissonAsk PRIVATE ${HTTPLIB_CFLAGS_OTHER})
target_link_libraries(AlissonAsk PRIVATE ${HTTPLIB_LIBRARIES})

find_package(ZLIB REQUIRED)
target_link_libraries(AlissonAsk PRIVATE ZLIB::ZLIB)

find_package(CURL REQUIRED)
target_link_libraries(AlissonAsk PRIVATE CURL::libcurl)

find_package(Threads REQUIRED)
target_link_libraries(AlissonAsk PRIVATE Threads::Threads)

find_package(OpenSSL REQUIRED)
target_link_libraries(AlissonAsk PRIVATE OpenSSL::SSL OpenSSL::Crypto)

target_compile_definitions(AlissonAsk PRIVATE
    ALISONASK_VERSION="0.7"
)

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════╗")
message(STATUS "║  AlissonAsk V0.7 — Build configurado    ║")
message(STATUS "╚══════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Dependências necessárias (Ubuntu/Debian):")
message(STATUS "  sudo apt install nasm libsqlite3-dev libcurl4-openssl-dev")
message(STATUS "  sudo apt install libssl-dev zlib1g-dev")
message(STATUS "")
message(STATUS "Build:")
message(STATUS "  cmake -B build -DCMAKE_BUILD_TYPE=Release")
message(STATUS "  cmake --build build -j$(nproc)")
message(STATUS "")
message(STATUS "Variáveis de ambiente necessárias:")
message(STATUS "  GEMINI_KEY_1  .. GEMINI_KEY_10  (ou GEMINI_API_KEY)")
message(STATUS "  WA_PHONE_NUMBER_ID, WA_ACCESS_TOKEN, WA_VERIFY_TOKEN")
message(STATUS "  DB_PATH         (padrão: alisonask.db)")
message(STATUS "  ADMIN_TOKEN     (padrão: admin123 — MUDE EM PRODUÇÃO)")
message(STATUS "")
