// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
namespace updater::detail {
struct Prepared {std::wstring stage,target;std::string version;std::vector<std::string> files;};
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
std::wstring FullPath(const std::wstring& path);
std::wstring ModulePath();
std::wstring CreateStage();
bool OwnedStage(const std::wstring& stage);
void RemoveStage(const std::wstring& stage) noexcept;
std::vector<unsigned char> ReadFileBytes(const std::wstring& path,std::uint64_t limit);
void WriteFileBytes(const std::wstring& path,const std::vector<unsigned char>& bytes);
std::string HashBytes(const std::vector<unsigned char>& bytes);
std::string HashFile(const std::wstring& path);
void ValidateExecutable(const std::wstring& path,const std::string& version);
void ExtractPackage(const std::wstring& stage,std::atomic<bool>& cancel);
bool LaunchInstaller(const Prepared& prepared,std::string& error);
void AcknowledgeStartup(const std::wstring& exe,const std::string& version);
}
