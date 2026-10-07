cmake_minimum_required(VERSION 3.21)
project(MiChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network)

add_executable(MiChat WIN32
    src/main.cpp
    src/Database.cpp
    src/Database.h
    src/LinkFetcher.cpp
    src/LinkFetcher.h
    src/ChatInput.h
    src/MainWindow.cpp
    src/MainWindow.h
)
target_link_libraries(MiChat PRIVATE Qt6::Widgets Qt6::Sql Qt6::Network)
