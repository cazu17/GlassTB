#include "profilemanager.hpp"
#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <wil/resource.h>

#include "../../ProgramLog/error/std.hpp"
#include "../../ProgramLog/error/win32.hpp"
#include "../../ProgramLog/log.hpp"

namespace {
	bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept
	{
		return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), true) == CSTR_EQUAL;
	}
}

ProfileManager::ProfileManager(std::filesystem::path folder) : m_Folder(std::move(folder))
{
	std::error_code errc;
	std::filesystem::create_directories(m_Folder, errc);
	StdErrorCodeVerify(errc, spdlog::level::warn, L"Failed to create profiles folder");
}

bool ProfileManager::IsBuiltInName(std::wstring_view name)
{
	const auto builtIn = Profile::BuiltIn();
	return std::ranges::any_of(builtIn, [name](const Profile &p) { return EqualsIgnoreCase(p.Name, name); });
}

std::filesystem::path ProfileManager::PathFor(std::wstring_view name) const
{
	return m_Folder / (Profile::FileStem(name) + L".json");
}

std::optional<std::wstring> ProfileManager::ReadUtf8File(const std::filesystem::path &file, std::wstring &error)
{
	std::error_code errc;
	const auto size = std::filesystem::file_size(file, errc);
	if (errc)
	{
		error = std::format(L"Cannot read {}", file.native());
		return std::nullopt;
	}
	if (size > MAX_FILE_SIZE)
	{
		error = std::format(L"{} is larger than {} bytes", file.native(), MAX_FILE_SIZE);
		return std::nullopt;
	}

	std::ifstream in(file, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (!in.good() && !in.eof())
	{
		error = std::format(L"Cannot read {}", file.native());
		return std::nullopt;
	}

	// Skip a UTF-8 byte order mark if present.
	std::string_view view = bytes;
	if (view.starts_with("\xEF\xBB\xBF"))
	{
		view.remove_prefix(3);
	}
	if (view.empty())
	{
		return std::wstring { };
	}

	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, view.data(), static_cast<int>(view.size()), nullptr, 0);
	if (length <= 0)
	{
		error = std::format(L"{} is not valid UTF-8", file.native());
		return std::nullopt;
	}

	std::wstring text(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, view.data(), static_cast<int>(view.size()), text.data(), length);
	return text;
}

bool ProfileManager::WriteUtf8FileAtomic(const std::filesystem::path &file, std::wstring_view content, std::wstring &error)
{
	std::string bytes;
	if (!content.empty())
	{
		const int length = WideCharToMultiByte(CP_UTF8, 0, content.data(), static_cast<int>(content.size()), nullptr, 0, nullptr, nullptr);
		bytes.resize(static_cast<std::size_t>(length));
		WideCharToMultiByte(CP_UTF8, 0, content.data(), static_cast<int>(content.size()), bytes.data(), length, nullptr, nullptr);
	}

	auto temp = file;
	temp += L".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		out.close();
		if (out.fail())
		{
			error = std::format(L"Cannot write {}", temp.native());
			return false;
		}
	}

	if (!MoveFileEx(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		error = std::format(L"Cannot replace {} (error {})", file.native(), GetLastError());
		std::error_code ignored;
		std::filesystem::remove(temp, ignored);
		return false;
	}

	return true;
}

std::vector<ProfileManager::Entry> ProfileManager::List() const
{
	std::vector<Entry> entries;
	for (auto &profile : Profile::BuiltIn())
	{
		entries.push_back({ std::move(profile), true });
	}

	std::vector<Entry> user;
	std::error_code errc;
	for (std::filesystem::directory_iterator it(m_Folder, errc), end; !errc && it != end; it.increment(errc))
	{
		if (!it->is_regular_file() || it->path().extension() != L".json")
		{
			continue;
		}

		std::wstring error;
		std::optional<Profile> profile;
		if (const auto text = ReadUtf8File(it->path(), error))
		{
			profile = Profile::FromJson(*text, error);
		}

		if (!profile)
		{
			MessagePrint(spdlog::level::warn, std::format(L"Skipping invalid profile {}: {}", it->path().native(), error));
			continue;
		}
		if (IsBuiltInName(profile->Name) || std::ranges::any_of(user, [&](const Entry &e) { return EqualsIgnoreCase(e.profile.Name, profile->Name); }))
		{
			MessagePrint(spdlog::level::warn, std::format(L"Skipping profile {}: duplicate name {}", it->path().native(), profile->Name));
			continue;
		}

		user.push_back({ std::move(*profile), false });
	}

	std::ranges::sort(user, [](const Entry &a, const Entry &b)
	{
		return CompareStringOrdinal(a.profile.Name.c_str(), -1, b.profile.Name.c_str(), -1, true) == CSTR_LESS_THAN;
	});
	std::ranges::move(user, std::back_inserter(entries));
	return entries;
}

std::optional<ProfileManager::Entry> ProfileManager::Find(std::wstring_view name) const
{
	for (auto &entry : List())
	{
		if (EqualsIgnoreCase(entry.profile.Name, name))
		{
			return std::move(entry);
		}
	}

	return std::nullopt;
}

std::wstring ProfileManager::UniqueName(std::wstring_view base) const
{
	const auto entries = List();
	const auto taken = [&entries](std::wstring_view name)
	{
		return std::ranges::any_of(entries, [name](const Entry &e) { return EqualsIgnoreCase(e.profile.Name, name); });
	};

	std::wstring trimmed(base.substr(0, Profile::MAX_NAME_LENGTH - 5));
	if (!taken(trimmed))
	{
		return trimmed;
	}

	for (int i = 2; i < 1000; ++i)
	{
		auto candidate = std::format(L"{} ({})", trimmed, i);
		if (!taken(candidate))
		{
			return candidate;
		}
	}

	return trimmed;
}

bool ProfileManager::Save(const Profile &profile, std::wstring &error) const
{
	if (!Profile::IsValidName(profile.Name))
	{
		error = L"Invalid profile name";
		return false;
	}
	if (IsBuiltInName(profile.Name))
	{
		error = L"Built-in profile names are reserved";
		return false;
	}

	return WriteUtf8FileAtomic(PathFor(profile.Name), Profile::ToJson(profile), error);
}

bool ProfileManager::Delete(std::wstring_view name, std::wstring &error) const
{
	if (IsBuiltInName(name))
	{
		error = L"Built-in profiles cannot be deleted";
		return false;
	}
	if (!Profile::IsValidName(name))
	{
		error = L"Invalid profile name";
		return false;
	}

	std::error_code errc;
	if (!std::filesystem::remove(PathFor(name), errc))
	{
		error = errc ? std::format(L"Cannot delete profile: {}", Error::MessageFromStdErrorCode(errc)) : std::wstring(L"Profile not found");
		return false;
	}

	return true;
}

std::optional<Profile> ProfileManager::Import(const std::filesystem::path &file, std::wstring &error) const
{
	const auto text = ReadUtf8File(file, error);
	if (!text)
	{
		return std::nullopt;
	}

	auto profile = Profile::FromJson(*text, error);
	if (!profile)
	{
		return std::nullopt;
	}

	profile->Name = UniqueName(profile->Name);
	if (!Save(*profile, error))
	{
		return std::nullopt;
	}

	return profile;
}

bool ProfileManager::Export(std::wstring_view name, const std::filesystem::path &file, std::wstring &error) const
{
	const auto entry = Find(name);
	if (!entry)
	{
		error = L"Profile not found";
		return false;
	}

	return WriteUtf8FileAtomic(file, Profile::ToJson(entry->profile), error);
}
