// RFC-BPR-0084 AC06. The profile's on-disk image cache: restart, isolation,
// expiry without use, the 128 MiB bound, owner-private storage and no key text.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/markdown_image_disk_cache.hpp"
#include "bpr/core/markdown_image.hpp"
#include "scratch_root.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#ifndef _WIN32
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using tests::expect;

	constexpr std::int64_t day = 24 * 60 * 60;

	auto key(const std::string_view aHost, const std::uint64_t aAccount, const std::string_view aSource) -> std::string
	{
		const auto source = core::admit_markdown_image_source(aSource, aHost);
		expect(source.has_value(), "fixture source admitted");
		return core::markdown_image_cache_key(aHost, aAccount, *source);
	}

	auto file_text(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "0084-images" };
	const auto                           path   = scratch.path() / "state/v1/markdown-images.sqlite3";
	std::int64_t                         now    = 1'800'000'000;
	const auto                           clock  = [&now] { return now; };
	const auto                           source = std::string{ "https://example.com/canary-source-address.png" };
	const auto                           first  = key("github.com", 7U, source);
	const auto                           bytes  = std::string{ "\x89PNG\r\n\x1a\nfixture image bytes" };

	auto cache = adapters::sqlite::make_markdown_image_disk_cache(scratch.path(), clock);
	expect(!std::filesystem::exists(path), "construction writes nothing");
	expect(!cache->load(first, { }), "an empty cache misses");
	expect(cache->store(first, bytes, { }), "an image is stored");
	cache.reset();
	cache = adapters::sqlite::make_markdown_image_disk_cache(scratch.path(), clock);
	expect(cache->load(first, { }) == bytes, "a restarted profile reads the same bytes back");

	expect(!cache->load(key("github.com", 8U, source), { }), "another account misses");
	expect(!cache->load(key("ghe.example.com", 7U, source), { }), "another host misses");
	expect(!cache->load(key("github.com", 0U, source), { }), "public content misses a signed-in entry");

	std::stop_source stopped;
	stopped.request_stop();
	expect(!cache->load(first, stopped.get_token()), "a stopped read returns nothing");
	expect(!cache->store(key("github.com", 7U, "https://example.com/b.png"), bytes, stopped.get_token()), "a stopped store writes nothing");
	expect(!cache->store(first, { }, { }), "empty bytes are refused");
	expect(!cache->store(first, std::string(core::markdown_image_encoded_bytes + 1U, 'x'), { }), "bytes over the fetch bound are refused");

	// Seven days without use expire an entry; a use restarts the seven days.
	now += 6 * day;
	expect(cache->load(first, { }) == bytes, "six days later the entry is still there");
	now += 6 * day;
	expect(cache->load(first, { }) == bytes, "a use six days ago keeps it");
	now += 7 * day + 1;
	expect(!cache->load(first, { }), "seven days without use expire it");
	expect(!cache->load(first, { }), "an expired entry is gone");

#ifndef _WIN32
	struct stat status{ };
	expect(::stat(path.c_str(), &status) == 0 && (status.st_mode & 077U) == 0U, "the cache file is owner-private");
#endif
	expect(file_text(path).find("canary-source-address") == std::string::npos, "no key text reaches the file");

	// The real bound: thirteen 10 MiB images do not fit 128 MiB; the least
	// recently used goes first.
	const auto large = [](const int aIndex) { return std::string(core::markdown_image_encoded_bytes, static_cast<char>('a' + aIndex)); };
	const auto named = [](const int aIndex) { return key("github.com", 7U, "https://example.com/large-" + std::to_string(aIndex) + ".png"); };
	for (int index = 0; index < 12; ++index) {
		expect(cache->store(named(index), large(index), { }), "a large image is stored");
	}
	expect(cache->load(named(0), { }) == large(0), "the oldest image is used again");
	expect(cache->store(named(12), large(12), { }), "a thirteenth image is stored");
	expect(cache->load(named(0), { }).has_value(), "the image just used stays");
	expect(!cache->load(named(1), { }), "the least recently used image is evicted");
	for (const auto index : { 2, 11, 12 }) {
		expect(cache->load(named(index), { }) == large(index), "the other images stay");
	}
	cache.reset();
	expect(std::filesystem::file_size(path) <= core::markdown_image_disk_cache_bytes + 8U * 1024U * 1024U, "the file follows the bound");

	std::cout << "markdown-image-disk-cache:ok\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return EXIT_FAILURE;
}
