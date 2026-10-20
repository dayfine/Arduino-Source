"""Stream SerialPrograms records; summarize or print bounded event excerpts."""
import argparse
from collections import Counter
from pathlib import Path
import re

HEADER = re.compile(r"^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) - \[([^]]+)\]: (.*)")
EVENT = re.compile(r"Starting Program:|Program State:|Received Stop Request|Quest:|Processing quests\.|Quest not possible|Current quest exists on list|Quest completed successfully\.|Quest did not complete successfully\.|Failed to complete a quest|Did not enter battle|Unable to locate the overworld radar ball|Saving and resetting\.|Exception|UserSetupError|Program stopped with an exception")


def records(path):
    pending = None
    with path.open(encoding="utf-8-sig") as source:
        for number, line in enumerate(source, 1):
            line = line.rstrip("\r\n")
            match = HEADER.match(line)
            if match:
                if pending:
                    yield pending
                pending = (number, match[1], match[2], match[3], [line])
            elif pending:
                pending[4].append(line)
        if pending:
            yield pending


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", type=Path, nargs="+")
    parser.add_argument("--since", default="")
    parser.add_argument("--until", default="")
    parser.add_argument("--events", action="store_true")
    parser.add_argument("--match", help="Override event selection with this regex")
    parser.add_argument("--limit", type=int, default=80)
    args = parser.parse_args()
    selector = re.compile(args.match) if args.match else EVENT
    for path in args.paths:
        tags, quests, messages = Counter(), Counter(), Counter()
        first = last = None
        selected = printed = total = 0
        for number, timestamp, tag, message, lines in records(path):
            if args.since and timestamp < args.since:
                continue
            if args.until and timestamp[:len(args.until)] > args.until:
                continue
            first = first or timestamp
            last = timestamp
            total += 1
            tags[tag] += 1
            if message.startswith("Quest: "):
                quests[message[7:]] += 1
            if selector.search("\n".join(lines)):
                selected += 1
                # Aggregate event messages, retaining no potentially huge payloads.
                if not message.startswith("<b>Starting Program:"):
                    messages[message] += 1
                if args.events and printed < args.limit:
                    for offset, line in enumerate(lines):
                        print(f"{path.name}:{number + offset}: {line}")
                    printed += 1
        print(f"{path.name}: {first} .. {last}; {total} records; {selected} selected")
        print("Tags:", dict(tags))
        print("Quest start messages:", dict(quests))
        if not args.events:
            print("Most common selected messages:", messages.most_common(15))
        elif selected > printed:
            print(f"Showing {printed}/{selected} selected records; narrow the interval or raise --limit.")


if __name__ == "__main__":
    main()
