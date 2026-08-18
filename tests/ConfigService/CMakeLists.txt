set(TARGET_NAME ${PROJECT_NAME}_ConfigService_test)

add_executable(${TARGET_NAME}
    config_service_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/ConfigService.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest_main
    Qt6::Core
)

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
