#!/usr/bin/env python3
"""
Turn one BDSP blink sample log (Screenshots/StarterRng/blinks/<time>.txt,
written by PokemonBDSP_BlinkSampleLog.cpp) into a JSON-lines stream file.

The first line is a "header" record. Every other line is one record from the
log, carrying the recovery phase it belongs to and, for per-watcher records,
the watcher's label. All times are seconds since the log was opened.

Usage: blink_log_to_stream.py LOG [-o OUT]   (OUT defaults to stdout)
"""

import argparse
import json
import sys


def parse_events(text):
    events = []
    if not text:
        return events
    for item in text.split(","):
        seconds, kind = item.split(":")
        events.append({"t": float(seconds), "type": "double" if kind == "D" else "single"})
    return events


def optional(text):
    return None if text == "-" else text


class Converter:
    def __init__(self):
        self.header = {"kind": "header", "comments": []}
        self.phase = None
        self.labels = {}

    def watcher_fields(self, index):
        index = int(index)
        return {"watcher": index, "label": self.labels.get(index)}

    def record(self, fields):
        tag = fields[0]
        if tag == "recovery":
            self.phase = fields[2]
            self.labels = {}
            return {"kind": "recovery", "t": float(fields[1]), "phase": self.phase, "npcs": int(fields[3])}
        if tag == "watcher":
            self.labels[int(fields[1])] = fields[2]
            return {"kind": "watcher", "phase": self.phase, **self.watcher_fields(fields[1])}
        out = {"kind": tag, "phase": self.phase, "t": float(fields[1])}
        if tag == "sample":
            out.update(self.watcher_fields(fields[2]), score=float(fields[3]))
        elif tag == "threshold":
            out.update(self.watcher_fields(fields[2]), mode=fields[3], value=float(fields[4]), samples=int(fields[5]))
        elif tag == "window":
            out.update(self.watcher_fields(fields[2]), keep_from=float(fields[3]))
        elif tag == "decision":
            out.update(decision=fields[2], rolls=int(fields[3]), state=optional(fields[4]), note=optional(fields[5]))
        elif tag == "events":
            out.update(self.watcher_fields(fields[2]), events=parse_events(fields[3]))
        else:
            raise ValueError("unknown record " + tag)
        return out

    def convert(self, lines):
        records = []
        for number, line in enumerate(lines, start=1):
            line = line.rstrip("\r\n")
            if not line:
                continue
            if line.startswith("#"):
                self.header["comments"].append(line[1:].strip())
                continue
            try:
                records.append(self.record(line.split("\t")))
            except (IndexError, ValueError) as error:
                raise SystemExit("line {}: {}: {!r}".format(number, error, line))
        return [self.header] + records


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log")
    parser.add_argument("-o", "--out")
    args = parser.parse_args()
    with open(args.log, encoding="utf-8") as file:
        records = Converter().convert(file)
    out = open(args.out, "w", encoding="utf-8") if args.out else sys.stdout
    try:
        for record in records:
            out.write(json.dumps(record) + "\n")
    finally:
        if args.out:
            out.close()


if __name__ == "__main__":
    main()
