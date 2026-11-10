"""Prepare isolated macOS lab instances; never launch apps or open devices.

Examples (run from repository root):
  python3 devtools/lab/lab_instance.py seed --source /offline/default-copy --root /offline/root --profile lab-A
  python3 devtools/lab/lab_instance.py preflight --app /build/SerialPrograms.app --profile lab-A
  python3 devtools/lab/lab_instance.py stage --app /build/SerialPrograms.app --out /staging/A --profile lab-A --console A --bundle-id com.dayfine.pokemonautomation.labA --signing-identity 'PokemonAutomation Local Development'
  python3 devtools/lab/lab_instance.py verify --manifest /staging/A/lab-deployment.json --profile lab-A --peer /staging/B/lab-deployment.json
"""

import argparse
import subprocess

from lab_package import preflight, stage, verify
from lab_profiles import seed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    seeder = commands.add_parser("seed", help="Create an unused sanitized profile")
    seeder.add_argument("--source", required=True)
    seeder.add_argument("--root", required=True, help="Application support root containing Profiles/")
    for command in (seeder, commands.add_parser("preflight"), commands.add_parser("stage"), commands.add_parser("verify")):
        command.add_argument("--profile", required=True)
        if command.prog.endswith(("preflight", "stage")):
            command.add_argument("--app", required=True)
        if command.prog.endswith(("stage", "verify")):
            command.add_argument("--peer", action="append", default=[])
        if command.prog.endswith("stage"):
            command.add_argument("--out", required=True)
            command.add_argument("--console", choices=("A", "B"), required=True)
            command.add_argument("--bundle-id", required=True)
            command.add_argument("--signing-identity", required=True)
        if command.prog.endswith("verify"):
            command.add_argument("--manifest", required=True)
    args = parser.parse_args()
    try:
        if args.command == "seed":
            print(seed(args.source, args.root, args.profile))
        elif args.command == "preflight":
            preflight(args.app, args.profile)
            print("Linked build matches " + args.profile)
        elif args.command == "stage":
            print(stage(args.app, args.out, args.profile, args.console, args.bundle_id, args.signing_identity, args.peer))
        else:
            verify(args.manifest, args.profile, args.peer)
            print("Staged package matches " + args.profile + "; hardware ownership is not enforced yet.")
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        detail = str(error) if isinstance(error, ValueError) else type(error).__name__
        parser.exit(2, "Lab preparation refused: " + detail + "\n")


if __name__ == "__main__":
    main()
