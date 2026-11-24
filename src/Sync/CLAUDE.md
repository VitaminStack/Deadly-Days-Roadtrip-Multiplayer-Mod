# Sync

Legt fest, was zwischen Host und Client abgeglichen wird. Darf einbinden: `Core`, `Net`, `Game`.

## Muster

- Lokal zuerst: native Abläufe und Darstellung beim Client nutzen, nur das Nötige übertragen. Kein zweiter Besitzer für gemeinsamen Zustand.
- Je Thema ein `*State.h` (Format, reine Logik, getestet in `tests/NetTests`) und ein `*Sync.cpp` (Senden, Empfangen, Anwenden über `Game`).
- Bestätigung mit stabilen IDs und Revision, Ablehnung nimmt die lokale Vorhersage zurück.

## Wo was liegt

- `Inventory/`: Airdrop-Auswahl und Inventar-Edits (`RewardSync.cpp`)
- `Players/`: Figurenwahl, Pausen
- `World/`: Karte, Bedrohungsstufe
- `Enemies/`, `Combat/`: Warnanzeigen, Schadenszahlen, Waffensounds
- `ProgressSync.cpp`, `DesyncSync.cpp`, `SessionLog.cpp`: Fortschritt, Desync-Melder, Log-Zustandszeile

## Tests

`tests/NetTests` (`*Tests.cpp` je State), lokal `bin\Release\NetTests.exe`.

## Nicht tun

- Keine Nachricht für Werte, die nativ replizieren oder ableitbar sind, siehe [Client-lokal-Audit](../../docs/CLIENT_LOCAL_AUDIT.md).

## Mehr

[README.md](README.md), [Kontrollprotokoll](../../docs/CONTROL_PROTOCOL.md).
