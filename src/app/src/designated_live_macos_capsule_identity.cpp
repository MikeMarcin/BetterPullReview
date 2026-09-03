#include "designated_live_macos_capsule_identity.hpp"

#if defined(__APPLE__)

	#include <CommonCrypto/CommonDigest.h>
	#include <dirent.h>
	#include <fcntl.h>
	#include <unistd.h>

	#include <algorithm>
	#include <array>
	#include <cerrno>
	#include <span>
	#include <string_view>
	#include <utility>

namespace bpr::app::designated_live_capsule_identity {
	namespace {
		/// A capsule is small and fixed. These bounds exist so a hostile tree
		/// cannot turn a verification into an exhaustion.
		constexpr auto maximum_entries       = std::size_t{ 8U * 1024U };
		constexpr auto maximum_file_bytes    = std::size_t{ 256U * 1024U * 1024U };
		constexpr auto maximum_symlink_bytes = std::size_t{ 4U * 1024U };
		constexpr auto read_chunk_bytes      = std::size_t{ 64U * 1024U };

		class descriptor
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

			~descriptor() { reset(); }

			[[nodiscard]] auto get() const noexcept -> int { return Value; }

			explicit operator bool() const noexcept { return Value >= 0; }

			[[nodiscard]] auto release() noexcept -> int
			{
				return std::exchange(Value, -1);
			}

		private:
			void reset() noexcept
			{
				if (Value >= 0) {
					static_cast<void>(::close(Value));
					Value = -1;
				}
			}

			int Value = -1;
		};

		[[nodiscard]] auto hex(const std::span<const unsigned char> aBytes)
			-> std::string
		{
			static constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto                  result = std::string{ };
			result.reserve(aBytes.size() * 2U);
			for (const auto byte : aBytes) {
				result.push_back(digits[(static_cast<unsigned>(byte) >> 4U) & 0x0fU]);
				result.push_back(digits[byte & 0x0fU]);
			}
			return result;
		}

		/// Hashes through the descriptor that was opened, never by reopening
		/// the name: reopening is a second resolution and can land elsewhere.
		[[nodiscard]] auto file_digest(const int aDescriptor)
			-> std::optional<std::string>
		{
			CC_SHA256_CTX context{ };
			CC_SHA256_Init(&context);
			auto buffer   = std::array<unsigned char, read_chunk_bytes>{ };
			auto consumed = std::size_t{ 0U };
			for (;;) {
				const auto read = ::read(aDescriptor, buffer.data(), buffer.size());
				if (read < 0) {
					if (errno == EINTR) {
						continue;
					}
					return std::nullopt;
				}
				if (read == 0) {
					break;
				}
				consumed += static_cast<std::size_t>(read);
				if (consumed > maximum_file_bytes) {
					return std::nullopt;
				}
				CC_SHA256_Update(
					&context, buffer.data(), static_cast<CC_LONG>(read));
			}
			auto digest = std::array<unsigned char, CC_SHA256_DIGEST_LENGTH>{ };
			CC_SHA256_Final(digest.data(), &context);
			return hex(digest);
		}

		[[nodiscard]] auto walk(
			const int           aDirectory,
			const std::string&  aPrefix,
			std::vector<entry>& aEntries) -> bool
		{
			// fdopendir takes ownership, so hand it a duplicate and keep the
			// caller's descriptor usable for the entries below.
			auto duplicated = descriptor{ ::openat(aDirectory, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
			if (!duplicated) {
				return false;
			}
			auto* const stream = ::fdopendir(duplicated.get());
			if (stream == nullptr) {
				return false;
			}
			static_cast<void>(duplicated.release());
			auto names = std::vector<std::string>{ };
			for (;;) {
				errno              = 0;
				const auto* record = ::readdir(stream);
				if (record == nullptr) {
					const auto failed = errno != 0;
					static_cast<void>(::closedir(stream));
					if (failed) {
						return false;
					}
					break;
				}
				const auto name = std::string_view{ record->d_name };
				if (name == "." || name == "..") {
					continue;
				}
				names.emplace_back(name);
			}
			// Sorted, so two captures of the same tree compare equal without
			// depending on directory order, which is not a promise anyone made.
			std::sort(names.begin(), names.end());
			for (const auto& name : names) {
				if (aEntries.size() >= maximum_entries) {
					return false;
				}
				struct stat status{ };
				if (::fstatat(aDirectory, name.c_str(), &status, AT_SYMLINK_NOFOLLOW)
					!= 0) {
					return false;
				}
				auto relative = name;
				if (!aPrefix.empty()) {
					relative = aPrefix;
					relative.push_back('/');
					relative.append(name);
				}
				auto record = entry{
					.Relative            = std::move(relative),
					.Device              = static_cast<std::uint64_t>(status.st_dev),
					.Inode               = static_cast<std::uint64_t>(status.st_ino),
					.Size                = static_cast<std::uint64_t>(status.st_size),
					.ModifiedSeconds     = status.st_mtimespec.tv_sec,
					.ModifiedNanoseconds = status.st_mtimespec.tv_nsec,
					.Mode                = static_cast<std::uint32_t>(status.st_mode),
				};
				if (S_ISLNK(status.st_mode)) {
					auto       target = std::string(maximum_symlink_bytes, '\0');
					const auto length = ::readlinkat(
						aDirectory, name.c_str(), target.data(), target.size());
					if (length < 0
						|| static_cast<std::size_t>(length) >= target.size()) {
						return false;
					}
					target.resize(static_cast<std::size_t>(length));
					if (target.starts_with('/') || target.contains("..")) {
						// An escaping symlink is refused rather than followed.
						return false;
					}
					record.SymlinkTarget = std::move(target);
					aEntries.push_back(std::move(record));
					continue;
				}
				if (S_ISDIR(status.st_mode)) {
					auto child = descriptor{ ::openat(
						aDirectory,
						name.c_str(),
						O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW) };
					if (!child) {
						return false;
					}
					const auto childPrefix = record.Relative;
					aEntries.push_back(std::move(record));
					if (!walk(child.get(), childPrefix, aEntries)) {
						return false;
					}
					continue;
				}
				if (!S_ISREG(status.st_mode)) {
					// A capsule holds files, directories and symlinks. Anything
					// else is refused rather than described.
					return false;
				}
				auto file = descriptor{ ::openat(
					aDirectory, name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
				if (!file) {
					return false;
				}
				auto digest = file_digest(file.get());
				if (!digest.has_value()) {
					return false;
				}
				record.Sha256 = std::move(*digest);
				aEntries.push_back(std::move(record));
			}
			return true;
		}
	} // namespace

	auto capture(const int aRootDescriptor) -> std::optional<std::vector<entry>>
	{
		if (aRootDescriptor < 0) {
			return std::nullopt;
		}
		auto entries = std::vector<entry>{ };
		if (!walk(aRootDescriptor, std::string{ }, entries)) {
			return std::nullopt;
		}
		return entries;
	}

	auto unchanged(
		const int aRootDescriptor, const std::vector<entry>& aCaptured) -> bool
	{
		const auto observed = capture(aRootDescriptor);
		return observed.has_value() && *observed == aCaptured;
	}

	auto open_root(const std::filesystem::path& aRoot) -> std::optional<int>
	{
		const auto opened = ::open(
			aRoot.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		if (opened < 0) {
			return std::nullopt;
		}
		return opened;
	}
}

#endif // defined(__APPLE__)
