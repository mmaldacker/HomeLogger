cmake_minimum_required(VERSION 3.14)

project(HomeLogger LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        v1.0+1)
FetchContent_MakeAvailable(crow)

add_library(SQLite3 STATIC
        sqlite3ext.h
        sqlite3.h
        sqlite3.c)

add_executable(home_logger
    main.cpp
    sqlite_wrapper.cpp
    sqlite_wrapper.h
    influxdb.hpp)

target_link_libraries(home_logger PRIVATE Crow::Crow SQLite3)