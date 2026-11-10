#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// Pausemenü des Clients beim Host (pausen-sync). Lokal bleibt das native Pausemenü mit Inventaransicht beim Client. Geteilt
// wird nur, ob es offen ist: Der Host hält dann eine eigene Zeitanfrage mit den Werten seines Pausemenüs, die native
// Replikation der Weltzeit (WorldSettings) trägt die Pause zum Client. Der Host entscheidet allein über die Weltzeit.
// Freigabe ohne Nachricht: Trennung, 3 s ohne Meldung oder Weltwechsel. Siehe docs/CONTROL_PROTOCOL.md, Pausemenü.
namespace Sync::Pause
{
inline constexpr uint16_t ClientPauseType = 0x700;
inline constexpr size_t ClientPauseBytes = 9;
inline constexpr uint64_t ResendMs = 500;
// Nach dem Schließen schickt der Client die Freigabe noch so lange, falls ein Paket fehlt.
inline constexpr uint64_t ReleaseRepeatMs = 3000;
// Ohne frische Meldung gibt der Host die Pause frei. Sechs verpasste Wiederholungen.
inline constexpr uint64_t LeaseMs = 3000;

// Sitzung: Kennung je Öffnen des Pausemenüs, ungleich 0. Open 0 mit der zuletzt offenen Sitzung heißt geschlossen.
struct Report
{
    uint64_t Session = 0;
    bool Open = false;
    bool operator==(const Report&) const = default;
};

inline bool Valid(const Report& Value)
{
    return Value.Session != 0;
}

inline std::vector<uint8_t> Encode(const Report& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    for (unsigned i = 0; i < 8; ++i)
        Out.push_back(static_cast<uint8_t>(Value.Session >> (8 * i)));
    Out.push_back(Value.Open ? 1 : 0);
    return Out;
}

inline std::optional<Report> Decode(std::span<const uint8_t> In)
{
    if (In.size() != ClientPauseBytes || In[8] > 1)
        return std::nullopt;
    Report Value;
    for (unsigned i = 0; i < 8; ++i)
        Value.Session |= uint64_t(In[i]) << (8 * i);
    Value.Open = In[8] == 1;
    return Valid(Value) ? std::optional(Value) : std::nullopt;
}

// Client: aus dem lokalen Zustand des Pausemenüs die nächste Meldung. Rein, Zeit in ms von außen.
class ClientReporter
{
public:
    // Base macht die Sitzungen je Prozess eindeutig, ungerade und damit ungleich 0.
    explicit ClientReporter(uint64_t Base) : Next(Base | 1) {}

    // Open: Pausemenü lokal offen. Liefert die zu sendende Meldung oder nichts.
    std::optional<Report> Update(bool Open, uint64_t Now)
    {
        if (Open && !Current.Open)
        {
            Current = {Next, true};
            Next += 2;
            Due = Now;
        }
        else if (!Open && Current.Open)
        {
            Current.Open = false;
            ReleaseUntil = Now + ReleaseRepeatMs;
            Due = Now;
        }
        if (!Current.Session || (!Current.Open && Now >= ReleaseUntil) || Now < Due)
            return std::nullopt;
        Due = Now + ResendMs;
        return Current;
    }

    // Verbindung neu oder weg: nichts nachschicken. Ist das Menü danach noch offen, beginnt eine neue Sitzung.
    void Reset()
    {
        Current = {};
        ReleaseUntil = 0;
    }

private:
    Report Current;
    uint64_t Next = 1, Due = 0, ReleaseUntil = 0;
};

// Warum der Host eine Pause des Clients nicht mehr hält.
enum class Release
{
    None,
    Closed,   // Client hat geschlossen
    Timeout,  // keine Meldung innerhalb LeaseMs
    World,    // Weltwechsel beim Host
    Peer      // Verbindung weg oder neu
};

// Host: hält die Pause genau einer Client-Sitzung. Rein, Zeit in ms und Welt als lokale Kennung von außen.
class HostHold
{
public:
    void Receive(const Report& Value, uint64_t Now)
    {
        if (Value.Open)
        {
            // Eine einmal beendete Sitzung kommt nicht zurück, etwa eine späte Wiederholung nach Zeitablauf.
            if (Value.Session == Ended)
                return;
            Session = Value.Session;
            Seen = Now;
        }
        else if (Value.Session == Session)
        {
            Ended = Session;
            Session = 0;
            Pending = Release::Closed;
        }
    }

    // Je Takt. Peer: Kennung der Verbindung, 0 ohne. World: Kennung der aktuellen Welt, 0 ohne.
    void Tick(uint64_t Peer, uint64_t World, uint64_t Now)
    {
        if (Peer != LastPeer)
        {
            LastPeer = Peer;
            End(Release::Peer);
            Ended = 0;
        }
        if (World != LastWorld)
        {
            LastWorld = World;
            End(Release::World);
        }
        if (Session && Now - Seen > LeaseMs)
            End(Release::Timeout);
        if (!Peer || !World)
            End(Release::Peer);
    }

    // Soll der Host jetzt pausieren?
    bool Held() const { return Session != 0; }
    uint64_t HeldSession() const { return Session; }

    // Letzter Freigabegrund seit dem vorigen Aufruf, für das Log.
    Release TakeRelease()
    {
        const auto Value = Pending;
        Pending = Release::None;
        return Value;
    }

private:
    void End(Release Why)
    {
        if (!Session)
            return;
        Ended = Session;
        Session = 0;
        Pending = Why;
    }

    uint64_t Session = 0, Ended = 0, Seen = 0, LastPeer = 0, LastWorld = 0;
    Release Pending = Release::None;
};
}
