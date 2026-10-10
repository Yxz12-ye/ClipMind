set(TARGET_NAME ${PROJECT_NAME}_SQLService_test)

add_executable(${TARGET_NAME}
    sql_service_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/LogService.cpp
    ${CMAKE_SOURCE_DIR}/src/service/SQLService.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Gui
    spdlog::spdlog
    unofficial::sqlite3::sqlite3
    sqlite_vec
)

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
