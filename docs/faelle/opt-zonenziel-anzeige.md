# opt-zonenziel-anzeige: Fortschritt des Zonenziels fehlt beim Client

## Reproduktion

Ein PC, zwei Instanzen. In PowerShell
`DDCOOP_TEST_MAP_PROFILE=/Game/Map_Generation/Blueprints/MapEvents/TitanBlockage/MGP_TitanBlockage.MGP_TitanBlockage` setzen, dann
`Start-Session.ps1 -Build -Mod -Tunnel Lobby -Godmode -Map /Game/Maps/DDDynamic_Map -Brief`.
Der Host zeigt oben die Leiste des Zonenziels „Töte die Titanen“, der Client ohne Fix nicht.

## Belege

- [Gemeldet] Live-Test 13.11.2026 abends (zwei PCs, Build `26495302`): Der Client sieht die Fortschrittsanzeige nicht, im Log keine Zeile zum Ziel.
- [Belegt, Asset-Index] Die Zone ist das Kartenprofil `MGP_TitanBlockage`; im selben Ordner liegen `BP_KillTitansObjective` und
  `BP_Titan_BlockEvent_Manager`. Das Schwesterprofil `MGP_HighwayBlockage` hat `BP_HighwayDestroyObjective` und
  `BP_Highway_BlockEvent_Manager`.
