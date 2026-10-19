cmake_minimum_required(VERSION 3.10)

project(yutovo-server
    VERSION 0.1.0 
    LANGUAGES CXX
)

set(PROJECT_LICENSE "AGPL-3.0-only")
message(STATUS "CXX Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Project license: ${PROJECT_LICENSE}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
else()
    add_compile_definitions(RELEASE)
endif()

if(NOT DEFINED CACHE{TEST})
    message(FATAL_ERROR "The TEST parameter is mandatory: pass -DTEST=ON (test build: captcha and email-code checks disabled) "
        "or -DTEST=OFF (production build with all checks enabled)")
endif()

option(TEST "Build unit tests" ON)

# set(REMOTE_SOLVER true)
# add_compile_definitions(REMOTE_SOLVER)

set(INSTALL_PATH "$ENV{YUTOVO_DEPLOY}")
set(THIRD_PARTY_PATH "$ENV{YUTOVO_DEPLOY}/../third-party/")

if(TEST)
    message(WARNING "TEST=ON: captcha and email-code verification are DISABLED, /auth/set-params is exposed. Never deploy this build publicly.")
    add_compile_definitions(TEST)
endif()

add_subdirectory(src)
if(TEST)
    add_subdirectory(test)
endif()
