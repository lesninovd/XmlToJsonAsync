cmake_minimum_required(VERSION 2.8)

project(XmlToJsonAsync)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
file(GLOB XmlToJsonAsync_sources *.cpp *.h *.hpp)

find_package(Boost 1.65.1 REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} ${XmlToJsonAsync_sources})

target_link_libraries (${PROJECT_NAME} ${Boost_LIBRARIES} -lpthread)
