# Console-free diagnostic executable; linked against the existing application library.
option(PA_FRLG_STORY_REPLAY "Build the native checkpoint replay diagnostic" OFF)
if(PA_FRLG_STORY_REPLAY)
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${REPO_ROOT_DIR}"
        OUTPUT_VARIABLE STORY_REPLAY_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND git diff --quiet HEAD WORKING_DIRECTORY "${REPO_ROOT_DIR}"
        RESULT_VARIABLE STORY_REPLAY_DIRTY)
    if(NOT STORY_REPLAY_DIRTY EQUAL 0)
        message(FATAL_ERROR "Commit tracked changes before configuring a provenance-bearing replay.")
    endif()
    add_executable(StoryCheckpointReplay ../devtools/story-replay/frlg_main.cpp)
    target_link_libraries(StoryCheckpointReplay PRIVATE SerialProgramsLib)
    apply_common_target_properties(StoryCheckpointReplay)
    target_compile_definitions(StoryCheckpointReplay PRIVATE PA_STORY_REPLAY_REVISION="${STORY_REPLAY_REVISION}")
endif()
