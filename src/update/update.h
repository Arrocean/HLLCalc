// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <string>

namespace updater {
enum class Phase { Idle, Checking, UpToDate, Available, Downloading, Ready, Launching, Error, Cancelled };
struct Snapshot {
    Phase phase=Phase::Idle;
    std::string currentVersion,latestVersion,releaseUrl,notes,error;
    std::uint64_t downloaded=0,total=0;
};
// Call Initialize/CheckAsync/DownloadAsync/Install/Shutdown on the application UI
// thread. Network work never runs there; GetSnapshot and Cancel are thread-safe.
bool Initialize(const std::string& currentVersion,const std::wstring& exePath,
    const std::string& repo="Arrocean/HLLCalc");
void CheckAsync();
void DownloadAsync();
Snapshot GetSnapshot();
void Cancel();
void Shutdown();
// True means a local helper was launched. The host should now save settings and
// quit normally; the helper waits for this process, atomically replaces its
// executable, updates allowlisted docs/license files, and restarts it. User
// configuration is preserved and no downloaded scripts are executed.
bool Install();
// Call once only after the first successfully presented UI frame. This confirms
// an updated process is healthy; the helper otherwise rolls the transaction back.
void AcknowledgeReady();
// Call at the very beginning of wWinMain, before creating a mutex or UI.
// If true, return exitCode immediately (this invocation is the updater helper).
bool HandleHelperCommandLine(int& exitCode);
}
