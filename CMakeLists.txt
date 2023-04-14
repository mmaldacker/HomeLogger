cmake_minimum_required(VERSION 3.14)

project(HomeLogger LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        master)
FetchContent_MakeAvailable(crow)

add_library(SQLite3 STATIC
        sqlite3ext.h
        sqlite3.h
        sqlite3.c)

if (UNIX)
    target_link_libraries(SQLite3 PRIVATE ${CMAKE_DL_LIBS})
endif (UNIX)


FetchContent_Declare(date
        GIT_REPOSITORY https://github.com/HowardHinnant/date.git
        GIT_TAG        master)
FetchContent_MakeAvailable(date)

add_executable(home_logger
    main.cpp
    influxdb.hpp
    sqlite_orm/sqlite_orm.h
        )

target_link_libraries(home_logger PRIVATE Crow::Crow SQLite3 date::date)
target_include_directories(home_logger PRIVATE .)