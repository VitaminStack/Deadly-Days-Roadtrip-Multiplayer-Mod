"""Fasst die Tunnel-Zeilen eines Spike-Logs zusammen: Verbindung, Ping, Qualitaet, Route, Abbrueche.

  py tools/spike/tunnel_report.py <spike-log> [--role Host|Client]

SteamIDs und IP-Adressen ausser 127.0.0.1 werden maskiert, die Ausgabe darf so in einen PR. Exitcode 0, wenn die
Verbindung stand und Ping und Route im Log sind, sonst 1. Siehe docs/TESTING.md, Abschnitt "Test auf zwei PCs".
"""
import argparse
import re
import statistics
import sys
from pathlib import Path

STEAM_ID = re.compile(r'\b7656119\d{10}\b|\[U:1:\d+\]|steamid:\d+', re.IGNORECASE)
IPV4 = re.compile(r'\b(?:\d{1,3}\.){3}\d{1,3}\b')
STATS = re.compile(r'Tunnel: Verbindung (\d+) (verbunden|wartet), zu Steam (\d+) Pakete/(\d+) B, '
                   r'von Steam (\d+) Pakete/(\d+) B, verworfen (\d+), Ping (-?\d+) ms, Qualitaet (-?[\d.]+)')


def mask(line):
    """Entfernt alles, was ein Konto oder einen Anschluss erkennbar macht."""
    line = STEAM_ID.sub('<steamid>', line)
    return IPV4.sub(lambda m: m.group(0) if m.group(0) == '127.0.0.1' else '<ip>', line)


def summarize(lines):
    pings, last, routes, ends, problems = [], None, [], [], []
    connected = False
    for raw in lines:
        line = raw.strip()
        if not line.startswith('Tunnel:'):
            continue
        stats = STATS.match(line)
        if stats:
            if stats.group(2) == 'verbunden':
                connected = True
                ping = int(stats.group(8))
                if ping >= 0:
                    pings.append(ping)
                last = stats
            continue
        if line.startswith('Tunnel: Route '):
            route = mask(line[len('Tunnel: Route '):].strip())
            if route and route not in routes:
                routes.append(route)
        elif line.startswith('Tunnel: Verbindung beendet'):
            ends.append(mask(line))
        elif 'abgelehnt' in line or 'nicht verfuegbar' in line or 'fehlt' in line:
            problems.append(mask(line))
    return {
        'connected': connected,
        'pings': pings,
        'last': last,
        'routes': routes,
        'ends': ends,
        'problems': problems,
        'ok': connected and bool(pings) and bool(routes),
    }


def render(summary, role):
    out = [f'Tunnel-Bericht {role}'.strip(), '']
    out.append('Verbindung: ' + ('stand' if summary['connected'] else 'nie verbunden'))
    pings = summary['pings']
    if pings:
        out.append(f'Ping: min {min(pings)} ms, Median {statistics.median(pings):g} ms, max {max(pings)} ms '
                   f'({len(pings)} Messungen alle 5 s)')
    else:
        out.append('Ping: keine Messung')
    last = summary['last']
    if last:
        out.append(f'Zuletzt: zu Steam {last.group(3)} Pakete/{last.group(4)} B, von Steam {last.group(5)} Pakete/'
                   f'{last.group(6)} B, verworfen {last.group(7)}, Qualitaet {float(last.group(9)):.2f}')
    out.append('Route:' if summary['routes'] else 'Route: keine Zeile im Log')
    out += [f'  {route}' for route in summary['routes']]
    for title, key in (('Abbrueche', 'ends'), ('Probleme', 'problems')):
        if summary[key]:
            out.append(f'{title}:')
            out += [f'  {line}' for line in summary[key]]
    out += ['', 'Ergebnis: ' + ('Ping und Route belegt' if summary['ok'] else 'NICHT belegt')]
    return '\n'.join(out)


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('log', type=Path)
    parser.add_argument('--role', default='')
    args = parser.parse_args()
    summary = summarize(args.log.read_text(encoding='utf-8', errors='replace').splitlines())
    print(render(summary, args.role))
    return 0 if summary['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())
