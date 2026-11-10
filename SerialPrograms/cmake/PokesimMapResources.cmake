# Resolve FRLG's prepared map input. Configuration never prepares or downloads maps.
# Supply -DPOKESIM_TOOLS_SOURCE=/path/to/tools -DPOKESIM_SOURCE_CACHE=/path/to/cache.
# See devtools/maps/RESOURCE_MAPS.md for the explicit prepare step and pinned CI.
function(pa_frlg_map_resources OUT)
    get_filename_component(CONSUMER "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(POKESIM_TOOLS_SOURCE "$ENV{POKESIM_TOOLS_SOURCE}" CACHE PATH "Pinned PokemonSimulation tools checkout")
    set(POKESIM_SOURCE_CACHE "$ENV{POKESIM_SOURCE_CACHE}" CACHE PATH "Approved publishing Git cache")
    if(NOT POKESIM_TOOLS_SOURCE OR NOT POKESIM_SOURCE_CACHE)
        message(FATAL_ERROR "Set POKESIM_TOOLS_SOURCE and POKESIM_SOURCE_CACHE after preparing devtools/maps/map-resources.lock.json; see devtools/maps/RESOURCE_MAPS.md.")
    endif()
    include("${POKESIM_TOOLS_SOURCE}/integrations/arduino-source/resource-maps.cmake")
    pa_pokesim_map_directory(MAPS frlg "${CONSUMER}" "${POKESIM_SOURCE_CACHE}")
    file(RELATIVE_PATH RELATIVE_MAPS "${CMAKE_CURRENT_SOURCE_DIR}" "${MAPS}")
    set(${OUT} "${RELATIVE_MAPS}" PARENT_SCOPE)
endfunction()
