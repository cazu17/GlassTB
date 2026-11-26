#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/profile.hpp"

// Stores user profiles as UTF-8 JSON files in <data folder>\profiles and exposes
// the built-in profiles. All operations validate their input and report failures
// through the error out-parameter instead of throwing.
class ProfileManager {
public:
	struct Entry {
		Profile profile;
		bool builtIn;
	};

	static constexpr std::uintmax_t MAX_FILE_SIZE = 256 * 1024;

	explicit ProfileManager(std::filesystem::path folder);

	// Built-in profiles first, then user profiles sorted by name. Invalid files are skipped and logged.
	std::vector<Entry> List() const;
	std::optional<Entry> Find(std::wstring_view name) const;

	// Creates or replaces a user profile. Built-in names are reserved.
	bool Save(const Profile &profile, std::wstring &error) const;
	bool Delete(std::wstring_view name, std::wstring &error) const;

	// Copies a profile file into the profiles folder, renaming it if the name is taken.
	std::optional<Profile> Import(const std::filesystem::path &file, std::wstring &error) const;
	bool Export(std::wstring_view name, const std::filesystem::path &file, std::wstring &error) const;

	std::wstring UniqueName(std::wstring_view base) const;

	const std::filesystem::path &Folder() const noexcept { return m_Folder; }

private:
	std::filesystem::path m_Folder;

	std::filesystem::path PathFor(std::wstring_view name) const;
	static bool IsBuiltInName(std::wstring_view name);
	static std::optional<std::wstring> ReadUtf8File(const std::filesystem::path &file, std::wstring &error);
	static bool WriteUtf8FileAtomic(const std::filesystem::path &file, std::wstring_view content, std::wstring &error);
};
