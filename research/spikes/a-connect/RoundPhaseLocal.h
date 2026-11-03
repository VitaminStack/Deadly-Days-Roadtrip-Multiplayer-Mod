// Windows same-user/same-PC diagnostic file transport. No authentication or network claim.
#pragma once
#include "RoundPhase.h"
#include <algorithm>
#include <windows.h>
#include <string>

namespace Spike::RoundPhase
{
inline bool WriteLocal(const std::wstring& Path, const Wire& Bytes)
{
    const auto Temporary = Path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    HANDLE File = CreateFileW(Temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (File == INVALID_HANDLE_VALUE)
        return false;
    DWORD Count = 0;
    const bool Good = WriteFile(File, Bytes.data(), static_cast<DWORD>(Bytes.size()), &Count, nullptr) &&
        Count == Bytes.size() && FlushFileBuffers(File);
    CloseHandle(File);
    const bool Published = Good && MoveFileExW(Temporary.c_str(), Path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!Published)
        DeleteFileW(Temporary.c_str());
    return Published;
}
inline bool ReadLocal(const std::wstring& Path, Wire& Bytes)
{
    HANDLE File = CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (File == INVALID_HANDLE_VALUE)
        return false;
    std::array<uint8_t, kWireSize + 1> Buffer{};
    DWORD Count = 0;
    const bool Good = ReadFile(File, Buffer.data(), static_cast<DWORD>(Buffer.size()), &Count, nullptr) &&
        Count == kWireSize;
    CloseHandle(File);
    Snapshot Value;
    if (!Good || !Decode(Buffer.data(), Count, Value))
        return false;
    std::copy_n(Buffer.begin(), kWireSize, Bytes.begin());
    return true;
}
}
