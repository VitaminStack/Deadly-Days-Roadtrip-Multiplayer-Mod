#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

// lobby-rauswurf: Wann der Host einen verbundenen Mitspieler wegen fehlender Lobby-Mitgliedschaft trennt. Reine Logik ohne
// Steam und ohne Uhr, die Zeit kommt als NowMs herein. Siehe docs/faelle/lobby-rauswurf.md.
namespace Net
{
// Wie brauchbar eine Mitgliederliste von Steam ist. Nur eine gültige Liste darf jemanden belasten.
enum class LobbyListState
{
    Valid,
    // Steam meldet niemanden, etwa weil das eigene Konto gerade nicht in der Lobby geführt wird.
    Empty,
    // Das eigene Konto fehlt in der eigenen Lobby: Die Liste ist veraltet oder gehört zu keiner gültigen Mitgliedschaft.
    OwnMissing,
    // Steam nennt mehr Mitglieder, als es SteamIDs liefert.
    Incomplete,
};

std::string_view ToString(LobbyListState State);

// Members wie von SteamLobbyApi::Members, ReportedCount aus MemberCount, Own ist das Lobby-Konto dieser Instanz.
LobbyListState ClassifyLobbyList(const std::vector<uint64_t>& Members, int ReportedCount, uint64_t Own);

struct MembershipRules
{
    // So lange muss ein Konto in gültigen Listen fehlen.
    unsigned RevokeAfterMs = 10000;
    // Und mindestens so oft hintereinander in einer gültigen Liste fehlen.
    unsigned MinMisses = 3;
    // Mehr Zeit rechnet eine einzelne Liste nicht an. Steht der Game Thread, kommt keine neue Liste, und eine alte Liste
    // altert so nicht in eine Trennung hinein.
    unsigned MaxStepMs = 1000;
};

// Stand eines verbundenen Kontos über die Listen hinweg.
struct MemberWatch
{
    // Angerechnete Zeit, in der das Konto in gültigen Listen fehlt.
    uint64_t MissingMs = 0;
    // Gültige Listen hintereinander ohne das Konto.
    unsigned Misses = 0;
    // Nummer und Zeit der zuletzt bewerteten Liste.
    uint64_t Sequence = 0;
    uint64_t SampleMs = 0;
    // Die zuletzt bewertete Liste war unbrauchbar.
    bool Paused = false;
};

enum class MemberVerdict
{
    Present,
    // Fehlt, Frist läuft.
    Missing,
    // Liste unbrauchbar, Frist steht.
    Paused,
    Revoke,
};

// Bewertet die Liste mit der Nummer Sequence. Eine schon bewertete Nummer ändert nichts. Contains: Das Konto steht in der
// Liste. Eine unbrauchbare Liste hält die Frist an, ohne sie zurückzusetzen.
MemberVerdict Judge(MemberWatch& Watch, const MembershipRules& Rules, uint64_t Sequence, LobbyListState State, bool Contains,
                    uint64_t NowMs);
}
