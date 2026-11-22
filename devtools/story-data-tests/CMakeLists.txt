# Offline S10 story checkpoint & grid story action tests, no Qt or controller initialization:
# cmake -S devtools/story-data-tests -B build/story-data-tests
# cmake --build build/story-data-tests -j4
# ctest --test-dir build/story-data-tests --output-on-failure
cmake_minimum_required(VERSION 3.24)
project(StoryDataTests LANGUAGES CXX)
enable_testing()
get_filename_component(ROOT "../.." ABSOLUTE)
set(STORY "${ROOT}/SerialPrograms/Source/Pokemon/AutoStory")
set(MAPS "${ROOT}/SerialPrograms/Source/Pokemon/Navigation/Grid/Maps")
add_executable(StoryDataTests
    "${CMAKE_CURRENT_SOURCE_DIR}/StoryDataMain.cpp"
    "${STORY}/Data/Pokemon_StoryCheckpointJson.cpp"
    "${STORY}/Data/Pokemon_StoryCheckpointJson_Tests.cpp"
    "${STORY}/Grid/Pokemon_GridStoryArgs.cpp"
    "${STORY}/Grid/Pokemon_GridStoryActions.cpp"
    "${STORY}/Grid/Pokemon_GridStoryActions_Tests.cpp"
    "${STORY}/Testing/Pokemon_StoryFakeBackend.cpp"
    "${ROOT}/SerialPrograms/Source/NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.cpp"
    "${MAPS}/Pokemon_Direction.cpp"
    "${MAPS}/Pokemon_Grid.cpp"
    "${ROOT}/Common/Cpp/CancellableScope.cpp"
    "${ROOT}/Common/Cpp/Exceptions.cpp"
    "${ROOT}/Common/Cpp/LifetimeSanitizer.cpp"
    "${ROOT}/Common/Cpp/Concurrency/SpinLock.cpp"
    "${ROOT}/Common/Cpp/Json/JsonValue.cpp"
    "${ROOT}/Common/Cpp/Json/JsonObject.cpp"
    "${ROOT}/Common/Cpp/Json/JsonArray.cpp"
    "${ROOT}/Common/Cpp/Json/JsonTools.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FilePath.cpp"
    "${ROOT}/Common/Cpp/Strings/StringTools.cpp"
    "${ROOT}/Common/Cpp/Strings/Unicode.cpp")
target_include_directories(StoryDataTests PRIVATE "${ROOT}" "${ROOT}/SerialPrograms/Source")
target_compile_features(StoryDataTests PRIVATE cxx_std_23)
if(MSVC)
    target_compile_definitions(StoryDataTests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(StoryDataTests PRIVATE /W4 /WX)
    set_property(SOURCE "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp" APPEND PROPERTY COMPILE_OPTIONS /wd4505)
else()
    target_compile_options(StoryDataTests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME StoryData COMMAND StoryDataTests)
