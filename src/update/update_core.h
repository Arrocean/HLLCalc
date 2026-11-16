// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace updater::detail {
struct Version { unsigned major=0,minor=0,patch=0;std::vector<std::string> prerelease; };
bool ParseVersion(const std::string& text,Version& out);
int CompareVersions(const Version& a,const Version& b);
struct Asset {std::string name,url;std::uint64_t size=0;};
struct Release {std::string version,url,notes;Asset archive,checksums;};
bool ValidRepository(const std::string& repo);
bool AllowedHttpsUrl(const std::string& url);
bool ParseRelease(const std::string& json,const std::string& repo,Release& out,std::string& error);
bool ParseChecksum(const std::string& text,const std::string& filename,std::string& hash,std::string& error);
bool SafeArchivePath(const std::string& name);
bool AllowedPayloadFile(const std::string& name);
bool ValidateArchive(const std::vector<unsigned char>& zip,std::vector<std::string>& files,std::string& error);
constexpr std::uint64_t MaxArchiveBytes=64ull*1024*1024;
constexpr std::uint64_t MaxExecutableBytes=32ull*1024*1024;
}
