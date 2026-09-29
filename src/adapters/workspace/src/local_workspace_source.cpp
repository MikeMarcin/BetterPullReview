#include "bpr/adapters/workspace/local_workspace_source.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
#else
	#include <cerrno>
	#include <dirent.h>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::workspace {
	namespace {
		using application::workspace_entry_kind;
		using application::workspace_entry_observation;
		using application::workspace_failure;
		using application::workspace_git_area;
		using application::workspace_read_observation;

		constexpr std::size_t read_chunk_bytes     = 256U * 1024U;
		constexpr std::size_t maximum_link_bytes   = 4096U;
		constexpr std::size_t maximum_gitfile_size = 4096U;

		template <typename T>
		using result = std::expected<T, workspace_failure>;

		auto fail(const workspace_failure aFailure) -> std::unexpected<workspace_failure>
		{
			return std::unexpected(aFailure);
		}

		auto ascii_lower(const char aValue) -> char
		{
			return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue - 'A' + 'a') : aValue;
		}

		/// `.git` in any ASCII case: never listed and never material.
		auto repository_entry(const std::string_view aName) -> bool
		{
			constexpr auto name = std::string_view{ ".git" };
			return aName.size() == name.size() && std::ranges::equal(aName, name, [](const char aLeft, const char aRight) { return ascii_lower(aLeft) == aRight; });
		}

		/// A read buffer no larger than the file needs, plus one byte to see growth.
		auto chunk_size(const std::uint64_t aExpected) -> std::size_t
		{
			return static_cast<std::size_t>(std::min<std::uint64_t>(read_chunk_bytes, aExpected + 1U));
		}

		/// Splits an admitted root-relative path; the empty path has no components.
		auto components(const std::string_view aPath) -> std::optional<std::vector<std::string>>
		{
			auto parts = std::vector<std::string>{ };
			if (aPath.empty()) {
				return parts;
			}
			if (core::admit_workspace_path(aPath) != core::workspace_path_refusal::none) {
				return std::nullopt;
			}
			for (std::size_t start = 0;;) {
				const auto end = aPath.find('/', start);
				parts.emplace_back(aPath.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
				if (end == std::string_view::npos) {
					return parts;
				}
				start = end + 1;
			}
		}

		/// Reads the `gitdir:` line of a `.git` file, trimmed of its line ending.
		auto gitfile_target(const std::string_view aContents) -> std::optional<std::string>
		{
			constexpr auto prefix = std::string_view{ "gitdir: " };
			if (!aContents.starts_with(prefix)) {
				return std::nullopt;
			}
			auto target = aContents.substr(prefix.size());
			target      = target.substr(0, target.find_first_of("\r\n"));
			if (target.empty() || target.find('\0') != std::string_view::npos) {
				return std::nullopt;
			}
			return std::string{ target };
		}

		/// Trims a `commondir` file's contents to its path.
		auto commondir_target(const std::string_view aContents) -> std::optional<std::string>
		{
			const auto target = aContents.substr(0, aContents.find_first_of("\r\n"));
			if (target.empty() || target.find('\0') != std::string_view::npos) {
				return std::nullopt;
			}
			return std::string{ target };
		}

		auto sorted(std::vector<workspace_entry_observation> aEntries) -> std::vector<workspace_entry_observation>
		{
			std::ranges::sort(aEntries, { }, &workspace_entry_observation::Name);
			return aEntries;
		}

		/// Before and after observations of one read agree, and the path still
		/// names the object that was read.
		auto stable_read(const workspace_entry_observation& aBefore, const workspace_entry_observation& aAfter, const workspace_entry_observation& aNamed,
						 const std::uint64_t aBytesRead) -> bool
		{
			return aBefore.Identity == aAfter.Identity && aBefore.Identity == aNamed.Identity && aBefore.Size == aAfter.Size && aBytesRead == aBefore.Size
				&& aBefore.Modified == aAfter.Modified && aBefore.Changed == aAfter.Changed && aNamed.Modified == aBefore.Modified
				&& aNamed.Changed == aBefore.Changed && aNamed.Size == aBefore.Size;
		}

#if defined(_WIN32)
		// ---------------------------------------------------------------- Windows

		class handle final
		{
		public:
			handle() = default;
			explicit handle(const HANDLE aValue) noexcept
				: Value{ aValue }
			{
			}
			handle(const handle&)                    = delete;
			auto operator=(const handle&) -> handle& = delete;
			handle(handle&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, INVALID_HANDLE_VALUE) }
			{
			}
			auto operator=(handle&& aOther) noexcept -> handle&
			{
				if (this != &aOther) {
					reset();
					Value = std::exchange(aOther.Value, INVALID_HANDLE_VALUE);
				}
				return *this;
			}
			~handle()
			{
				reset();
			}
			[[nodiscard]] auto get() const noexcept -> HANDLE
			{
				return Value;
			}
			[[nodiscard]] auto valid() const noexcept -> bool
			{
				return Value != INVALID_HANDLE_VALUE && Value != nullptr;
			}

		private:
			void reset() noexcept
			{
				if (valid()) {
					::CloseHandle(Value);
				}
				Value = INVALID_HANDLE_VALUE;
			}
			HANDLE Value = INVALID_HANDLE_VALUE;
		};

		/// WTF-8 to UTF-16: well-formed UTF-8 plus encoded lone surrogates, so
		/// every name Windows can hold round-trips through a byte path.
		auto wide(const std::string_view aText) -> std::optional<std::wstring>
		{
			auto out = std::wstring{ };
			out.reserve(aText.size());
			for (std::size_t offset = 0; offset < aText.size();) {
				const auto lead   = static_cast<std::uint8_t>(aText[offset]);
				auto       length = std::size_t{ 1 };
				auto       value  = static_cast<std::uint32_t>(lead);
				if (lead >= 0xF0U && lead <= 0xF4U) {
					length = 4;
					value  = lead & 0x07U;
				} else if (lead >= 0xE0U && lead <= 0xEFU) {
					length = 3;
					value  = lead & 0x0FU;
				} else if (lead >= 0xC2U && lead <= 0xDFU) {
					length = 2;
					value  = lead & 0x1FU;
				} else if (lead >= 0x80U) {
					return std::nullopt;
				}
				if (offset + length > aText.size()) {
					return std::nullopt;
				}
				for (std::size_t index = 1; index < length; ++index) {
					const auto next = static_cast<std::uint8_t>(aText[offset + index]);
					if ((next & 0xC0U) != 0x80U) {
						return std::nullopt;
					}
					value = (value << 6U) | (next & 0x3FU);
				}
				const auto minimum = length == 1 ? 0U : length == 2 ? 0x80U
					: length == 3                                   ? 0x800U
																	: 0x10000U;
				if (value < minimum || value > 0x10FFFFU) {
					return std::nullopt;
				}
				if (value >= 0x10000U) {
					value -= 0x10000U;
					out.push_back(static_cast<wchar_t>(0xD800U + (value >> 10U)));
					out.push_back(static_cast<wchar_t>(0xDC00U + (value & 0x3FFU)));
				} else {
					out.push_back(static_cast<wchar_t>(value));
				}
				offset += length;
			}
			return out;
		}

		/// UTF-16 to WTF-8; a lone surrogate becomes its three-byte encoding.
		auto narrow(const std::wstring_view aText) -> std::string
		{
			auto out = std::string{ };
			out.reserve(aText.size());
			const auto push = [&out](const std::uint32_t aByte) { out.push_back(static_cast<char>(static_cast<std::uint8_t>(aByte))); };
			for (std::size_t index = 0; index < aText.size(); ++index) {
				auto       value = static_cast<std::uint32_t>(static_cast<std::uint16_t>(aText[index]));
				const auto high  = value >= 0xD800U && value <= 0xDBFFU;
				if (high && index + 1 < aText.size()) {
					const auto low = static_cast<std::uint32_t>(static_cast<std::uint16_t>(aText[index + 1]));
					if (low >= 0xDC00U && low <= 0xDFFFU) {
						value = 0x10000U + ((value - 0xD800U) << 10U) + (low - 0xDC00U);
						++index;
					}
				}
				if (value < 0x80U) {
					push(value);
				} else if (value < 0x800U) {
					push(0xC0U | (value >> 6U));
					push(0x80U | (value & 0x3FU));
				} else if (value < 0x10000U) {
					push(0xE0U | (value >> 12U));
					push(0x80U | ((value >> 6U) & 0x3FU));
					push(0x80U | (value & 0x3FU));
				} else {
					push(0xF0U | (value >> 18U));
					push(0x80U | ((value >> 12U) & 0x3FU));
					push(0x80U | ((value >> 6U) & 0x3FU));
					push(0x80U | (value & 0x3FU));
				}
			}
			return out;
		}

		auto windows_failure(const DWORD aError) -> workspace_failure
		{
			switch (aError) {
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
			case ERROR_DIRECTORY:
				return workspace_failure::missing;
			case ERROR_ACCESS_DENIED:
			case ERROR_SHARING_VIOLATION:
				return workspace_failure::permission_denied;
			case ERROR_CANT_ACCESS_FILE:
			case ERROR_CANT_RESOLVE_FILENAME:
				return workspace_failure::escape;
			case ERROR_INVALID_NAME:
			case ERROR_BAD_PATHNAME:
			case ERROR_FILENAME_EXCED_RANGE:
				return workspace_failure::unsafe_path;
			case ERROR_OPERATION_ABORTED:
				return workspace_failure::cancelled;
			default:
				return workspace_failure::io;
			}
		}

		auto stamp(const LARGE_INTEGER aValue) -> std::int64_t
		{
			return static_cast<std::int64_t>(aValue.QuadPart);
		}

		auto identity_from(const std::uint64_t aVolume, const FILE_ID_128& aId) -> core::workspace_object_identity
		{
			auto high = std::uint64_t{ 0 };
			auto low  = std::uint64_t{ 0 };
			for (std::size_t index = 0; index < 8U; ++index) {
				low |= static_cast<std::uint64_t>(aId.Identifier[index]) << (8U * index);
				high |= static_cast<std::uint64_t>(aId.Identifier[index + 8U]) << (8U * index);
			}
			return { .Volume = aVolume, .ObjectHigh = high, .ObjectLow = low };
		}

		/// Opens one object without following a reparse point. Directories are
		/// held without delete sharing, so they cannot be renamed or replaced
		/// while a descendant is in use.
		auto open_object(const std::wstring& aPath, const bool aRead) -> result<handle>
		{
			const auto access = aRead ? GENERIC_READ : (FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | SYNCHRONIZE);
			const auto share  = aRead ? (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE) : (FILE_SHARE_READ | FILE_SHARE_WRITE);
			auto       opened = handle{ ::CreateFileW(aPath.c_str(), access, share, nullptr, OPEN_EXISTING,
													  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | (aRead ? FILE_FLAG_SEQUENTIAL_SCAN : 0U), nullptr) };
			if (!opened.valid()) {
				return fail(windows_failure(::GetLastError()));
			}
			return opened;
		}

		struct object_facts
		{
			workspace_entry_observation Entry;
			bool                        Reparse = false;
		};

		auto facts(const HANDLE aHandle, std::string aName) -> result<object_facts>
		{
			FILE_ATTRIBUTE_TAG_INFO tag{ };
			FILE_STANDARD_INFO      standard{ };
			FILE_ID_INFO            identity{ };
			FILE_BASIC_INFO         basic{ };
			if (::GetFileInformationByHandleEx(aHandle, FileAttributeTagInfo, &tag, sizeof(tag)) == 0
				|| ::GetFileInformationByHandleEx(aHandle, FileStandardInfo, &standard, sizeof(standard)) == 0
				|| ::GetFileInformationByHandleEx(aHandle, FileIdInfo, &identity, sizeof(identity)) == 0
				|| ::GetFileInformationByHandleEx(aHandle, FileBasicInfo, &basic, sizeof(basic)) == 0) {
				return fail(windows_failure(::GetLastError()));
			}
			const auto reparse   = (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
			const auto directory = (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
			auto       kind      = directory ? workspace_entry_kind::directory : workspace_entry_kind::file;
			if (reparse) {
				kind = tag.ReparseTag == IO_REPARSE_TAG_SYMLINK ? workspace_entry_kind::symlink : workspace_entry_kind::other;
			}
			return object_facts{
				.Entry = {
					.Name     = std::move(aName),
					.Kind     = kind,
					.Identity = identity_from(identity.VolumeSerialNumber, identity.FileId),
					.Size     = directory ? 0U : static_cast<std::uint64_t>(standard.EndOfFile.QuadPart),
					.Modified = stamp(basic.LastWriteTime),
					.Changed  = stamp(basic.ChangeTime),
				},
				.Reparse = reparse,
			};
		}

		/// The `\\?\` path of an open object, so later opens bypass Win32 path
		/// rewriting such as trailing-dot stripping.
		auto final_path(const HANDLE aHandle) -> result<std::wstring>
		{
			auto buffer = std::wstring(512, L'\0');
			for (;;) {
				const auto length = ::GetFinalPathNameByHandleW(aHandle, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
				if (length == 0) {
					return fail(windows_failure(::GetLastError()));
				}
				if (length < buffer.size()) {
					buffer.resize(length);
					return buffer;
				}
				buffer.resize(static_cast<std::size_t>(length) + 1U);
			}
		}

		/// A component Windows would reinterpret: a separator, stream colon,
		/// device name or trailing dot or space. Such names never reach CreateFileW.
		auto safe_component(const std::string& aName) -> bool
		{
			const auto portability = core::path_portability(aName);
			return !portability.WindowsReservedName && !portability.WindowsForbiddenCharacter && !portability.TrailingDotOrSpace;
		}

		auto read_whole(const HANDLE aHandle, const std::uint64_t aMaximum, std::stop_token aStop) -> result<std::vector<std::uint8_t>>
		{
			auto bytes = std::vector<std::uint8_t>{ };
			auto chunk = std::vector<std::uint8_t>(chunk_size(aMaximum));
			for (;;) {
				if (aStop.stop_requested()) {
					return fail(workspace_failure::cancelled);
				}
				DWORD got = 0;
				if (::ReadFile(aHandle, chunk.data(), static_cast<DWORD>(chunk.size()), &got, nullptr) == 0) {
					return fail(windows_failure(::GetLastError()));
				}
				if (got == 0) {
					return bytes;
				}
				if (bytes.size() + got > aMaximum) {
					return fail(workspace_failure::too_large);
				}
				bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + got);
			}
		}

		struct opened_root
		{
			handle                           Directory;
			std::wstring                     DirectoryPath;
			std::string                      FileName;
			core::workspace_root_observation Observation;
			handle                           GitWorktree;
			std::wstring                     GitWorktreePath;
			handle                           GitCommon;
			std::wstring                     GitCommonPath;
		};

		auto join(const std::wstring& aBase, const std::wstring& aRelative) -> std::wstring
		{
			auto path = aBase;
			if (!path.empty() && path.back() != L'\\') {
				path.push_back(L'\\');
			}
			for (const auto character : aRelative) {
				path.push_back(character == L'/' ? L'\\' : character);
			}
			return path;
		}

		auto absolute_windows_path(const std::wstring& aPath) -> bool
		{
			return (aPath.size() >= 2 && aPath[1] == L':') || aPath.starts_with(L"\\\\") || aPath.starts_with(L"//");
		}

		/// A final `\\?\` path in the ordinary form, so a relative metadata
		/// path joined to it is normalized (`..` resolved) the way Git does.
		auto ordinary_path(const std::wstring& aPath) -> std::wstring
		{
			if (aPath.starts_with(L"\\\\?\\UNC\\")) {
				return L"\\\\" + aPath.substr(8);
			}
			if (aPath.starts_with(L"\\\\?\\")) {
				return aPath.substr(4);
			}
			return aPath;
		}

		/// Opens a directory named by repository metadata, following links: the
		/// checkout, not BPR, chose where its Git directory lives.
		auto open_metadata_directory(const std::wstring& aBase, const std::string& aTarget) -> result<std::pair<handle, std::wstring>>
		{
			const auto target = wide(aTarget);
			if (!target) {
				return fail(workspace_failure::unsafe_path);
			}
			auto path = *target;
			std::ranges::replace(path, L'/', L'\\');
			if (!absolute_windows_path(path)) {
				path = join(ordinary_path(aBase), path);
			}
			auto opened = handle{ ::CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
												OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
			if (!opened.valid()) {
				return fail(windows_failure(::GetLastError()));
			}
			auto finalPath = final_path(opened.get());
			if (!finalPath) {
				return fail(finalPath.error());
			}
			return std::pair{ std::move(opened), std::move(*finalPath) };
		}

		auto read_small(const std::wstring& aPath) -> std::optional<std::string>
		{
			auto opened = open_object(aPath, true);
			if (!opened) {
				return std::nullopt;
			}
			const auto known = facts(opened->get(), { });
			if (!known || known->Reparse || known->Entry.Kind != workspace_entry_kind::file) {
				return std::nullopt;
			}
			auto bytes = read_whole(opened->get(), maximum_gitfile_size, { });
			if (!bytes) {
				return std::nullopt;
			}
			return std::string{ bytes->begin(), bytes->end() };
		}

		/// Recognizes `.git` as a directory or a `gitdir:` file.
		void detect_repository(opened_root& aRoot)
		{
			const auto dotGit = join(aRoot.DirectoryPath, L".git");
			auto       opened = open_object(dotGit, false);
			if (!opened) {
				return;
			}
			const auto known = facts(opened->get(), ".git");
			if (!known || known->Reparse) {
				return;
			}
			auto worktree = std::optional<std::pair<handle, std::wstring>>{ };
			if (known->Entry.Kind == workspace_entry_kind::directory) {
				auto finalPath = final_path(opened->get());
				if (!finalPath) {
					return;
				}
				worktree.emplace(std::move(*opened), std::move(*finalPath));
			} else {
				opened              = handle{ };
				const auto contents = read_small(dotGit);
				const auto target   = contents ? gitfile_target(*contents) : std::nullopt;
				if (!target) {
					return;
				}
				auto resolved = open_metadata_directory(aRoot.DirectoryPath, *target);
				if (!resolved) {
					return;
				}
				worktree.emplace(std::move(*resolved));
			}
			const auto head = read_small(join(worktree->second, L"HEAD"));
			if (!head) {
				return;
			}
			const auto commonText = read_small(join(worktree->second, L"commondir"));
			const auto commonPath = commonText ? commondir_target(*commonText) : std::nullopt;
			auto       common     = open_metadata_directory(worktree->second, commonPath.value_or("."));
			if (!common) {
				return;
			}
			const auto worktreeFacts = facts(worktree->first.get(), { });
			const auto commonFacts   = facts(common->first.get(), { });
			if (!worktreeFacts || !commonFacts) {
				return;
			}
			aRoot.Observation.Kind       = commonPath ? core::workspace_root_kind::git_linked_worktree : core::workspace_root_kind::git_checkout;
			aRoot.Observation.Repository = commonFacts->Entry.Identity;
			aRoot.Observation.Worktree   = worktreeFacts->Entry.Identity;
			aRoot.GitWorktree            = std::move(worktree->first);
			aRoot.GitWorktreePath        = std::move(worktree->second);
			aRoot.GitCommon              = std::move(common->first);
			aRoot.GitCommonPath          = std::move(common->second);
		}

		auto open_root(const std::string_view aLocation) -> result<opened_root>
		{
			if (aLocation.empty() || aLocation.size() > core::maximum_workspace_location_bytes || aLocation.find('\0') != std::string_view::npos) {
				return fail(workspace_failure::unsafe_path);
			}
			const auto location = wide(aLocation);
			if (!location) {
				return fail(workspace_failure::unsafe_path);
			}
			// The chosen location may be reached through links; nothing beneath it is.
			auto target = handle{ ::CreateFileW(location->c_str(), FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
												OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
			if (!target.valid()) {
				return fail(windows_failure(::GetLastError()));
			}
			const auto targetFacts = facts(target.get(), { });
			auto       targetPath  = final_path(target.get());
			if (!targetFacts || !targetPath) {
				return fail(!targetFacts ? targetFacts.error() : targetPath.error());
			}
			auto root                 = opened_root{ };
			root.Observation.Location = std::string{ aLocation };
			if (targetFacts->Entry.Kind == workspace_entry_kind::directory) {
				auto directory = open_object(*targetPath, false);
				if (!directory) {
					return fail(directory.error());
				}
				root.Directory        = std::move(*directory);
				root.DirectoryPath    = std::move(*targetPath);
				root.Observation.Kind = core::workspace_root_kind::folder;
				detect_repository(root);
			} else if (targetFacts->Entry.Kind == workspace_entry_kind::file) {
				const auto separator = targetPath->find_last_of(L'\\');
				if (separator == std::wstring::npos) {
					return fail(workspace_failure::unsupported);
				}
				auto directory = open_object(targetPath->substr(0, separator), false);
				if (!directory) {
					return fail(directory.error());
				}
				root.Directory        = std::move(*directory);
				root.DirectoryPath    = targetPath->substr(0, separator);
				root.FileName         = narrow(std::wstring_view{ *targetPath }.substr(separator + 1));
				root.Observation.Kind = core::workspace_root_kind::file;
			} else {
				return fail(workspace_failure::unsupported);
			}
			const auto directoryFacts = facts(root.Directory.get(), { });
			if (!directoryFacts) {
				return fail(directoryFacts.error());
			}
			root.Observation.Root = directoryFacts->Entry.Identity;
			return root;
		}

		class session final : public application::workspace_root_session
		{
		public:
			explicit session(opened_root aRoot)
				: Root{ std::move(aRoot) }
			{
			}

			[[nodiscard]] auto observation() const -> const core::workspace_root_observation& override
			{
				return Root.Observation;
			}

			auto verify(std::stop_token aStop) -> result<core::workspace_verification> override
			{
				if (aStop.stop_requested()) {
					return fail(workspace_failure::cancelled);
				}
				const auto fresh = open_root(Root.Observation.Location);
				if (!fresh) {
					return fail(fresh.error());
				}
				return core::verify_workspace({ .Version = 1, .Root = Root.Observation }, fresh->Observation);
			}

			auto list(const std::string_view aDirectory, std::stop_token aStop) -> result<std::vector<workspace_entry_observation>> override
			{
				if (Root.Observation.Kind == core::workspace_root_kind::file) {
					if (!aDirectory.empty()) {
						return fail(workspace_failure::missing);
					}
					auto only = stat(Root.FileName, aStop);
					if (!only) {
						return fail(only.error());
					}
					return std::vector{ std::move(*only) };
				}
				auto chain = walk(aDirectory, aStop);
				if (!chain) {
					return fail(chain.error());
				}
				const auto directory = chain->Handles.empty() ? Root.Directory.get() : chain->Handles.back().get();
				auto       entries   = std::vector<workspace_entry_observation>{ };
				auto       buffer    = std::vector<std::uint64_t>(64U * 1024U / sizeof(std::uint64_t));
				auto       infoClass = FileIdExtdDirectoryRestartInfo;
				for (;;) {
					if (aStop.stop_requested()) {
						return fail(workspace_failure::cancelled);
					}
					if (::GetFileInformationByHandleEx(directory, infoClass, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(std::uint64_t))) == 0) {
						const auto error = ::GetLastError();
						if (error == ERROR_NO_MORE_FILES) {
							break;
						}
						return fail(windows_failure(error));
					}
					infoClass    = FileIdExtdDirectoryInfo;
					auto* cursor = reinterpret_cast<const std::uint8_t*>(buffer.data());
					for (;;) {
						const auto* info = reinterpret_cast<const FILE_ID_EXTD_DIR_INFO*>(cursor);
						const auto  name = narrow(std::wstring_view{ info->FileName, info->FileNameLength / sizeof(wchar_t) });
						if (name != "." && name != ".." && !(aDirectory.empty() && repository_entry(name))) {
							if (entries.size() == application::maximum_workspace_directory_entries) {
								return fail(workspace_failure::too_large);
							}
							entries.push_back(entry_from(*info, name));
						}
						if (info->NextEntryOffset == 0) {
							break;
						}
						cursor += info->NextEntryOffset;
					}
				}
				return sorted(std::move(entries));
			}

			auto stat(const std::string_view aPath, std::stop_token aStop) -> result<workspace_entry_observation> override
			{
				auto leaf = open_leaf(aPath, false, aStop);
				if (!leaf) {
					return fail(leaf.error());
				}
				return std::move(leaf->Facts.Entry);
			}

			auto read(const std::string_view aPath, const std::uint64_t aMaximumBytes, const application::workspace_chunk_sink& aSink, std::stop_token aStop)
				-> result<workspace_read_observation> override
			{
				auto leaf = open_leaf(aPath, true, aStop);
				if (!leaf) {
					return fail(leaf.error());
				}
				if (leaf->Facts.Reparse) {
					return fail(workspace_failure::escape);
				}
				if (leaf->Facts.Entry.Kind != workspace_entry_kind::file) {
					return fail(workspace_failure::unsupported);
				}
				if (leaf->Facts.Entry.Size > aMaximumBytes) {
					return fail(workspace_failure::too_large);
				}
				auto observed = workspace_read_observation{ .Before = leaf->Facts.Entry };
				auto chunk    = std::vector<std::uint8_t>(chunk_size(observed.Before.Size));
				for (;;) {
					if (aStop.stop_requested()) {
						return fail(workspace_failure::cancelled);
					}
					DWORD got = 0;
					if (::ReadFile(leaf->Object.get(), chunk.data(), static_cast<DWORD>(chunk.size()), &got, nullptr) == 0) {
						return fail(windows_failure(::GetLastError()));
					}
					if (got == 0) {
						break;
					}
					observed.BytesRead += got;
					if (observed.BytesRead > observed.Before.Size) {
						return fail(workspace_failure::changed);
					}
					if (!aSink(std::span<const std::uint8_t>{ chunk.data(), got })) {
						return fail(workspace_failure::cancelled);
					}
				}
				const auto after = facts(leaf->Object.get(), observed.Before.Name);
				if (!after) {
					return fail(after.error());
				}
				observed.After = after->Entry;
				auto named     = open_leaf(aPath, false, aStop);
				if (!named) {
					return fail(named.error() == workspace_failure::missing ? workspace_failure::changed : named.error());
				}
				if (!stable_read(observed.Before, observed.After, named->Facts.Entry, observed.BytesRead)) {
					return fail(workspace_failure::changed);
				}
				return observed;
			}

			auto read_link(const std::string_view aPath, std::stop_token aStop) -> result<std::string> override
			{
				auto leaf = open_leaf(aPath, false, aStop);
				if (!leaf) {
					return fail(leaf.error());
				}
				// Reparse-data parsing is not implemented; the entry stays visible.
				return fail(leaf->Facts.Entry.Kind == workspace_entry_kind::symlink ? workspace_failure::unsupported : workspace_failure::missing);
			}

			auto read_git(const workspace_git_area aArea, const std::string_view aPath, const std::uint64_t aMaximumBytes, std::stop_token aStop)
				-> result<std::vector<std::uint8_t>> override
			{
				if (!Root.GitWorktree.valid()) {
					return fail(workspace_failure::unsupported);
				}
				const auto parts = components(aPath);
				if (!parts || parts->empty()) {
					return fail(workspace_failure::unsafe_path);
				}
				auto path = aArea == workspace_git_area::worktree ? Root.GitWorktreePath : Root.GitCommonPath;
				for (const auto& part : *parts) {
					const auto name = wide(part);
					if (!name || !safe_component(part)) {
						return fail(workspace_failure::unsafe_path);
					}
					path = join(path, *name);
				}
				auto opened = open_object(path, true);
				if (!opened) {
					return fail(opened.error());
				}
				const auto known = facts(opened->get(), { });
				if (!known) {
					return fail(known.error());
				}
				if (known->Reparse) {
					return fail(workspace_failure::escape);
				}
				if (known->Entry.Kind != workspace_entry_kind::file) {
					return fail(workspace_failure::unsupported);
				}
				if (known->Entry.Size > aMaximumBytes) {
					return fail(workspace_failure::too_large);
				}
				return read_whole(opened->get(), aMaximumBytes, aStop);
			}

		private:
			struct directory_chain
			{
				std::vector<handle> Handles;
				std::wstring        Path;
			};
			struct opened_leaf
			{
				directory_chain Chain;
				handle          Object;
				object_facts    Facts;
			};

			auto entry_from(const FILE_ID_EXTD_DIR_INFO& aInfo, std::string aName) const -> workspace_entry_observation
			{
				const auto reparse   = (aInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
				const auto directory = (aInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
				auto       kind      = directory ? workspace_entry_kind::directory : workspace_entry_kind::file;
				if (reparse) {
					kind = aInfo.ReparsePointTag == IO_REPARSE_TAG_SYMLINK ? workspace_entry_kind::symlink : workspace_entry_kind::other;
				}
				return {
					.Name     = std::move(aName),
					.Kind     = kind,
					.Identity = identity_from(Root.Observation.Root.Volume, aInfo.FileId),
					.Size     = directory ? 0U : static_cast<std::uint64_t>(aInfo.EndOfFile.QuadPart),
					.Modified = stamp(aInfo.LastWriteTime),
					.Changed  = stamp(aInfo.ChangeTime),
				};
			}

			/// Opens every directory component, holding each without delete
			/// sharing and refusing reparse points and other volumes.
			auto walk(const std::string_view aDirectory, std::stop_token aStop) -> result<directory_chain>
			{
				const auto parts = components(aDirectory);
				if (!parts) {
					return fail(workspace_failure::unsafe_path);
				}
				auto chain = directory_chain{ .Path = Root.DirectoryPath };
				for (const auto& part : *parts) {
					if (aStop.stop_requested()) {
						return fail(workspace_failure::cancelled);
					}
					const auto name = wide(part);
					if (!name || !safe_component(part)) {
						return fail(workspace_failure::unsafe_path);
					}
					chain.Path  = join(chain.Path, *name);
					auto opened = open_object(chain.Path, false);
					if (!opened) {
						return fail(opened.error());
					}
					const auto known = facts(opened->get(), part);
					if (!known) {
						return fail(known.error());
					}
					if (known->Reparse || known->Entry.Identity.Volume != Root.Observation.Root.Volume) {
						return fail(workspace_failure::escape);
					}
					if (known->Entry.Kind != workspace_entry_kind::directory) {
						return fail(workspace_failure::missing);
					}
					chain.Handles.push_back(std::move(*opened));
				}
				return chain;
			}

			auto open_leaf(const std::string_view aPath, const bool aRead, std::stop_token aStop) -> result<opened_leaf>
			{
				if (Root.Observation.Kind == core::workspace_root_kind::file && aPath != Root.FileName) {
					return fail(workspace_failure::missing);
				}
				const auto parts = components(aPath);
				if (!parts || parts->empty()) {
					return fail(workspace_failure::unsafe_path);
				}
				const auto separator = aPath.find_last_of('/');
				auto       chain     = walk(separator == std::string_view::npos ? std::string_view{ } : aPath.substr(0, separator), aStop);
				if (!chain) {
					return fail(chain.error());
				}
				const auto name = wide(parts->back());
				if (!name || !safe_component(parts->back())) {
					return fail(workspace_failure::unsafe_path);
				}
				auto object = open_object(join(chain->Path, *name), aRead);
				if (!object) {
					return fail(object.error());
				}
				auto known = facts(object->get(), parts->back());
				if (!known) {
					return fail(known.error());
				}
				if (known->Entry.Identity.Volume != Root.Observation.Root.Volume) {
					return fail(workspace_failure::escape);
				}
				return opened_leaf{ .Chain = std::move(*chain), .Object = std::move(*object), .Facts = std::move(*known) };
			}

			opened_root Root;
		};
#else
		// ------------------------------------------------------------------ POSIX

		class descriptor final
		{
		public:
			descriptor() = default;
			explicit descriptor(const int aValue) noexcept
				: Value{ aValue }
			{
			}
			descriptor(const descriptor&)                    = delete;
			auto operator=(const descriptor&) -> descriptor& = delete;
			descriptor(descriptor&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{
			}
			auto operator=(descriptor&& aOther) noexcept -> descriptor&
			{
				if (this != &aOther) {
					reset();
					Value = std::exchange(aOther.Value, -1);
				}
				return *this;
			}
			~descriptor()
			{
				reset();
			}
			[[nodiscard]] auto get() const noexcept -> int
			{
				return Value;
			}
			[[nodiscard]] auto valid() const noexcept -> bool
			{
				return Value >= 0;
			}
			auto release() noexcept -> int
			{
				return std::exchange(Value, -1);
			}

		private:
			void reset() noexcept
			{
				if (Value >= 0) {
					::close(Value);
				}
				Value = -1;
			}
			int Value = -1;
		};

		auto errno_failure(const int aError) -> workspace_failure
		{
			switch (aError) {
			case ENOENT:
			case ENOTDIR:
				return workspace_failure::missing;
			case EACCES:
			case EPERM:
				return workspace_failure::permission_denied;
			case ELOOP:
				return workspace_failure::escape;
			case ENAMETOOLONG:
				return workspace_failure::unsafe_path;
			case EFBIG:
				return workspace_failure::too_large;
			default:
				return workspace_failure::io;
			}
		}

		auto stamp(const struct timespec& aTime) -> std::int64_t
		{
			return static_cast<std::int64_t>(aTime.tv_sec) * 1'000'000'000 + static_cast<std::int64_t>(aTime.tv_nsec);
		}

		auto observe(const struct stat& aStat, std::string aName) -> workspace_entry_observation
		{
			auto kind = workspace_entry_kind::other;
			if (S_ISREG(aStat.st_mode)) {
				kind = workspace_entry_kind::file;
			} else if (S_ISDIR(aStat.st_mode)) {
				kind = workspace_entry_kind::directory;
			} else if (S_ISLNK(aStat.st_mode)) {
				kind = workspace_entry_kind::symlink;
			}
	#if defined(__APPLE__)
			const auto modified = stamp(aStat.st_mtimespec);
			const auto changed  = stamp(aStat.st_ctimespec);
	#else
			const auto modified = stamp(aStat.st_mtim);
			const auto changed  = stamp(aStat.st_ctim);
	#endif
			return {
				.Name       = std::move(aName),
				.Kind       = kind,
				.Identity   = { .Volume = static_cast<std::uint64_t>(aStat.st_dev), .ObjectLow = static_cast<std::uint64_t>(aStat.st_ino) },
				.Size       = kind == workspace_entry_kind::directory ? 0U : static_cast<std::uint64_t>(aStat.st_size),
				.Executable = kind == workspace_entry_kind::file ? std::optional{ (static_cast<unsigned>(aStat.st_mode) & static_cast<unsigned>(S_IXUSR)) != 0U } : std::nullopt,
				.Modified   = modified,
				.Changed    = changed,
			};
		}

		auto open_at(const int aDirectory, const std::string& aName, const int aFlags) -> descriptor
		{
			for (;;) {
				const auto value = ::openat(aDirectory, aName.c_str(), static_cast<int>(static_cast<unsigned>(aFlags) | static_cast<unsigned>(O_CLOEXEC)));
				if (value >= 0 || errno != EINTR) {
					return descriptor{ value };
				}
			}
		}

		auto stat_at(const int aDirectory, const std::string& aName) -> result<struct stat>
		{
			struct stat value{ };
			if (::fstatat(aDirectory, aName.c_str(), &value, AT_SYMLINK_NOFOLLOW) != 0) {
				return fail(errno_failure(errno));
			}
			return value;
		}

		auto stat_of(const int aDescriptor) -> result<struct stat>
		{
			struct stat value{ };
			if (::fstat(aDescriptor, &value) != 0) {
				return fail(errno_failure(errno));
			}
			return value;
		}

		/// Reads an open descriptor in bounded chunks.
		auto read_descriptor(const int aDescriptor, const std::uint64_t aMaximum, const application::workspace_chunk_sink& aSink, std::stop_token aStop)
			-> result<std::uint64_t>
		{
			auto chunk = std::vector<std::uint8_t>(chunk_size(aMaximum));
			auto total = std::uint64_t{ 0 };
			for (;;) {
				if (aStop.stop_requested()) {
					return fail(workspace_failure::cancelled);
				}
				const auto got = ::read(aDescriptor, chunk.data(), chunk.size());
				if (got < 0) {
					if (errno == EINTR) {
						continue;
					}
					return fail(errno_failure(errno));
				}
				if (got == 0) {
					return total;
				}
				total += static_cast<std::uint64_t>(got);
				if (total > aMaximum) {
					return fail(workspace_failure::changed);
				}
				if (!aSink(std::span<const std::uint8_t>{ chunk.data(), static_cast<std::size_t>(got) })) {
					return fail(workspace_failure::cancelled);
				}
			}
		}

		/// Reads a small regular file beneath `aDirectory` without following it.
		auto read_small(const int aDirectory, const std::string& aName) -> std::optional<std::string>
		{
			const auto file = open_at(aDirectory, aName, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
			if (!file.valid()) {
				return std::nullopt;
			}
			const auto known = stat_of(file.get());
			if (!known || !S_ISREG(known->st_mode) || known->st_size > static_cast<off_t>(maximum_gitfile_size)) {
				return std::nullopt;
			}
			auto       text = std::string{ };
			const auto sink = [&text](const std::span<const std::uint8_t> aBytes) {
				text.append(reinterpret_cast<const char*>(aBytes.data()), aBytes.size());
				return true;
			};
			if (!read_descriptor(file.get(), maximum_gitfile_size, sink, { })) {
				return std::nullopt;
			}
			return text;
		}

		struct opened_root
		{
			descriptor                       Directory;
			std::string                      FileName;
			core::workspace_root_observation Observation;
			descriptor                       GitWorktree;
			descriptor                       GitCommon;
		};

		/// Opens a directory named by repository metadata, following links: the
		/// checkout, not BPR, chose where its Git directory lives.
		auto open_metadata_directory(const int aBase, const std::string& aTarget) -> descriptor
		{
			return open_at(aBase, aTarget, O_RDONLY | O_DIRECTORY);
		}

		/// Recognizes `.git` as a directory or a `gitdir:` file.
		void detect_repository(opened_root& aRoot)
		{
			const auto dotGit = stat_at(aRoot.Directory.get(), ".git");
			if (!dotGit) {
				return;
			}
			auto worktree = descriptor{ };
			if (S_ISDIR(dotGit->st_mode)) {
				worktree = open_at(aRoot.Directory.get(), ".git", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
			} else if (S_ISREG(dotGit->st_mode)) {
				const auto contents = read_small(aRoot.Directory.get(), ".git");
				const auto target   = contents ? gitfile_target(*contents) : std::nullopt;
				if (target) {
					worktree = open_metadata_directory(aRoot.Directory.get(), *target);
				}
			}
			if (!worktree.valid() || !read_small(worktree.get(), "HEAD")) {
				return;
			}
			const auto commonText = read_small(worktree.get(), "commondir");
			const auto commonPath = commonText ? commondir_target(*commonText) : std::nullopt;
			auto       common     = open_metadata_directory(worktree.get(), commonPath.value_or("."));
			if (!common.valid()) {
				return;
			}
			const auto worktreeStat = stat_of(worktree.get());
			const auto commonStat   = stat_of(common.get());
			if (!worktreeStat || !commonStat) {
				return;
			}
			aRoot.Observation.Kind       = commonPath ? core::workspace_root_kind::git_linked_worktree : core::workspace_root_kind::git_checkout;
			aRoot.Observation.Repository = observe(*commonStat, { }).Identity;
			aRoot.Observation.Worktree   = observe(*worktreeStat, { }).Identity;
			aRoot.GitWorktree            = std::move(worktree);
			aRoot.GitCommon              = std::move(common);
		}

		auto open_root(const std::string_view aLocation) -> result<opened_root>
		{
			if (aLocation.empty() || aLocation.size() > core::maximum_workspace_location_bytes || aLocation.find('\0') != std::string_view::npos) {
				return fail(workspace_failure::unsafe_path);
			}
			const auto location = std::string{ aLocation };
			// The chosen location may be reached through links; nothing beneath it is.
			auto target = open_at(AT_FDCWD, location, O_RDONLY | O_NONBLOCK);
			if (!target.valid()) {
				return fail(errno_failure(errno));
			}
			const auto targetStat = stat_of(target.get());
			if (!targetStat) {
				return fail(targetStat.error());
			}
			auto root                 = opened_root{ };
			root.Observation.Location = location;
			if (S_ISDIR(targetStat->st_mode)) {
				root.Directory        = std::move(target);
				root.Observation.Kind = core::workspace_root_kind::folder;
				detect_repository(root);
			} else if (S_ISREG(targetStat->st_mode)) {
				const auto trimmed   = location.substr(0, location.find_last_not_of('/') + 1);
				const auto separator = trimmed.find_last_of('/');
				const auto parent    = separator == std::string::npos ? std::string{ "." } : separator == 0 ? std::string{ "/" }
																											: trimmed.substr(0, separator);
				root.FileName        = separator == std::string::npos ? trimmed : trimmed.substr(separator + 1);
				root.Directory       = open_at(AT_FDCWD, parent, O_RDONLY | O_DIRECTORY);
				if (!root.Directory.valid()) {
					return fail(errno_failure(errno));
				}
				// The file must be the object the location named, not a link to it.
				const auto named = stat_at(root.Directory.get(), root.FileName);
				if (!named) {
					return fail(named.error());
				}
				if (!S_ISREG(named->st_mode) || named->st_ino != targetStat->st_ino || named->st_dev != targetStat->st_dev) {
					return fail(workspace_failure::escape);
				}
				root.Observation.Kind = core::workspace_root_kind::file;
			} else {
				return fail(workspace_failure::unsupported);
			}
			const auto directoryStat = stat_of(root.Directory.get());
			if (!directoryStat) {
				return fail(directoryStat.error());
			}
			root.Observation.Root = observe(*directoryStat, { }).Identity;
			return root;
		}

		class session final : public application::workspace_root_session
		{
		public:
			explicit session(opened_root aRoot)
				: Root{ std::move(aRoot) }
			{
			}

			[[nodiscard]] auto observation() const -> const core::workspace_root_observation& override
			{
				return Root.Observation;
			}

			auto verify(std::stop_token aStop) -> result<core::workspace_verification> override
			{
				if (aStop.stop_requested()) {
					return fail(workspace_failure::cancelled);
				}
				const auto fresh = open_root(Root.Observation.Location);
				if (!fresh) {
					return fail(fresh.error());
				}
				return core::verify_workspace({ .Version = 1, .Root = Root.Observation }, fresh->Observation);
			}

			auto list(const std::string_view aDirectory, std::stop_token aStop) -> result<std::vector<workspace_entry_observation>> override
			{
				if (Root.Observation.Kind == core::workspace_root_kind::file) {
					if (!aDirectory.empty()) {
						return fail(workspace_failure::missing);
					}
					auto only = stat(Root.FileName, aStop);
					if (!only) {
						return fail(only.error());
					}
					return std::vector{ std::move(*only) };
				}
				auto directory = walk(aDirectory, aStop);
				if (!directory) {
					return fail(directory.error());
				}
				// A fresh open file description, so listings never share an offset.
				auto listing = open_at(directory->get(), ".", O_RDONLY | O_DIRECTORY);
				if (!listing.valid()) {
					return fail(errno_failure(errno));
				}
				auto* stream = ::fdopendir(listing.get());
				if (stream == nullptr) {
					return fail(errno_failure(errno));
				}
				static_cast<void>(listing.release());
				const auto closer  = std::unique_ptr<DIR, int (*)(DIR*)>{ stream, &::closedir };
				auto       entries = std::vector<workspace_entry_observation>{ };
				for (;;) {
					if (aStop.stop_requested()) {
						return fail(workspace_failure::cancelled);
					}
					errno             = 0;
					const auto* entry = ::readdir(stream);
					if (entry == nullptr) {
						if (errno != 0) {
							return fail(errno_failure(errno));
						}
						break;
					}
					const auto name = std::string{ entry->d_name };
					if (name == "." || name == ".." || (aDirectory.empty() && repository_entry(name))) {
						continue;
					}
					const auto known = stat_at(::dirfd(stream), name);
					if (!known) {
						// Removed between listing and observation: a re-enumeration sees it.
						if (known.error() == workspace_failure::missing) {
							continue;
						}
						return fail(known.error());
					}
					if (entries.size() == application::maximum_workspace_directory_entries) {
						return fail(workspace_failure::too_large);
					}
					entries.push_back(observe(*known, name));
				}
				return sorted(std::move(entries));
			}

			auto stat(const std::string_view aPath, std::stop_token aStop) -> result<workspace_entry_observation> override
			{
				auto leaf = resolve(aPath, aStop);
				if (!leaf) {
					return fail(leaf.error());
				}
				const auto known = stat_at(leaf->Parent.get(), leaf->Name);
				if (!known) {
					return fail(known.error());
				}
				return observe(*known, leaf->Name);
			}

			auto read(const std::string_view aPath, const std::uint64_t aMaximumBytes, const application::workspace_chunk_sink& aSink, std::stop_token aStop)
				-> result<workspace_read_observation> override
			{
				auto leaf = resolve(aPath, aStop);
				if (!leaf) {
					return fail(leaf.error());
				}
				const auto file = open_at(leaf->Parent.get(), leaf->Name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
				if (!file.valid()) {
					return fail(errno_failure(errno));
				}
				const auto before = stat_of(file.get());
				if (!before) {
					return fail(before.error());
				}
				if (!S_ISREG(before->st_mode)) {
					return fail(workspace_failure::unsupported);
				}
				auto observed = workspace_read_observation{ .Before = observe(*before, leaf->Name) };
				if (observed.Before.Size > aMaximumBytes) {
					return fail(workspace_failure::too_large);
				}
				const auto bytes = read_descriptor(file.get(), observed.Before.Size, aSink, aStop);
				if (!bytes) {
					return fail(bytes.error());
				}
				observed.BytesRead = *bytes;
				const auto after   = stat_of(file.get());
				const auto named   = stat_at(leaf->Parent.get(), leaf->Name);
				if (!after) {
					return fail(after.error());
				}
				if (!named) {
					return fail(named.error() == workspace_failure::missing ? workspace_failure::changed : named.error());
				}
				observed.After = observe(*after, leaf->Name);
				if (!stable_read(observed.Before, observed.After, observe(*named, leaf->Name), observed.BytesRead)) {
					return fail(workspace_failure::changed);
				}
				return observed;
			}

			auto read_link(const std::string_view aPath, std::stop_token aStop) -> result<std::string> override
			{
				auto leaf = resolve(aPath, aStop);
				if (!leaf) {
					return fail(leaf.error());
				}
				auto       target = std::string(maximum_link_bytes + 1U, '\0');
				const auto length = ::readlinkat(leaf->Parent.get(), leaf->Name.c_str(), target.data(), target.size());
				if (length < 0) {
					return fail(errno == EINVAL ? workspace_failure::unsupported : errno_failure(errno));
				}
				if (static_cast<std::size_t>(length) > maximum_link_bytes) {
					return fail(workspace_failure::too_large);
				}
				target.resize(static_cast<std::size_t>(length));
				return target;
			}

			auto read_git(const workspace_git_area aArea, const std::string_view aPath, const std::uint64_t aMaximumBytes, std::stop_token aStop)
				-> result<std::vector<std::uint8_t>> override
			{
				if (!Root.GitWorktree.valid()) {
					return fail(workspace_failure::unsupported);
				}
				const auto parts = components(aPath);
				if (!parts || parts->empty()) {
					return fail(workspace_failure::unsafe_path);
				}
				const auto base    = aArea == workspace_git_area::worktree ? Root.GitWorktree.get() : Root.GitCommon.get();
				auto       current = open_at(base, ".", O_RDONLY | O_DIRECTORY);
				for (std::size_t index = 0; index + 1 < parts->size(); ++index) {
					current = open_at(current.get(), (*parts)[index], O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
					if (!current.valid()) {
						return fail(errno_failure(errno));
					}
				}
				const auto file = open_at(current.get(), parts->back(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
				if (!file.valid()) {
					return fail(errno_failure(errno));
				}
				const auto known = stat_of(file.get());
				if (!known) {
					return fail(known.error());
				}
				if (!S_ISREG(known->st_mode)) {
					return fail(workspace_failure::unsupported);
				}
				if (static_cast<std::uint64_t>(known->st_size) > aMaximumBytes) {
					return fail(workspace_failure::too_large);
				}
				auto       bytes = std::vector<std::uint8_t>{ };
				const auto sink  = [&bytes](const std::span<const std::uint8_t> aChunk) {
					bytes.insert(bytes.end(), aChunk.begin(), aChunk.end());
					return true;
				};
				if (const auto read = read_descriptor(file.get(), aMaximumBytes, sink, aStop); !read) {
					return fail(read.error() == workspace_failure::changed ? workspace_failure::too_large : read.error());
				}
				return bytes;
			}

		private:
			struct leaf_location
			{
				descriptor  Parent;
				std::string Name;
			};

			/// Opens every directory component beneath the held root without
			/// following links and without leaving the root's device.
			auto walk(const std::string_view aDirectory, std::stop_token aStop) -> result<descriptor>
			{
				const auto parts = components(aDirectory);
				if (!parts) {
					return fail(workspace_failure::unsafe_path);
				}
				auto current = open_at(Root.Directory.get(), ".", O_RDONLY | O_DIRECTORY);
				if (!current.valid()) {
					return fail(errno_failure(errno));
				}
				for (const auto& part : *parts) {
					if (aStop.stop_requested()) {
						return fail(workspace_failure::cancelled);
					}
					auto next = open_at(current.get(), part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
					if (!next.valid()) {
						const auto error = errno;
						const auto known = stat_at(current.get(), part);
						if (known && S_ISLNK(known->st_mode)) {
							return fail(workspace_failure::escape);
						}
						return fail(errno_failure(error));
					}
					const auto known = stat_of(next.get());
					if (!known) {
						return fail(known.error());
					}
					if (static_cast<std::uint64_t>(known->st_dev) != Root.Observation.Root.Volume) {
						return fail(workspace_failure::escape);
					}
					current = std::move(next);
				}
				return current;
			}

			auto resolve(const std::string_view aPath, std::stop_token aStop) -> result<leaf_location>
			{
				if (Root.Observation.Kind == core::workspace_root_kind::file) {
					if (aPath != Root.FileName) {
						return fail(workspace_failure::missing);
					}
					auto parent = open_at(Root.Directory.get(), ".", O_RDONLY | O_DIRECTORY);
					if (!parent.valid()) {
						return fail(errno_failure(errno));
					}
					return leaf_location{ .Parent = std::move(parent), .Name = Root.FileName };
				}
				const auto parts = components(aPath);
				if (!parts || parts->empty()) {
					return fail(workspace_failure::unsafe_path);
				}
				const auto separator = aPath.find_last_of('/');
				auto       parent    = walk(separator == std::string_view::npos ? std::string_view{ } : aPath.substr(0, separator), aStop);
				if (!parent) {
					return fail(parent.error());
				}
				return leaf_location{ .Parent = std::move(*parent), .Name = parts->back() };
			}

			opened_root Root;
		};
#endif

		class local_source final : public application::workspace_source_port
		{
		public:
			auto open(const std::string_view aLocation, std::stop_token aStop) -> result<std::unique_ptr<application::workspace_root_session>> override
			{
				if (aStop.stop_requested()) {
					return fail(workspace_failure::cancelled);
				}
				auto root = open_root(aLocation);
				if (!root) {
					return fail(root.error());
				}
				return std::make_unique<session>(std::move(*root));
			}
		};
	}

	auto make_local_workspace_source() -> std::unique_ptr<application::workspace_source_port>
	{
		return std::make_unique<local_source>();
	}
}
