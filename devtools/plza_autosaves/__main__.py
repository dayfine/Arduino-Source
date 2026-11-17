"""plza_autosaves: which unverified PLZA plan rows a run log shows autosaving at their end.

    python3 -m devtools.plza_autosaves "$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B/"SerialPrograms*.log
    python3 -m devtools.plza_autosaves --apply <logs...>     # also rewrite those rows to "save": "autosave"
"""

import argparse
import sys
from pathlib import Path

from devtools.plza_autosaves import promote

PLAN_DIR = Path(__file__).resolve().parents[2] / "SerialPrograms/Source/VideoGames/PokemonLZA/Programs/AutoStory/StoryData"


def main(argv=None):
    parser = argparse.ArgumentParser(prog="python3 -m devtools.plza_autosaves", description=__doc__.splitlines()[0])
    parser.add_argument("logs", nargs="+", type=Path, help="SerialPrograms.log files, read only")
    parser.add_argument("--plans", type=Path, default=PLAN_DIR, help="folder of plza-*.plan.json")
    parser.add_argument("--apply", action="store_true", help="rewrite the rows seen autosaving at their end")
    args = parser.parse_args(argv)
    sys.stdout.write(promote.run(args.logs, args.plans, apply=args.apply))


if __name__ == "__main__":
    main()
