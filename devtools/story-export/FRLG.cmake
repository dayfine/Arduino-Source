# Evaluates the real game table without constructing a console executor.
# Full application libraries are build dependencies; this is not PA_CORE_ONLY.
option(PA_FRLG_STORY_EXPORT "Build the offline evaluated FRLG checkpoint exporter" OFF)
if(PA_FRLG_STORY_EXPORT)
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${REPO_ROOT_DIR}"
        OUTPUT_VARIABLE STORY_EXPORT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND git diff --quiet HEAD WORKING_DIRECTORY "${REPO_ROOT_DIR}"
        RESULT_VARIABLE STORY_EXPORT_DIRTY)
    if(NOT STORY_EXPORT_DIRTY EQUAL 0)
        message(FATAL_ERROR "Commit tracked changes before configuring a provenance-bearing story export.")
    endif()
    add_executable(StoryCheckpointExport
        ../devtools/story-export/frlg_main.cpp)
    target_link_libraries(StoryCheckpointExport PRIVATE SerialProgramsLib)
    apply_common_target_properties(StoryCheckpointExport)
    target_compile_definitions(StoryCheckpointExport PRIVATE PA_STORY_EXPORT_REVISION="${STORY_EXPORT_REVISION}")
endif()
