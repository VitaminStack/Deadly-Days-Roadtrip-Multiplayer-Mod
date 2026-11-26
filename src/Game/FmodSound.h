#pragma once

#include <cstdint>

// Startet einen FMOD-Sound lokal über die Blueprint-Bibliothek des Spiels (UFMODBlueprintStatics::PlayEventAtLocation), so wie
// es die Blueprints des Spiels selbst tun. Für Sounds, die das Spiel nur beim Host auslöst und die der Client aus einem
// vorhandenen Signal selbst abspielt (opt-gegner-sound, opt-airdrop-sound). Nur Game Thread.
namespace Game::FmodSound
{
// Am Ort des Actors (ComponentToWorld seiner Wurzel). Falsch, wenn Event, Actor oder die Bibliothek fehlen.
bool PlayAtActor(uintptr_t Actor, uintptr_t Event);
// An einem Weltort ohne Drehung. WorldContext ist ein beliebiges Objekt der Welt.
bool PlayAtLocation(uintptr_t WorldContext, uintptr_t Event, double X, double Y, double Z);
// Ohne Ort (UFMODBlueprintStatics::PlayEvent2D), wie die Ansage des Sensenmanns beim Host (opt-sensenmann-ansage-client).
bool Play2D(uintptr_t WorldContext, uintptr_t Event);
}
