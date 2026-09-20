# ============================================================================
# WindowPositioner Test Module
# ============================================================================
# Tests the platform-independent window placement strategies.
# Requires Qt because the implementations use QScreen / QCursor.
# A custom main() creates QApplication before running GoogleTest.
# ============================================================================

set(TARGET_NAME ${PROJECT_NAME}_WindowPositioner_test)

add_executable(${TARGET_NAME}
    window_positioner_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/WindowPositioner.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Gui
)

# Windows-specific: WindowPositioner.cpp calls Win32 caret / UIAutomation APIs
if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE user32 oleacc uiautomationcore)
endif()

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
