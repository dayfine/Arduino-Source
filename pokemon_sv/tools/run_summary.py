# Summarize one BBQ Farmer run from a profile's SerialPrograms.log, as a
# markdown block for pokemon_sv/STAGE_RUNS.md, and check it against a stage's
# pass criteria.
#
#   python3 run_summary.py PROFILE [--stage 0|1] [--run -1] [--label R24]
#
# PROFILE is a profile name under ~/Library/Application Support/SerialPrograms/
# Profiles or a path to a log. A log holds every launch of that profile; runs
# are split at "Starting Program: PokemonSV:BBQSoloFarmer" and --run picks one
# (default: the last).
import argparse, os, re
from datetime import datetime

BASE = os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
START = "Starting Program: PokemonSV:BBQSoloFarmer"
STATS = re.compile(r"Current Stats: (.*)$")
STALL_MIN = 15

ts = lambda line: datetime.strptime(line[:26], "%Y-%m-%d %H:%M:%S.%f")


def load_runs(path):
    runs, cur = [], None
    for line in open(path, errors="replace"):
        if len(line) < 27 or line[4] != "-" or line[10] != " ":
            continue
        if START in line:
            cur = []
            runs.append(cur)
        if cur is not None:
            cur.append(line.rstrip("\n"))
    return runs


def parse_stats(text):
    out = {}
    for part in text.split(" - "):
        if ": " in part:
            k, v = part.split(": ", 1)
            out[k.strip()] = v.strip()
    return out


def summarize(lines):
    s = {
        "start": ts(lines[0]), "end": ts(lines[-1]), "stats": {}, "bp_check": None,
        "attempts": 0, "failed": 0, "rerolls": 0, "verified_rerolls": 0, "reroll_all": 0,
        "errors": [], "stops": [], "user_stop": False, "completions": [],
    }
    last_done = 0
    for line in lines:
        msg = line.split(" - ", 1)[1] if " - " in line else line
        m = STATS.search(line)
        if m:
            st = parse_stats(m.group(1))
            s["stats"] = st
            done = int(st.get("Quests Completed", "0").replace(",", ""))
            if done > last_done:
                s["completions"].append(ts(line))
                last_done = done
        if "BP check:" in line:
            s["bp_check"] = msg.split("BP check:", 1)[1].strip()
        if "Doing quest." in line:
            s["attempts"] += 1
        if "Quest did not complete successfully" in line:
            s["failed"] += 1
        # main logs "Rerolling ... quest" per reroll; stage 1 also confirms each
        # one with "Quest N rerolled."
        if re.search(r"Rerolling (the failed |Egg )?quest", line):
            s["rerolls"] += 1
        if re.search(r"Quest \d rerolled\.", line):
            s["verified_rerolls"] += 1
        if "Rerolling all quests" in line:
            s["reroll_all"] += 1
        if "Received Stop Request" in line:
            s["user_stop"] = True
        if re.search(r"Stopping program|Program Error|FatalProgramException|Exception:", line) \
                and "SpectrogramMatcher" not in line:
            (s["stops"] if "Stopping program" in line else s["errors"]).append(msg[:160])
    # Longest stretch without a completion, from program start to run end.
    marks = [s["start"]] + s["completions"] + [s["end"]]
    gaps = [(b - a).total_seconds() / 60 for a, b in zip(marks, marks[1:])]
    s["max_gap"] = max(gaps) if gaps else 0
    s["stalls"] = sum(g > STALL_MIN for g in gaps)
    return s


def verdict(s, stage):
    checks = []
    if stage >= 0:
        checks.append(("stats show BP", "BP Earned" in s["stats"]))
        checks.append(("BP check OK", (s["bp_check"] or "").startswith("OK")))
        checks.append(("no crash or program error", not s["errors"]))
    if stage >= 1:
        checks.append((f"no stretch over {STALL_MIN} min without a completion", s["stalls"] == 0))
        checks.append(("no stop by the program", not s["stops"]))
    return checks


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("profile")
    ap.add_argument("--stage", type=int, default=0)
    ap.add_argument("--run", type=int, default=-1)
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    path = a.profile if os.path.exists(a.profile) else f"{BASE}/{a.profile}/SerialPrograms.log"
    runs = load_runs(path)
    if not runs:
        raise SystemExit(f"no BBQ Farmer run in {path}")
    s = summarize(runs[a.run])
    st = s["stats"]
    hours = (s["end"] - s["start"]).total_seconds() / 3600
    checks = verdict(s, a.stage)
    ok = all(c for _, c in checks)
    print(f"### {a.label or 'Run'}: stage {a.stage}, {s['start']:%Y-%m-%d %H:%M} → {s['end']:%H:%M} "
          f"({hours * 60:.0f} min), {'PASS' if ok else 'FAIL'}\n")
    print(f"- Log: `{path}` (run {(a.run if a.run >= 0 else len(runs) + a.run) + 1} of {len(runs)})")
    print(f"- Stats: {' · '.join(f'{k} {v}' for k, v in st.items()) or 'none logged'}")
    try:
        net = int(st.get("Net BP", "0").replace(",", ""))
        print(f"- Wall-clock net BP/h: {net / hours:.0f}" if hours > 0 else "")
    except ValueError:
        pass
    rerolls = max(s["rerolls"], s["verified_rerolls"])
    print(f"- Attempts {s['attempts']}, failed {s['failed']}, rerolls {rerolls}"
          + (f" ({s['verified_rerolls']} confirmed)" if s["verified_rerolls"] else "")
          + f" (+{s['reroll_all']} whole-panel)")
    print(f"- Longest stretch without a completion: {s['max_gap']:.1f} min; stretches over {STALL_MIN} min: {s['stalls']}")
    print(f"- BP check: {s['bp_check'] or 'not logged'}")
    print(f"- Ended by: {'user stop' if s['user_stop'] else 'the program (finished or stopped itself)'}"
          + (f"; program stops: {'; '.join(s['stops'])}" if s["stops"] else ""))
    for e in s["errors"][:5]:
        print(f"- Error: {e}")
    print()
    for name, c in checks:
        print(f"- [{'x' if c else ' '}] {name}")


if __name__ == "__main__":
    main()
