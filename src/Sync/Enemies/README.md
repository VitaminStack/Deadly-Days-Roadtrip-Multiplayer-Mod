# Enemies

Gegner-Sync. Stand `gegnertyp` (06.11.2026): **kein eigener Code nötig**, der Ordner hält nur die Entscheidung fest.

| Frage | Antwort |
|---|---|
| Was bleibt lokal? | Darstellung, Animation und Bewegungsglättung der Gegner beim Client (`SimulatedProxy`) |
| Was wird geteilt und warum? | Nichts Eigenes. Spawn, Bewegung und Tod kommen über die native Actor-Replikation (Actor-Kanal öffnen und schließen) |
| Wer entscheidet? | Der Host: `AEnemySpawner`, Schaden und Tod laufen nur dort |
| Welcher Weg wird genutzt? | Native UE-Replikation aus Gate G1; geprüft wird über den Desync-Melder (Feld „Gegner“, seit `gegnertyp` im Abgleich) |
| Wie werden Ablehnung, Wiederholung, Neuaufbau getestet? | Join/Travel bauen die Gegner nativ neu auf. Abweichungen zeigt der Melder; `tools/spike/enemy_sync.py` misst Spawn und Tod je Gegner |

Nicht repliziert und darum hier nicht nachgebaut: Gegner-HP beim Client (dort 0/0) und die gepoolten Tod-Effekte
(`EnemyChaosDestructionProxyActor`, Schadenszahlen), siehe `opt-blut` und `vfx-sound`. Bosse und weitere Gegnertypen prüft
`gegner-alle`. [Befund und Messwerte](../../../docs/GAME_ANALYSIS.md#ein-gegnertyp-synchron-gegnertyp)
