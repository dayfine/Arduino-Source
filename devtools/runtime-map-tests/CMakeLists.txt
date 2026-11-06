# Offline production-decoder tests:
# cmake -S devtools/runtime-map-tests -B /path/to/build
# cmake --build /path/to/build -j4
# /path/to/build/RuntimeMapTests [publisher-parity-pack.json]
cmake_minimum_required(VERSION 3.24)
project(RuntimeMapTests LANGUAGES CXX)
enable_testing()
get_filename_component(ROOT "../.." ABSOLUTE)
set(NAV "${ROOT}/SerialPrograms/Source/Pokemon/Navigation")
add_library(RuntimeMapDecoder STATIC
    "${NAV}/Pokemon_RuntimeMap.cpp"
    "${NAV}/Pokemon_MapData.cpp"
    "${NAV}/Pokemon_Grid.cpp"
    "${ROOT}/Common/Cpp/Exceptions.cpp"
    "${ROOT}/Common/Cpp/Json/JsonValue.cpp"
    "${ROOT}/Common/Cpp/Json/JsonObject.cpp"
    "${ROOT}/Common/Cpp/Json/JsonArray.cpp"
    "${ROOT}/Common/Cpp/Json/JsonTools.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp"
    "${ROOT}/Common/Cpp/Filesystem/FilePath.cpp"
    "${ROOT}/Common/Cpp/Strings/Unicode.cpp"
)
target_include_directories(RuntimeMapDecoder PUBLIC "${ROOT}" "${ROOT}/SerialPrograms/Source")
target_compile_features(RuntimeMapDecoder PUBLIC cxx_std_23)
# Match the application's existing policy for this unchanged platform-specific helper.
if(MSVC)
    set_property(SOURCE "${ROOT}/Common/Cpp/Filesystem/FileIO.cpp" APPEND PROPERTY COMPILE_OPTIONS /wd4505)
endif()
add_executable(RuntimeMapTests "${NAV}/Pokemon_RuntimeMap_Tests.cpp")
target_compile_definitions(RuntimeMapTests PRIVATE PA_RUNTIME_MAP_STANDALONE)
target_link_libraries(RuntimeMapTests PRIVATE RuntimeMapDecoder)
add_executable(RuntimeMapLegacyTests
    legacy_tests.cpp
    "${NAV}/Pokemon_MapData_Tests.cpp"
    "${ROOT}/Common/Cpp/CancellableScope.cpp"
    "${ROOT}/Common/Cpp/LifetimeSanitizer.cpp"
    "${ROOT}/Common/Cpp/Concurrency/SpinLock.cpp"
)
target_link_libraries(RuntimeMapLegacyTests PRIVATE RuntimeMapDecoder)
include("${ROOT}/SerialPrograms/cmake/EmbedTextFiles.cmake")
# Resource consumers also register canonical paths; embed dependencies must not
# retain a second ../ alias, which Ninja can treat as duplicate generated outputs.
file(GLOB CANONICAL_MAPS "${NAV}/TestMaps/*.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CANONICAL_MAPS})
pa_embed_text_files(RuntimeMapLegacyTests ../../SerialPrograms/Source/Pokemon/Navigation/TestMaps
    Pokemon/Navigation/Pokemon_Navigation_Tests.h PokemonAutomation::Pokemon navigation_test_map_json)
get_property(MAP_DEPENDENCIES DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
foreach(INPUT IN LISTS MAP_DEPENDENCIES)
    if(INPUT MATCHES "TestMaps/.*\\.json$")
        file(REAL_PATH "${INPUT}" CANONICAL_INPUT)
        if(NOT INPUT STREQUAL CANONICAL_INPUT)
            message(FATAL_ERROR "Map embedding registered a noncanonical dependency: ${INPUT}")
        endif()
    endif()
endforeach()
foreach(target RuntimeMapDecoder RuntimeMapTests RuntimeMapLegacyTests)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endforeach()
add_test(NAME RuntimeMap COMMAND RuntimeMapTests)
add_test(NAME RuntimeMapLegacy COMMAND RuntimeMapLegacyTests)
