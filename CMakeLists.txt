cmake_minimum_required(VERSION 3.19)
project(DocumentSearchEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Sql Widgets Concurrent)
qt_standard_project_setup()

add_library(search_common STATIC
    src/common/common.cpp
    src/common/common.h
)
target_include_directories(search_common PUBLIC src/common)
target_link_libraries(search_common PUBLIC Qt6::Core Qt6::Sql)
target_compile_definitions(search_common PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

qt_add_executable(indexer src/indexer/indexer.cpp)
target_link_libraries(indexer PRIVATE search_common)

qt_add_executable(searcher WIN32
    src/searcher/main.cpp
    src/searcher/mainwindow.cpp
    src/searcher/mainwindow.h
    src/searcher/mainwindow.ui
)
target_link_libraries(searcher PRIVATE search_common Qt6::Widgets Qt6::Concurrent)
