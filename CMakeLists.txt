cmake_minimum_required(VERSION 3.30)

project(proj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP OFF)

# 1. Find the packages
find_package(Boost CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(proj main.cpp)

# 2. Link the targets
# nlohmann_json handles its own include directories via the target.
target_link_libraries(proj
    PRIVATE
        Boost::boost
        nlohmann_json::nlohmann_json
)