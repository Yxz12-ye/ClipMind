# ============================================================================
# CopyEventListener Test Module
# ============================================================================
# Tests the platform-independent AbstractCopyEventListener base class.
# Requires Qt because the class inherits QObject and uses signal/slot.
# A custom main() creates QApplication before running GoogleTest.
# ============================================================================

set(TARGET_NAME ${PROJECT_NAME}_CopyEventListener_test)

add_executable(${TARGET_NAME}
    copy_event_listener_test.cpp
    ${CMAKE_SOURCE_DIR}/src/service/CopyEventListener.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Windows-specific: CopyEventListener.cpp calls Win32 clipboard APIs
if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE user32)
endif()

if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()

gtest_discover_tests(${TARGET_NAME})
