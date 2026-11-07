#include "github_application/theme_document_file.hpp"

#include "bpr/core/theme_exchange.hpp"

#include <QFileInfo>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cerrno>
#include <expected>
#include <filesystem>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::app {
	namespace {
		[[nodiscard]] auto unavailable_theme_document()
			-> application::theme_document_result<std::string>
		{
			return std::unexpected{ application::theme_document_error{ } };
		}

		[[nodiscard]] auto cancelled_theme_document()
			-> application::theme_document_result<std::string>
		{
			return std::unexpected{ application::theme_document_error{
				.Failure = application::theme_document_failure::cancelled,
			} };
		}

#if defined(_WIN32)
		class theme_file_handle final
		{
		public:
			explicit theme_file_handle(void* aHandle) noexcept
				: Handle{ aHandle }
			{
			}
			theme_file_handle(const theme_file_handle&)                    = delete;
			auto operator=(const theme_file_handle&) -> theme_file_handle& = delete;
			theme_file_handle(theme_file_handle&&)                         = delete;
			auto operator=(theme_file_handle&&) -> theme_file_handle&      = delete;
			~theme_file_handle()
			{
				if (valid()) {
					(void)::CloseHandle(Handle);
				}
			}

			[[nodiscard]] auto get() const noexcept -> void*
			{
				return Handle;
			}

			[[nodiscard]] auto valid() const noexcept -> bool
			{
				return Handle != nullptr && Handle != INVALID_HANDLE_VALUE;
			}

		private:
			void* Handle = INVALID_HANDLE_VALUE;
		};
#else
		class theme_file_descriptor final
		{
		public:
			explicit theme_file_descriptor(const int aDescriptor) noexcept
				: Descriptor{ aDescriptor }
			{
			}
			theme_file_descriptor(const theme_file_descriptor&) = delete;
			auto operator=(const theme_file_descriptor&)
				-> theme_file_descriptor&                  = delete;
			theme_file_descriptor(theme_file_descriptor&&) = delete;
			auto operator=(theme_file_descriptor&&)
				-> theme_file_descriptor& = delete;
			~theme_file_descriptor()
			{
				if (Descriptor >= 0) {
					(void)::close(Descriptor);
				}
			}

			[[nodiscard]] auto get() const noexcept -> int
			{
				return Descriptor;
			}

		private:
			int Descriptor = -1;
		};
#endif

		[[nodiscard]] auto read_bounded_regular_theme_document(
			const std::string_view aPath,
			const std::size_t      aMaximumBytes,
			const std::stop_token  aStop)
			-> application::theme_document_result<std::string>
		{
			if (aStop.stop_requested()) {
				return cancelled_theme_document();
			}
			if (aPath.empty() || aPath.find('\0') != std::string_view::npos) {
				return unavailable_theme_document();
			}

#if defined(_WIN32)
			// `std::filesystem::u8path` is deprecated in C++20. A `char8_t` view
			// over the same bytes selects the identical UTF-8 interpretation.
			const auto utf8 = std::u8string_view{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() };
			const auto path = std::filesystem::path{ utf8 };
			auto       file = theme_file_handle{ ::CreateFileW(
				path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				nullptr) };
			if (!file.valid() || ::GetFileType(file.get()) != FILE_TYPE_DISK) {
				return unavailable_theme_document();
			}
			auto information = BY_HANDLE_FILE_INFORMATION{ };
			if (::GetFileInformationByHandle(file.get(), &information) == 0
				|| (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
				return unavailable_theme_document();
			}
#else
			const auto path = std::filesystem::path{ aPath };
			auto       file = theme_file_descriptor{
				::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK)
			};
			if (file.get() < 0) {
				return unavailable_theme_document();
			}
			struct stat information{ };
			if (::fstat(file.get(), &information) != 0
				|| !S_ISREG(information.st_mode)) {
				return unavailable_theme_document();
			}
#endif

			auto document = std::string(aMaximumBytes, '\0');
			auto offset   = std::size_t{ 0U };
			while (offset < document.size()) {
				if (aStop.stop_requested()) {
					return cancelled_theme_document();
				}
				const auto remaining = document.size() - offset;
				const auto chunk
					= std::min(remaining, std::size_t{ 64U * 1'024U });
#if defined(_WIN32)
				auto readBytes = DWORD{ 0U };
				if (::ReadFile(
						file.get(),
						document.data() + offset,
						static_cast<DWORD>(chunk),
						&readBytes,
						nullptr)
					== 0) {
					return unavailable_theme_document();
				}
				if (readBytes == 0U) {
					break;
				}
				offset += static_cast<std::size_t>(readBytes);
#else
				const auto readBytes
					= ::read(file.get(), document.data() + offset, chunk);
				if (readBytes == 0) {
					break;
				}
				if (readBytes < 0) {
					if (errno == EINTR) {
						continue;
					}
					return unavailable_theme_document();
				}
				offset += static_cast<std::size_t>(readBytes);
#endif
			}
			document.resize(offset);
			if (aStop.stop_requested()) {
				return cancelled_theme_document();
			}
			return document;
		}
	}

	auto bounded_theme_document_reader::read(
		const std::string_view aPath,
		const std::size_t      aMaximumBytes,
		const std::stop_token  aStop)
		-> application::theme_document_result<std::string>
	{
		return read_bounded_regular_theme_document(
			aPath, aMaximumBytes, aStop);
	}

	auto bounded_theme_document_reader::write(const std::string_view aPath,
											  const std::string_view aDocument, const std::stop_token aStop)
		-> application::theme_document_result<void>
	{
		const auto refused = [] { return std::unexpected{ application::theme_document_error{ } }; };
		if (aPath.empty() || aPath.find('\0') != std::string_view::npos
			|| aDocument.size() > core::theme_document_byte_limit || aStop.stop_requested()) {
			return refused();
		}
		const auto path = QString::fromUtf8(aPath.data(), static_cast<qsizetype>(aPath.size()));
		const auto info = QFileInfo{ path };
		if (info.isSymLink() || (info.exists() && !info.isFile())) {
			return refused();
		}
		auto file = QSaveFile{ path };
		file.setDirectWriteFallback(false);
		if (!file.open(QIODevice::WriteOnly)) {
			return refused();
		}
		const auto size = static_cast<qint64>(aDocument.size());
		if (file.write(aDocument.data(), static_cast<qint64>(aDocument.size())) != size || aStop.stop_requested()) {
			file.cancelWriting();
			return refused();
		}
		if (!file.commit()) {
			return refused();
		}
		return { };
	}
}
