"""Limits for class shapes and the repository layout used by the layering check."""

CLASS_LIMITS = {
    'public_methods': 7,
    'public_data': 0,
    'single_method_interface': 0,
}

# Advisory floors: new or changed values above these (and within the limit) are reported.
# Changed files above their floor are always reported.
ADVISORY = {'file_lines': 500, 'function_lines': 25, 'public_methods': 5}

HINTS = {
    'public_methods': 'split the class or move helpers into free functions',
    'public_data': 'make data private, or split it into a plain struct without methods',
    'single_method_interface': 'prefer a callable (MoveOnlyFunction / std::function) '
                               'over a single-method interface',
}

# Layering: shared code may not include game modules. Directory names are relative to
# SOURCE_ROOT, which is also the include root; Common/ lives at the repository root.
SOURCE_ROOT = 'SerialPrograms/Source/'
SHARED_DIRS = ('Common/', SOURCE_ROOT + 'CommonFramework/', SOURCE_ROOT + 'CommonTools/')
# Game-specific programs, inference and data.
GAME_MODULES = ('Pokemon', 'PokemonBDSP', 'VideoGames')
# Frameworks, consoles, controllers and tools that games build on. NintendoSwitch is
# the console layer; shared code including it is not a game dependency.
PLATFORM_MODULES = ('CommandLine', 'CommonFramework', 'CommonTools', 'ComputerPrograms',
                    'ControllerInput', 'Controllers', 'GameConsole', 'Integrations',
                    'Kernels', 'ML', 'NintendoSwitch', 'Tests')
