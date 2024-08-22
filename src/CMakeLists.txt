cmake_minimum_required(VERSION 3.17)

set(CMAKE_DEBUG_POSTFIX d)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(Boost_INCLUDE_DIR "/home/denis/programs/Math/yutovo/deploy/include")

find_package(PkgConfig REQUIRED)
find_package(yutovo_logger REQUIRED PATHS ${INSTALL_PATH}/lib/cmake/)
find_package(Trantor REQUIRED PATHS ${THIRD_PARTY_PATH}/drogon/lib/cmake/)
find_package(Drogon REQUIRED PATHS ${THIRD_PARTY_PATH}/drogon/lib/cmake/)
find_package(jwt-cpp REQUIRED PATHS ${THIRD_PARTY_PATH}/jwt-cpp/lib/cmake/)
find_package(Boost 1.81.0 REQUIRED PATHS ${INSTALL_PATH}/lib/cmake/)

add_executable(${PROJECT_NAME}
    main.cpp
    utils.cpp
    logic/clear_db.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)

target_compile_options(${PROJECT_NAME} PUBLIC -Wno-deprecated-declarations)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    stdc++
    stdc++fs
    pthread
    crypto
    ssl
    yutovo_logger
    Drogon::Drogon
    X11
    jpeg
)

target_sources(${PROJECT_NAME} PRIVATE
    ${SRC_DIR}
    ${CTL_SRC}
    ${FILTER_SRC}
    ${PLUGIN_SRC}
    ${MODEL_SRC}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${INSTALL_PATH}/include
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${THIRD_PARTY_PATH}
    ${THIRD_PARTY_PATH}/drogon/include
    ${THIRD_PARTY_PATH}/jwt-cpp/lib/include/
    /usr/include/jsoncpp
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}
    DESTINATION ${INSTALL_PATH}/bin
    COMPONENT ${PROJECT_NAME}
)

install(
    EXPORT ${PROJECT_NAME}
    DESTINATION ${INSTALL_PATH}/lib/cmake/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
    FILE ${PROJECT_NAME}.cmake
)

install(
    FILES ${PROJECT_NAME}-config.cmake
    DESTINATION ${INSTALL_PATH}/lib/cmake/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    DESTINATION ${INSTALL_PATH}/bin
    COMPONENT ${PROJECT_NAME}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../keys
    DESTINATION ${INSTALL_PATH}
    COMPONENT ${PROJECT_NAME}
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/nginx.conf
    DESTINATION ${INSTALL_PATH}/bin
    COMPONENT ${PROJECT_NAME}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/config.json ${CMAKE_CURRENT_BINARY_DIR}/../
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/config.json ${CMAKE_CURRENT_BINARY_DIR}/../test/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../keys/yutovo_service.crt ${CMAKE_CURRENT_BINARY_DIR}/../
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../keys/yutovo_service.key ${CMAKE_CURRENT_BINARY_DIR}/../
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../keys/yutovo_service.pub ${CMAKE_CURRENT_BINARY_DIR}/../
)
