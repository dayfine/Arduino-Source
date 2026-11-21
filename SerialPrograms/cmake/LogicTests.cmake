# SerialProgramsLogicTests: the unit tests that need no Qt, OpenCV, ONNX Runtime,
# Tesseract or test images.
#
# Built only with -DPA_CORE_ONLY=ON -DPA_LOGIC_TESTS=ON, so it configures and builds
# even when the full build's third-party downloads fail, e.g.:
#   cmake -S SerialPrograms -B build-logic -G Ninja -DPA_CORE_ONLY=ON -DCOMMAND_LINE_EXE=OFF -DPA_LOGIC_TESTS=ON
#   cmake --build build-logic --target SerialProgramsLogicTests
#   build-logic/SerialProgramsLogicTests [--filter <substring>]
#
# It links CoreLib plus LogicLib, a static library of ${LOGIC_LIBRARY_SOURCES}
# (see SourceFiles.cmake). The full build does not use this file.
#
# Requires CoreLib.cmake (CoreLib and pa_apply_gui_free_target_properties()).

add_library(LogicLib STATIC ${LOGIC_LIBRARY_SOURCES})
pa_apply_gui_free_target_properties(LogicLib)

add_executable(SerialProgramsLogicTests ${LOGIC_TEST_SOURCES})
pa_apply_gui_free_target_properties(SerialProgramsLogicTests)
target_link_libraries(SerialProgramsLogicTests PRIVATE LogicLib CoreLib)

foreach(logic_target LogicLib SerialProgramsLogicTests)
    if (MSVC)
        target_compile_options(${logic_target} PRIVATE /W4 /WX /external:W0 /utf-8 /wd5054 /wd4505)
    elseif(APPLE)
        target_compile_options(${logic_target} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wshorten-64-to-32)
    else()
        target_compile_options(${logic_target} PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-strict-aliasing)
    endif()
endforeach()
