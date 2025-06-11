include(FetchContent)

cmake_minimum_required(VERSION 3.14)

project(sc-trace-debugger LANGUAGES CXX)

cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRC_CPP_FILES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE HPP_FILES CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE TESTS_CPP_FILES CONFIGURE_DEPENDS tests/*.cpp)
set(SRC_CPP_FILES_NO_CLI ${SRC_CPP_FILES})
list(REMOVE_ITEM SRC_CPP_FILES_NO_CLI "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_executable(${PROJECT_NAME} ${SRC_CPP_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 6910c9d9165801d8827d628cb72eb7ea9dd538c5
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(tests ${SRC_CPP_FILES_NO_CLI} ${TESTS_CPP_FILES})
target_include_directories(tests PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(tests PRIVATE gtest_main)
target_compile_options(tests PRIVATE -fsanitize=address -fsanitize=undefined)
target_link_options(tests PRIVATE -fsanitize=address -fsanitize=undefined)
