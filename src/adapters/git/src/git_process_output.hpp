#ifndef BPR_SRC_ADAPTERS_GIT_SRC_GIT_PROCESS_OUTPUT_HPP
#define BPR_SRC_ADAPTERS_GIT_SRC_GIT_PROCESS_OUTPUT_HPP

// The readers of what the installed Git writes to standard output. They take
// the captured bytes and nothing else: no process, file or Qt. Git is trusted
// to run, but what it prints carries repository-controlled names and must
// still parse exactly or be refused as malformed. [[BPR-0970]] fuzzes them
// through this header, so the readers it calls are the ones the adapter uses.

#include "bpr/application/git_process_port.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace bpr::adapters::git::detail {
	[[nodiscard]] constexpr auto hex_value(const char aValue) noexcept
		-> std::optional<std::uint8_t>
	{
		if (aValue >= '0' && aValue <= '9') {
			return static_cast<std::uint8_t>(aValue - '0');
		}
		if (aValue >= 'a' && aValue <= 'f') {
			return static_cast<std::uint8_t>(aValue - 'a' + 10);
		}
		return std::nullopt;
	}

	/// \brief Reads an object name of exactly the algorithm's width, in
	/// lowercase hexadecimal.
	template <typename Object>
	[[nodiscard]] auto parse_object(
		const std::string_view                aText,
		const application::git_hash_algorithm aAlgorithm) -> std::optional<Object>
	{
		const auto byteCount = aAlgorithm == application::git_hash_algorithm::sha1 ? 20U : 32U;
		if (aText.size() != byteCount * 2U) {
			return std::nullopt;
		}
		Object result{ .Algorithm = aAlgorithm };
		for (std::size_t index = 0; index < byteCount; ++index) {
			const auto high = hex_value(aText[index * 2U]);
			const auto low  = hex_value(aText[index * 2U + 1U]);
			if (!high.has_value() || !low.has_value()) {
				return std::nullopt;
			}
			result.Digest[index] = static_cast<std::uint8_t>(
				static_cast<std::uint8_t>(*high << 4U) | *low);
		}
		if (!application::valid(result)) {
			return std::nullopt;
		}
		return result;
	}

	[[nodiscard]] inline auto valid_tree_entry_path(
		const std::filesystem::path& aPath) noexcept -> bool
	{
		const auto text = aPath.generic_u8string();
		return !text.empty() && !aPath.is_absolute()
			&& text.size() <= application::maximum_git_tree_entry_path_bytes
			&& text.front() != u8'/'
			&& text.find(u8'\0') == decltype(text)::npos
			&& std::ranges::none_of(
				aPath,
				[](const auto& aComponent) {
					return aComponent == "." || aComponent == "..";
				});
	}

	[[nodiscard]] inline auto tree_entry_path_text(
		const std::filesystem::path& aPath) -> std::string
	{
		const auto text = aPath.generic_u8string();
		return {
			reinterpret_cast<const char*>(text.data()),
			text.size()
		};
	}

	[[nodiscard]] inline auto tree_entry_path(
		const std::string_view aText) -> std::filesystem::path
	{
		const auto* const begin = reinterpret_cast<const char8_t*>(aText.data());
		return std::filesystem::path{
			std::u8string{ begin, begin + aText.size() }
		};
	}

	/// \brief `git merge-base --all`: one object name per line, none repeated.
	/// Exit one with nothing printed means no common ancestor.
	[[nodiscard]] inline auto parse_comparison_bases(
		const int                             aExitCode,
		const std::string_view                aOutput,
		const application::git_hash_algorithm aAlgorithm)
		-> application::git_process_result<application::git_comparison_base_observation>
	{
		using status = application::git_comparison_base_status;
		using application::git_process_failure;
		if (aExitCode == 1 && aOutput.empty()) {
			return application::git_comparison_base_observation{ .Status = status::no_common_ancestor };
		}
		if (aExitCode != 0 || aOutput.empty()) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		auto remaining = aOutput;
		auto bases     = std::vector<application::git_commit_id>{ };
		while (!remaining.empty()) {
			const auto newline = remaining.find('\n');
			if (newline == std::string_view::npos) {
				return std::unexpected{ git_process_failure::malformed_output };
			}
			const auto base = parse_object<application::git_commit_id>(remaining.substr(0U, newline), aAlgorithm);
			if (!base || std::ranges::find(bases, *base) != bases.end()) {
				return std::unexpected{ git_process_failure::malformed_output };
			}
			bases.push_back(*base);
			remaining.remove_prefix(newline + 1U);
		}
		return application::git_comparison_base_observation{
			.Status = bases.size() == 1U ? status::unique : status::multiple_merge_bases,
			.Base   = bases.size() == 1U ? std::optional{ bases.front() } : std::nullopt,
		};
	}

	/// \brief `git ls-tree -z --full-tree <commit> -- :(literal)<path>`: nothing
	/// when the path is absent, otherwise exactly one blob record for exactly
	/// the requested path.
	[[nodiscard]] inline auto parse_ls_tree_blob(
		const std::string_view                aOutput,
		const std::string_view                aRequestedPath,
		const application::git_hash_algorithm aAlgorithm)
		-> application::git_process_result<std::optional<application::git_blob_id>>
	{
		using application::git_process_failure;
		if (aOutput.empty()) {
			return std::optional<application::git_blob_id>{ };
		}
		if (aOutput.back() != '\0'
			|| aOutput.find('\0') != aOutput.size() - 1U) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		const auto record = aOutput.substr(0U, aOutput.size() - 1U);
		const auto tab    = record.find('\t');
		if (tab == std::string_view::npos
			|| record.substr(tab + 1U) != aRequestedPath) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		const auto header     = record.substr(0U, tab);
		const auto firstSpace = header.find(' ');
		const auto lastSpace  = header.rfind(' ');
		if (firstSpace == std::string_view::npos || firstSpace == lastSpace
			|| (header.substr(0U, firstSpace) != "100644"
				&& header.substr(0U, firstSpace) != "100755"
				&& header.substr(0U, firstSpace) != "120000")
			|| header.substr(firstSpace + 1U, lastSpace - firstSpace - 1U)
				!= "blob") {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		const auto blob = parse_object<application::git_blob_id>(
			header.substr(lastSpace + 1U),
			aAlgorithm);
		if (!blob.has_value()) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		return blob;
	}

	/// \brief `git diff-tree -r -z --name-status --diff-filter=R`: rename
	/// records of status, left and right path, kept when either side is the
	/// current or the previous path. Scanning and keeping are both bounded.
	[[nodiscard]] inline auto parse_rename_pairs(
		const std::string_view                aOutput,
		const std::string_view                aCurrentPath,
		const std::optional<std::string_view> aPreviousPath)
		-> application::git_process_result<application::git_rename_pair_observation>
	{
		using application::git_process_failure;
		auto       observation = application::git_rename_pair_observation{ };
		auto       offset      = std::size_t{ 0U };
		auto       scanned     = std::size_t{ 0U };
		const auto field       = [&]() -> std::optional<std::string_view> {
			const auto end = aOutput.find('\0', offset);
			if (end == std::string_view::npos) {
				return std::nullopt;
			}
			const auto result = aOutput.substr(offset, end - offset);
			offset            = end + 1U;
			return result;
		};
		while (offset < aOutput.size()) {
			const auto status = field();
			const auto left   = field();
			const auto right  = field();
			if (!status.has_value() || !left.has_value() || !right.has_value()) {
				return std::unexpected{ git_process_failure::malformed_output };
			}
			++scanned;
			if (scanned > application::maximum_git_rename_scan_records) {
				return std::unexpected{ git_process_failure::standard_output_limit };
			}
			if (status->size() != 4U || status->front() != 'R' || left->empty()
				|| right->empty() || *left == *right) {
				return std::unexpected{ git_process_failure::malformed_output };
			}
			const auto score = status->substr(1U);
			if (!std::ranges::all_of(score, [](const char aValue) {
					return aValue >= '0' && aValue <= '9';
				})
				|| score < "050" || score > "100") {
				return std::unexpected{ git_process_failure::malformed_output };
			}
			const auto leftPath  = tree_entry_path(*left);
			const auto rightPath = tree_entry_path(*right);
			const auto bound     = *left == aCurrentPath || *right == aCurrentPath
				|| (aPreviousPath.has_value()
					&& (*left == *aPreviousPath || *right == *aPreviousPath));
			if (!valid_tree_entry_path(leftPath)
				|| !valid_tree_entry_path(rightPath)) {
				return std::unexpected{ git_process_failure::malformed_output };
			}
			if (!bound) {
				continue;
			}
			if (observation.Count
				>= application::maximum_git_rename_pair_records) {
				return std::unexpected{ git_process_failure::standard_output_limit };
			}
			observation.Records[observation.Count++] = {
				.LeftPath  = leftPath,
				.RightPath = rightPath,
			};
		}
		if (offset != aOutput.size()) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		return observation;
	}

	/// \brief `git cat-file --batch-check` for one blob: its size, refused
	/// above `aMaximumBytes`, or unavailable when Git reports it missing.
	[[nodiscard]] inline auto parse_blob_size(
		const std::string_view aOutput,
		const std::string_view aBlob,
		const std::uint64_t    aMaximumBytes)
		-> application::git_process_result<std::uint64_t>
	{
		using application::git_process_failure;
		if (aOutput == std::string{ aBlob } + " missing\n") {
			return std::unexpected{ git_process_failure::object_unavailable };
		}
		const auto prefix = std::string{ aBlob } + " blob ";
		if (
			!aOutput.starts_with(prefix)
			|| aOutput.empty()
			|| aOutput.back() != '\n') {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		const auto sizeText = aOutput.substr(
			prefix.size(),
			aOutput.size() - prefix.size() - 1U);
		std::uint64_t size      = 0U;
		const auto [end, error] = std::from_chars(
			sizeText.data(), sizeText.data() + sizeText.size(), size);
		if (
			error != std::errc{ } || end != sizeText.data() + sizeText.size()
			|| size > aMaximumBytes
			|| size > static_cast<std::uint64_t>(
				   std::numeric_limits<std::size_t>::max())) {
			return std::unexpected{
				size > aMaximumBytes
					? git_process_failure::standard_output_limit
					: git_process_failure::malformed_output
			};
		}
		return size;
	}

	/// \brief `git show --no-patch --format=%ct`: one decimal number of seconds
	/// and a newline.
	[[nodiscard]] inline auto parse_committer_seconds(const std::string_view aOutput)
		-> application::git_process_result<std::int64_t>
	{
		if (aOutput.size() < 2U || aOutput.back() != '\n') {
			return std::unexpected{ application::git_process_failure::malformed_output };
		}
		const auto   digits  = aOutput.substr(0U, aOutput.size() - 1U);
		const auto   first   = digits.begin();
		const auto   last    = digits.end();
		std::int64_t seconds = 0;
		const auto   parsed  = std::from_chars(std::to_address(first), std::to_address(last), seconds);
		if (parsed.ec != std::errc{ } || parsed.ptr != std::to_address(last)) {
			return std::unexpected{ application::git_process_failure::malformed_output };
		}
		return seconds;
	}

	/// \brief `git for-each-ref --format=%(refname)%00%(objectname)%00%(symref)
	/// --count=2 <ref>`: nothing when the ref is absent, otherwise exactly one
	/// record naming exactly `aRef`, a commit and no symbolic target.
	[[nodiscard]] inline auto parse_ref_record(
		const std::string_view                aOutput,
		const std::string_view                aRef,
		const application::git_hash_algorithm aAlgorithm)
		-> application::git_process_result<std::optional<application::git_commit_id>>
	{
		using application::git_process_failure;
		if (aOutput.empty()) {
			return std::optional<application::git_commit_id>{ };
		}
		const auto nameSeparator   = aOutput.find('\0');
		const auto objectSeparator = nameSeparator == std::string_view::npos
			? std::string_view::npos
			: aOutput.find('\0', nameSeparator + 1U);
		if (
			nameSeparator == std::string_view::npos
			|| objectSeparator == std::string_view::npos
			|| aOutput.substr(0U, nameSeparator) != aRef
			|| aOutput.back() != '\n'
			|| aOutput.find('\n') != aOutput.size() - 1U
			|| objectSeparator + 1U != aOutput.size() - 1U) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		const auto object = parse_object<application::git_commit_id>(
			aOutput.substr(
				nameSeparator + 1U,
				objectSeparator - nameSeparator - 1U),
			aAlgorithm);
		if (!object.has_value()) {
			return std::unexpected{ git_process_failure::malformed_output };
		}
		return std::optional<application::git_commit_id>{ object };
	}

	/// \brief `git version`: three decimal components, then a newline, an
	/// Apple Git build suffix or a Git for Windows build suffix.
	[[nodiscard]] inline auto parse_version(
		const std::string_view aOutput)
		-> std::optional<std::array<std::uint32_t, 3U>>
	{
		constexpr std::string_view prefix = "git version ";
		if (!aOutput.starts_with(prefix) || aOutput.back() != '\n') {
			return std::nullopt;
		}
		auto                          cursor = prefix.size();
		std::array<std::uint32_t, 3U> version{ };
		for (std::size_t component = 0U; component < version.size(); ++component) {
			if (cursor >= aOutput.size()) {
				return std::nullopt;
			}
			std::uint64_t value = 0U;
			const auto    start = cursor;
			while (
				cursor < aOutput.size()
				&& aOutput[cursor] >= '0' && aOutput[cursor] <= '9') {
				value = value * 10U
					+ static_cast<std::uint64_t>(
							aOutput[cursor] - '0');
				if (value > std::numeric_limits<std::uint32_t>::max()) {
					return std::nullopt;
				}
				++cursor;
			}
			if (cursor == start) {
				return std::nullopt;
			}
			version[component] = static_cast<std::uint32_t>(value);
			if (component + 1U < version.size()) {
				if (cursor >= aOutput.size() || aOutput[cursor] != '.') {
					return std::nullopt;
				}
				++cursor;
			}
		}
		if (cursor >= aOutput.size()) {
			return std::nullopt;
		}
		if (aOutput[cursor] == '\n') {
			return cursor + 1U == aOutput.size()
				? std::optional{ version }
				: std::nullopt;
		}
		const auto suffix = aOutput.substr(
			cursor,
			aOutput.size() - cursor - 1U);
		if (aOutput.back() != '\n') {
			return std::nullopt;
		}
		const auto decimal =
			[](const std::string_view aValue) {
				return !aValue.empty()
					&& std::all_of(
						aValue.begin(),
						aValue.end(),
						[](const char aCharacter) {
							return aCharacter >= '0'
								&& aCharacter <= '9';
						});
			};
		constexpr std::string_view applePrefix = " (Apple Git-";
		if (
			suffix.starts_with(applePrefix)
			&& suffix.ends_with(')')
			&& decimal(suffix.substr(
				applePrefix.size(),
				suffix.size() - applePrefix.size() - 1U))) {
			return version;
		}
		constexpr std::string_view windowsPrefix = ".windows.";
		if (
			suffix.starts_with(windowsPrefix)
			&& decimal(suffix.substr(windowsPrefix.size()))) {
			return version;
		}
		return std::nullopt;
	}
}

#endif
