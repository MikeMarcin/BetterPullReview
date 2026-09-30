#include "bpr/adapters/workspace/local_export_destination.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <system_error>
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
	#include <cstdio>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::workspace {
	namespace {
		using application::cycle_export_failure;
		using application::cycle_object_ref;

		constexpr std::size_t chunk_bytes = 64U * 1024U;
		/// Distinguishes this process's temporary files from one another.
		std::atomic<std::uint64_t> temporary_counter{ 0 };

		auto process_number() -> std::uint64_t
		{
#if defined(_WIN32)
			return ::GetCurrentProcessId();
#else
			return static_cast<std::uint64_t>(::getpid());
#endif
		}

#if defined(_WIN32)
		class handle
		{
		public:
			explicit handle(HANDLE aValue) noexcept
				: Value{ aValue }
			{
			}
			handle(const handle&)                    = delete;
			auto operator=(const handle&) -> handle& = delete;
			handle(handle&&)                         = delete;
			auto operator=(handle&&) -> handle&      = delete;
			~handle()
			{
				if (valid()) {
					::CloseHandle(Value);
				}
			}
			[[nodiscard]] auto valid() const noexcept -> bool
			{
				return Value != INVALID_HANDLE_VALUE && Value != nullptr;
			}
			[[nodiscard]] auto get() const noexcept -> HANDLE
			{
				return Value;
			}

		private:
			HANDLE Value;
		};
#else
		class descriptor
		{
		public:
			explicit descriptor(const int aValue) noexcept
				: Value{ aValue }
			{
			}
			descriptor(const descriptor&)                    = delete;
			auto operator=(const descriptor&) -> descriptor& = delete;
			descriptor(descriptor&&)                         = delete;
			auto operator=(descriptor&&) -> descriptor&      = delete;
			~descriptor()
			{
				if (Value >= 0) {
					::close(Value);
				}
			}
			[[nodiscard]] auto get() const noexcept -> int
			{
				return Value;
			}
			/// Closes now and says whether the close succeeded.
			auto close() noexcept -> bool
			{
				const auto result = ::close(std::exchange(Value, -1));
				return result == 0;
			}

		private:
			int Value;
		};

		/// Makes a completed move durable; a folder that cannot be synced is
		/// not a failed save.
		void sync_folder(const std::filesystem::path& aFolder)
		{
			const auto folder = descriptor{ ::open(aFolder.c_str(), O_RDONLY | O_CLOEXEC) };
			if (folder.get() >= 0) {
				::fsync(folder.get());
			}
		}
#endif

		/// Streams one regular file without following a link. Nothing when no
		/// file is there; a link, directory or device is `not_regular`.
		template <typename TSink>
		auto stream_regular(const std::filesystem::path& aFile, const std::uint64_t aMaximumBytes, std::stop_token aStop, TSink&& aSink)
			-> std::expected<std::optional<std::uint64_t>, cycle_export_failure>
		{
			auto buffer = std::array<std::uint8_t, chunk_bytes>{ };
			auto total  = std::uint64_t{ 0 };
#if defined(_WIN32)
			const auto attributes = ::GetFileAttributesW(aFile.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) {
				if (::GetLastError() == ERROR_FILE_NOT_FOUND) {
					return std::optional<std::uint64_t>{ };
				}
				return std::unexpected(cycle_export_failure::unavailable);
			}
			if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) != 0U) {
				return std::unexpected(cycle_export_failure::not_regular);
			}
			const auto file = handle{ ::CreateFileW(aFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr) };
			auto       size = LARGE_INTEGER{ };
			if (!file.valid() || ::GetFileType(file.get()) != FILE_TYPE_DISK || !::GetFileSizeEx(file.get(), &size)) {
				return std::unexpected(cycle_export_failure::unavailable);
			}
			if (static_cast<std::uint64_t>(size.QuadPart) > aMaximumBytes) {
				return std::unexpected(cycle_export_failure::too_large);
			}
			while (true) {
				if (aStop.stop_requested()) {
					return std::unexpected(cycle_export_failure::cancelled);
				}
				auto read = DWORD{ 0 };
				if (!::ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
					return std::unexpected(cycle_export_failure::unavailable);
				}
				if (read == 0U) {
					break;
				}
				total += read;
				if (total > aMaximumBytes) {
					return std::unexpected(cycle_export_failure::too_large);
				}
				aSink(std::span<const std::uint8_t>{ buffer.data(), read });
			}
#else
			struct stat status{ };
			if (::lstat(aFile.c_str(), &status) != 0) {
				if (errno == ENOENT) {
					return std::optional<std::uint64_t>{ };
				}
				return std::unexpected(cycle_export_failure::unavailable);
			}
			if (!S_ISREG(status.st_mode)) {
				return std::unexpected(cycle_export_failure::not_regular);
			}
			if (static_cast<std::uint64_t>(status.st_size) > aMaximumBytes) {
				return std::unexpected(cycle_export_failure::too_large);
			}
			const auto file = descriptor{ ::open(aFile.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
			if (file.get() < 0) {
				return std::unexpected(errno == ELOOP ? cycle_export_failure::not_regular : cycle_export_failure::unavailable);
			}
			while (true) {
				if (aStop.stop_requested()) {
					return std::unexpected(cycle_export_failure::cancelled);
				}
				const auto read = ::read(file.get(), buffer.data(), buffer.size());
				if (read < 0 && errno == EINTR) {
					continue;
				}
				if (read < 0) {
					return std::unexpected(cycle_export_failure::unavailable);
				}
				if (read == 0) {
					break;
				}
				total += static_cast<std::uint64_t>(read);
				if (total > aMaximumBytes) {
					return std::unexpected(cycle_export_failure::too_large);
				}
				aSink(std::span<const std::uint8_t>{ buffer.data(), static_cast<std::size_t>(read) });
			}
#endif
			return std::optional{ total };
		}

		class local_export_destination final : public application::cycle_export_destination
		{
		public:
			explicit local_export_destination(std::filesystem::path aFile)
				: File{ std::move(aFile) }
			{
			}

			auto observe(const std::uint64_t aMaximumBytes, std::stop_token aStop) -> std::expected<std::optional<cycle_object_ref>, cycle_export_failure> override
			{
				if (!usable()) {
					return std::unexpected(cycle_export_failure::unavailable);
				}
				auto stream = core::sha256_stream{ };
				auto total  = stream_regular(File, aMaximumBytes, aStop, [&](const std::span<const std::uint8_t> aChunk) { stream.update(aChunk); });
				if (!total) {
					return std::unexpected(total.error());
				}
				if (!*total) {
					return std::optional<cycle_object_ref>{ };
				}
				return std::optional{ cycle_object_ref{ .Digest = core::cycle_material_digest{ stream.finish() }, .Size = **total } };
			}

			auto create(const std::span<const std::uint8_t> aBytes, std::stop_token aStop) -> std::expected<void, cycle_export_failure> override
			{
				return commit(aBytes, false, aStop);
			}

			auto replace(const std::span<const std::uint8_t> aBytes, const cycle_object_ref& aExpected, std::stop_token aStop) -> std::expected<void, cycle_export_failure> override
			{
				const auto current = observe(aExpected.Size, aStop);
				if (!current) {
					return std::unexpected(current.error() == cycle_export_failure::too_large ? cycle_export_failure::changed : current.error());
				}
				if (!*current || **current != aExpected) {
					return std::unexpected(cycle_export_failure::changed);
				}
				return commit(aBytes, true, aStop);
			}

		private:
			[[nodiscard]] auto usable() const -> bool
			{
				return File.is_absolute() && File.has_filename() && File.filename() != "." && File.filename() != "..";
			}

			/// Writes a flushed temporary file beside the destination and
			/// moves it into place; the temporary file never survives a failure.
			auto commit(const std::span<const std::uint8_t> aBytes, const bool aReplace, std::stop_token aStop) -> std::expected<void, cycle_export_failure>
			{
				if (!usable()) {
					return std::unexpected(cycle_export_failure::unavailable);
				}
				auto temporary = write_temporary(aBytes);
				if (!temporary) {
					return std::unexpected(temporary.error());
				}
				if (aStop.stop_requested()) {
					remove(*temporary);
					return std::unexpected(cycle_export_failure::cancelled);
				}
				auto moved = move_into_place(*temporary, aReplace);
				if (!moved) {
					remove(*temporary);
				}
				return moved;
			}

			static void remove(const std::filesystem::path& aPath)
			{
				auto ignored = std::error_code{ };
				std::filesystem::remove(aPath, ignored);
			}

			auto temporary_path() const -> std::filesystem::path
			{
				auto name = std::filesystem::path{ "." };
				name += File.filename();
				name += "." + std::to_string(process_number()) + "-" + std::to_string(temporary_counter.fetch_add(1U)) + ".bpr-tmp";
				return File.parent_path() / name;
			}

			auto write_temporary(const std::span<const std::uint8_t> aBytes) const -> std::expected<std::filesystem::path, cycle_export_failure>
			{
				auto path = temporary_path();
#if defined(_WIN32)
				auto ok = false;
				{
					const auto file = handle{ ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr) };
					if (!file.valid()) {
						return std::unexpected(cycle_export_failure::unavailable);
					}
					auto written = std::size_t{ 0 };
					ok           = true;
					while (ok && written < aBytes.size()) {
						const auto chunk = static_cast<DWORD>(std::min<std::size_t>(aBytes.size() - written, chunk_bytes));
						auto       done  = DWORD{ 0 };
						ok               = ::WriteFile(file.get(), aBytes.data() + written, chunk, &done, nullptr) != 0 && done == chunk;
						written += done;
					}
					ok = ok && ::FlushFileBuffers(file.get()) != 0;
				}
				if (!ok) {
					remove(path);
					return std::unexpected(cycle_export_failure::unavailable);
				}
#else
				auto file = descriptor{ ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666) };
				if (file.get() < 0) {
					return std::unexpected(cycle_export_failure::unavailable);
				}
				auto written = std::size_t{ 0 };
				auto ok      = true;
				while (ok && written < aBytes.size()) {
					const auto done = ::write(file.get(), aBytes.data() + written, std::min<std::size_t>(aBytes.size() - written, chunk_bytes));
					if (done < 0 && errno == EINTR) {
						continue;
					}
					ok = done > 0;
					written += ok ? static_cast<std::size_t>(done) : 0U;
				}
				ok = ok && ::fsync(file.get()) == 0;
				ok = file.close() && ok;
				if (!ok) {
					remove(path);
					return std::unexpected(cycle_export_failure::unavailable);
				}
#endif
				return path;
			}

			auto move_into_place(const std::filesystem::path& aTemporary, const bool aReplace) const -> std::expected<void, cycle_export_failure>
			{
#if defined(_WIN32)
				const auto flags = MOVEFILE_WRITE_THROUGH | (aReplace ? MOVEFILE_REPLACE_EXISTING : 0U);
				if (!::MoveFileExW(aTemporary.c_str(), File.c_str(), flags)) {
					const auto error = ::GetLastError();
					return std::unexpected(error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS ? cycle_export_failure::exists : cycle_export_failure::unavailable);
				}
				return { };
#else
				if (aReplace) {
					if (::rename(aTemporary.c_str(), File.c_str()) != 0) {
						return std::unexpected(cycle_export_failure::unavailable);
					}
					sync_folder(File.parent_path());
					return { };
				}
				auto moved = move_without_replacing(aTemporary);
				if (moved) {
					sync_folder(File.parent_path());
				}
				return moved;
#endif
			}

#if !defined(_WIN32)
			auto move_without_replacing(const std::filesystem::path& aTemporary) const -> std::expected<void, cycle_export_failure>
			{
	#if defined(__APPLE__)
				if (::renamex_np(aTemporary.c_str(), File.c_str(), RENAME_EXCL) == 0) {
					return { };
				}
				if (errno == EEXIST) {
					return std::unexpected(cycle_export_failure::exists);
				}
	#elif defined(RENAME_NOREPLACE)
				if (::renameat2(AT_FDCWD, aTemporary.c_str(), AT_FDCWD, File.c_str(), RENAME_NOREPLACE) == 0) {
					return { };
				}
				if (errno == EEXIST) {
					return std::unexpected(cycle_export_failure::exists);
				}
	#endif
				// A filesystem without an exclusive rename: a hard link fails on
				// an existing name, and the temporary name is then removed.
				if (::link(aTemporary.c_str(), File.c_str()) != 0) {
					return std::unexpected(errno == EEXIST ? cycle_export_failure::exists : cycle_export_failure::unavailable);
				}
				remove(aTemporary);
				return { };
			}
#endif

			std::filesystem::path File;
		};
	}

	auto read_chosen_file(const std::filesystem::path& aFile, const std::uint64_t aMaximumBytes, std::stop_token aStop) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure>
	{
		if (!aFile.is_absolute() || !aFile.has_filename()) {
			return std::unexpected(cycle_export_failure::unavailable);
		}
		auto bytes = std::vector<std::uint8_t>{ };
		auto total = stream_regular(aFile, aMaximumBytes, std::move(aStop), [&](const std::span<const std::uint8_t> aChunk) { bytes.insert(bytes.end(), aChunk.begin(), aChunk.end()); });
		if (!total) {
			return std::unexpected(total.error());
		}
		if (!*total) {
			return std::unexpected(cycle_export_failure::unavailable);
		}
		return bytes;
	}

	auto make_local_export_destination(std::filesystem::path aFile) -> std::unique_ptr<application::cycle_export_destination>
	{
		return std::make_unique<local_export_destination>(std::move(aFile));
	}
}
