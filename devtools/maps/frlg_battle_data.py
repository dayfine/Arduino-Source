"""Forward native battle-data generation to the pinned PokemonSimulation tools.

Example:
    PYTHONPATH=/path/to/pokesim/src python3 devtools/maps/frlg_battle_data.py \
        --out SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/BattleData --check

The implementation and FireRed source pin belong to PokemonSimulation. Use the
same tools checkout documented in RESOURCE_MAPS.md; no map-builder copy is needed.
"""

from pokesim.games.frlg.battle_data_export import main


if __name__ == '__main__':
    main()
