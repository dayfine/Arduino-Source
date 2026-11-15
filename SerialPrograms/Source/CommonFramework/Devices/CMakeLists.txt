# Focused native tests, without configuring/linking the GUI application:
# cmake -S SerialPrograms/Source/CommonFramework/Devices -B /path/to/test-build
# cmake --build /path/to/test-build -j4
# ctest --test-dir /path/to/test-build --output-on-failure
cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LabDeviceAccessTests LANGUAGES CXX)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Core)
endif()
add_executable(LabDeviceAccessTests LabDeviceAccessQt.cpp LabDeviceAccessQt_Tests.cpp)
target_compile_features(LabDeviceAccessTests PRIVATE cxx_std_23)
target_compile_definitions(LabDeviceAccessTests PRIVATE PA_LAB_DEVICE_STANDALONE_TESTS)
target_link_libraries(LabDeviceAccessTests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(LabDeviceAccessTests PRIVATE /W4 /WX)
else()
    target_compile_options(LabDeviceAccessTests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME LabDeviceAccess COMMAND LabDeviceAccessTests)
