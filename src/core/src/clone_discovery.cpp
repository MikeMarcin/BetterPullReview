#include "bpr/core/clone_discovery.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] auto same_text(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) { return lower(aA) == lower(aB); });
		}

		[[nodiscard]] auto lowered(std::string_view aText) -> std::string
		{
			auto text = std::string{ aText };
			std::ranges::transform(text, text.begin(), lower);
			return text;
		}

		[[nodiscard]] constexpr auto space(const char aCharacter) noexcept -> bool { return aCharacter == ' ' || aCharacter == '\t' || aCharacter == '\r'; }

		[[nodiscard]] auto trimmed(std::string_view aText) -> std::string_view
		{
			while (!aText.empty() && space(aText.front())) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && space(aText.back())) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		[[nodiscard]] auto control(const std::string_view aText) noexcept -> bool
		{
			return std::ranges::any_of(aText, [](const char aCharacter) { return static_cast<unsigned char>(aCharacter) < 0x20U || aCharacter == 0x7F; });
		}

		/// `[remote "NAME"]` gives NAME; any other header gives nothing.
		[[nodiscard]] auto remote_header(const std::string_view aLine) -> std::optional<std::string>
		{
			const auto close = aLine.rfind(']');
			if (aLine.empty() || aLine.front() != '[' || close == std::string_view::npos) {
				return std::nullopt;
			}
			auto       inner = trimmed(aLine.substr(1U, close - 1U));
			const auto quote = inner.find('"');
			if (quote == std::string_view::npos || !same_text(trimmed(inner.substr(0U, quote)), "remote")) {
				return std::nullopt;
			}
			auto name = std::string{ };
			for (auto position = quote + 1U; position < inner.size(); ++position) {
				const auto character = inner[position];
				if (character == '"') {
					return position + 1U == inner.size() && !name.empty() ? std::optional{ name } : std::nullopt;
				}
				if (character == '\\' && position + 1U < inner.size()) {
					name += inner[++position];
					continue;
				}
				name += character;
			}
			return std::nullopt;
		}

		/// A value as Git reads it on one line: quotes joined, escapes taken,
		/// a comment outside quotes ending it, outer space trimmed.
		[[nodiscard]] auto config_value(const std::string_view aText) -> std::optional<std::string>
		{
			auto value   = std::string{ };
			auto quoted  = false;
			auto pending = std::string{ };
			for (auto position = std::size_t{ 0U }; position < aText.size(); ++position) {
				const auto character = aText[position];
				if (!quoted && (character == '#' || character == ';')) {
					break;
				}
				if (character == '"') {
					value += pending;
					pending.clear();
					quoted = !quoted;
					continue;
				}
				if (character == '\\') {
					if (position + 1U >= aText.size()) {
						return std::nullopt;
					}
					const auto next = aText[++position];
					value += pending;
					pending.clear();
					value += next == 'n' ? '\n' : next == 't' ? '\t'
						: next == 'b'                         ? '\b'
															  : next;
					continue;
				}
				if (!quoted && space(character)) {
					if (!value.empty()) {
						pending += character;
					}
					continue;
				}
				value += pending;
				pending.clear();
				value += character;
			}
			if (quoted) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] auto split_location(std::string_view aHost, std::string_view aPath) -> std::optional<remote_location>
		{
			if (const auto port = aHost.find(':'); port != std::string_view::npos) {
				aHost = aHost.substr(0U, port);
			}
			while (aPath.starts_with('/')) {
				aPath.remove_prefix(1U);
			}
			while (aPath.ends_with('/')) {
				aPath.remove_suffix(1U);
			}
			if (aPath.ends_with(".git")) {
				aPath.remove_suffix(4U);
			}
			const auto slash = aPath.find('/');
			if (aHost.empty() || slash == std::string_view::npos || slash == 0U || slash + 1U == aPath.size()
				|| aPath.find('/', slash + 1U) != std::string_view::npos) {
				return std::nullopt;
			}
			const auto owner = aPath.substr(0U, slash);
			const auto name  = aPath.substr(slash + 1U);
			const auto plain = [](const std::string_view aPart) {
				return !aPart.empty() && aPart != "." && aPart != ".."
					&& std::ranges::all_of(aPart, [](const char aCharacter) {
						   return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9')
							   || aCharacter == '-' || aCharacter == '_' || aCharacter == '.';
					   });
			};
			const auto host = [](const std::string_view aName) {
				return std::ranges::all_of(aName, [](const char aCharacter) {
					return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-'
						|| aCharacter == '.';
				});
			};
			if (!plain(owner) || !plain(name) || !host(aHost)) {
				return std::nullopt;
			}
			return remote_location{ .Host = lowered(aHost), .Owner = std::string{ owner }, .Name = std::string{ name } };
		}
	} // namespace

	auto parse_git_config_remotes(std::string_view aConfig) -> std::vector<git_remote>
	{
		aConfig       = aConfig.substr(0U, std::min(aConfig.size(), maximum_clone_config_bytes));
		auto remotes  = std::vector<git_remote>{ };
		auto section  = std::optional<std::string>{ };
		auto position = std::size_t{ 0U };
		while (position < aConfig.size() && remotes.size() < maximum_clone_remotes) {
			const auto end  = aConfig.find('\n', position);
			const auto line = trimmed(aConfig.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position));
			position        = end == std::string_view::npos ? aConfig.size() : end + 1U;
			if (line.empty() || line.front() == '#' || line.front() == ';') {
				continue;
			}
			if (line.front() == '[') {
				section = remote_header(line);
				continue;
			}
			if (!section) {
				continue;
			}
			const auto equals = line.find('=');
			if (equals == std::string_view::npos || !same_text(trimmed(line.substr(0U, equals)), "url")) {
				continue;
			}
			auto value = config_value(trimmed(line.substr(equals + 1U)));
			if (!value || value->empty() || value->size() > maximum_remote_text_bytes || section->size() > maximum_remote_text_bytes || control(*value) || control(*section)) {
				continue;
			}
			remotes.push_back({ .Name = *section, .Url = std::move(*value) });
		}
		return remotes;
	}

	auto normalize_remote_url(const std::string_view aUrl) -> std::optional<remote_location>
	{
		if (aUrl.empty() || aUrl.size() > maximum_remote_text_bytes || control(aUrl)) {
			return std::nullopt;
		}
		if (const auto scheme = aUrl.find("://"); scheme != std::string_view::npos) {
			const auto name = lowered(aUrl.substr(0U, scheme));
			if (name != "https" && name != "http" && name != "ssh" && name != "git" && name != "git+ssh" && name != "ssh+git") {
				return std::nullopt;
			}
			auto       rest  = aUrl.substr(scheme + 3U);
			const auto slash = rest.find('/');
			if (slash == std::string_view::npos) {
				return std::nullopt;
			}
			auto authority = rest.substr(0U, slash);
			// User information goes first, before anything else reads the URL.
			if (const auto at = authority.rfind('@'); at != std::string_view::npos) {
				authority.remove_prefix(at + 1U);
			}
			return split_location(authority, rest.substr(slash));
		}
		// scp-like: [USER@]HOST:PATH, with no slash before the colon.
		const auto colon = aUrl.find(':');
		const auto slash = aUrl.find('/');
		if (colon == std::string_view::npos || (slash != std::string_view::npos && slash < colon)) {
			return std::nullopt;
		}
		auto authority = aUrl.substr(0U, colon);
		if (const auto at = authority.rfind('@'); at != std::string_view::npos) {
			authority.remove_prefix(at + 1U);
		}
		// A Windows drive letter is a local path, not a host.
		if (authority.size() == 1U) {
			return std::nullopt;
		}
		return split_location(authority, aUrl.substr(colon + 1U));
	}

	auto head_branch(std::string_view aHead) -> std::string
	{
		aHead = trimmed(aHead.substr(0U, std::min(aHead.size(), std::size_t{ 1024U })));
		while (!aHead.empty() && aHead.back() == '\n') {
			aHead.remove_suffix(1U);
		}
		constexpr auto prefix = std::string_view{ "ref: refs/heads/" };
		if (!aHead.starts_with(prefix) || control(aHead.substr(prefix.size()))) {
			return { };
		}
		return std::string{ trimmed(aHead.substr(prefix.size())) };
	}

	auto match_clones(const std::span<const clone_candidate> aCandidates, const clone_target& aTarget, const std::span<const std::string> aFolders,
					  const std::string_view aLinked) -> std::vector<clone_match>
	{
		auto matches = std::vector<clone_match>{ };
		for (const auto& candidate : aCandidates) {
			if (std::ranges::contains(matches, candidate.Path, &clone_match::Path)) {
				continue;
			}
			for (const auto& remote : candidate.Remotes) {
				const auto location = normalize_remote_url(remote.Url);
				if (!location || !same_text(location->Host, aTarget.Host) || !same_text(location->Owner, aTarget.Owner) || !same_text(location->Name, aTarget.Name)) {
					continue;
				}
				matches.push_back({
					.Path     = candidate.Path,
					.Folder   = candidate.Folder,
					.Remote   = remote.Name,
					.Location = location->Host + "/" + location->Owner + "/" + location->Name,
					.Branch   = candidate.Branch,
				});
				break;
			}
		}
		const auto rank = [&aFolders, aLinked](const clone_match& aMatch) {
			if (!aLinked.empty() && aMatch.Path == aLinked) {
				return std::size_t{ 0U };
			}
			const auto folder = std::ranges::find(aFolders, aMatch.Folder);
			return std::size_t{ 1U } + static_cast<std::size_t>(folder - aFolders.begin());
		};
		std::ranges::stable_sort(matches, [&rank](const clone_match& aLeft, const clone_match& aRight) {
			const auto left  = rank(aLeft);
			const auto right = rank(aRight);
			return left != right ? left < right : aLeft.Path < aRight.Path;
		});
		return matches;
	}

	auto valid_clone_folder(const std::string_view aFolder, const bool aWindows) noexcept -> bool
	{
		if (aFolder.empty() || aFolder.size() > maximum_remote_text_bytes || control(aFolder)) {
			return false;
		}
		if (aWindows) {
			const auto drive = aFolder.size() >= 3U && ((aFolder[0] >= 'A' && aFolder[0] <= 'Z') || (aFolder[0] >= 'a' && aFolder[0] <= 'z')) && aFolder[1] == ':'
				&& (aFolder[2] == '\\' || aFolder[2] == '/');
			const auto unc = aFolder.starts_with("\\\\");
			return drive || unc;
		}
		return aFolder.front() == '/';
	}

	auto usual_clone_folders(const std::string_view aHome, const bool aWindows) -> std::vector<std::string>
	{
		if (!valid_clone_folder(aHome, aWindows)) {
			return { };
		}
		const auto separator = aWindows ? '\\' : '/';
		auto       base      = std::string{ aHome };
		while (base.size() > 1U && (base.back() == '/' || base.back() == '\\')) {
			base.pop_back();
		}
		auto folders = std::vector<std::string>{ };
		for (const auto* const below : { "Documents/GitHub", "Developer", "src", "code", "Projects" }) {
			auto folder = base + separator + below;
			if (aWindows) {
				std::ranges::replace(folder, '/', '\\');
			}
			folders.push_back(std::move(folder));
		}
		return folders;
	}
} // namespace bpr::core
