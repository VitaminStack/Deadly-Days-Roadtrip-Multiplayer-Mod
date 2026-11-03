"""Zaehlt Belege aus EventProbe-Logs; Installation ist kein Ausfuehrungsbeleg.

Keine Graphrekonstruktion und keine automatische Schlussfolgerung zur Zustellung.
Die enter-Ausgabe ist pro Funktion begrenzt. Explizite count-Snapshots stehen
deshalb getrennt von den tatsaechlich ausgegebenen enter-Zeilen.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re


def summarize(lines):
    installed, observed, counters, authority, actions = set(), Counter(), {}, [], []
    installed_count = None
    for line in lines:
        stamp, separator, text = line.strip().partition(" ")
        if not separator or not stamp.isdecimal():
            continue
        if text.startswith("hook "):
            installed.add(text[5:])
        elif text.startswith("hooks=") and text[6:].isdigit():
            installed_count = int(text[6:])
        elif match := re.fullmatch(r"enter (\S+) class=\S+ depth=\d+", text):
            observed[match[1]] += 1
        elif match := re.fullmatch(r"count (\S+)=(\d+)", text):
            counters[match[1]] = int(match[2])
        elif text in ("authority-result=true", "authority-result=false"):
            authority.append(text.endswith("=true"))
        elif match := re.fullmatch(r"action=(\d+)", text):
            actions.append(int(match[1]))
    return {"installed": sorted(installed), "reported_hook_count": installed_count,
            "installation_count_matches": installed_count == len(installed),
            "observed_enter_lines": dict(sorted(observed.items())),
            "last_counter_snapshot": dict(sorted(counters.items())),
            "authority_results": authority, "actions": actions}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.logs:
        with path.open(encoding="utf-8-sig") as stream:
            print(json.dumps(summarize(stream), sort_keys=True))


if __name__ == "__main__":
    main()
