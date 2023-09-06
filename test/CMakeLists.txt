cmake_minimum_required(VERSION 3.17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(GTest_DIR "/usr/lib/x86_64-linux-gnu/cmake/GTest")

enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})

find_package(Trantor REQUIRED PATHS ${THIRD_PARTY_PATH}/drogon/lib/cmake/)
find_package(Drogon REQUIRED PATHS ${THIRD_PARTY_PATH}/drogon/lib/cmake/)
find_package(yutovo_logger REQUIRED PATHS ${INSTALL_PATH}/lib/cmake/)

add_executable(${PROJECT_NAME}_tests
	main.cpp
    mock.cpp
    auth_test.cpp
    session_test.cpp
    service_test.cpp
)

target_link_libraries(${PROJECT_NAME}_tests PUBLIC
	stdc++
    pthread
	gtest
	gtest_main
	gmock
	gmock_main
	m
	dl
    yutovo_logger
    Drogon::Drogon
)

target_include_directories(${PROJECT_NAME}_tests PRIVATE
    ${THIRD_PARTY_PATH}
    ${THIRD_PARTY_PATH}/drogon/include
    /usr/include/jsoncpp
)

gtest_discover_tests(${PROJECT_NAME}_tests)

install(
    TARGETS ${PROJECT_NAME}_tests
    EXPORT ${PROJECT_NAME}
    DESTINATION ${INSTALL_PATH}/bin
    COMPONENT ${PROJECT_NAME}
)
