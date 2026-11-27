#pragma once

#include <cstdint>
#include <string>
#include <vector>

// client-haenger-nach-niederlage: Stapelprobe eines anderen Threads im eigenen Prozess, für den Wächter des Game Threads
// (GameThread.cpp). Der Thread steht nur, solange Register und Stapel kopiert werden: kein Heap, keine Sperre, kein Log.
// Ausgewertet wird danach auf der Kopie, der Game Thread läuft dann schon weiter.
namespace Game::Stall
{
struct StackSample
{
    // Rücksprungadressen, innerste zuerst.
    std::vector<uint64_t> Frames;
    // Blueprint-Funktionen (UFunction), deren Zeiger auf dem kopierten Stapel liegen, als "Klasse.Funktion". Grob: kann
    // alte Einträge zeigen, wie tools/spike/gamethread.py.
    std::vector<std::string> Functions;
    bool Ok = false;
};

// Hält den Thread kurz an und kopiert Register und bis zu StackBytes Stapel. Danach Abwicklung auf der Kopie.
StackSample CaptureStack(unsigned long ThreadId, size_t StackBytes = 0x10000, size_t MaxFrames = 32, size_t MaxFunctions = 8);

// "Modul+0xRVA" ohne Pfad, sonst die Adresse. Logs dürfen keine lokalen Pfade enthalten.
std::string DescribeAddress(uint64_t Address);

// Eine Logzeile: Frames und Blueprint-Funktionen.
std::string DescribeSample(const StackSample& Sample);
}
