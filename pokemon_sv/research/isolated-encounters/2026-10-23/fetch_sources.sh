#!/bin/sh
# Download research inputs only; no application build or tests.
set -eu
research_cache="${1:?usage: fetch_sources.sh CACHE_DIR}"
mkdir -p "$research_cache"
for research_file in pokemon_info.js pokemon_calculated_scarlet.js pokemon_calculated_violet.js area_type_pokemon_scarlet.js area_type_pokemon_violet.js type_success_scarlet.js type_success_violet.js version_exclusive_sets.js pmarkers.js; do
    curl -fLsS "https://alexis-matuk.github.io/shinyHuntingOptimizer/js/$research_file" -o "$research_cache/$research_file"
done
curl -fLsS https://www.serebii.net/pokearth/paldea/ -o "$research_cache/serebii-map.html"
curl -fLsS https://raw.githubusercontent.com/PokeAPI/pokeapi/master/data/v2/csv/pokemon_species.csv -o "$research_cache/pokemon_species.csv"
