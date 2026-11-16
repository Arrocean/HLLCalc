// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <string>

namespace i18n {
enum class Language { Chinese, English };
// Initialize/SetLanguage belong to the UI thread. Missing/unknown preferences
// select zh-CN for a Chinese Windows UI language, otherwise en-US. Initialize
// does not write a preference; SetLanguage persists [Appearance] Language.
void Initialize(const std::wstring& ini);
Language Current();
// False on an invalid enum/path or failed write; current language is unchanged.
bool SetLanguage(Language language);
// Immutable dictionary literals; unknown text and internal ##/### IDs are kept.
// Display labels with ImGui IDs should be split by the caller before translation.
const char* Tr(const char* chinese);
// Also handles documented dynamic prefixes/suffixes while preserving their data.
std::string Translate(const std::string& text);
// A single NUL-terminated display string, not a Win32 MULTI_SZ/filter block.
std::wstring Wide(const wchar_t* chinese);
// Read-only dictionary diagnostics for unit tests and future additions.
std::size_t EntryCount();
bool ValidateDictionary(std::string& error);
}
