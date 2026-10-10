set(TARGET_NAME ${PROJECT_NAME}_UIController_test)

add_executable(${TARGET_NAME}
    ui_controller_test.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/UIController.cpp
    ${CMAKE_SOURCE_DIR}/src/service/CopyEventListener.cpp
    ${CMAKE_SOURCE_DIR}/src/service/EmbeddingService.cpp
    ${CMAKE_SOURCE_DIR}/src/service/LogService.cpp
    ${CMAKE_SOURCE_DIR}/src/service/SettingService.cpp
    ${CMAKE_SOURCE_DIR}/src/service/SQLService.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(${TARGET_NAME} PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
    spdlog::spdlog
    sqlite_vec
)
if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE user32)
endif()
if(ENABLE_WARNINGS)
    target_link_libraries(${TARGET_NAME} PRIVATE project_warnings)
endif()
gtest_discover_tests(${TARGET_NAME})
