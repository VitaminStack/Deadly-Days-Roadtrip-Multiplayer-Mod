#pragma once

// Inline-Hooks. Vor Initialize schlägt jedes Install fehl, siehe src/Core/README.md.
namespace Core::Hooks
{
bool Initialize();
void Shutdown();

bool Install(void* Target, void* Detour, void** Original);
bool Remove(void* Target);

template <typename Fn> bool Install(Fn Target, Fn Detour, Fn* Original)
{
    return Install(reinterpret_cast<void*>(Target), reinterpret_cast<void*>(Detour), reinterpret_cast<void**>(Original));
}
}
