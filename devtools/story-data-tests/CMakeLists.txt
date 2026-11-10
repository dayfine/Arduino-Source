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
    "${STORY}/Data/Pokemon_StoryStepJson.cpp"
    "${STORY}/Data/Pokemon_StoryStepJson_Tests.cpp"
    "${STORY}/Pokemon_TilePose.cpp"
    "${STORY}/Types/Pokemon_StoryArgs.cpp"
    "${ROOT}/SerialPrograms/Source/Pokemon/Navigation/Pokemon_Grid.cpp"
    "${ROOT}/Common/Cpp/Exceptions.cpp"
    "${ROOT}/Common/Cpp/Json/JsonValue.cpp"
    "${ROOT}/Common/Cpp/Json/JsonObject.cpp"
    "${ROOT}/Common/Cpp/Json/JsonArray.cpp"
    "${ROOT}/Common/Cpp/Json/JsonTools.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FilePath.cpp"
    "${ROOT}/Common/Cpp/Strings/Unicode.cpp")
# Reuse the Qt/controller-free support sources; each codec has its own test main.
get_target_property(STORY_DATA_SOURCES StoryDataTests SOURCES)
list(REMOVE_ITEM STORY_DATA_SOURCES "${STORY}/Data/Pokemon_StoryStepJson_Tests.cpp")
add_executable(AuthoredCheckpointTests ${STORY_DATA_SOURCES}
    "${STORY}/Data/Pokemon_AuthoredCheckpointJson.cpp"
    "${STORY}/Data/Pokemon_AuthoredCheckpointJson_Tests.cpp")
add_executable(StoryArgsTests
    "${STORY}/Types/Pokemon_StoryArgs.cpp"
    "${STORY}/Pokemon_StoryVariant.cpp"
    "${STORY}/Pokemon_StoryArgs_Tests.cpp")

foreach(TEST_TARGET IN ITEMS StoryDataTests AuthoredCheckpointTests StoryArgsTests)
target_include_directories(${TEST_TARGET} PRIVATE "${ROOT}" "${ROOT}/SerialPrograms/Source")
target_compile_features(${TEST_TARGET} PRIVATE cxx_std_23)
target_compile_definitions(${TEST_TARGET} PRIVATE PA_STORY_DATA_STANDALONE)
if(MSVC)
    target_compile_definitions(${TEST_TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${TEST_TARGET} PRIVATE /W4 /WX)
    set_property(SOURCE "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp" APPEND PROPERTY COMPILE_OPTIONS /wd4505)
else()
    target_compile_options(${TEST_TARGET} PRIVATE -Wall -Wextra -Werror)
endif()
endforeach()
add_test(NAME StoryData COMMAND StoryDataTests)
add_test(NAME AuthoredCheckpoint COMMAND AuthoredCheckpointTests)
add_test(NAME StoryArgs COMMAND StoryArgsTests)
