# Offline production-decoder tests:
# cmake -S devtools/runtime-map-tests -B /path/to/build
# cmake --build /path/to/build -j4
# /path/to/build/RuntimeMapTests [publisher-parity-pack.json]
cmake_minimum_required(VERSION 3.24)
project(RuntimeMapTests LANGUAGES CXX)
enable_testing()
get_filename_component(ROOT "../.." ABSOLUTE)
set(NAV "${ROOT}/SerialPrograms/Source/Pokemon/Navigation")
add_executable(RuntimeMapTests
    "${NAV}/Pokemon_RuntimeMap.cpp"
    "${NAV}/Pokemon_RuntimeMap_Tests.cpp"
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
target_include_directories(RuntimeMapTests PRIVATE "${ROOT}" "${ROOT}/SerialPrograms/Source")
target_compile_features(RuntimeMapTests PRIVATE cxx_std_23)
target_compile_definitions(RuntimeMapTests PRIVATE PA_RUNTIME_MAP_STANDALONE)
if(MSVC)
    target_compile_definitions(RuntimeMapTests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(RuntimeMapTests PRIVATE /W4 /WX)
else()
    target_compile_options(RuntimeMapTests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME RuntimeMap COMMAND RuntimeMapTests)
