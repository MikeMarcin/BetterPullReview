#include "bpr/adapters/workspace/app_bundle_read.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#if defined(__APPLE__)
	#include <CoreFoundation/CoreFoundation.h>
#endif

namespace bpr::adapters::workspace {
	namespace {
		namespace fs = std::filesystem;

		[[nodiscard]] auto is_folder(const fs::path& aPath) -> bool
		{
			auto error = std::error_code{ };
			return fs::is_directory(fs::symlink_status(aPath, error));
		}

		[[nodiscard]] auto list_programs(const fs::path& aFolder) -> std::vector<core::app_bundle_file>
		{
			auto programs = std::vector<core::app_bundle_file>{ };
			auto error    = std::error_code{ };
			for (auto entry = fs::directory_iterator{ aFolder, error }; !error && entry != fs::directory_iterator{ } && programs.size() < core::maximum_app_bundle_programs; entry.increment(error)) {
				auto       statusError = std::error_code{ };
				const auto status      = entry->symlink_status(statusError);
				if (statusError) {
					continue;
				}
				constexpr auto anyExecute = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
				programs.push_back({ .Name = entry->path().filename().string(), .Directory = fs::is_directory(status), .Executable = fs::is_regular_file(status) && (status.permissions() & anyExecute) != fs::perms::none });
			}
			return programs;
		}

#if defined(__APPLE__)
		[[nodiscard]] auto read_info(const fs::path& aInfo) -> std::optional<std::vector<char>>
		{
			auto       error  = std::error_code{ };
			const auto status = fs::symlink_status(aInfo, error);
			if (error || !fs::is_regular_file(status)) {
				return std::nullopt;
			}
			const auto size = fs::file_size(aInfo, error);
			if (error || size > maximum_app_bundle_info_bytes) {
				return std::nullopt;
			}
			auto file = std::ifstream{ aInfo, std::ios::binary };
			auto data = std::vector<char>(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ });
			if (!file.good() && !file.eof()) {
				return std::nullopt;
			}
			return data;
		}

		[[nodiscard]] auto named_program(const fs::path& aInfo) -> std::optional<std::string>
		{
			const auto data = read_info(aInfo);
			if (!data) {
				return std::nullopt;
			}
			const auto bytes = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data->data()), static_cast<CFIndex>(data->size()));
			if (bytes == nullptr) {
				return std::nullopt;
			}
			const auto list = CFPropertyListCreateWithData(kCFAllocatorDefault, bytes, kCFPropertyListImmutable, nullptr, nullptr);
			CFRelease(bytes);
			if (list == nullptr) {
				return std::nullopt;
			}
			auto result = std::optional<std::string>{ };
			if (CFGetTypeID(list) == CFDictionaryGetTypeID()) {
				const auto value = CFDictionaryGetValue(static_cast<CFDictionaryRef>(list), CFSTR("CFBundleExecutable"));
				if (value != nullptr && CFGetTypeID(value) == CFStringGetTypeID()) {
					const auto text   = static_cast<CFStringRef>(value);
					const auto length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(text), kCFStringEncodingUTF8) + 1;
					auto       buffer = std::vector<char>(static_cast<std::size_t>(length));
					if (CFStringGetCString(text, buffer.data(), length, kCFStringEncodingUTF8)) {
						result = std::string{ buffer.data() };
					}
				}
			}
			CFRelease(list);
			return result;
		}
#else
		[[nodiscard]] auto named_program(const fs::path&) -> std::optional<std::string> { return std::nullopt; }
#endif
	} // namespace

	auto read_app_bundle(const std::string& aBundle) -> std::optional<core::app_bundle_listing>
	{
		const auto bundle = fs::path{ aBundle };
		if (aBundle.empty() || !is_folder(bundle)) {
			return std::nullopt;
		}
		auto       listing  = core::app_bundle_listing{ .Bundle = aBundle, .NamedProgram = named_program(bundle / "Contents" / "Info.plist"), .Programs = std::nullopt };
		const auto programs = bundle / "Contents" / "MacOS";
		if (is_folder(bundle / "Contents") && is_folder(programs)) {
			listing.Programs = list_programs(programs);
		}
		return listing;
	}
} // namespace bpr::adapters::workspace
