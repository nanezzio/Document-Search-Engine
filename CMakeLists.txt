cmake_minimum_required(VERSION 3.19)
project(DocumentSearchEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Sql Widgets)
qt_standard_project_setup()

add_library(search_common STATIC common.cpp common.h)
target_link_libraries(search_common PUBLIC Qt6::Core Qt6::Sql)
target_compile_definitions(search_common PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

qt_add_executable(indexer indexer.cpp)
target_link_libraries(indexer PRIVATE search_common)

qt_add_executable(searcher WIN32 main.cpp mainwindow.cpp mainwindow.h mainwindow.ui)
target_link_libraries(searcher PRIVATE search_common Qt6::Widgets)
