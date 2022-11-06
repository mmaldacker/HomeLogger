cmake_minimum_required(VERSION 3.14)

project(HomeLogger)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        v1.0+1)
FetchContent_MakeAvailable(crow)

add_executable(home_logger
    main.cpp
)

target_link_libraries(home_logger PRIVATE crow)