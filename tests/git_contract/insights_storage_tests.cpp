#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"

#include <fstream>
#include <iostream>

int main()
{
	using namespace bpr;
	using tests::expect;
	namespace fs = std::filesystem;
	tests::sqlite_contract::scratch_root scratch{ "git-insights" };
	const auto                           root   = scratch.path();
	const auto                           first  = root / "repositories/v1" / (std::string(64, 'a') + ".git");
	const auto                           second = root / "repositories/v1" / (std::string(64, 'b') + ".git");
	for (const auto& repository : { first, second }) {
		fs::create_directories(repository / "objects/pack");
		for (auto directory = repository / "objects/pack"; directory != root; directory = directory.parent_path()) {
			expect(adapters::sqlite::make_storage_root_owner_private(directory), "private repository inventory fixture");
		}
		const auto file = repository / "objects/pack/fixture.pack";
		{
			std::ofstream stream{ file };
			stream << std::string(repository == first ? 1234 : 5678, 'p');
		}
		fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
	}
	const auto measured = adapters::git::measure_installed_git_storage(root);
	expect(measured.size() == 2 && measured[0].Bytes == 1234 && measured[1].Bytes == 5678, "shared cache inventory reports every repository's exact bytes without a Git executable or process");
#if !defined(_WIN32)
	fs::create_symlink(second / "objects/pack/fixture.pack", first / "alias");
	const auto linked = adapters::git::measure_installed_git_storage(root);
	expect(!linked[0].Bytes && linked[1].Bytes == 5678, "a linked entry cannot be counted as owned repository data");
#endif
	std::cout << "git-insights-storage:pass\n";
}
