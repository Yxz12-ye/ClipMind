set(TARGET_NAME ${PROJECT_NAME}_EventBus_test)

find_package(Threads REQUIRED)

add_executable(${TARGET_NAME}
    event_bus_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/EventBus.cpp
    ${CMAKE_SOURCE_DIR}/src/service/LogService.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(${TARGET_NAME} PRIVATE GTest::gtest Qt6::Core spdlog::spdlog Threads::Threads)

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
