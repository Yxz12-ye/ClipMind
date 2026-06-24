# ============================================================================
# ClipMind Test Suite — Top-Level Orchestrator
# ============================================================================
# Each module lives in its own subdirectory and compiles into a separate test
# executable.  This keeps build times down and avoids symbol clashes.
#
# To add a new test module:
#   1. Create a directory under tests/ (e.g. tests/MyService/)
#   2. Add a CMakeLists.txt inside that directory
#   3. Add `add_subdirectory(MyService)` below
# ============================================================================

include(GoogleTest)

# ---------------------------------------------------------------------------
# string_utils (non-Qt, uses GTest::gtest_main)
# NOTE: currently disabled — src/utils/string_utils.cpp does not exist yet.
# Uncomment when the implementation is ready.
# ---------------------------------------------------------------------------
# add_executable(${PROJECT_NAME}_string_utils_test
#     string_utils_test.cpp
#     ${CMAKE_SOURCE_DIR}/src/utils/string_utils.cpp
# )
# target_include_directories(${PROJECT_NAME}_string_utils_test PRIVATE
#     ${CMAKE_SOURCE_DIR}/src
# )
# target_link_libraries(${PROJECT_NAME}_string_utils_test PRIVATE
#     GTest::gtest_main
# )
# if(ENABLE_WARNINGS)
#     target_link_libraries(${PROJECT_NAME}_string_utils_test PRIVATE project_warnings)
# endif()
# gtest_discover_tests(${PROJECT_NAME}_string_utils_test)

# ---------------------------------------------------------------------------
# Modular tests — one subdirectory per service / component
# ---------------------------------------------------------------------------
add_subdirectory(CopyEventListener)
