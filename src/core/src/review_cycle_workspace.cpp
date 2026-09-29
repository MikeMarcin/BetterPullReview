#include "bpr/core/review_cycle_workspace.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace bpr::core {
	namespace {
		auto valid_identity(const workspace_object_identity& aIdentity) noexcept -> bool
		{
			return aIdentity.Volume != 0 || aIdentity.ObjectHigh != 0 || aIdentity.ObjectLow != 0;
		}

		auto git_kind(const workspace_root_kind aKind) noexcept -> bool
		{
			return aKind == workspace_root_kind::git_checkout || aKind == workspace_root_kind::git_linked_worktree;
		}

		auto ascii_lower(const char aValue) noexcept -> char
		{
			return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue - 'A' + 'a') : aValue;
		}

		auto equals_ignoring_ascii_case(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size()
				&& std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) { return ascii_lower(aA) == ascii_lower(aB); });
		}

		auto segments(const std::string_view aPath)
		{
			return aPath | std::views::split('/') | std::views::transform([](const auto& aSegment) {
					   return std::string_view{ aSegment.begin(), aSegment.end() };
				   });
		}

		/// Length of the UTF-8 sequence starting at `aOffset`, or zero when it
		/// is not a well-formed shortest-form scalar value.
		auto utf8_sequence(const std::string_view aText, const std::size_t aOffset) noexcept -> std::size_t
		{
			const auto byte = [&](const std::size_t aIndex) { return static_cast<std::uint8_t>(aText[aIndex]); };
			const auto lead = byte(aOffset);
			if (lead < 0x80U) {
				return 1;
			}
			const auto remaining    = aText.size() - aOffset;
			const auto continuation = [&](const std::size_t aIndex) { return aOffset + aIndex < aText.size() && (byte(aOffset + aIndex) & 0xC0U) == 0x80U; };
			if (lead >= 0xC2U && lead <= 0xDFU) {
				return remaining >= 2 && continuation(1) ? 2 : 0;
			}
			if (lead >= 0xE0U && lead <= 0xEFU) {
				if (remaining < 3 || !continuation(1) || !continuation(2)) {
					return 0;
				}
				const auto second    = byte(aOffset + 1);
				const auto shortest  = lead != 0xE0U || second >= 0xA0U;
				const auto surrogate = lead == 0xEDU && second >= 0xA0U;
				return shortest && !surrogate ? 3 : 0;
			}
			if (lead >= 0xF0U && lead <= 0xF4U) {
				if (remaining < 4 || !continuation(1) || !continuation(2) || !continuation(3)) {
					return 0;
				}
				const auto second  = byte(aOffset + 1);
				const auto inRange = (lead != 0xF0U || second >= 0x90U) && (lead != 0xF4U || second < 0x90U);
				return inRange ? 4 : 0;
			}
			return 0;
		}

		auto reserved_device_name(const std::string_view aSegment) noexcept -> bool
		{
			// Windows reserves these names with any extension and in any case.
			const auto     stem    = aSegment.substr(0, aSegment.find('.'));
			const auto     trimmed = stem.substr(0, stem.find_last_not_of(' ') + 1);
			constexpr auto plain   = std::array<std::string_view, 6>{ "con", "prn", "aux", "nul", "conin$", "conout$" };
			if (std::ranges::any_of(plain, [trimmed](const std::string_view aName) { return equals_ignoring_ascii_case(trimmed, aName); })) {
				return true;
			}
			if (trimmed.size() != 4) {
				return false;
			}
			const auto prefix = trimmed.substr(0, 3);
			const auto digit  = trimmed[3];
			return (equals_ignoring_ascii_case(prefix, "com") || equals_ignoring_ascii_case(prefix, "lpt")) && digit >= '0' && digit <= '9';
		}

		auto forbidden_on_windows(const char aValue) noexcept -> bool
		{
			const auto byte = static_cast<std::uint8_t>(aValue);
			return byte < 0x20U || aValue == '<' || aValue == '>' || aValue == ':' || aValue == '"' || aValue == '|' || aValue == '?'
				|| aValue == '*' || aValue == '\\';
		}
	}

	auto valid(const workspace_root_observation& aRoot) noexcept -> bool
	{
		const auto repositoryShape = git_kind(aRoot.Kind) ? aRoot.Repository.has_value() && valid_identity(*aRoot.Repository)
				&& aRoot.Worktree.has_value() && valid_identity(*aRoot.Worktree)
														  : !aRoot.Repository.has_value() && !aRoot.Worktree.has_value();
		return aRoot.Kind <= workspace_root_kind::git_linked_worktree && !aRoot.Location.empty()
			&& aRoot.Location.size() <= maximum_workspace_location_bytes && aRoot.Location.find('\0') == std::string::npos
			&& valid_identity(aRoot.Root) && repositoryShape;
	}

	auto valid(const cycle_workspace_binding& aBinding) noexcept -> bool
	{
		return aBinding.Project.valid() && aBinding.Version > 0 && valid(aBinding.Root);
	}

	auto verify_workspace(const cycle_workspace_binding& aBinding, const workspace_root_observation& aObserved) noexcept
		-> workspace_verification
	{
		const auto& bound = aBinding.Root;
		if (directory_root(bound.Kind) != directory_root(aObserved.Kind)) {
			return workspace_verification::kind_changed;
		}
		if (bound.Root != aObserved.Root) {
			return workspace_verification::replaced;
		}
		if (git_kind(bound.Kind) != git_kind(aObserved.Kind)) {
			return workspace_verification::kind_changed;
		}
		if (bound.Kind != aObserved.Kind || bound.Repository != aObserved.Repository || bound.Worktree != aObserved.Worktree) {
			return workspace_verification::repository_changed;
		}
		return workspace_verification::matches;
	}

	auto resolve_imported_project(const std::span<const workspace_import_mapping> aMappings, const workspace_project_id& aImported)
		-> std::optional<workspace_project_id>
	{
		if (!aImported.valid()) {
			return std::nullopt;
		}
		const auto found = std::ranges::find(aMappings, aImported, &workspace_import_mapping::Imported);
		if (found == aMappings.end() || !found->Local.valid()) {
			return std::nullopt;
		}
		return found->Local;
	}

	auto admit_workspace_path(const std::string_view aPath) noexcept -> workspace_path_refusal
	{
		if (aPath.empty()) {
			return workspace_path_refusal::empty;
		}
		if (aPath.size() > maximum_workspace_path_bytes) {
			return workspace_path_refusal::too_long;
		}
		if (aPath.find('\0') != std::string_view::npos) {
			return workspace_path_refusal::nul;
		}
		const auto drive = aPath.size() >= 2 && aPath[1] == ':'
			&& ((aPath[0] >= 'A' && aPath[0] <= 'Z') || (aPath[0] >= 'a' && aPath[0] <= 'z'));
		if (aPath.front() == '/' || aPath.front() == '\\' || drive) {
			return workspace_path_refusal::absolute;
		}
		for (const auto segment : segments(aPath)) {
			if (segment.empty() || segment == "." || segment == "..") {
				return workspace_path_refusal::invalid_segment;
			}
			if (equals_ignoring_ascii_case(segment, ".git")) {
				return workspace_path_refusal::repository_internal;
			}
		}
		return workspace_path_refusal::none;
	}

	auto path_portability(const std::string_view aPath) noexcept -> workspace_path_portability
	{
		auto result = workspace_path_portability{ };
		for (std::size_t offset = 0; offset < aPath.size();) {
			const auto length = utf8_sequence(aPath, offset);
			result.NonUtf8    = result.NonUtf8 || length == 0;
			offset += length == 0 ? 1 : length;
		}
		result.WindowsForbiddenCharacter = std::ranges::any_of(aPath, forbidden_on_windows);
		for (const auto segment : segments(aPath)) {
			result.WindowsReservedName = result.WindowsReservedName || reserved_device_name(segment);
			result.TrailingDotOrSpace  = result.TrailingDotOrSpace || (!segment.empty() && (segment.back() == '.' || segment.back() == ' '));
		}
		return result;
	}

	auto workspace_path_label(const std::string_view aPath) -> std::string
	{
		constexpr auto hex   = std::string_view{ "0123456789ABCDEF" };
		auto           label = std::string{ };
		label.reserve(aPath.size());
		for (std::size_t offset = 0; offset < aPath.size();) {
			const auto length = utf8_sequence(aPath, offset);
			const auto byte   = static_cast<std::uint8_t>(aPath[offset]);
			if (length == 0 || byte < 0x20U || byte == 0x7FU || aPath[offset] == '\\') {
				label += "\\x";
				label += hex[byte >> 4U];
				label += hex[byte & 0x0FU];
				++offset;
				continue;
			}
			label.append(aPath.substr(offset, length));
			offset += length;
		}
		return label;
	}
}
