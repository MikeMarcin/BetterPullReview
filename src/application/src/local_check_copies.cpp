#include "bpr/application/local_check_copies.hpp"

#include "bpr/application/agent_scratch_folder.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <ranges>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr auto manifest_name = "manifest";

		/// A relative path of plain parts: no root, no `.` or `..`, no empty part.
		auto plain_relative(const std::string_view aPath) -> bool
		{
			if (aPath.empty() || aPath.front() == '/' || aPath.find('\\') != std::string_view::npos || aPath.find('\0') != std::string_view::npos) {
				return false;
			}
			return std::ranges::all_of(aPath | std::views::split('/'), [](const auto aPart) {
				const auto part = std::string_view{ aPart.begin(), aPart.end() };
				return !part.empty() && part != "." && part != "..";
			});
		}

		/// Every folder between the copy and the file is a real folder, never a link.
		auto folders_plain(const std::filesystem::path& aCopy, const std::filesystem::path& aRelative) -> bool
		{
			auto at    = aCopy;
			auto error = std::error_code{ };
			for (const auto& part : aRelative.parent_path()) {
				at /= part;
				const auto status = std::filesystem::symlink_status(at, error);
				if (std::filesystem::is_symlink(status) || (std::filesystem::exists(status) && !std::filesystem::is_directory(status))) {
					return false;
				}
			}
			return true;
		}

		auto digest_of_file(const std::filesystem::path& aFile) -> std::optional<std::array<std::uint8_t, 32>>
		{
			auto error = std::error_code{ };
			if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(aFile, error))) {
				return std::nullopt;
			}
			auto in = std::ifstream{ aFile, std::ios::binary };
			if (!in) {
				return std::nullopt;
			}
			const auto bytes = std::vector<std::uint8_t>{ std::istreambuf_iterator<char>{ in }, std::istreambuf_iterator<char>{ } };
			return core::sha256(bytes);
		}
	} // namespace

	local_check_copies::local_check_copies(std::filesystem::path aRoot)
		: Root{ std::move(aRoot) }
	{
	}

	auto local_check_copies::key(const std::string_view aDraft, const std::uint64_t aCandidate) -> std::string
	{
		return "check-" + std::string{ aDraft } + "-" + std::to_string(aCandidate);
	}

	auto local_check_copies::locate(const std::string_view aDraft, const std::uint64_t aCandidate) const -> local_check_copy
	{
		auto copy    = local_check_copy{ .Key = key(aDraft, aCandidate) };
		copy.Folder  = Root / copy.Key;
		copy.Copy    = copy.Folder / "copy";
		copy.Scratch = copy.Folder / "scratch";
		return copy;
	}

	auto local_check_copies::ready(const local_check_copy& aCopy) const -> bool
	{
		auto error = std::error_code{ };
		return std::filesystem::is_regular_file(std::filesystem::symlink_status(aCopy.Folder / manifest_name, error)) && std::filesystem::is_directory(std::filesystem::symlink_status(aCopy.Copy, error));
	}

	auto local_check_copies::begin(const local_check_copy& aCopy) -> bool
	{
		auto error = std::error_code{ };
		std::filesystem::create_directories(Root, error);
		if (!valid_agent_scratch_key(aCopy.Key) || !remove_agent_scratch_folder(Root, aCopy.Key)) {
			return false;
		}
		const auto folder = prepare_agent_scratch_folder(Root, aCopy.Key);
		if (!folder || !std::filesystem::create_directory(aCopy.Copy, error) || !std::filesystem::create_directory(aCopy.Scratch, error)) {
			return false;
		}
		Writing[aCopy.Key].clear();
		return true;
	}

	auto local_check_copies::put(const local_check_copy& aCopy, const std::string_view aPath, const std::span<const std::uint8_t> aBytes, const bool aExecutable) -> bool
	{
		const auto writing = Writing.find(aCopy.Key);
		if (writing == Writing.end() || !plain_relative(aPath)) {
			return false;
		}
		const auto relative = std::filesystem::path{ std::string{ aPath } };
		if (!folders_plain(aCopy.Copy, relative)) {
			return false;
		}
		const auto file  = aCopy.Copy / relative;
		auto       error = std::error_code{ };
		std::filesystem::create_directories(file.parent_path(), error);
		if (error) {
			return false;
		}
		{
			auto out = std::ofstream{ file, std::ios::binary | std::ios::trunc };
			out.write(reinterpret_cast<const char*>(aBytes.data()), static_cast<std::streamsize>(aBytes.size()));
			if (!out) {
				return false;
			}
		}
		const auto mode = aExecutable ? std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec | std::filesystem::perms::others_read | std::filesystem::perms::others_exec
									  : std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read | std::filesystem::perms::others_read;
		std::filesystem::permissions(file, mode, std::filesystem::perm_options::replace, error);
		writing->second[std::string{ aPath }] = core::sha256(aBytes);
		return true;
	}

	auto local_check_copies::seal(const local_check_copy& aCopy) -> bool
	{
		const auto writing = Writing.find(aCopy.Key);
		if (writing == Writing.end()) {
			return false;
		}
		auto text = std::string{ };
		for (const auto& [path, digest] : writing->second) {
			text += core::lower_hex(digest);
			text += ' ';
			text += path;
			text += '\n';
		}
		Writing.erase(writing);
		auto out = std::ofstream{ aCopy.Folder / manifest_name, std::ios::binary | std::ios::trunc };
		out << text;
		return static_cast<bool>(out);
	}

	auto local_check_copies::changed(const local_check_copy& aCopy) const -> std::vector<std::string>
	{
		auto in      = std::ifstream{ aCopy.Folder / manifest_name, std::ios::binary };
		auto changed = std::vector<std::string>{ };
		for (auto line = std::string{ }; std::getline(in, line);) {
			const auto space = line.find(' ');
			if (space == std::string::npos) {
				continue;
			}
			const auto path   = line.substr(space + 1U);
			const auto digest = digest_of_file(aCopy.Copy / std::filesystem::path{ path });
			if (!digest || core::lower_hex(*digest) != line.substr(0U, space)) {
				changed.push_back(path);
			}
		}
		return changed;
	}

	void local_check_copies::prune(const std::string_view aDraft, const std::optional<std::uint64_t> aKeep)
	{
		const auto prefix = key(aDraft, 0U).substr(0U, key(aDraft, 0U).size() - 1U);
		const auto kept   = aKeep ? key(aDraft, *aKeep) : std::string{ };
		for (const auto& entry : survey_agent_scratch_root(Root)) {
			if (entry.Key.starts_with(prefix) && entry.Key != kept) {
				static_cast<void>(remove_agent_scratch_folder(Root, entry.Key));
			}
		}
	}

	auto local_check_copies::root() const noexcept -> const std::filesystem::path&
	{
		return Root;
	}
}
