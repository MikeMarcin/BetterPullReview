#include "bpr/adapters/update/release_update_staging.hpp"
#include "macos_release_update_mounts.hpp"
#include "release_update_transaction.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QCryptographicHash>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVersionNumber>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstring>
#include <set>

namespace bpr::adapters::update {
	namespace {
		namespace fs               = std::filesystem;
		using reason               = core::release_update_reason;
		using deadline             = std::chrono::steady_clock::time_point;
		constexpr auto bundle_name = "BetterPullReview.app";
		struct descriptor
		{
			int Value = -1;
			explicit descriptor(int aValue = -1)
				: Value{ aValue }
			{ }
			~descriptor()
			{
				if (Value >= 0) {
					close(Value);
				}
			}
			descriptor(const descriptor&)                    = delete;
			auto operator=(const descriptor&) -> descriptor& = delete;
		};
		template <typename T>
		struct cf_owner
		{
			T Value = nullptr;
			~cf_owner()
			{
				if (Value) {
					CFRelease(Value);
				}
			}
			cf_owner()                                   = default;
			cf_owner(const cf_owner&)                    = delete;
			auto operator=(const cf_owner&) -> cf_owner& = delete;
		};
		auto interrupted(std::stop_token aStop, deadline aDeadline) -> reason
		{
			return aStop.stop_requested() ? reason::cancelled : std::chrono::steady_clock::now() >= aDeadline ? reason::timed_out
																											  : reason::none;
		}
		auto private_directory(const fs::path& aPath) -> bool
		{
			std::error_code error;
			struct stat     value{ };
			return aPath.is_absolute() && fs::canonical(aPath, error) == aPath && !error
				&& lstat(aPath.c_str(), &value) == 0 && S_ISDIR(value.st_mode) && value.st_uid == getuid() && (value.st_mode & 0077U) == 0;
		}
		auto same_inode(const fs::path& aPath, int aDescriptor) -> bool
		{
			struct stat named{ }, held{ };
			return lstat(aPath.c_str(), &named) == 0 && fstat(aDescriptor, &held) == 0
				&& named.st_dev == held.st_dev && named.st_ino == held.st_ino && named.st_mode == held.st_mode;
		}
		void erase_contents(int aDirectory, std::size_t& aRemaining)
		{
			const auto copy = dup(aDirectory);
			if (copy < 0) {
				return;
			}
			auto* entries = fdopendir(copy);
			if (!entries) {
				close(copy);
				return;
			}
			while (auto* item = readdir(entries)) {
				const std::string_view name{ item->d_name };
				if (name == "." || name == "..") {
					continue;
				}
				if (aRemaining == 0U) {
					break;
				}
				--aRemaining;
				struct stat info{ };
				if (fstatat(aDirectory, item->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0) {
					continue;
				}
				if (S_ISDIR(info.st_mode)) {
					descriptor child{ openat(aDirectory, item->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
					if (child.Value >= 0) {
						erase_contents(child.Value, aRemaining);
						static_cast<void>(unlinkat(aDirectory, item->d_name, AT_REMOVEDIR));
					}
				} else {
					static_cast<void>(unlinkat(aDirectory, item->d_name, 0));
				}
			}
			closedir(entries);
		}
		struct transaction
		{
			fs::path                      Path;
			descriptor                    Directory;
			descriptor                    Archive;
			descriptor                    Lock;
			application::effect_executor& Cleanup;
			transaction(fs::path aPath, int aDirectory, int aArchive, int aLock, application::effect_executor& aCleanup)
				: Path{ std::move(aPath) }
				, Directory{ aDirectory }
				, Archive{ aArchive }
				, Lock{ aLock }
				, Cleanup{ aCleanup }
			{ }
			~transaction()
			{
				const auto directory = std::exchange(Directory.Value, -1);
				const auto lock      = std::exchange(Lock.Value, -1);
				try {
					Cleanup.post([path = Path, directory, lock] {
						descriptor held{ directory };
						descriptor heldLock{ lock };
						if (!same_inode(path, held.Value)) {
							return;
						}
						std::size_t remaining = application::release_update_maximum_entries * 2U + 8U;
						erase_contents(held.Value, remaining);
						if (same_inode(path, held.Value)) {
							static_cast<void>(rmdir(path.c_str()));
						}
					});
				} catch (...) {
					// Preserve owned material when cleanup cannot be queued. Never guess
					// that another launch may delete it from name or age alone.
					if (directory >= 0) {
						close(directory);
					}
					if (lock >= 0) {
						close(lock);
					}
				}
			}
		};
		struct owned_archive final : application::release_update_archive
		{
			std::shared_ptr<transaction> Transaction;
			QByteArray                   Digest;
			owned_archive(core::release_update_candidate aCandidate, std::shared_ptr<transaction> aTransaction, QByteArray aDigest)
				: release_update_archive{ std::move(aCandidate) }
				, Transaction{ std::move(aTransaction) }
				, Digest{ std::move(aDigest) }
			{ }
		};
		struct owned_bundle final : application::release_update_verified_bundle
		{
			std::shared_ptr<transaction> Transaction;
			QByteArray                   Manifest;
			owned_bundle(core::release_update_candidate aCandidate, std::shared_ptr<transaction> aTransaction, QByteArray aManifest)
				: release_update_verified_bundle{ std::move(aCandidate) }
				, Transaction{ std::move(aTransaction) }
				, Manifest{ std::move(aManifest) }
			{ }
		};
		class archive_writer final : public application::release_update_archive_writer
		{
		public:
			archive_writer(core::release_update_candidate aCandidate, std::shared_ptr<transaction> aTransaction)
				: Candidate{ std::move(aCandidate) }
				, Transaction{ std::move(aTransaction) }
			{ }
			auto append(std::span<const std::byte> aBytes) -> bool override
			{
				if (Finished || Failed || aBytes.size() > Candidate.Size - Bytes) {
					Failed = true;
					return false;
				}
				auto remaining = aBytes;
				while (!remaining.empty()) {
					const auto written = write(Transaction->Archive.Value, remaining.data(), remaining.size());
					if (written <= 0) {
						Failed = true;
						return false;
					}
					remaining = remaining.subspan(static_cast<std::size_t>(written));
				}
				Hash.addData(QByteArrayView{ reinterpret_cast<const char*>(aBytes.data()), static_cast<qsizetype>(aBytes.size()) });
				Bytes += aBytes.size();
				return true;
			}
			auto finish() -> std::shared_ptr<const application::release_update_archive> override
			{
				if (Finished || Failed || Bytes != Candidate.Size || fsync(Transaction->Archive.Value) != 0) {
					return { };
				}
				Finished = true;
				return std::make_shared<owned_archive>(Candidate, Transaction, Hash.result());
			}

		private:
			core::release_update_candidate Candidate;
			std::shared_ptr<transaction>   Transaction;
			QCryptographicHash             Hash{ QCryptographicHash::Sha256 };
			std::uint64_t                  Bytes    = 0;
			bool                           Finished = false;
			bool                           Failed   = false;
		};
		auto hash_file(int aFile, std::uint64_t aLimit, std::stop_token aStop, deadline aDeadline) -> std::optional<QByteArray>
		{
			QCryptographicHash      hash{ QCryptographicHash::Sha256 };
			std::array<char, 65536> bytes{ };
			std::uint64_t           offset = 0;
			while (interrupted(aStop, aDeadline) == reason::none) {
				const auto count = pread(aFile, bytes.data(), bytes.size(), static_cast<off_t>(offset));
				if (count < 0) {
					return { };
				}
				if (count == 0) {
					return hash.result();
				}
				if (static_cast<std::uint64_t>(count) > aLimit - offset) {
					return { };
				}
				offset += static_cast<std::uint64_t>(count);
				hash.addData(QByteArrayView{ bytes.data(), static_cast<qsizetype>(count) });
			}
			return { };
		}
		auto archive_path(const char* aName) -> std::optional<fs::path>
		{
			if (!aName || std::strlen(aName) > 4096U) {
				return { };
			}
			std::string name{ aName };
			if (name.ends_with('/')) {
				name.pop_back();
			}
			fs::path path{ name };
			if (path.empty() || path.is_absolute() || name.find('\\') != std::string::npos) {
				return { };
			}
			std::size_t components = 0;
			for (const auto& component : path) {
				if (component.empty() || component == "." || component == ".." || ++components > 64U) {
					return { };
				}
			}
			return path;
		}
		auto ensure_directories(int aRoot, const fs::path& aPath) -> int
		{
			auto current = dup(aRoot);
			for (const auto& component : aPath) {
				if (mkdirat(current, component.c_str(), 0755) != 0 && errno != EEXIST) {
					close(current);
					return -1;
				}
				const auto next = openat(current, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
				close(current);
				if (next < 0) {
					return -1;
				}
				current = next;
			}
			return current;
		}
		auto extract_entry(archive* aReader, archive_entry* aEntry, int aRoot, const fs::path& aPath,
						   std::uint64_t& aTotal, std::stop_token aStop, deadline aDeadline) -> reason
		{
			descriptor parent{ ensure_directories(aRoot, aPath.parent_path()) };
			if (parent.Value < 0) {
				return reason::unsafe_archive;
			}
			const auto type = archive_entry_filetype(aEntry);
			if (type == AE_IFDIR) {
				descriptor directory{ ensure_directories(aRoot, aPath) };
				return directory.Value >= 0 ? reason::none : reason::unsafe_archive;
			}
			if (type != AE_IFREG) {
				return reason::unsafe_archive;
			}
			descriptor file{ openat(parent.Value, aPath.filename().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600) };
			if (file.Value < 0) {
				return reason::unsafe_archive;
			}
			std::array<std::byte, 65536> bytes{ };
			std::uint64_t                written = 0;
			for (;;) {
				if (const auto stop = interrupted(aStop, aDeadline); stop != reason::none) {
					return stop;
				}
				const auto count = archive_read_data(aReader, bytes.data(), bytes.size());
				if (count < 0) {
					return reason::unsafe_archive;
				}
				if (count == 0) {
					break;
				}
				const auto size = static_cast<std::uint64_t>(count);
				if (size > application::release_update_maximum_expanded_bytes - aTotal) {
					return reason::size_limit;
				}
				aTotal += size;
				written += size;
				std::size_t offset = 0;
				while (offset < size) {
					const auto output = write(file.Value, bytes.data() + offset, static_cast<std::size_t>(size) - offset);
					if (output <= 0) {
						return reason::unavailable;
					}
					offset += static_cast<std::size_t>(output);
				}
			}
			if (archive_entry_size(aEntry) < 0 || written != static_cast<std::uint64_t>(archive_entry_size(aEntry))) {
				return reason::unsafe_archive;
			}
			return fchmod(file.Value, static_cast<mode_t>(0644U | (archive_entry_perm(aEntry) & 0111U))) == 0 ? reason::none : reason::unavailable;
		}
		auto extract_archive(const transaction& aTransaction, std::stop_token aStop, deadline aDeadline) -> reason
		{
			auto reader = std::unique_ptr<archive, decltype(&archive_read_free)>{ archive_read_new(), archive_read_free };
			if (!reader || archive_read_support_format_zip(reader.get()) != ARCHIVE_OK || lseek(aTransaction.Archive.Value, 0, SEEK_SET) < 0
				|| archive_read_open_fd(reader.get(), aTransaction.Archive.Value, 65536) != ARCHIVE_OK) {
				return reason::unsafe_archive;
			}
			std::set<fs::path>                         paths;
			std::vector<std::pair<fs::path, fs::path>> links;
			std::uint64_t                              total   = 0;
			std::uint64_t                              entries = 0;
			archive_entry*                             entry   = nullptr;
			int                                        next    = ARCHIVE_OK;
			while ((next = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
				if (const auto stop = interrupted(aStop, aDeadline); stop != reason::none) {
					return stop;
				}
				if (++entries > application::release_update_maximum_entries) {
					return reason::size_limit;
				}
				const auto path = archive_path(archive_entry_pathname(entry));
				if (!path || !paths.insert(*path).second || archive_entry_hardlink(entry) || archive_entry_is_encrypted(entry) == 1) {
					return reason::unsafe_archive;
				}
				// ditto's AppleDouble metadata is ignored, never extracted or executed.
				const bool metadata = *path->begin() == "__MACOSX" || path->filename().string().starts_with("._");
				if (*path->begin() != bundle_name && !metadata) {
					return reason::invalid_bundle;
				}
				const auto size = archive_entry_size(entry);
				if (size < 0 || static_cast<std::uint64_t>(size) > application::release_update_maximum_expanded_bytes - total) {
					return reason::size_limit;
				}
				if (metadata) {
					std::array<std::byte, 65536> bytes{ };
					for (;;) {
						if (const auto stop = interrupted(aStop, aDeadline); stop != reason::none) {
							return stop;
						}
						const auto count = archive_read_data(reader.get(), bytes.data(), bytes.size());
						if (count < 0) {
							return reason::unsafe_archive;
						}
						if (count == 0) {
							break;
						}
						if (static_cast<std::uint64_t>(count) > application::release_update_maximum_expanded_bytes - total) {
							return reason::size_limit;
						}
						total += static_cast<std::uint64_t>(count);
					}
				} else if (archive_entry_filetype(entry) == AE_IFLNK) {
					const auto* target = archive_entry_symlink(entry);
					if (!target || std::strlen(target) > 4096U || fs::path{ target }.is_absolute()) {
						return reason::unsafe_archive;
					}
					const auto resolved = (path->parent_path() / target).lexically_normal();
					if (resolved.empty() || *resolved.begin() != bundle_name) {
						return reason::unsafe_archive;
					}
					links.emplace_back(*path, target);
				} else {
					const auto result = extract_entry(reader.get(), entry, aTransaction.Directory.Value, *path, total, aStop, aDeadline);
					if (result != reason::none) {
						return result;
					}
				}
			}
			if (next != ARCHIVE_EOF) {
				return reason::unsafe_archive;
			}
			for (const auto& [path, target] : links) {
				descriptor parent{ ensure_directories(aTransaction.Directory.Value, path.parent_path()) };
				if (parent.Value < 0 || symlinkat(target.c_str(), parent.Value, path.filename().c_str()) != 0) {
					return reason::unsafe_archive;
				}
			}
			const auto bundle = aTransaction.Path / bundle_name;
			for (const auto& [path, target] : links) {
				static_cast<void>(target);
				std::error_code error;
				const auto      resolved = fs::canonical(aTransaction.Path / path, error);
				const auto      relative = resolved.lexically_relative(bundle);
				if (error || relative.empty() || *relative.begin() == "..") {
					return reason::unsafe_archive;
				}
			}
			return reason::none;
		}
		auto tool(const QString& aExecutable, const QStringList& aArguments, std::stop_token aStop, deadline aDeadline, QByteArray* aOutput = nullptr) -> reason
		{
			if (const auto stop = interrupted(aStop, aDeadline); stop != reason::none) {
				return stop;
			}
			QProcess process;
			process.setProcessEnvironment(QProcessEnvironment{ });
			process.setProcessChannelMode(QProcess::SeparateChannels);
			process.setInputChannelMode(QProcess::ManagedInputChannel);
			process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::DisableCoreDumps | QProcess::UnixProcessFlag::ResetSignalHandlers);
			process.start(aExecutable, aArguments, QIODevice::ReadOnly);
			if (!process.waitForStarted(3000)) {
				return reason::unavailable;
			}
			std::uint64_t bytes   = 0;
			reason        result  = reason::none;
			auto          consume = [&] {
				const auto output = process.readAllStandardOutput();
				bytes += static_cast<std::uint64_t>(output.size() + process.readAllStandardError().size());
				if (bytes > 65536U) {
					result = reason::size_limit;
				} else if (aOutput) {
					aOutput->append(output);
				}
			};
			while (process.state() != QProcess::NotRunning) {
				static_cast<void>(process.waitForFinished(20));
				result = interrupted(aStop, aDeadline);
				consume();
				if (result != reason::none) {
					process.kill();
					static_cast<void>(process.waitForFinished(3000));
					break;
				}
			}
			consume();
			if (result != reason::none) {
				return result;
			}
			return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 ? reason::none : reason::wrong_signer;
		}
		auto string_value(CFDictionaryRef aDictionary, CFStringRef aKey) -> std::string
		{
			const auto value = CFDictionaryGetValue(aDictionary, aKey);
			if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
				return { };
			}
			std::array<char, 4096> bytes{ };
			return CFStringGetCString(static_cast<CFStringRef>(value), bytes.data(), static_cast<CFIndex>(bytes.size()), kCFStringEncodingUTF8) ? std::string{ bytes.data() } : std::string{ };
		}
		auto trusted_requirement(const fs::path& aInstalled) -> std::optional<std::string>
		{
			cf_owner<CFURLRef> url;
			url.Value = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(aInstalled.c_str()), static_cast<CFIndex>(aInstalled.native().size()), true);
			cf_owner<SecStaticCodeRef>  code;
			cf_owner<CFDictionaryRef>   info;
			cf_owner<SecRequirementRef> requirement;
			cf_owner<CFStringRef>       requirementText;
			if (!url.Value || SecStaticCodeCreateWithPath(url.Value, kSecCSDefaultFlags, &code.Value) != errSecSuccess
				|| SecCodeCopySigningInformation(code.Value, kSecCSSigningInformation, &info.Value) != errSecSuccess
				|| string_value(info.Value, kSecCodeInfoIdentifier) != application::release_update_bundle_identifier
				|| string_value(info.Value, kSecCodeInfoTeamIdentifier) != application::release_update_signing_team
				|| SecCodeCopyDesignatedRequirement(code.Value, kSecCSDefaultFlags, &requirement.Value) != errSecSuccess
				|| SecRequirementCopyString(requirement.Value, kSecCSDefaultFlags, &requirementText.Value) != errSecSuccess) {
				return { };
			}
			std::array<char, 8192> bytes{ };
			if (!CFStringGetCString(requirementText.Value, bytes.data(), static_cast<CFIndex>(bytes.size()), kCFStringEncodingUTF8)) {
				return { };
			}
			return std::string{ bytes.data() };
		}

		auto numbered_bundle_identity(CFDictionaryRef aDictionary, const std::string& aVersion) -> bool
		{
			const auto build         = string_value(aDictionary, CFSTR("BPRReleaseBuild"));
			const auto nativeVersion = string_value(aDictionary, CFSTR("CFBundleVersion"));
			const auto configuration = string_value(aDictionary, CFSTR("BPRReleaseConfiguration"));
			const auto cohort        = string_value(aDictionary, CFSTR("BPRReleaseCohort"));
			if (build.empty() || build == "0") {
				return configuration.empty() && cohort.empty() && (nativeVersion.empty() || nativeVersion == aVersion);
			}
			unsigned   number = 0U;
			const auto parsed = std::from_chars(build.data(), build.data() + build.size(), number);
			const auto digest = [](const std::string& aText) {
				return aText.size() == 64U && std::ranges::all_of(aText, [](const char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); });
			};
			return parsed.ec == std::errc{ } && parsed.ptr == build.data() + build.size() && number > 0U && number <= 9999U
				&& !build.starts_with('0') && nativeVersion == build && digest(configuration) && digest(cohort);
		}
		auto bundle_identity(const fs::path& aBundle, const core::release_update_candidate& aCandidate) -> reason
		{
			descriptor               file{ open((aBundle / "Contents/Info.plist").c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
			std::array<UInt8, 65537> bytes{ };
			const auto               count = file.Value < 0 ? -1 : read(file.Value, bytes.data(), bytes.size());
			if (count <= 0 || count > 65536) {
				return reason::invalid_bundle;
			}
			cf_owner<CFDataRef> data;
			data.Value = CFDataCreate(nullptr, bytes.data(), static_cast<CFIndex>(count));
			cf_owner<CFPropertyListRef> plist;
			plist.Value = CFPropertyListCreateWithData(nullptr, data.Value, kCFPropertyListImmutable, nullptr, nullptr);
			if (!plist.Value || CFGetTypeID(plist.Value) != CFDictionaryGetTypeID()) {
				return reason::invalid_bundle;
			}
			const auto dictionary   = static_cast<CFDictionaryRef>(plist.Value);
			const auto identifier   = string_value(dictionary, CFSTR("CFBundleIdentifier"));
			const auto version      = string_value(dictionary, CFSTR("CFBundleShortVersionString"));
			const auto minimumText  = string_value(dictionary, CFSTR("LSMinimumSystemVersion"));
			const auto minimum      = QVersionNumber::fromString(QString::fromStdString(minimumText));
			const auto os           = QOperatingSystemVersion::current();
			const auto exactVersion = aCandidate.Tag.starts_with('v') ? aCandidate.Tag.substr(1) : aCandidate.Tag;
			if (identifier != application::release_update_bundle_identifier || version != exactVersion || core::parse_release_update_version(version) != aCandidate.Version
				|| !numbered_bundle_identity(dictionary, version)
				|| (!minimumText.empty() && (minimum.isNull() || minimum > QVersionNumber{ os.majorVersion(), os.minorVersion(), os.microVersion() }))) {
				return reason::invalid_bundle;
			}
			const auto executable = string_value(dictionary, CFSTR("CFBundleExecutable"));
			if (executable.empty() || executable.find_first_of("/\\") != std::string::npos || executable == "." || executable == "..") {
				return reason::invalid_bundle;
			}
			descriptor                    binary{ open((aBundle / "Contents/MacOS" / executable).c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
			std::array<unsigned char, 72> header{ };
			const auto                    available = binary.Value < 0 ? -1 : read(binary.Value, header.data(), header.size());
			auto                          word      = [&header](std::size_t aOffset) { return (static_cast<std::uint32_t>(header[aOffset]) << 24U) | (static_cast<std::uint32_t>(header[aOffset + 1U]) << 16U) | (static_cast<std::uint32_t>(header[aOffset + 2U]) << 8U) | header[aOffset + 3U]; };
			if (available < 48 || (word(0) != 0xcafebabeU && word(0) != 0xcafebabfU) || word(4) != 2U) {
				return reason::wrong_architecture;
			}
			const bool fat64 = word(0) == 0xcafebabfU;
			if (fat64 && available < 72) {
				return reason::wrong_architecture;
			}
			const auto                    second = fat64 ? 40U : 28U;
			const std::set<std::uint32_t> architectures{ word(8), word(second) };
			if (architectures != std::set<std::uint32_t>{ 0x01000007U, 0x0100000cU }) {
				return reason::wrong_architecture;
			}
			struct stat binaryInfo{ };
			if (fstat(binary.Value, &binaryInfo) != 0 || !S_ISREG(binaryInfo.st_mode) || binaryInfo.st_size <= 0) {
				return reason::invalid_bundle;
			}
			std::uint64_t previousEnd = fat64 ? 72U : 48U;
			for (const auto architecture : { 8U, second }) {
				const auto offset = fat64 ? (static_cast<std::uint64_t>(word(architecture + 8U)) << 32U) | word(architecture + 12U) : word(architecture + 8U);
				const auto size   = fat64 ? (static_cast<std::uint64_t>(word(architecture + 16U)) << 32U) | word(architecture + 20U) : word(architecture + 12U);
				if (offset < previousEnd || offset > static_cast<std::uint64_t>(binaryInfo.st_size) || size < 32U
					|| size > static_cast<std::uint64_t>(binaryInfo.st_size) - offset) {
					return reason::wrong_architecture;
				}
				previousEnd = offset + size;
				std::array<unsigned char, 32> mach{ };
				auto                          little = [&mach](std::size_t aOffset) { return static_cast<std::uint32_t>(mach[aOffset]) | (static_cast<std::uint32_t>(mach[aOffset + 1U]) << 8U) | (static_cast<std::uint32_t>(mach[aOffset + 2U]) << 16U) | (static_cast<std::uint32_t>(mach[aOffset + 3U]) << 24U); };
				if (pread(binary.Value, mach.data(), mach.size(), static_cast<off_t>(offset)) != 32 || little(0) != 0xfeedfacfU || little(4) != word(architecture) || little(12) != 2U) {
					return reason::wrong_architecture;
				}
				const auto commands = little(16), commandBytes = little(20);
				if (commands > 4096U || commandBytes > 1024U * 1024U || commandBytes > size - 32U) {
					return reason::invalid_bundle;
				}
				std::uint64_t cursor     = 32U;
				bool          deployment = false;
				for (std::uint32_t index = 0; index < commands; ++index) {
					if (cursor + 8U > 32U + commandBytes || pread(binary.Value, mach.data(), 8, static_cast<off_t>(offset + cursor)) != 8) {
						return reason::invalid_bundle;
					}
					const auto kind = little(0), length = little(4);
					if (length < 8U || length > 32U + commandBytes - cursor) {
						return reason::invalid_bundle;
					}
					if (kind == 0x24U || kind == 0x32U) {
						const auto required = kind == 0x24U ? 16U : 24U;
						if (deployment || length < required || pread(binary.Value, mach.data(), required, static_cast<off_t>(offset + cursor)) != static_cast<ssize_t>(required)
							|| (kind == 0x32U && little(8) != 1U)) {
							return reason::invalid_bundle;
						}
						const auto packed = little(kind == 0x24U ? 8U : 12U);
						const auto target = QVersionNumber{ static_cast<int>(packed >> 16U), static_cast<int>((packed >> 8U) & 255U), static_cast<int>(packed & 255U) };
						if (packed == 0U || target > QVersionNumber{ os.majorVersion(), os.minorVersion(), os.microVersion() }) {
							return reason::invalid_bundle;
						}
						deployment = true;
					}
					cursor += length;
				}
				if (!deployment || cursor != 32U + commandBytes) {
					return reason::invalid_bundle;
				}
			}
			return reason::none;
		}
		auto manifest(const fs::path& aBundle, std::stop_token aStop, deadline aDeadline) -> std::optional<QByteArray>
		{
			std::vector<fs::path> paths;
			std::error_code       error;
			for (fs::recursive_directory_iterator iterator{ aBundle, error }; !error && iterator != fs::recursive_directory_iterator{ }; iterator.increment(error)) {
				if (paths.size() >= application::release_update_maximum_entries || interrupted(aStop, aDeadline) != reason::none) {
					return { };
				}
				paths.push_back(iterator->path());
			}
			if (error) {
				return { };
			}
			std::ranges::sort(paths);
			QCryptographicHash hash{ QCryptographicHash::Sha256 };
			std::uint64_t      total = 0;
			for (const auto& path : paths) {
				const auto name = path.lexically_relative(aBundle).generic_string();
				hash.addData(QByteArrayView{ name.data(), static_cast<qsizetype>(name.size()) });
				hash.addData(QByteArrayView{ "\0", 1 });
				struct stat info{ };
				if (lstat(path.c_str(), &info) != 0) {
					return { };
				}
				const auto facts = std::to_string(info.st_mode) + ":" + std::to_string(info.st_ino) + ":" + std::to_string(info.st_dev);
				hash.addData(QByteArrayView{ facts.data(), static_cast<qsizetype>(facts.size()) });
				if (S_ISLNK(info.st_mode)) {
					const auto link     = fs::read_symlink(path, error).generic_string();
					const auto resolved = fs::canonical(path, error).lexically_relative(aBundle);
					if (error || resolved.empty() || *resolved.begin() == "..") {
						return { };
					}
					hash.addData(QByteArrayView{ link.data(), static_cast<qsizetype>(link.size()) });
				} else if (S_ISREG(info.st_mode)) {
					if (info.st_nlink != 1 || info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) > application::release_update_maximum_expanded_bytes - total) {
						return { };
					}
					total += static_cast<std::uint64_t>(info.st_size);
					descriptor file{ open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
					const auto digest = hash_file(file.Value, static_cast<std::uint64_t>(info.st_size), aStop, aDeadline);
					if (!digest || !same_inode(path, file.Value)) {
						return { };
					}
					hash.addData(*digest);
				} else if (!S_ISDIR(info.st_mode)) {
					return { };
				}
			}
			return hash.result();
		}
		auto native_validation(const fs::path& aBundle, const fs::path& aInstalled,
							   const core::release_update_candidate& aCandidate, std::stop_token aStop, deadline aDeadline) -> reason
		{
			const auto identity = bundle_identity(aBundle, aCandidate);
			if (identity != reason::none) {
				return identity;
			}
			const auto requirement = trusted_requirement(aInstalled);
			if (!requirement) {
				return reason::wrong_signer;
			}
			const auto installed = tool(QStringLiteral("/usr/bin/codesign"), { QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), QStringLiteral("--all-architectures"), QString::fromStdString(aInstalled.string()) }, aStop, aDeadline);
			if (installed != reason::none) {
				return installed;
			}
			const auto installedTrust = tool(QStringLiteral("/usr/sbin/spctl"), { QStringLiteral("--assess"), QStringLiteral("--type"), QStringLiteral("execute"), QString::fromStdString(aInstalled.string()) }, aStop, aDeadline);
			if (installedTrust != reason::none) {
				return installedTrust;
			}
			const auto signature = tool(QStringLiteral("/usr/bin/codesign"), { QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), QStringLiteral("--all-architectures"), QStringLiteral("--test-requirement"), QStringLiteral("=") + QString::fromStdString(*requirement), QString::fromStdString(aBundle.string()) }, aStop, aDeadline);
			if (signature != reason::none) {
				return signature;
			}
			if (!trusted_requirement(aBundle)) {
				return reason::wrong_signer;
			}
			return tool(QStringLiteral("/usr/sbin/spctl"), { QStringLiteral("--assess"), QStringLiteral("--type"), QStringLiteral("execute"), QString::fromStdString(aBundle.string()) }, aStop, aDeadline);
		}
		class macos_staging final : public release_update_staging
		{
		public:
			macos_staging(fs::path aRoot, fs::path aInstalled, application::effect_executor& aCleanup)
				: Root{ std::move(aRoot) }
				, Installed{ std::move(aInstalled) }
				, Cleanup{ aCleanup }
			{ }
			auto begin(const core::release_update_candidate& aCandidate) -> std::unique_ptr<application::release_update_archive_writer> override
			{
				if (aCandidate.Size == 0U || aCandidate.Size > core::release_update_maximum_archive_bytes || !private_directory(Root)) {
					return { };
				}
				descriptor root{ open(Root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
				if (root.Value < 0 || (mkdirat(root.Value, "updates", 0700) != 0 && errno != EEXIST) || !private_directory(Root / "updates")) {
					return { };
				}
				descriptor updates{ openat(root.Value, "updates", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
				if (updates.Value < 0) {
					return { };
				}
				descriptor  lock{ openat(updates.Value, "staging.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600) };
				struct stat lockInfo{ };
				if (lock.Value < 0 || fstat(lock.Value, &lockInfo) != 0 || !S_ISREG(lockInfo.st_mode) || lockInfo.st_nlink != 1
					|| lockInfo.st_uid != getuid() || (lockInfo.st_mode & 0077U) != 0 || flock(lock.Value, LOCK_EX | LOCK_NB) != 0) {
					return { };
				}
				std::error_code error;
				const auto      space = fs::space(Root, error);
				if (error || space.available < aCandidate.Size + application::release_update_maximum_expanded_bytes * 2U) {
					return { };
				}
				auto pattern = (Root / "updates" / "transaction-XXXXXX").string();
				if (!mkdtemp(pattern.data())) {
					return { };
				}
				descriptor directory{ open(pattern.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
				if (directory.Value < 0) {
					static_cast<void>(rmdir(pattern.c_str()));
					return { };
				}
				descriptor archive{ openat(directory.Value, "download.zip", O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600) };
				auto       owner = std::make_shared<transaction>(pattern, std::exchange(directory.Value, -1), std::exchange(archive.Value, -1), std::exchange(lock.Value, -1), Cleanup);
				if (owner->Directory.Value < 0 || owner->Archive.Value < 0) {
					return { };
				}
				return std::make_unique<archive_writer>(aCandidate, std::move(owner));
			}
			auto verify(std::shared_ptr<const application::release_update_archive> aArchive, std::stop_token aStop)
				-> application::release_update_verification_result override
			{
				const auto archive = std::dynamic_pointer_cast<const owned_archive>(aArchive);
				const auto end     = std::chrono::steady_clock::now() + std::chrono::seconds{ application::release_update_verification_timeout_seconds };
				if (const auto stop = interrupted(aStop, end); stop != reason::none) {
					return { stop };
				}
				if (!archive || !same_inode(archive->Transaction->Path, archive->Transaction->Directory.Value)
					|| !same_inode(archive->Transaction->Path / "download.zip", archive->Transaction->Archive.Value)) {
					return { reason::changed_candidate };
				}
				const auto digest = hash_file(archive->Transaction->Archive.Value, archive->candidate().Size, aStop, end);
				if (!digest || *digest != archive->Digest) {
					const auto stop = interrupted(aStop, end);
					return { stop == reason::none ? reason::changed_candidate : stop };
				}
				if (archive->candidate().Sha256 && *digest != QByteArray{ reinterpret_cast<const char*>(archive->candidate().Sha256->data()), 32 }) {
					return { reason::digest_mismatch };
				}
				const auto extracted = extract_archive(*archive->Transaction, aStop, end);
				if (extracted != reason::none) {
					return { extracted };
				}
				if (hash_file(archive->Transaction->Archive.Value, archive->candidate().Size, aStop, end) != digest) {
					return { reason::changed_candidate };
				}
				const auto path   = archive->Transaction->Path / bundle_name;
				const auto before = manifest(path, aStop, end);
				if (!before) {
					return { reason::unsafe_archive };
				}
				const auto native = native_validation(path, Installed, archive->candidate(), aStop, end);
				if (native != reason::none) {
					return { native };
				}
				const auto after = manifest(path, aStop, end);
				if (!after || *before != *after) {
					return { reason::changed_candidate };
				}
				return { reason::none, std::make_shared<owned_bundle>(archive->candidate(), archive->Transaction, *after) };
			}
			auto revalidate(std::shared_ptr<const application::release_update_verified_bundle> aBundle, std::stop_token aStop) -> reason override
			{
				const auto bundle = std::dynamic_pointer_cast<const owned_bundle>(aBundle);
				const auto end    = std::chrono::steady_clock::now() + std::chrono::seconds{ application::release_update_verification_timeout_seconds };
				if (const auto stop = interrupted(aStop, end); stop != reason::none) {
					return stop;
				}
				if (!bundle || !same_inode(bundle->Transaction->Path, bundle->Transaction->Directory.Value)) {
					return reason::changed_candidate;
				}
				const auto path = bundle->Transaction->Path / bundle_name;
				if (manifest(path, aStop, end) != bundle->Manifest) {
					return reason::changed_candidate;
				}
				const auto native = native_validation(path, Installed, bundle->candidate(), aStop, end);
				if (native != reason::none) {
					return native;
				}
				return manifest(path, aStop, end) == bundle->Manifest ? reason::none : reason::changed_candidate;
			}

		private:
			fs::path                      Root;
			fs::path                      Installed;
			application::effect_executor& Cleanup;
		};
	}
	auto make_release_update_staging(fs::path aPrivateRoot, fs::path aInstalled, application::effect_executor& aCleanup)
		-> std::shared_ptr<release_update_staging>
	{
		return std::make_shared<macos_staging>(std::move(aPrivateRoot), std::move(aInstalled), aCleanup);
	}

	auto detail::install_tree_identity(const fs::path& aPath) -> std::optional<std::string>
	{
		const auto value = manifest(aPath, { }, std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
		return value ? std::optional{ value->toHex().toStdString() } : std::nullopt;
	}
	auto detail::install_source(std::shared_ptr<const application::release_update_verified_bundle> aBundle,
								std::stop_token                                                    aStop) -> std::optional<install_material>
	{
		const auto bundle = std::dynamic_pointer_cast<const owned_bundle>(aBundle);
		if (aStop.stop_requested() || !bundle || !same_inode(bundle->Transaction->Path, bundle->Transaction->Directory.Value)) {
			return { };
		}
		const auto path    = bundle->Transaction->Path / bundle_name;
		const auto current = install_tree_identity(path);
		if (!current || *current != bundle->Manifest.toHex().toStdString()) {
			return { };
		}
		return install_material{ path, *current };
	}
	auto detail::validate_install_tree(const fs::path& aPath, const fs::path& aInstalled,
									   const core::release_update_candidate& aCandidate, std::stop_token aStop) -> reason
	{
		return native_validation(aPath, aInstalled, aCandidate, aStop, std::chrono::steady_clock::now() + std::chrono::minutes{ 5 });
	}
	auto detail::trusted_install_helper(const fs::path& aBundle, const fs::path& aHelper) -> bool
	{
		std::error_code error;
		const auto      expected = aBundle / "Contents/MacOS/bpr_update_helper";
		if (aHelper != expected || fs::canonical(aHelper, error) != expected || error || !trusted_requirement(aBundle)) {
			return false;
		}
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{ 30 };
		return tool(QStringLiteral("/usr/bin/codesign"), { QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), QStringLiteral("--all-architectures"), QString::fromStdString(aBundle.string()) }, { }, end) == reason::none
			&& tool(QStringLiteral("/usr/sbin/spctl"), { QStringLiteral("--assess"), QStringLiteral("--type"), QStringLiteral("execute"), QString::fromStdString(aBundle.string()) }, { }, end) == reason::none;
	}

	auto detail::release_update_volume_is_image(std::string_view aInventory, std::string_view aDevice) -> std::optional<bool>
	{
		if (aInventory.empty() || aInventory.size() > 65536U || aDevice.empty()) {
			return { };
		}
		cf_owner<CFDataRef> data;
		data.Value = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(aInventory.data()), static_cast<CFIndex>(aInventory.size()));
		cf_owner<CFPropertyListRef> plist;
		plist.Value = data.Value ? CFPropertyListCreateWithData(nullptr, data.Value, kCFPropertyListImmutable, nullptr, nullptr) : nullptr;
		if (!plist.Value || CFGetTypeID(plist.Value) != CFDictionaryGetTypeID()) {
			return { };
		}
		const auto images = CFDictionaryGetValue(static_cast<CFDictionaryRef>(plist.Value), CFSTR("images"));
		if (!images || CFGetTypeID(images) != CFArrayGetTypeID()) {
			return { };
		}
		const auto list    = static_cast<CFArrayRef>(images);
		bool       mounted = false;
		for (CFIndex index = 0; index < CFArrayGetCount(list); ++index) {
			const auto image = CFArrayGetValueAtIndex(list, index);
			if (CFGetTypeID(image) != CFDictionaryGetTypeID()) {
				return { };
			}
			const auto entities = CFDictionaryGetValue(static_cast<CFDictionaryRef>(image), CFSTR("system-entities"));
			if (!entities || CFGetTypeID(entities) != CFArrayGetTypeID()) {
				return { };
			}
			const auto entries = static_cast<CFArrayRef>(entities);
			for (CFIndex entry = 0; entry < CFArrayGetCount(entries); ++entry) {
				const auto entity = CFArrayGetValueAtIndex(entries, entry);
				if (CFGetTypeID(entity) != CFDictionaryGetTypeID()) {
					return { };
				}
				const auto device = string_value(static_cast<CFDictionaryRef>(entity), CFSTR("dev-entry"));
				if (device.empty()) {
					return { };
				}
				mounted = mounted || device == aDevice;
			}
		}
		return mounted;
	}

	auto release_update_installation_capability(const fs::path& aInstalled, std::stop_token aStop) -> core::release_update_capability
	{
		using capability = core::release_update_capability;
		cf_owner<CFPropertyListRef> managed;
		managed.Value = CFPreferencesCopyAppValue(CFSTR("BPRManagedUpdates"), CFSTR("com.gamecoretech.betterpullreview"));
		if (managed.Value && CFGetTypeID(managed.Value) == CFBooleanGetTypeID() && CFBooleanGetValue(static_cast<CFBooleanRef>(managed.Value))) {
			return capability::managed;
		}
		if (aInstalled.string().find("/AppTranslocation/") != std::string::npos) {
			return capability::translocated;
		}
		struct statfs volume{ };
		if (statfs(aInstalled.c_str(), &volume) != 0) {
			return capability::untrusted_installation;
		}
		if ((volume.f_flags & MNT_RDONLY) != 0) {
			return capability::read_only;
		}
		if (!trusted_requirement(aInstalled)) {
			return capability::untrusted_installation;
		}
		if (access(aInstalled.parent_path().c_str(), W_OK) != 0) {
			return capability::read_only;
		}
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{ 30 };
		QByteArray inventory;
		if (tool(QStringLiteral("/usr/bin/hdiutil"), { QStringLiteral("info"), QStringLiteral("-plist") }, aStop, end, &inventory) != reason::none) {
			return capability::untrusted_installation;
		}
		const auto mounted = detail::release_update_volume_is_image({ inventory.constData(), static_cast<std::size_t>(inventory.size()) }, volume.f_mntfromname);
		if (!mounted) {
			return capability::untrusted_installation;
		}
		if (*mounted) {
			return capability::mounted_image;
		}
		const auto signature = tool(QStringLiteral("/usr/bin/codesign"), { QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), QStringLiteral("--all-architectures"), QString::fromStdString(aInstalled.string()) }, aStop, end);
		if (signature != reason::none) {
			return capability::untrusted_installation;
		}
		const auto trust = tool(QStringLiteral("/usr/sbin/spctl"), { QStringLiteral("--assess"), QStringLiteral("--type"), QStringLiteral("execute"), QString::fromStdString(aInstalled.string()) }, aStop, end);
		return trust == reason::none ? capability::eligible : capability::untrusted_installation;
	}
}
