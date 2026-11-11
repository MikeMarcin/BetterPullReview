/// \file
/// \brief [[BPR-2007]], RFC-BPR-0104 AC03. A scratch folder is made private
/// under its root, measured without following links, reports what a turn
/// wrote, counts as full at 256 MiB, and goes without taking a link's target.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_scratch_folder.hpp"
#include "bpr/core/agent_grant.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void write(const std::filesystem::path& aPath, const std::string_view aText)
	{
		auto out = std::ofstream{ aPath, std::ios::binary };
		out << aText;
	}
} // namespace

auto main() -> int
{
	const auto base = std::filesystem::temp_directory_path() / ("bpr-scratch-test-" + std::to_string(std::rand()));
	std::filesystem::remove_all(base);
	const auto root    = base / "agent-scratch-v1";
	const auto outside = base / "outside";
	std::filesystem::create_directories(outside);
	write(outside / "keep.txt", "keep");

	expect(!application::prepare_agent_scratch_folder(root, "../x") && !application::prepare_agent_scratch_folder(root, "-x") && !application::prepare_agent_scratch_folder("relative", "a"), "a key or root that could escape is refused");
	const auto folder = application::prepare_agent_scratch_folder(root, "0123abcd");
	expect(folder && *folder == root / "0123abcd" && std::filesystem::is_directory(*folder), "the folder is made under its root");
	const auto perms = std::filesystem::status(*folder).permissions();
	expect((perms & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) == std::filesystem::perms::none, "and only its owner can reach it");
	expect(application::valid_agent_grant_path(folder->string()) == (folder->string().find(' ') == std::string::npos), "a rule can name it when its path has no space");

	const auto before = application::list_agent_scratch_folder(*folder);
	write(*folder / "count.py", "print(3)\n");
	std::filesystem::create_directories(*folder / "lib");
	write(*folder / "lib" / "util.py", "x = 1\n");
	std::filesystem::create_symlink(outside, *folder / "away");
	const auto after = application::list_agent_scratch_folder(*folder);
	const auto wrote = application::agent_scratch_written(before, after, core::maximum_agent_activity_listed);
	expect(wrote == std::vector<std::string>{ "count.py", "lib/util.py" }, "the turn's new files are listed, a link is not followed");
	expect(application::agent_scratch_written(after, after, 16U).empty(), "an unchanged folder lists nothing");
	expect(!application::agent_scratch_full(after, core::maximum_agent_scratch_bytes), "a small folder is not full");
	expect(application::agent_scratch_full(after, 4U), "but it is at a limit it passed");

	// A sparse file reaches the limit without using the disk.
	{
		auto big = std::ofstream{ *folder / "big.bin", std::ios::binary };
		big.seekp(static_cast<std::streamoff>(core::maximum_agent_scratch_bytes));
		big.put('\0');
	}
	expect(application::agent_scratch_full(application::list_agent_scratch_folder(*folder), core::maximum_agent_scratch_bytes), "at 256 MiB the folder is full");
	expect(!application::agent_scratch_full(application::list_agent_scratch_folder(*folder), core::agent_scratch_limit_bytes(512U)), "and not at a 512 MiB limit");

	// Owner look e97c6a3d: folders are surveyed and removed by age, a kept
	// one excepted.
	const auto second = application::prepare_agent_scratch_folder(root, "second");
	const auto third  = application::prepare_agent_scratch_folder(root, "third");
	expect(second && third, "more folders");
	write(*second / "a.txt", "abc");
	std::filesystem::last_write_time(*second / "a.txt", std::filesystem::file_time_type::clock::now() - std::chrono::hours{ 24 * 40 });
	std::filesystem::last_write_time(*second, std::filesystem::file_time_type::clock::now() - std::chrono::hours{ 24 * 40 });
	const auto survey = application::survey_agent_scratch_root(root);
	expect(survey.size() == 3U && survey.front().Key == "0123abcd" && survey.front().Bytes > core::maximum_agent_scratch_bytes, "each folder with its size");
	const auto now   = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	const auto byAge = application::remove_agent_scratch_folders(root, now - 30LL * 24LL * 3600LL, { });
	expect(byAge.Removed == 1U && byAge.Bytes == 3U && !std::filesystem::exists(*second) && std::filesystem::exists(*third), "only the folder untouched for 30 days goes");
	const auto clear = application::remove_agent_scratch_folders(root, std::nullopt, { "third" });
	expect(clear.Removed == 1U && std::filesystem::exists(*third) && !std::filesystem::exists(root / "0123abcd"), "clear now keeps a folder in use");
	static_cast<void>(application::prepare_agent_scratch_folder(root, "0123abcd"));

	expect(application::remove_agent_scratch_folder(root, "0123abcd") && !std::filesystem::exists(*folder), "the folder goes with its session");
	expect(std::filesystem::exists(outside / "keep.txt"), "what a link inside named stays");
	expect(application::remove_agent_scratch_folder(root, "0123abcd"), "removing a folder already gone succeeds");
	std::filesystem::remove_all(base);
	std::cout << "agent-scratch-folder-tests:pass\n";
	return EXIT_SUCCESS;
}
