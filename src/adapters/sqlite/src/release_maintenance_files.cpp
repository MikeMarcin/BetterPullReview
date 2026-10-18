#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "../../native/windows_private_storage.hpp"
#include "bpr/application/diagnostic_log_files.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/core/profile_data_reset.hpp"
#include "bpr/core/sha256.hpp"
#include "storage_internal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <random>
#include <utility>
#include <vector>
#if !defined(_WIN32)
	#include <dirent.h>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::sqlite {
	namespace {
		using reason                         = core::release_maintenance_reason;
		using command                        = core::release_maintenance_command;
		using result                         = application::release_maintenance_result;
		constexpr char        journal_name[] = ".bpr-maintenance-v1";
		constexpr std::size_t journal_limit  = 2U * 1024U * 1024U;
		struct failure final : std::exception
		{
			reason Reason;
			explicit failure(reason aReason)
				: Reason{ aReason }
			{ }
			auto what() const noexcept -> const char* override { return "release maintenance native refusal"; }
		};
		[[noreturn]] void fail(reason aReason) { throw failure{ aReason }; }
		auto              digest(std::string_view aBytes) -> std::array<std::uint8_t, 32>
		{
			return core::sha256({ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() });
		}
		void cancelled(std::stop_token aStop)
		{
			if (aStop.stop_requested()) {
				fail(reason::cancelled);
			}
		}
		auto safe_relative(std::string_view aPath) -> bool
		{
			if (aPath.empty() || aPath.size() > 1024U || aPath.front() == '/' || aPath.back() == '/') {
				return false;
			}
			std::size_t depth = 0U;
			while (!aPath.empty()) {
				const auto end  = aPath.find('/');
				const auto part = aPath.substr(0U, end);
				if (++depth > 16U || part.empty() || part == "." || part == ".." || part.size() > 255U
					|| part.back() == '.' || part.back() == ' ' || std::ranges::any_of(part, [](unsigned char aCharacter) {
						   return aCharacter < 32U || aCharacter > 126U || aCharacter == '\\' || aCharacter == ':';
					   })) {
					return false;
				}
				if (end == std::string_view::npos) {
					break;
				}
				aPath.remove_prefix(end + 1U);
			}
			return true;
		}
		auto store_name(std::string_view aName) -> bool
		{
			constexpr std::array        names{ "appearance-settings.sqlite3", "diagnostic-preferences.sqlite3", "workbench-settings.sqlite3", "review-queue.sqlite3",
											   "review-ledger.sqlite3", "review-tours.sqlite3", "review-cycles.sqlite3", "review-overviews.sqlite3", "window-restoration.sqlite3", "remembered-connections.sqlite3", "device-bindings.sqlite3", "review-mutation-recovery.sqlite3", "epoch-catalog.sqlite3", "markdown-images.sqlite3" };
			static constexpr std::array suffixes{ "", "-wal", "-shm", "-journal", "-super-journal" };
			return std::ranges::any_of(names, [aName](std::string_view aBase) {
				return aName.starts_with(aBase) && std::ranges::find(suffixes, aName.substr(aBase.size())) != suffixes.end();
			});
		}
		auto excluded(std::string_view aPath) -> bool
		{
			constexpr std::array roots{ "profiles", "updates" };
			if (aPath == journal_name || aPath == "profile-runtime.lock") {
				return true;
			}
			if (std::ranges::find(roots, aPath) != roots.end()) {
				return true;
			}
			constexpr std::array shared{ "profile-registry.sqlite3", "release-update.sqlite3", "update-draft-recovery.sqlite3" };
			return std::ranges::any_of(shared, [aPath](std::string_view aName) {
				const auto base = std::string{ "state/v1/" } + std::string{ aName };
				if (!aPath.starts_with(base)) {
					return false;
				}
				const auto tail = aPath.substr(base.size());
				return tail.empty() || tail == "-wal" || tail == "-shm" || tail == "-journal";
			});
		}
		auto hexadecimal(std::string_view aText, std::size_t aSize) -> bool
		{
			return aText.size() == aSize && std::ranges::all_of(aText, [](char aCharacter) {
					   return (aCharacter >= '0' && aCharacter <= '9') || (aCharacter >= 'a' && aCharacter <= 'f');
				   });
		}
		auto hash_name(std::string_view aText, std::string_view aSuffix, std::size_t aSize = 64U) -> bool
		{
			return aText.ends_with(aSuffix) && hexadecimal(aText.substr(0U, aText.size() - aSuffix.size()), aSize);
		}
		auto parts(std::string_view aPath) -> std::vector<std::string_view>
		{
			std::vector<std::string_view> result;
			while (!aPath.empty()) {
				const auto end = aPath.find('/');
				result.push_back(aPath.substr(0U, end));
				if (end == std::string_view::npos) {
					break;
				}
				aPath.remove_prefix(end + 1U);
			}
			return result;
		}
		auto private_ref(std::string_view aPath, bool aDirectory) -> bool
		{
			const auto path = parts(aPath);
			if (path.size() <= 3U) {
				constexpr std::array prefix{ "refs", "betterpullreview", "v1" };
				return aDirectory && std::equal(path.begin(), path.end(), prefix.begin());
			}
			if (path[0] != "refs" || path[1] != "betterpullreview" || path[2] != "v1" || (path[3] != "scope" && path[3] != "stage")) {
				return false;
			}
			if (path.size() == 4U) {
				return aDirectory;
			}
			if (!hexadecimal(path[4], 64U)) {
				return false;
			}
			if (path.size() == 5U) {
				return aDirectory;
			}
			if (path[3] == "stage") {
				return !aDirectory && path.size() == 6U && (path[5] == "base" || path[5] == "head");
			}
			if (path[5] != "epoch") {
				return false;
			}
			if (path.size() == 6U) {
				return aDirectory;
			}
			if (!hexadecimal(path[6], 16U)) {
				return false;
			}
			if (path.size() == 7U) {
				return aDirectory;
			}
			return !aDirectory && path.size() == 8U && (path[7] == "base" || path[7] == "head");
		}
		auto repository_artifact(std::string_view aPath, bool aDirectory) -> bool
		{
			if (aPath.empty()) {
				return aDirectory;
			}
			constexpr std::array files{ "HEAD", "config", "betterpullreview-cache-manifest-v1" };
			if (std::ranges::find(files, aPath) != files.end()) {
				return !aDirectory;
			}
			constexpr std::array directories{ "objects", "objects/pack", "objects/info", "refs/heads", "refs/tags" };
			if (std::ranges::find(directories, aPath) != directories.end()) {
				return aDirectory;
			}
			if (aPath.starts_with("refs")) {
				return private_ref(aPath, aDirectory);
			}
			if (aPath.starts_with("objects/pack/pack-")) {
				const auto name = aPath.substr(18U);
				return !aDirectory && std::ranges::any_of(std::array{ ".pack", ".idx", ".rev" }, [name](std::string_view aSuffix) {
					return hash_name(name, aSuffix, 40U) || hash_name(name, aSuffix);
				});
			}
			const auto path = parts(aPath);
			if (path.size() < 2U || path[0] != "objects" || !hexadecimal(path[1], 2U)) {
				return false;
			}
			if (path.size() == 2U) {
				return aDirectory;
			}
			return !aDirectory && path.size() == 3U && (hexadecimal(path[2], 38U) || hexadecimal(path[2], 62U));
		}
		auto cache_artifact(std::string_view aPath, bool aDirectory) -> bool
		{
			const auto path = parts(aPath);
			if (path.size() < 2U || path[1] != "v1") {
				return false;
			}
			if (path.size() == 2U) {
				return aDirectory;
			}
			if (path[0] == "locks") {
				return !aDirectory && path.size() == 3U && hash_name(path[2], ".lock");
			}
			if (path[0] == "quarantine") {
				return !aDirectory && path.size() == 3U && hash_name(path[2], ".deleted");
			}
			// Unfinished spools/quarantines require their original recovery owner first.
			if (path[0] != "repositories" || !hash_name(path[2], ".git")) {
				return false;
			}
			const auto prefixSize = std::string_view{ "repositories/v1/" }.size() + path[2].size();
			return repository_artifact(aPath.size() == prefixSize ? std::string_view{ } : aPath.substr(prefixSize + 1U), aDirectory);
		}
		/// [[BPR-1283]]. The publication catalog's own isolation root
		/// ([[BPR-1177]]), relative to `publication-git-v1/`: the installed Git
		/// owner's closed isolation layout, whose work/home/temp directories and
		/// empty cache skeleton must be empty, and one bare object store per
		/// hash algorithm under `publication/v1`.
		auto publication_artifact(std::string_view aPath, bool aDirectory) -> bool
		{
			if (aPath == ".git-process-boundary.lock") {
				return !aDirectory;
			}
			constexpr std::array files{ "config-system", "config-global", "attributes-system" };
			if (std::ranges::find(files, aPath) != files.end()) {
				return !aDirectory;
			}
			constexpr std::array directories{ "home", "xdg", "tmp", "work", "hooks", "templates", "repositories", "repositories/v1", "incoming", "incoming/v1", "publication", "publication/v1" };
			if (std::ranges::find(directories, aPath) != directories.end()) {
				return aDirectory;
			}
			for (const auto store : { std::string_view{ "publication/v1/sha1.git" }, std::string_view{ "publication/v1/sha256.git" } }) {
				if (aPath == store) {
					return aDirectory;
				}
				if (aPath.starts_with(store) && aPath[store.size()] == '/') {
					return repository_artifact(aPath.substr(store.size() + 1U), aDirectory);
				}
			}
			return false;
		}
		/// \brief What one inventory covers. Its value is the journal's scope
		/// field: 0 and 1 are the original clear and purge, so their journals
		/// read as before; 11-14 are [[BPR-1602]]'s resets.
		enum class scope : std::uint64_t {
			clear            = 0U,
			purge            = 1U,
			reset_caches     = 11U,
			reset_settings   = 12U,
			reset_review     = 13U,
			reset_everything = 14U,
		};
		auto scope_of(core::profile_data_reset aReset) -> scope
		{
			switch (aReset) {
			case core::profile_data_reset::caches:
				return scope::reset_caches;
			case core::profile_data_reset::settings:
				return scope::reset_settings;
			case core::profile_data_reset::review_data:
				return scope::reset_review;
			case core::profile_data_reset::everything:
				return scope::reset_everything;
			}
			fail(reason::invalid_selection);
		}
		auto reset_of(scope aScope) -> std::optional<core::profile_data_reset>
		{
			switch (aScope) {
			case scope::reset_caches:
				return core::profile_data_reset::caches;
			case scope::reset_settings:
				return core::profile_data_reset::settings;
			case scope::reset_review:
				return core::profile_data_reset::review_data;
			case scope::reset_everything:
				return core::profile_data_reset::everything;
			case scope::clear:
			case scope::purge:
				break;
			}
			return std::nullopt;
		}
		auto known_scope(std::uint64_t aValue) -> bool
		{
			return aValue == 0U || aValue == 1U || (aValue >= 11U && aValue <= 14U);
		}
		/// \brief A `state/v1` entry the reset selects by name: a known store
		/// or one of its sidecars whose area the reset removes, or the epoch
		/// catalog's lock with the caches. Core owns which area a store is in.
		auto reset_store(std::string_view aName, core::profile_data_reset aReset) -> bool
		{
			if (aName == "epoch-catalog.lock") {
				return core::profile_data_reset_removes(aReset, core::profile_data_area::caches);
			}
			const auto extension = aName.find(".sqlite3");
			return extension != std::string_view::npos && store_name(aName)
				&& core::profile_data_reset_removes(aReset, core::profile_data_store_area(aName.substr(0U, extension)));
		}
		/// \brief Whether a reset's scan passes over `aPath` untouched: anything
		/// outside the folders and stores it removes. Nothing passed over is
		/// recorded, opened or deleted.
		auto reset_skips(std::string_view aPath, core::profile_data_reset aReset) -> bool
		{
			const auto path = parts(aPath);
			if (path.size() == 1U) {
				return aPath != "state" && !core::profile_data_reset_removes(aReset, core::profile_data_folder_area(aPath));
			}
			if (path[0] == "state") {
				return path.size() == 2U ? aPath != "state/v1" : path.size() != 3U || !reset_store(path[2], aReset);
			}
			return !core::profile_data_reset_removes(aReset, core::profile_data_folder_area(path[0]));
		}
		/// \brief Whether an entry the reset recorded is one it may remove. The
		/// cache folders are the application's own downloads, so every
		/// owner-private file and folder in them goes, partial spools and
		/// unindexed objects included: a reset is how a damaged cache is
		/// cleared. The publication store keeps the purge's exact layout.
		auto reset_member(std::string_view aPath, bool aDirectory, core::profile_data_reset aReset) -> bool
		{
			if (aPath == "state" || aPath == "state/v1") {
				return aDirectory;
			}
			const auto path = parts(aPath);
			if (path[0] == "state") {
				return !aDirectory && path.size() == 3U && reset_store(path[2], aReset);
			}
			if (!core::profile_data_reset_removes(aReset, core::profile_data_folder_area(path[0]))) {
				return false;
			}
			if (path[0] == "publication-git-v1") {
				return path.size() == 1U ? aDirectory : publication_artifact(aPath.substr(19U), aDirectory);
			}
			return path.size() > 1U || aDirectory;
		}
		auto skipped(std::string_view aPath, std::string_view aPrefix, scope aScope) -> bool
		{
			if (aPath == "logs/.retention.lock" || excluded(aPath)) {
				return true;
			}
			if (const auto reset = reset_of(aScope)) {
				return reset_skips(aPath, *reset);
			}
			return aScope == scope::clear && aPrefix.empty() && aPath != "logs";
		}
		auto classified(std::string_view aPath, bool aDirectory, scope aScope) -> bool
		{
			if (const auto reset = reset_of(aScope)) {
				return reset_member(aPath, aDirectory, *reset);
			}
			const auto purge = aScope == scope::purge;
			if (aPath == "logs") {
				return aDirectory;
			}
			if (aPath.starts_with("logs/") && application::diagnostic_log_file(aPath.substr(5U))) {
				return !aDirectory;
			}
			if (!purge) {
				return false;
			}
			if (aPath == "state" || aPath == "state/v1") {
				return aDirectory;
			}
			if (aPath == "state/v1/epoch-catalog.lock") {
				return !aDirectory;
			}
			if (aPath.starts_with("state/v1/")) {
				return !aDirectory && store_name(aPath.substr(9U));
			}
			if (aPath == ".git-process-boundary.lock" || aPath == "agent-workspace-ownership-v1/.git-process-boundary.lock") {
				return !aDirectory;
			}
			// The installed Git owner creates these exact private isolation files.
			// Its work/home/temp directories must be empty after producer retirement;
			// no descendant is accepted as an arbitrary recursively-owned tree.
			constexpr std::array gitFiles{ "config-system", "config-global", "attributes-system" };
			if (std::ranges::find(gitFiles, aPath) != gitFiles.end()) {
				return !aDirectory;
			}
			constexpr std::array gitDirectories{ "home", "xdg", "tmp", "work" };
			if (std::ranges::find(gitDirectories, aPath) != gitDirectories.end()) {
				return aDirectory;
			}
			// These namespaces belong exclusively to the existing cache/snapshot owners.
			// Their closed top-level layout prevents adopting a developer worktree as a cache.
			constexpr std::array containers{ "repositories", "incoming", "locks", "quarantine", "templates", "hooks", "mcp-snapshot", "agent-workspace-ownership-v1" };
			if (std::ranges::find(containers, aPath) != containers.end()) {
				return aDirectory;
			}
			if (aPath.starts_with("agent-workspace-ownership-v1/")) {
				return false; // Existing owner must recover these first.
			}
			if (aPath.starts_with("templates/") || aPath.starts_with("hooks/")) {
				return false;
			}
			if (aPath.starts_with("mcp-snapshot/")) {
				return false; // Window shutdown owns exact snapshot removal.
			}
			if (aPath == "publication-git-v1") {
				return aDirectory;
			}
			if (aPath.starts_with("publication-git-v1/")) {
				return publication_artifact(aPath.substr(19U), aDirectory);
			}
			return cache_artifact(aPath, aDirectory);
		}
		struct entry
		{
			std::string   Path;
			std::uint64_t Device                                 = 0U;
			std::uint64_t Inode                                  = 0U;
			std::uint64_t ObjectHigh                             = 0U;
			std::uint64_t Size                                   = 0U;
			std::uint64_t Modified                               = 0U;
			std::uint64_t ModifiedNano                           = 0U;
			bool          Directory                              = false;
			auto          operator==(const entry&) const -> bool = default;
		};
		struct inventory
		{
			std::uint64_t                                   Device          = 0U;
			std::uint64_t                                   Inode           = 0U;
			std::uint64_t                                   ObjectHigh      = 0U;
			std::uint64_t                                   Profile         = 0U;
			scope                                           Scope           = scope::clear;
			bool                                            CredentialsDone = false;
			application::release_profile_resource_inventory Credentials;
			std::vector<entry>                              Entries;
		};
		void append_integer(std::string& aBytes, std::uint64_t aValue)
		{
			for (unsigned shift = 0U; shift < 64U; shift += 8U) {
				aBytes.push_back(static_cast<char>((aValue >> shift) & 0xffU));
			}
		}
		auto read_integer(std::string_view& aBytes) -> std::uint64_t
		{
			if (aBytes.size() < 8U) {
				fail(reason::recovery_required);
			}
			std::uint64_t value = 0U;
			for (unsigned index = 0U; index < 8U; ++index) {
				value |= static_cast<std::uint64_t>(static_cast<unsigned char>(aBytes[index])) << (index * 8U);
			}
			aBytes.remove_prefix(8U);
			return value;
		}
		auto encode(const inventory& aInventory) -> std::string
		{
			std::string bytes{ "BPRMNT03" };
			append_integer(bytes, aInventory.Device);
			append_integer(bytes, aInventory.Inode);
			append_integer(bytes, aInventory.ObjectHigh);
			append_integer(bytes, aInventory.Profile);
			append_integer(bytes, static_cast<std::uint64_t>(aInventory.Scope));
			append_integer(bytes, aInventory.Credentials.Entries);
			bytes.append(reinterpret_cast<const char*>(aInventory.Credentials.Digest.data()), aInventory.Credentials.Digest.size());
			append_integer(bytes, aInventory.Entries.size());
			for (const auto& item : aInventory.Entries) {
				append_integer(bytes, item.Path.size());
				bytes.append(item.Path);
				append_integer(bytes, item.Device);
				append_integer(bytes, item.Inode);
				append_integer(bytes, item.ObjectHigh);
				append_integer(bytes, item.Size);
				append_integer(bytes, item.Modified);
				append_integer(bytes, item.ModifiedNano);
				append_integer(bytes, item.Directory ? 1U : 0U);
			}
			const auto hash = digest(bytes);
			bytes.append(reinterpret_cast<const char*>(hash.data()), hash.size());
			if (bytes.size() > journal_limit) {
				fail(reason::limit_exceeded);
			}
			return bytes;
		}
		auto decode(std::string aBytes) -> inventory
		{
			if (aBytes.size() < 8U + 32U || !aBytes.starts_with("BPRMNT03")) {
				fail(reason::recovery_required);
			}
			std::string_view bytes{ aBytes };
			bytes.remove_prefix(8U);
			inventory result;
			result.Device      = read_integer(bytes);
			result.Inode       = read_integer(bytes);
			result.ObjectHigh  = read_integer(bytes);
			result.Profile     = read_integer(bytes);
			const auto covered = read_integer(bytes);
			if (!known_scope(covered)) {
				fail(reason::recovery_required);
			}
			result.Scope           = static_cast<scope>(covered);
			const auto credentials = read_integer(bytes);
			if (credentials > application::maximum_device_bindings * 2U || bytes.size() < 32U) {
				fail(reason::recovery_required);
			}
			result.Credentials.Entries = static_cast<std::uint32_t>(credentials);
			std::copy_n(reinterpret_cast<const std::uint8_t*>(bytes.data()), 32U, result.Credentials.Digest.begin());
			bytes.remove_prefix(32U);
			const auto count = read_integer(bytes);
			if (count > core::release_maintenance_entry_limit - credentials) {
				fail(reason::recovery_required);
			}
			for (std::uint64_t index = 0U; index < count; ++index) {
				const auto size = read_integer(bytes);
				if (size > bytes.size() || size > 1024U) {
					fail(reason::recovery_required);
				}
				entry item;
				item.Path = bytes.substr(0U, static_cast<std::size_t>(size));
				bytes.remove_prefix(static_cast<std::size_t>(size));
				item.Device          = read_integer(bytes);
				item.Inode           = read_integer(bytes);
				item.ObjectHigh      = read_integer(bytes);
				item.Size            = read_integer(bytes);
				item.Modified        = read_integer(bytes);
				item.ModifiedNano    = read_integer(bytes);
				const auto directory = read_integer(bytes);
				item.Directory       = directory == 1U;
				if (directory > 1U || !safe_relative(item.Path) || !classified(item.Path, item.Directory, result.Scope)
					|| (!result.Entries.empty() && result.Entries.back().Path >= item.Path)) {
					fail(reason::recovery_required);
				}
				result.Entries.push_back(std::move(item));
			}
			const auto             hash = digest(std::string_view{ aBytes }.substr(0U, aBytes.size() - bytes.size()));
			const std::string_view expected{ reinterpret_cast<const char*>(hash.data()), hash.size() };
			if (bytes.size() < 32U || bytes.size() > 72U || bytes.substr(0U, 32U) != expected) {
				fail(reason::recovery_required);
			}
			bytes.remove_prefix(32U);
			// The inventory is immutable. An appended, digest-bound receipt is durable
			// before any native file is deleted. A torn receipt safely retries exact
			// idempotent credential deletion; it cannot be mistaken for completion.
			const auto receipt = std::string{ "BPRCRED1" } + std::string{ expected };
			if (!std::string_view{ receipt }.starts_with(bytes)) {
				fail(reason::recovery_required);
			}
			result.CredentialsDone = bytes.size() == receipt.size();
			return result;
		}
		class prepared final : public application::release_maintenance_preparation
		{
		public:
			const void*                         Owner = nullptr;
			core::release_maintenance_selection Descriptor;
			std::string                         Bytes;
			inventory                           Inventory;
			auto                                selection() const -> core::release_maintenance_selection override { return Descriptor; }
			auto                                preview() const noexcept -> std::string_view override { return Bytes; }
		};
#if !defined(_WIN32)
		using native_handle = int;
		struct descriptor
		{
			int Value = -1;
			explicit descriptor(int aValue = -1)
				: Value{ aValue }
			{ }
			~descriptor()
			{
				if (Value >= 0) {
					::close(Value);
				}
			}
			descriptor(const descriptor&)                    = delete;
			auto operator=(const descriptor&) -> descriptor& = delete;
			descriptor(descriptor&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{ }
			auto operator=(descriptor&& aOther) noexcept -> descriptor&
			{
				if (this != &aOther) {
					if (Value >= 0) {
						::close(Value);
					}
					Value = std::exchange(aOther.Value, -1);
				}
				return *this;
			}
		};
		// A stateless deleter: a function-pointer deleter would drop closedir's
		// declaration attributes from the template argument.
		struct directory_closer
		{
			void operator()(DIR* aStream) const noexcept { static_cast<void>(::closedir(aStream)); }
		};
		auto identity(const struct stat& aStat, std::string aPath) -> entry
		{
			if ((!S_ISREG(aStat.st_mode) && !S_ISDIR(aStat.st_mode)) || aStat.st_uid != ::geteuid()
				|| (aStat.st_mode & 0077U) != 0U || (S_ISREG(aStat.st_mode) && aStat.st_nlink != 1) || aStat.st_size < 0) {
				fail(reason::invalid_authority);
			}
			entry item{ .Path = std::move(aPath), .Device = static_cast<std::uint64_t>(aStat.st_dev), .Inode = static_cast<std::uint64_t>(aStat.st_ino), .Directory = S_ISDIR(aStat.st_mode) };
			if (!item.Directory) {
				item.Size = static_cast<std::uint64_t>(aStat.st_size);
	#if defined(__APPLE__)
				item.Modified     = static_cast<std::uint64_t>(aStat.st_mtimespec.tv_sec);
				item.ModifiedNano = static_cast<std::uint64_t>(aStat.st_mtimespec.tv_nsec);
	#else
				item.Modified     = static_cast<std::uint64_t>(aStat.st_mtim.tv_sec);
				item.ModifiedNano = static_cast<std::uint64_t>(aStat.st_mtim.tv_nsec);
	#endif
			}
			return item;
		}
		auto held_identity(int aHandle, std::string aPath) -> entry
		{
			struct stat status{ };
			if (::fstat(aHandle, &status) != 0) {
				fail(reason::io_failure);
			}
			return identity(status, std::move(aPath));
		}
		auto open_root(const std::filesystem::path& aPath) -> descriptor
		{
			descriptor root{ ::open(aPath.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			if (root.Value < 0 || !held_identity(root.Value, "").Directory) {
				fail(reason::invalid_authority);
			}
			return root;
		}
		auto inspect(int aParent, std::string_view aName) -> std::optional<entry>
		{
			const std::string name{ aName };
			struct stat       status{ };
			if (::fstatat(aParent, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) == 0) {
				return identity(status, name);
			}
			if (errno == ENOENT) {
				return std::nullopt;
			}
			fail(reason::io_failure);
		}
		auto open_directory(int aParent, std::string_view aName) -> descriptor
		{
			const std::string name{ aName };
			descriptor        result{ ::openat(aParent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			struct stat       status{ };
			if (result.Value < 0 || ::fstat(result.Value, &status) != 0) {
				fail(reason::invalid_authority);
			}
			static_cast<void>(identity(status, name));
			return result;
		}
		auto parent(int aRoot, std::string_view aPath) -> descriptor
		{
			if (!safe_relative(aPath)) {
				fail(reason::invalid_authority);
			}
			descriptor result{ ::dup(aRoot) };
			const auto final = aPath.rfind('/');
			if (final == std::string_view::npos) {
				return result;
			}
			auto path = aPath.substr(0U, final);
			while (!path.empty()) {
				const auto end = path.find('/');
				result         = open_directory(result.Value, path.substr(0U, end));
				if (end == std::string_view::npos) {
					break;
				}
				path.remove_prefix(end + 1U);
			}
			return result;
		}
		auto leaf(std::string_view aPath) -> std::string { return std::string{ aPath.substr(aPath.rfind('/') + 1U) }; }
		auto read_file(int aParent, std::string_view aName, std::size_t aLimit) -> std::string
		{
			const std::string name{ aName };
			descriptor        file{ ::openat(aParent, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC) };
			if (file.Value < 0) {
				if (errno == ENOENT) {
					return { };
				}
				fail(reason::io_failure);
			}
			struct stat status{ };
			if (::fstat(file.Value, &status) != 0) {
				fail(reason::io_failure);
			}
			const auto admitted = identity(status, name);
			if (admitted.Directory || admitted.Size > aLimit) {
				fail(reason::limit_exceeded);
			}
			std::string bytes(static_cast<std::size_t>(admitted.Size), '\0');
			std::size_t offset = 0U;
			while (offset < bytes.size()) {
				const auto count = ::read(file.Value, bytes.data() + offset, bytes.size() - offset);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					fail(reason::io_failure);
				}
				offset += static_cast<std::size_t>(count);
			}
			if (::fstat(file.Value, &status) != 0 || identity(status, name) != admitted || inspect(aParent, name) != admitted) {
				fail(reason::selection_changed);
			}
			return bytes;
		}
		void write_all(int aFile, std::string_view aBytes)
		{
			while (!aBytes.empty()) {
				const auto count = ::write(aFile, aBytes.data(), aBytes.size());
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					fail(reason::io_failure);
				}
				aBytes.remove_prefix(static_cast<std::size_t>(count));
			}
			if (::fsync(aFile) != 0) {
				fail(reason::io_failure);
			}
		}
		auto create_journal(int aRoot, std::string_view aBytes) -> descriptor
		{
			descriptor journal{ ::openat(aRoot, journal_name, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600) };
			if (journal.Value < 0) {
				fail(reason::recovery_required);
			}
			write_all(journal.Value, aBytes);
			if (::fsync(aRoot) != 0) {
				fail(reason::io_failure);
			}
			return journal;
		}
		auto open_journal(int aRoot) -> descriptor
		{
			descriptor journal{ ::openat(aRoot, journal_name, O_RDWR | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC) };
			if (journal.Value < 0) {
				fail(reason::recovery_required);
			}
			if (held_identity(journal.Value, "").Directory) {
				fail(reason::invalid_authority);
			}
			return journal;
		}
		void credentials_done(int aJournal, std::size_t aOffset, std::string_view aBytes)
		{
			if (::lseek(aJournal, static_cast<off_t>(aOffset), SEEK_SET) < 0) {
				fail(reason::io_failure);
			}
			write_all(aJournal, aBytes);
		}
		void remove_journal(int aRoot, int)
		{
			if (::unlinkat(aRoot, journal_name, 0) != 0 || ::fsync(aRoot) != 0) {
				fail(reason::io_failure);
			}
		}
		void remove_exact(int aRoot, const entry& aItem)
		{
			auto directory = parent(aRoot, aItem.Path);
			auto current   = inspect(directory.Value, leaf(aItem.Path));
			if (!current) {
				return;
			}
			current->Path = aItem.Path;
			if (*current != aItem) {
				fail(reason::selection_changed);
			}
			if (::unlinkat(directory.Value, leaf(aItem.Path).c_str(), aItem.Directory ? AT_REMOVEDIR : 0) != 0) {
				if (aItem.Directory && errno == ENOTEMPTY && (aItem.Path == "state" || aItem.Path == "state/v1" || aItem.Path == "logs")) {
					return;
				}
				fail(reason::recovery_required);
			}
			if (::fsync(directory.Value) != 0) {
				fail(reason::io_failure);
			}
		}
		void scan(int aDirectory, std::string_view aPrefix, scope aScope, std::vector<entry>& aEntries, std::stop_token aStop)
		{
			auto  directory = open_directory(aDirectory, ".");
			auto* stream    = ::fdopendir(std::exchange(directory.Value, -1));
			if (!stream) {
				fail(reason::io_failure);
			}
			const std::unique_ptr<DIR, directory_closer> owned{ stream };
			std::size_t                                  observed = 0U;
			while (true) {
				cancelled(aStop);
				errno      = 0;
				auto* item = ::readdir(stream);
				if (!item) {
					if (errno != 0) {
						fail(reason::io_failure);
					}
					break;
				}
				const auto name = std::string_view{ item->d_name };
				if (name == "." || name == "..") {
					continue;
				}
				if (++observed > core::release_maintenance_entry_limit) {
					fail(reason::limit_exceeded);
				}
				const auto path = aPrefix.empty() ? std::string{ name } : std::string{ aPrefix } + "/" + std::string{ name };
				if (skipped(path, aPrefix, aScope)) {
					continue;
				}
				auto observedEntry = inspect(aDirectory, name);
				if (!observedEntry) {
					fail(reason::selection_changed);
				}
				observedEntry->Path = path;
				if (observedEntry->Device != held_identity(aDirectory, "").Device) {
					fail(reason::invalid_authority);
				}
				if (!safe_relative(path) || !classified(path, observedEntry->Directory, aScope)) {
					fail(reason::incomplete_inventory);
				}
				if (aEntries.size() == core::release_maintenance_entry_limit) {
					fail(reason::limit_exceeded);
				}
				aEntries.push_back(*observedEntry);
				if (observedEntry->Directory) {
					auto child = open_directory(aDirectory, name);
					if (held_identity(child.Value, path) != *observedEntry) {
						fail(reason::selection_changed);
					}
					scan(child.Value, path, aScope, aEntries, aStop);
				}
			}
		}
		class local_output final : public application::release_diagnostic_output
		{
		public:
			std::filesystem::path ParentPath;
			std::string           Name;
			descriptor            Parent;
			std::uint64_t         Device = 0U;
			std::uint64_t         Inode  = 0U;
			auto                  still_owned() const -> bool
			{
				struct stat held{ }, named{ };
				return ::fstat(Parent.Value, &held) == 0 && ::lstat(ParentPath.c_str(), &named) == 0
					&& std::filesystem::canonical(ParentPath) == ParentPath
					&& S_ISDIR(named.st_mode) && named.st_uid == ::geteuid() && (named.st_mode & 0022U) == 0U
					&& static_cast<std::uint64_t>(held.st_dev) == Device && static_cast<std::uint64_t>(held.st_ino) == Inode
					&& held.st_dev == named.st_dev && held.st_ino == named.st_ino;
			}
			auto write(std::string_view aBytes, std::stop_token aStop, const std::function<bool()>& aBeginCommit) -> result override
			{
				std::string temporary;
				descriptor  file;
				const auto  removeTemporary = [&] {
					if (temporary.empty() || file.Value < 0) {
						return;
					}
					struct stat held{ }, named{ };
					if (::fstat(file.Value, &held) == 0 && ::fstatat(Parent.Value, temporary.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0
						&& held.st_dev == named.st_dev && held.st_ino == named.st_ino) {
						::unlinkat(Parent.Value, temporary.c_str(), 0);
					}
				};
				try {
					if (!still_owned() || aBytes.empty() || aBytes.size() > core::release_diagnostic_byte_limit) {
						fail(reason::invalid_authority);
					}
					cancelled(aStop);
					static std::atomic<std::uint64_t> sequence{ 0U };
					temporary = ".bpr-export-" + std::to_string(::getpid()) + "-" + std::to_string(sequence.fetch_add(1U));
					file      = descriptor{ ::openat(Parent.Value, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600) };
					if (file.Value < 0) {
						temporary.clear();
						fail(reason::io_failure);
					}
					write_all(file.Value, aBytes);
					cancelled(aStop);
					if (!still_owned()) {
						fail(reason::selection_changed);
					}
					if (inspect(Parent.Value, temporary) != held_identity(file.Value, temporary)) {
						fail(reason::selection_changed);
					}
					if (!aBeginCommit()) {
						fail(reason::cancelled);
					}
					if (::linkat(Parent.Value, temporary.c_str(), Parent.Value, Name.c_str(), 0) != 0) {
						fail(reason::io_failure);
					}
					if (::unlinkat(Parent.Value, temporary.c_str(), 0) != 0 || ::fsync(Parent.Value) != 0) {
						fail(reason::io_failure);
					}
					return { };
				} catch (const failure& error) {
					removeTemporary();
					return std::unexpected(error.Reason);
				} catch (...) {
					removeTemporary();
					return std::unexpected(reason::io_failure);
				}
			}
		};
#else
		namespace win       = native_storage;
		using native_handle = HANDLE;
		auto output_parent_owned(HANDLE aDirectory) -> bool
		{
			const auto identity = win::private_security::load();
			if (!identity) {
				return false;
			}
			PSID                 owner    = nullptr;
			PACL                 acl      = nullptr;
			PSECURITY_DESCRIPTOR raw      = nullptr;
			const auto           observed = ::GetSecurityInfo(aDirectory, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
															  &owner, nullptr, &acl, nullptr, &raw);
			win::local_memory    owned{ raw };
			if (observed != ERROR_SUCCESS || !owner || !acl || !::EqualSid(owner, identity->sid())) {
				return false;
			}
			std::array<std::byte, SECURITY_MAX_SID_SIZE> system{ }, administrators{ };
			DWORD                                        systemSize = static_cast<DWORD>(system.size()), adminSize = static_cast<DWORD>(administrators.size());
			if (!::CreateWellKnownSid(WinLocalSystemSid, nullptr, system.data(), &systemSize)
				|| !::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administrators.data(), &adminSize)) {
				return false;
			}
			constexpr ACCESS_MASK writes = GENERIC_ALL | GENERIC_WRITE | WRITE_DAC | WRITE_OWNER | DELETE
				| FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;
			for (DWORD index = 0U; index < acl->AceCount; ++index) {
				void* rawAce = nullptr;
				if (!::GetAce(acl, index, &rawAce)) {
					return false;
				}
				const auto* header = static_cast<ACE_HEADER*>(rawAce);
				if ((header->AceFlags & INHERIT_ONLY_ACE) != 0U || header->AceType == ACCESS_DENIED_ACE_TYPE) {
					continue;
				}
				if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) {
					return false;
				}
				const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(rawAce);
				if ((ace->Mask & writes) == 0U) {
					continue;
				}
				auto* sid = const_cast<DWORD*>(&ace->SidStart);
				if (!::EqualSid(sid, identity->sid()) && !::EqualSid(sid, system.data()) && !::EqualSid(sid, administrators.data())
					&& !(identity->contained() && ::EqualSid(sid, identity->container_sid()))) {
					return false;
				}
			}
			return true;
		}
		struct descriptor
		{
			HANDLE                                   Value = INVALID_HANDLE_VALUE;
			std::vector<win::private_windows_object> Ancestors;
			explicit descriptor(HANDLE aValue = INVALID_HANDLE_VALUE)
				: Value{ aValue }
			{ }
			~descriptor()
			{
				if (Value != INVALID_HANDLE_VALUE) {
					::CloseHandle(Value);
				}
			}
			descriptor(const descriptor&)                    = delete;
			auto operator=(const descriptor&) -> descriptor& = delete;
			descriptor(descriptor&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, INVALID_HANDLE_VALUE) }
				, Ancestors{ std::move(aOther.Ancestors) }
			{ }
			auto operator=(descriptor&& aOther) noexcept -> descriptor&
			{
				if (this != &aOther) {
					if (Value != INVALID_HANDLE_VALUE) {
						::CloseHandle(Value);
					}
					Value     = std::exchange(aOther.Value, INVALID_HANDLE_VALUE);
					Ancestors = std::move(aOther.Ancestors);
				}
				return *this;
			}
		};
		auto security() -> win::private_security
		{
			auto admitted = win::private_security::load();
			if (!admitted) {
				fail(reason::invalid_authority);
			}
			return std::move(*admitted);
		}
		auto path_for(HANDLE aParent, std::string_view aName) -> std::filesystem::path
		{
			const auto path = win::final_path_of(aParent);
			if (!path) {
				fail(reason::invalid_authority);
			}
			if (aName == ".") {
				return *path;
			}
			if (!safe_relative(aName) || aName.find('/') != std::string_view::npos) {
				fail(reason::invalid_authority);
			}
			return std::filesystem::path{ *path } / std::string{ aName };
		}
		auto held_identity(HANDLE aHandle, std::string aPath) -> entry
		{
			FILE_ATTRIBUTE_TAG_INFO attributes{ };
			FILE_STANDARD_INFO      standard{ };
			FILE_BASIC_INFO         times{ };
			if (!::GetFileInformationByHandleEx(aHandle, FileAttributeTagInfo, &attributes, sizeof(attributes))
				|| !::GetFileInformationByHandleEx(aHandle, FileStandardInfo, &standard, sizeof(standard))
				|| !::GetFileInformationByHandleEx(aHandle, FileBasicInfo, &times, sizeof(times))) {
				fail(reason::io_failure);
			}
			const bool directory = (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
			const auto kind      = directory ? win::windows_object_kind::directory : win::windows_object_kind::regular_file;
			const auto native    = win::identity_of(aHandle, kind);
			if (!native || standard.EndOfFile.QuadPart < 0) {
				fail(reason::invalid_authority);
			}
			// [[BPR-1038]]. The walk admits each entry only after its parent,
			// so an entry whose descriptor is exactly the owner-private entries
			// inherited from that parent is as private as the parent. The
			// installed Git owner's isolation directories, and the ones Git
			// creates inside them, are made that way. A descriptor that is
			// neither that nor the protected one is refused for what it is.
			if (!win::validate_private_security(aHandle, security(), kind, true)) {
				fail(reason::storage_not_private);
			}
			std::string_view object{ reinterpret_cast<const char*>(native->Object.data()), native->Object.size() };
			entry            observed{ .Path = std::move(aPath), .Device = native->Volume, .Inode = read_integer(object), .ObjectHigh = read_integer(object), .Directory = directory };
			if (!directory) {
				observed.Size     = static_cast<std::uint64_t>(standard.EndOfFile.QuadPart);
				observed.Modified = static_cast<std::uint64_t>(times.LastWriteTime.QuadPart);
			}
			return observed;
		}
		auto open_native(HANDLE aParent, std::string_view aName, DWORD aAccess, DWORD aSharing = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE) -> descriptor
		{
			const auto path = path_for(aParent, aName);
			descriptor object{ ::CreateFileW(path.c_str(), aAccess | READ_CONTROL | FILE_READ_ATTRIBUTES, aSharing,
											 nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
			if (object.Value == INVALID_HANDLE_VALUE || !win::path_matches_handle(object.Value, path)) {
				fail(reason::invalid_authority);
			}
			static_cast<void>(held_identity(object.Value, std::string{ aName }));
			return object;
		}
		auto open_root(const std::filesystem::path& aPath) -> descriptor
		{
			const auto canonical = win::canonical_windows_root(aPath);
			if (!canonical) {
				fail(reason::invalid_authority);
			}
			auto ancestors = win::validate_ancestor_chain(*canonical);
			auto root      = win::open_private_directory(*canonical, security(), false);
			if (!ancestors || !root || !win::reliable_local_filesystem(*root)) {
				fail(reason::invalid_authority);
			}
			descriptor result{ std::exchange(root->Handle.Value, INVALID_HANDLE_VALUE) };
			result.Ancestors = std::move(*ancestors);
			return result;
		}
		auto inspect(HANDLE aParent, std::string_view aName) -> std::optional<entry>
		{
			const auto path = path_for(aParent, aName);
			descriptor object{ ::CreateFileW(path.c_str(), READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
											 nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
			if (object.Value == INVALID_HANDLE_VALUE) {
				const auto error = ::GetLastError();
				if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
					return std::nullopt;
				}
				fail(reason::io_failure);
			}
			if (!win::path_matches_handle(object.Value, path)) {
				fail(reason::invalid_authority);
			}
			return held_identity(object.Value, std::string{ aName });
		}
		auto open_directory(HANDLE aParent, std::string_view aName) -> descriptor
		{
			auto object = open_native(aParent, aName, FILE_LIST_DIRECTORY | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE);
			if (!held_identity(object.Value, std::string{ aName }).Directory) {
				fail(reason::invalid_authority);
			}
			return object;
		}
		auto parent(HANDLE aRoot, std::string_view aPath) -> descriptor
		{
			if (!safe_relative(aPath)) {
				fail(reason::invalid_authority);
			}
			auto       result = open_directory(aRoot, ".");
			const auto final  = aPath.rfind('/');
			if (final == std::string_view::npos) {
				return result;
			}
			auto path = aPath.substr(0U, final);
			while (!path.empty()) {
				const auto end   = path.find('/');
				auto       child = open_directory(result.Value, path.substr(0U, end));
				child.Ancestors  = std::move(result.Ancestors);
				win::private_windows_object held;
				held.Handle = win::windows_handle{ std::exchange(result.Value, INVALID_HANDLE_VALUE) };
				child.Ancestors.push_back(std::move(held));
				result = std::move(child);
				if (end == std::string_view::npos) {
					break;
				}
				path.remove_prefix(end + 1U);
			}
			return result;
		}
		auto leaf(std::string_view aPath) -> std::string { return std::string{ aPath.substr(aPath.rfind('/') + 1U) }; }
		auto read_file(HANDLE aParent, std::string_view aName, std::size_t aLimit) -> std::string
		{
			if (!inspect(aParent, aName)) {
				return { };
			}
			auto       file     = open_native(aParent, aName, GENERIC_READ);
			const auto admitted = held_identity(file.Value, std::string{ aName });
			if (admitted.Directory || admitted.Size > aLimit) {
				fail(reason::limit_exceeded);
			}
			std::string bytes(static_cast<std::size_t>(admitted.Size), '\0');
			std::size_t offset = 0U;
			while (offset < bytes.size()) {
				DWORD      count  = 0U;
				const auto wanted = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 65536U));
				if (!::ReadFile(file.Value, bytes.data() + offset, wanted, &count, nullptr) || count == 0U) {
					fail(reason::io_failure);
				}
				offset += count;
			}
			if (held_identity(file.Value, std::string{ aName }) != admitted || inspect(aParent, aName) != admitted) {
				fail(reason::selection_changed);
			}
			return bytes;
		}
		void write_all(HANDLE aFile, std::string_view aBytes)
		{
			while (!aBytes.empty()) {
				DWORD      count  = 0U;
				const auto wanted = static_cast<DWORD>(std::min<std::size_t>(aBytes.size(), 65536U));
				if (!::WriteFile(aFile, aBytes.data(), wanted, &count, nullptr) || count == 0U) {
					fail(reason::io_failure);
				}
				aBytes.remove_prefix(count);
			}
			if (!::FlushFileBuffers(aFile)) {
				fail(reason::io_failure);
			}
		}
		auto create_private_file(HANDLE aParent, std::string_view aName) -> descriptor
		{
			const auto path = path_for(aParent, aName);
			// [[BPR-1030]]. The descriptor names its owner by pointer into this
			// object, so it must outlive the CreateFileW below.
			const auto                       owner = security();
			win::private_security_attributes attributes;
			if (!attributes.initialize(owner, win::windows_object_kind::regular_file)) {
				fail(reason::invalid_authority);
			}
			descriptor file{ ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE | READ_CONTROL,
										   FILE_SHARE_READ | FILE_SHARE_WRITE, attributes.attributes(), CREATE_NEW,
										   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, nullptr) };
			if (file.Value == INVALID_HANDLE_VALUE) {
				fail(reason::io_failure);
			}
			if (!win::path_matches_handle(file.Value, path) || held_identity(file.Value, std::string{ aName }).Directory) {
				fail(reason::invalid_authority);
			}
			return file;
		}
		auto create_journal(HANDLE aRoot, std::string_view aBytes) -> descriptor
		{
			auto journal = create_private_file(aRoot, journal_name);
			write_all(journal.Value, aBytes);
			return journal;
		}
		auto open_journal(HANDLE aRoot) -> descriptor { return open_native(aRoot, journal_name, GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE); }
		void credentials_done(HANDLE aJournal, std::size_t aOffset, std::string_view aBytes)
		{
			LARGE_INTEGER offset{ };
			offset.QuadPart = static_cast<LONGLONG>(aOffset);
			if (!::SetFilePointerEx(aJournal, offset, nullptr, FILE_BEGIN)) {
				fail(reason::io_failure);
			}
			write_all(aJournal, aBytes);
		}
		void dispose(HANDLE aHandle)
		{
			FILE_DISPOSITION_INFO disposition{ TRUE };
			if (!::SetFileInformationByHandle(aHandle, FileDispositionInfo, &disposition, sizeof(disposition))) {
				fail(reason::io_failure);
			}
		}
		void remove_journal(HANDLE, HANDLE aJournal) { dispose(aJournal); }
		auto empty_directory(const std::filesystem::path& aPath) -> bool
		{
			WIN32_FIND_DATAW data{ };
			const auto       search = ::FindFirstFileW((aPath / L"*").c_str(), &data);
			if (search == INVALID_HANDLE_VALUE) {
				fail(reason::io_failure);
			}
			auto empty = true;
			do {
				const std::wstring_view name{ data.cFileName };
				if (name != L"." && name != L"..") {
					empty = false;
					break;
				}
			} while (::FindNextFileW(search, &data));
			::FindClose(search);
			return empty;
		}
		void remove_exact(HANDLE aRoot, const entry& aItem)
		{
			auto directory = parent(aRoot, aItem.Path);
			if (!inspect(directory.Value, leaf(aItem.Path))) {
				return;
			}
			// [[BPR-1038]]. `state` and `state/v1` keep the shared registry and
			// updater stores, which the running desktop holds open. POSIX answers
			// a removal there with "not empty", which is accepted below; Windows
			// refuses the DELETE handle first, with a sharing violation. So they
			// are kept, without asking, while they still hold anything.
			if (aItem.Directory && (aItem.Path == "state" || aItem.Path == "state/v1" || aItem.Path == "logs") && !empty_directory(path_for(directory.Value, leaf(aItem.Path)))) {
				return;
			}
			auto object = open_native(directory.Value, leaf(aItem.Path), DELETE, FILE_SHARE_READ);
			if (held_identity(object.Value, aItem.Path) != aItem) {
				fail(reason::selection_changed);
			}
			// [[BPR-1038]]. Git writes its packs and pack indexes read-only, and
			// the plain disposition refuses a read-only file, so a purge that
			// reached the cache stopped part-way and needed recovery. This one
			// deletes the file as it is, without clearing the attribute first.
			auto disposition  = FILE_DISPOSITION_INFO_EX{ };
			disposition.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;
			if (!::SetFileInformationByHandle(object.Value, FileDispositionInfoEx, &disposition, sizeof(disposition))) {
				if (aItem.Directory && ::GetLastError() == ERROR_DIR_NOT_EMPTY && (aItem.Path == "state" || aItem.Path == "state/v1" || aItem.Path == "logs")) {
					return;
				}
				fail(reason::recovery_required);
			}
		}
		void scan(HANDLE aDirectory, std::string_view aPrefix, scope aScope, std::vector<entry>& aEntries, std::stop_token aStop)
		{
			const auto path = win::final_path_of(aDirectory);
			if (!path) {
				fail(reason::invalid_authority);
			}
			WIN32_FIND_DATAW data{ };
			const auto       search = ::FindFirstFileW((std::filesystem::path{ *path } / L"*").c_str(), &data);
			if (search == INVALID_HANDLE_VALUE) {
				if (::GetLastError() == ERROR_FILE_NOT_FOUND) {
					return;
				}
				fail(reason::io_failure);
			}
			struct search_guard
			{
				HANDLE Value;
				~search_guard() { ::FindClose(Value); }
			} guard{ search };
			std::size_t count = 0U;
			do {
				cancelled(aStop);
				const std::wstring_view nativeName{ data.cFileName };
				if (nativeName == L"." || nativeName == L"..") {
					continue;
				}
				if (++count > core::release_maintenance_entry_limit) {
					fail(reason::limit_exceeded);
				}
				if (!std::ranges::all_of(nativeName, [](wchar_t aCharacter) { return aCharacter >= 32 && aCharacter <= 126; })) {
					fail(reason::incomplete_inventory);
				}
				std::string name;
				name.reserve(nativeName.size());
				std::ranges::transform(nativeName, std::back_inserter(name), [](wchar_t aCharacter) { return static_cast<char>(aCharacter); });
				const auto relative = aPrefix.empty() ? name : std::string{ aPrefix } + "/" + name;
				if (skipped(relative, aPrefix, aScope)) {
					continue;
				}
				auto observed = inspect(aDirectory, name);
				if (!observed) {
					fail(reason::selection_changed);
				}
				observed->Path = relative;
				if (observed->Device != held_identity(aDirectory, "").Device) {
					fail(reason::invalid_authority);
				}
				if (!safe_relative(relative) || !classified(relative, observed->Directory, aScope)) {
					fail(reason::incomplete_inventory);
				}
				if (aEntries.size() == core::release_maintenance_entry_limit) {
					fail(reason::limit_exceeded);
				}
				aEntries.push_back(*observed);
				if (observed->Directory) {
					auto child = open_directory(aDirectory, name);
					if (held_identity(child.Value, relative) != *observed) {
						fail(reason::selection_changed);
					}
					scan(child.Value, relative, aScope, aEntries, aStop);
				}
			} while (::FindNextFileW(search, &data));
			if (::GetLastError() != ERROR_NO_MORE_FILES) {
				fail(reason::io_failure);
			}
		}
		class local_output final : public application::release_diagnostic_output
		{
		public:
			descriptor            Parent;
			std::filesystem::path Name;
			std::filesystem::path ParentPath;
			win::file_identity    Identity;
			auto                  still_owned() const -> bool
			{
				return win::path_matches_handle(Parent.Value, ParentPath).has_value()
					&& win::identity_of(Parent.Value, win::windows_object_kind::directory) == Identity && output_parent_owned(Parent.Value);
			}
			auto write(std::string_view aBytes, std::stop_token aStop, const std::function<bool()>& aBeginCommit) -> result override
			{
				descriptor file;
				bool       published       = false;
				const auto removeTemporary = [&] {
					if (file.Value != INVALID_HANDLE_VALUE && !published) {
						FILE_DISPOSITION_INFO remove{ TRUE };
						::SetFileInformationByHandle(file.Value, FileDispositionInfo, &remove, sizeof(remove));
					}
				};
				try {
					if (!still_owned() || aBytes.empty() || aBytes.size() > core::release_diagnostic_byte_limit) {
						fail(reason::invalid_authority);
					}
					cancelled(aStop);
					static std::atomic<std::uint64_t> sequence{ 0U };
					const auto                        temporary = ".bpr-export-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(sequence.fetch_add(1U));
					file                                        = create_private_file(Parent.Value, temporary);
					write_all(file.Value, aBytes);
					cancelled(aStop);
					if (!still_owned()) {
						fail(reason::selection_changed);
					}
					if (!aBeginCommit()) {
						fail(reason::cancelled);
					}
					const auto             target = (ParentPath / Name).native();
					const auto             length = target.size() * sizeof(wchar_t);
					std::vector<std::byte> buffer(sizeof(FILE_RENAME_INFO) + length);
					auto*                  rename = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
					rename->ReplaceIfExists       = FALSE;
					rename->RootDirectory         = nullptr;
					rename->FileNameLength        = static_cast<DWORD>(length);
					std::copy(target.begin(), target.end(), rename->FileName);
					if (!::SetFileInformationByHandle(file.Value, FileRenameInfo, rename, static_cast<DWORD>(buffer.size()))) {
						fail(reason::io_failure);
					}
					published = true;
					if (!::FlushFileBuffers(file.Value)) {
						fail(reason::io_failure);
					}
					return { };
				} catch (const failure& error) {
					removeTemporary();
					return std::unexpected(error.Reason);
				} catch (...) {
					removeTemporary();
					return std::unexpected(reason::io_failure);
				}
			}
		};
#endif
		auto read_diagnostic_files(native_handle aRoot, std::stop_token aStop, std::string_view aSession = { }) -> std::string
		{
			if (!inspect(aRoot, "logs")) {
				return { };
			}
			auto               logs = open_directory(aRoot, "logs");
			std::vector<entry> entries;
			scan(logs.Value, "logs", scope::clear, entries, aStop);
			std::ranges::sort(entries, { }, &entry::Path);
			std::string bytes;
			std::size_t inputBytes = 0U;
			for (const auto& item : entries) {
				cancelled(aStop);
				if (!aSession.empty() && !leaf(item.Path).starts_with(std::string{ "session-" } + std::string{ aSession } + "-")) {
					continue;
				}
				auto part = read_file(logs.Value, leaf(item.Path), application::release_diagnostic_input_limit - inputBytes);
				inputBytes += part.size();
				// Count original input even when discarding a tail. One empty line is
				// one omission; no partial prefix can join the next file's first line.
				if (!part.empty() && part.back() != '\n') {
					part.resize(part.rfind('\n') + 1U);
					part.push_back('\n');
				}
				bytes += part;
			}
			return bytes;
		}
		auto export_namespace() -> std::string
		{
			std::random_device           source;
			std::array<std::uint8_t, 16> bytes{ };
			for (auto& byte : bytes) {
				byte = static_cast<std::uint8_t>(source() & 255U);
			}
			return core::lower_hex(bytes);
		}

		void validate_cache_manifests(native_handle aRoot, const std::vector<entry>& aEntries)
		{
			for (const auto& item : aEntries) {
				const auto path = parts(item.Path);
				if (!item.Directory || path.size() != 3U || path[0] != "repositories") {
					continue;
				}
				auto       directory = parent(aRoot, item.Path + "/betterpullreview-cache-manifest-v1");
				const auto bytes     = read_file(directory.Value, "betterpullreview-cache-manifest-v1", 142U);
				if ((bytes.size() != 141U && bytes.size() != 142U) || bytes.substr(0U, 11U) != std::string{ "BPRCACHE\0\0\0", 11U }
					|| (bytes[11U] != '\1' && bytes[11U] != '\2') || (bytes[12U] != '\1' && bytes[12U] != '\2')) {
					fail(reason::incomplete_inventory);
				}
				const auto cache = core::lower_hex(std::string_view{ bytes }.substr(85U, 32U));
				if (path[2] != cache + ".git") {
					fail(reason::invalid_authority);
				}
				auto locks = parent(aRoot, "locks/v1/" + cache + ".lock");
				if (!inspect(locks.Value, cache + ".lock")) {
					fail(reason::incomplete_inventory);
				}
			}
		}
	}

	struct release_maintenance_files::implementation
	{
		std::filesystem::path                                 Root;
		std::shared_ptr<application::profile_runtime_lease>   Lease;
		core::release_maintenance_authority                   Authority;
		application::release_profile_cleanup_port*            Credentials;
		std::shared_ptr<application::diagnostic_control_port> Diagnostics;
		std::uint64_t                                         Revision  = 0U;
		bool                                                  Available = false;
		descriptor                                            RootHandle;
		std::uint64_t                                         Device     = 0U;
		std::uint64_t                                         Inode      = 0U;
		std::uint64_t                                         ObjectHigh = 0U;
		implementation(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
					   core::bpr_profile_id aProfile, core::application_session_id aSession, application::release_profile_cleanup_port* aCredentials, std::shared_ptr<application::diagnostic_control_port> aDiagnostics)
			: Root{ std::move(aRoot) }
			, Lease{ std::move(aLease) }
			, Authority{ aProfile, aSession, { } }
			, Credentials{ aCredentials }
			, Diagnostics{ std::move(aDiagnostics) }
		{
			if (aProfile.value() == 0U || aSession.value() == 0U || !detail::validates_profile_runtime_lease(Root, Lease)) {
				return;
			}
			try {
				RootHandle      = open_root(Root);
				const auto root = held_identity(RootHandle.Value, "");
				Device          = root.Device;
				Inode           = root.Inode;
				ObjectHigh      = root.ObjectHigh;
				Authority.Root  = core::release_maintenance_root_id{ Inode == 0U ? ObjectHigh : Inode };
				Available       = Authority.Root.value() != 0U;
			} catch (const failure&) {
				// A replacement during admission leaves an unavailable capability.
			}
		}
		void validate() const
		{
			if (!Available || !detail::validates_profile_runtime_lease(Root, Lease)) {
				fail(reason::invalid_authority);
			}
			const auto held = held_identity(RootHandle.Value, "");
			if (!held.Directory || held.Device != Device || held.Inode != Inode || held.ObjectHigh != ObjectHigh) {
				fail(reason::invalid_authority);
			}
		}
		auto capture(scope aScope, std::stop_token aStop) -> inventory
		{
			const auto purge = aScope == scope::purge;
			inventory  observed{ Device, Inode, ObjectHigh, Authority.Profile.value(), aScope, false, { }, { } };
			// A scoped binding-store read is closed before inventorying its native files.
			if (purge && Credentials) {
				auto credentials = Credentials->inspect(aStop);
				if (!credentials) {
					fail(credentials.error());
				}
				observed.Credentials = *credentials;
			}
			scan(RootHandle.Value, "", aScope, observed.Entries, aStop);
			std::ranges::sort(observed.Entries, { }, &entry::Path);
			if (purge) {
				auto workspaceRoot = Root;
				workspaceRoot += "-agent-workspaces-v1";
				std::error_code error;
				if (std::filesystem::exists(workspaceRoot, error)) {
					if (!detail::validate_private_storage_root(workspaceRoot) || !std::filesystem::is_empty(workspaceRoot, error) || error) {
						fail(reason::recovery_required);
					}
				} else if (error) {
					fail(reason::io_failure);
				}
				const auto bindings = std::ranges::any_of(observed.Entries, [](const entry& aEntry) { return aEntry.Path.starts_with("state/v1/device-bindings.sqlite3"); });
				if (bindings && !Credentials) {
					fail(reason::unavailable);
				}
				validate_cache_manifests(RootHandle.Value, observed.Entries);
			}
			if (observed.Credentials.Entries > core::release_maintenance_entry_limit - observed.Entries.size()) {
				fail(reason::limit_exceeded);
			}
			return observed;
		}
		auto validate_inventory(const inventory& aInventory, bool aRecovery) -> std::vector<entry>
		{
			validate();
			if (aInventory.Device != Device || aInventory.Inode != Inode || aInventory.ObjectHigh != ObjectHigh || aInventory.Profile != Authority.Profile.value()) {
				fail(reason::invalid_authority);
			}
			std::vector<entry> current;
			scan(RootHandle.Value, "", aInventory.Scope, current, { });
			std::ranges::sort(current, { }, &entry::Path);
			if (!aRecovery && current != aInventory.Entries) {
				fail(reason::selection_changed);
			}
			for (const auto& item : current) {
				auto found = std::ranges::lower_bound(aInventory.Entries, item.Path, { }, &entry::Path);
				if (found == aInventory.Entries.end() || *found != item) {
					fail(reason::selection_changed);
				}
			}
			return current;
		}
		void delete_files(const std::vector<entry>& aEntries)
		{
			for (const auto& item : aEntries) {
				if (item.Directory) {
					continue;
				}
				validate();
				remove_exact(RootHandle.Value, item);
			}
		}
		void delete_empty_directories(const std::vector<entry>& aEntries)
		{
			for (auto item = aEntries.rbegin(); item != aEntries.rend(); ++item) {
				if (!item->Directory) {
					continue;
				}
				validate();
				remove_exact(RootHandle.Value, *item);
			}
		}
		/// \brief Copies every recorded `state/v1` file into `aFolder`, which it
		/// creates owner-private and refuses when it exists. The sources are
		/// the exact recorded files: the inventory is checked against them
		/// again after copying, so a copy of a changed file is never kept.
		void copy_stores(const inventory& aInventory, const std::filesystem::path& aFolder) const
		{
			if (!aFolder.is_absolute() || aFolder.lexically_normal() != aFolder || !aFolder.has_filename()) {
				fail(reason::invalid_authority);
			}
			std::error_code error;
			if (!std::filesystem::create_directory(aFolder, error) || error) {
				fail(reason::selection_changed);
			}
			try {
				std::filesystem::permissions(aFolder, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
				for (const auto& item : aInventory.Entries) {
					if (item.Directory || !item.Path.starts_with("state/v1/")) {
						continue;
					}
					validate();
					std::filesystem::copy_file(Root / std::filesystem::path{ item.Path }, aFolder / leaf(item.Path), std::filesystem::copy_options::none);
				}
				std::vector<entry> current;
				scan(RootHandle.Value, "", aInventory.Scope, current, { });
				std::ranges::sort(current, { }, &entry::Path);
				if (current != aInventory.Entries) {
					fail(reason::selection_changed);
				}
			} catch (...) {
				std::filesystem::remove_all(aFolder, error);
				throw;
			}
		}
		void validate_journal(native_handle aJournal)
		{
			const auto held = held_identity(aJournal, std::string{ journal_name });
			if (held.Directory || inspect(RootHandle.Value, journal_name) != held) {
				fail(reason::selection_changed);
			}
		}
		void replay(inventory aInventory, native_handle aJournal)
		{
			validate_journal(aJournal);
			static_cast<void>(validate_inventory(aInventory, true));
			if (!aInventory.CredentialsDone) {
				if (aInventory.Scope == scope::purge && Credentials) {
					auto erased = Credentials->erase(aInventory.Credentials);
					if (!erased) {
						fail(erased.error());
					}
				} else if (aInventory.Credentials.Entries != 0U) {
					fail(reason::unavailable);
				}
				validate_journal(aJournal);
				const auto encoded = encode(aInventory);
				const auto receipt = std::string{ "BPRCRED1" } + encoded.substr(encoded.size() - 32U);
				credentials_done(aJournal, encoded.size(), receipt);
			}
			const auto current = validate_inventory(aInventory, true);
			delete_files(current);
			delete_empty_directories(current);
			validate_journal(aJournal);
			remove_journal(RootHandle.Value, aJournal);
		}
	};
	release_maintenance_files::release_maintenance_files(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
														 core::bpr_profile_id aProfile, core::application_session_id aSession, application::release_profile_cleanup_port* aCredentials, std::shared_ptr<application::diagnostic_control_port> aDiagnostics)
		: Implementation{ std::make_unique<implementation>(std::move(aRoot), std::move(aLease), aProfile, aSession, aCredentials, std::move(aDiagnostics)) }
	{ }
	release_maintenance_files::~release_maintenance_files() = default;
	auto release_maintenance_files::authority() const noexcept -> core::release_maintenance_authority { return Implementation->Authority; }
	auto release_maintenance_files::capabilities() const noexcept -> core::release_maintenance_capabilities
	{
		const auto available = Implementation->Available;
		return { available, available, available, available };
	}
	auto release_maintenance_files::prepare(const core::release_maintenance_context& aContext, std::stop_token aStop)
		-> std::expected<std::shared_ptr<const application::release_maintenance_preparation>, reason>
	{
		try {
			auto& self = *Implementation;
			self.validate();
			cancelled(aStop);
			if (aContext.Authority != self.Authority) {
				fail(reason::invalid_authority);
			}
			if (self.Revision == std::numeric_limits<std::uint64_t>::max()) {
				fail(reason::identity_exhausted);
			}
			const core::release_maintenance_revision revision{ ++self.Revision };
			auto                                     candidate = std::make_shared<prepared>();
			candidate->Owner                                   = &self;
			if (inspect(self.RootHandle.Value, journal_name)) {
				fail(reason::recovery_required);
			}
			if (aContext.Operation == command::preview_diagnostics) {
				application::diagnostic_export_capture capture;
				if (self.Diagnostics) {
					auto flushed = self.Diagnostics->flush_for_export(aContext.ExportOptions.IncludeRecent);
					if (!flushed) {
						fail(reason::unavailable);
					}
					capture = std::move(*flushed);
				} else if (aContext.ExportOptions.IncludeRecent || aContext.ExportOptions.Scope == core::diagnostic_export_scope::current_session) {
					fail(reason::unavailable);
				}
				const auto bytes    = read_diagnostic_files(self.RootHandle.Value, aStop,
															aContext.ExportOptions.Scope == core::diagnostic_export_scope::current_session ? std::string_view{ capture.Session } : std::string_view{ });
				auto       document = aContext.ExportOptions.Policy == core::diagnostic_export_policy::metadata
					? application::prepare_release_diagnostics(bytes, revision)
					: application::prepare_diagnostic_export(bytes, revision, aContext.ExportOptions, capture, export_namespace());
				if (!document) {
					fail(document.error());
				}
				candidate->Descriptor = document->Descriptor;
				candidate->Bytes      = std::move(document->Bytes);
			} else if (aContext.Operation == command::prepare_clear || aContext.Operation == command::prepare_purge) {
				candidate->Inventory  = self.capture(aContext.Operation == command::prepare_purge ? scope::purge : scope::clear, aStop);
				const auto encoded    = encode(candidate->Inventory);
				candidate->Descriptor = core::release_maintenance_inventory{ revision, digest(encoded),
																			 candidate->Inventory.Scope == scope::purge ? core::release_profile_purge_categories : 1U,
																			 static_cast<std::uint32_t>(candidate->Inventory.Entries.size()) + candidate->Inventory.Credentials.Entries, 0U, true };
			} else {
				fail(reason::invalid_command);
			}
			self.validate();
			cancelled(aStop);
			return candidate;
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}
	auto release_maintenance_files::erase(const core::release_maintenance_context&            aContext,
										  const application::release_maintenance_preparation& aPreparation, std::stop_token aStop,
										  const std::function<bool()>& aBeginCommit) -> result
	{
		try {
			auto& self = *Implementation;
			self.validate();
			const auto* candidate = dynamic_cast<const prepared*>(&aPreparation);
			if (!candidate || candidate->Owner != &self || aContext.Authority != self.Authority || candidate->Descriptor != aContext.Selection) {
				fail(reason::invalid_authority);
			}
			if ((aContext.Operation != command::clear_diagnostics && aContext.Operation != command::purge_profile)
				|| candidate->Inventory.Scope != (aContext.Operation == command::purge_profile ? scope::purge : scope::clear)) {
				fail(reason::invalid_command);
			}
			if (self.capture(candidate->Inventory.Scope, aStop).Credentials != candidate->Inventory.Credentials) {
				fail(reason::selection_changed);
			}
			static_cast<void>(self.validate_inventory(candidate->Inventory, false));
			cancelled(aStop);
			if (!aBeginCommit()) {
				fail(reason::cancelled);
			}
			const auto bytes   = encode(candidate->Inventory);
			auto       journal = create_journal(self.RootHandle.Value, bytes);
			self.replay(candidate->Inventory, journal.Value);
			return { };
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}
	auto release_maintenance_files::revalidate(const core::release_maintenance_context&            aContext,
											   const application::release_maintenance_preparation& aPreparation) -> result
	{
		try {
			auto& self = *Implementation;
			self.validate();
			const auto* candidate = dynamic_cast<const prepared*>(&aPreparation);
			if (!candidate || candidate->Owner != &self || aContext.Authority != self.Authority || candidate->Descriptor != aContext.Selection) {
				fail(reason::invalid_authority);
			}
			return { };
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}
	auto release_maintenance_files::reset(const core::profile_data_reset aReset, const std::optional<std::filesystem::path>& aCopy, std::stop_token aStop)
		-> std::expected<std::uint32_t, reason>
	{
		try {
			auto& self = *Implementation;
			self.validate();
			cancelled(aStop);
			if (inspect(self.RootHandle.Value, journal_name)) {
				fail(reason::recovery_required);
			}
			const auto observed = self.capture(scope_of(aReset), aStop);
			if (aCopy) {
				self.copy_stores(observed, *aCopy);
			}
			static_cast<void>(self.validate_inventory(observed, false));
			cancelled(aStop);
			auto journal = create_journal(self.RootHandle.Value, encode(observed));
			self.replay(observed, journal.Value);
			return static_cast<std::uint32_t>(observed.Entries.size());
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}
	auto release_maintenance_files::recover() -> result
	{
		try {
			auto& self = *Implementation;
			self.validate();
			if (!inspect(self.RootHandle.Value, journal_name)) {
				return { };
			}
			auto       journal   = open_journal(self.RootHandle.Value);
			const auto admitted  = held_identity(journal.Value, std::string{ journal_name });
			auto       bytes     = read_file(self.RootHandle.Value, journal_name, journal_limit + 40U);
			auto       inventory = decode(std::move(bytes));
			if (held_identity(journal.Value, std::string{ journal_name }) != admitted) {
				fail(reason::selection_changed);
			}
			self.replay(std::move(inventory), journal.Value);
			return { };
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}
	namespace {
		auto admit_output_directory(const std::filesystem::path& aPath) -> std::shared_ptr<local_output>
		{
			if (!aPath.is_absolute() || aPath.lexically_normal() != aPath) {
				fail(reason::invalid_authority);
			}
#if !defined(_WIN32)
			if (std::filesystem::canonical(aPath) != aPath) {
				fail(reason::invalid_authority);
			}
			auto output        = std::make_shared<local_output>();
			output->ParentPath = aPath;
			output->Parent     = descriptor{ ::open(aPath.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			struct stat status{ };
			if (output->Parent.Value < 0 || ::fstat(output->Parent.Value, &status) != 0 || ::faccessat(output->Parent.Value, ".", W_OK | X_OK, AT_EACCESS) != 0) {
				fail(reason::invalid_authority);
			}
			output->Device = static_cast<std::uint64_t>(status.st_dev);
			output->Inode  = static_cast<std::uint64_t>(status.st_ino);
#else
			const auto canonical = win::canonical_windows_root(aPath);
			if (!canonical) {
				fail(reason::invalid_authority);
			}
			auto ancestors = win::validate_ancestor_chain(*canonical);
			auto parent    = win::open_existing_object(*canonical, win::windows_object_kind::directory, FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_ADD_FILE, false);
			if (!ancestors || !parent || !output_parent_owned(parent->Value)) {
				fail(reason::invalid_authority);
			}
			const auto identity = win::identity_of(parent->Value, win::windows_object_kind::directory);
			if (!identity || !win::path_matches_handle(parent->Value, *canonical)) {
				fail(reason::invalid_authority);
			}
			auto output              = std::make_shared<local_output>();
			output->Parent           = descriptor{ std::exchange(parent->Value, INVALID_HANDLE_VALUE) };
			output->Parent.Ancestors = std::move(*ancestors);
			output->ParentPath       = *canonical;
			output->Identity         = *identity;
#endif
			if (!output->still_owned()) {
				fail(reason::invalid_authority);
			}
			return output;
		}
	}

	auto admit_release_diagnostic_directory(const std::filesystem::path& aPath) -> result
	{
		try {
			static_cast<void>(admit_output_directory(aPath));
			return { };
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}

	auto admit_release_diagnostic_output(const std::filesystem::path& aPath)
		-> std::expected<std::shared_ptr<application::release_diagnostic_output>, reason>
	{
		try {
#if !defined(_WIN32)
			if (!aPath.is_absolute() || aPath.lexically_normal() != aPath || !aPath.has_filename()) {
				fail(reason::invalid_authority);
			}
			auto output  = admit_output_directory(aPath.parent_path());
			output->Name = aPath.filename().string();
			struct stat existing{ };
			if (::fstatat(output->Parent.Value, output->Name.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) {
				fail(reason::selection_changed);
			}
			return output;
#else
			if (!aPath.is_absolute() || aPath.lexically_normal() != aPath || !aPath.has_filename()) {
				fail(reason::invalid_authority);
			}
			const auto canonicalTarget = win::canonical_windows_root(aPath);
			if (!canonicalTarget) {
				fail(reason::invalid_authority);
			}
			auto output  = admit_output_directory(aPath.parent_path());
			output->Name = aPath.filename();
			if (!output->still_owned() || ::GetFileAttributesW(canonicalTarget->c_str()) != INVALID_FILE_ATTRIBUTES
				|| ::GetLastError() != ERROR_FILE_NOT_FOUND) {
				fail(reason::selection_changed);
			}
			return output;
#endif
		} catch (const failure& error) {
			return std::unexpected(error.Reason);
		} catch (...) {
			return std::unexpected(reason::io_failure);
		}
	}
}
