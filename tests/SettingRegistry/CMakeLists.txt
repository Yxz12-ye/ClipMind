# ============================================================================
# SettingRegistry Test Module
# ============================================================================
# Tests the metadata-only setting registry: it never touches the config file,
# so the module can be exercised without a SettingService.
# A custom main() creates QCoreApplication before running GoogleTest.
# ============================================================================

set(TARGET_NAME ${PROJECT_NAME}_SettingRegistry_test)

add_executable(${TARGET_NAME}
    setting_registry_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/LogService.cpp
    ${CMAKE_SOURCE_DIR}/src/service/SettingRegistry.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Gui
    spdlog::spdlog
)

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
