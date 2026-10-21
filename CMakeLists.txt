cmake_minimum_required(VERSION 3.21)
project(USBIPManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(usbip-devices STATIC devices.cpp)
target_link_libraries(usbip-devices PUBLIC Qt6::Core)
target_include_directories(usbip-devices PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(usbip-manager main.cpp)
target_link_libraries(usbip-manager PRIVATE usbip-devices Qt6::Widgets)
target_compile_definitions(usbip-manager PRIVATE
    USBIP_HELPER_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/usbip-manager-helper")

add_executable(usbip-manager-helper helper.cpp)
target_link_libraries(usbip-manager-helper PRIVATE usbip-devices)

foreach(target usbip-devices usbip-manager usbip-manager-helper)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

configure_file(org.usbip.manager.policy.in org.usbip.manager.policy @ONLY)
install(TARGETS usbip-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS usbip-manager-helper RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.usbip.manager.policy
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES org.usbip.manager.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/usbip-manager.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(device-tests tests.cpp)
    target_link_libraries(device-tests PRIVATE usbip-devices Qt6::Test)
    add_test(NAME devices COMMAND device-tests)
    add_test(NAME gui-smoke COMMAND usbip-manager --smoke-test)
    set_tests_properties(gui-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME helper-rejects-invalid-command COMMAND usbip-manager-helper invalid)
    set_tests_properties(helper-rejects-invalid-command PROPERTIES WILL_FAIL TRUE)
endif()
