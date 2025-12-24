cmake_minimum_required(VERSION 3.30)

project(proj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP OFF)


find_package(Boost CONFIG REQUIRED)

add_executable(proj main.cpp)

target_link_libraries(proj
    PRIVATE
        Boost::boost
)
