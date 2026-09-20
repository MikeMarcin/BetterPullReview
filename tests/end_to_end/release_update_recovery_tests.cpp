#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {
	using namespace bpr;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	QCoreApplication application{ aArgc, aArgv };
	QTemporaryDir    temporary;
	const auto       root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	expect(temporary.isValid() && adapters::sqlite::make_storage_root_owner_private(root), "disposable private state");
	auto store = adapters::sqlite::make_release_update_recovery_store(root);
	expect(store->load() && store->load()->empty(), "fresh store is empty");
	const std::vector<application::release_update_saved_draft> original{ { "draft:1", "inline pull request head", "First private draft", false }, { "draft:2", "uncertain review", "May already be published", true } };
	expect(store->save(original) && store->save(original), "exact duplicates do not lose or multiply text");
	auto changed         = original;
	changed.front().Body = "Second private draft";
	expect(store->save(changed), "later private text appends without overwriting prior recovery");
	store.reset();
	store      = adapters::sqlite::make_release_update_recovery_store(root);
	auto saved = store->load();
	expect(saved && saved->size() == 3U && (*saved)[0] == original[0] && (*saved)[1] == original[1] && (*saved)[2] == changed[0], "restart preserves text and indeterminate state exactly");
	changed.front().Body.assign(65537U, 'x');
	expect(!store->save(changed) && store->load() == saved, "oversized save refuses atomically");
	changed.assign(application::release_update_maximum_saved_drafts + 1U, original.front());
	expect(!store->save(changed) && store->load() == saved, "row bound refuses without changing retained text");
	store.reset();
	const auto file = root / "state/v1/update-draft-recovery.sqlite3";
	std::filesystem::rename(file, root / "retained.sqlite3");
	const auto foreign = root / "foreign";
	{
		std::ofstream output{ foreign };
		output << "untouched foreign file";
	}
	std::filesystem::create_symlink(foreign, file);
	store = adapters::sqlite::make_release_update_recovery_store(root);
	expect(!store->save(original) && !store->load(), "symlink substitution refuses before opening foreign data");
	std::ifstream     input{ foreign };
	const std::string text{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	expect(text == "untouched foreign file", "foreign data is unchanged");
	std::cout << "release-update-recovery:pass\n";
}
