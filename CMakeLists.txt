cmake_minimum_required(VERSION 3.10)

project(yutovo-server
    VERSION 0.1.0 
    LANGUAGES CXX
)

set(PROJECT_LICENSE "AGPL-3.0-only")

message(STATUS "Project license: ${PROJECT_LICENSE}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
else()
    add_compile_definitions(RELEASE)
endif()

if (NOT DEFINED no-test)
    add_compile_definitions(TEST)
endif()

# set(REMOTE_SOLVER true)
# add_compile_definitions(REMOTE_SOLVER)

set(INSTALL_PATH "$ENV{YUTOVO_DEPLOY}")
set(THIRD_PARTY_PATH "$ENV{YUTOVO_DEPLOY}/../third-party/")

add_subdirectory(src)
add_subdirectory(test)
