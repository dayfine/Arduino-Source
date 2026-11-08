# Offline codec checks, no Qt application or controller initialization:
# cmake -S devtools/story-data-tests -B build/story-data-tests
# cmake --build build/story-data-tests -j4
# ctest --test-dir build/story-data-tests --output-on-failure
cmake_minimum_required(VERSION 3.24)
project(StoryDataTests LANGUAGES CXX)
enable_testing()
get_filename_component(ROOT "../.." ABSOLUTE)
set(STORY "${ROOT}/SerialPrograms/Source/Pokemon/AutoStory")
add_executable(StoryDataTests
    "${STORY}/Pokemon_StoryStepJson.cpp"
    "${STORY}/Pokemon_StoryStepJson_Tests.cpp"
    "${STORY}/Pokemon_TilePose.cpp"
    "${ROOT}/SerialPrograms/Source/Pokemon/Navigation/Pokemon_Grid.cpp"
    "${ROOT}/Common/Cpp/Exceptions.cpp"
    "${ROOT}/Common/Cpp/Json/JsonValue.cpp"
    "${ROOT}/Common/Cpp/Json/JsonObject.cpp"
    "${ROOT}/Common/Cpp/Json/JsonArray.cpp"
    "${ROOT}/Common/Cpp/Json/JsonTools.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FilePath.cpp"
    "${ROOT}/Common/Cpp/Strings/Unicode.cpp")
target_include_directories(StoryDataTests PRIVATE "${ROOT}" "${ROOT}/SerialPrograms/Source")
target_compile_features(StoryDataTests PRIVATE cxx_std_23)
target_compile_definitions(StoryDataTests PRIVATE PA_STORY_DATA_STANDALONE)
if(MSVC)
    target_compile_definitions(StoryDataTests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(StoryDataTests PRIVATE /W4 /WX)
    set_property(SOURCE "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp" APPEND PROPERTY COMPILE_OPTIONS /wd4505)
else()
    target_compile_options(StoryDataTests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME StoryData COMMAND StoryDataTests)
