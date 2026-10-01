set(TARGET_NAME ${PROJECT_NAME}_EmbeddingService_test)

add_executable(${TARGET_NAME}
    embedding_service_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/EmbeddingService.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Network
    Qt6::Test
)

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
