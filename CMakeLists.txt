cmake_minimum_required(VERSION 3.10)

project(yutovo_server
    VERSION 0.1.0 
    LANGUAGES CXX
)

add_compile_definitions(DEBUG)
add_compile_definitions(TEST)
# set(REMOTE_SOLVER true)
# add_compile_definitions(REMOTE_SOLVER)

set(INSTALL_PATH "$ENV{YUTOVO_DEPLOY}")
set(THIRD_PARTY_PATH "$ENV{YUTOVO_DEPLOY}/../third_party/")

add_subdirectory(src)
add_subdirectory(test)
