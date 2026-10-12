#include "../../src/app/src/private_review_workflow.hpp"
#include "../core/private_review_fixture.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "bpr/application/manual_executor.hpp"

#include <QApplication>
#include <cstdlib>
#include <iostream>
#include <mutex>

namespace {
	using namespace bpr;
	using tests::expect;

	void restore_after_comparison_change(bool aCompleteStoreFirst, bool aClose)
	{
		tests::sqlite_contract::scratch_root           scratch{ "private-draft-restore" };
		application::manual_executor                   effects;
		application::manual_executor                   decisions;
		std::mutex                                     gate;
		app::private_review_recovery_registry          recovery;
		qt::private_review_panel                       panel;
		app::private_review_workflow                   workflow{ effects, decisions, gate, recovery, panel, nullptr, { }, { } };
		adapters::sqlite::private_review_configuration configuration{
			{ scratch.path() / "data", "github.com", "octo", "example", 5 },
			core::bpr_profile_id{ 1 },
			core::git_host_id{ 2 },
			core::github_account_id{ 3 },
			core::repository_id{ 4 },
			core::pull_request_id{ 5 }
		};
		expect(adapters::sqlite::create_storage_root_owner_private(configuration.Ledger.StorageRoot), "the course makes its data root owner-private");
		auto binding = private_review_fixture::binding();
		bool opened  = false;
		workflow.open(configuration, [&] {
			opened = true;
			workflow.observe(binding, { }, std::nullopt);
		});
		const auto drain = [&] {
			for (auto round = 0U; round < 16U && (effects.pending() || decisions.pending()); ++round) {
				effects.drain();
				decisions.drain();
			}
			expect(workflow.is_quiescent() && effects.pending() == 0 && decisions.pending() == 0, "private work finishes without an unbounded retry");
		};
		drain();
		expect(opened && workflow.binding().has_value(), "the real private store opens and accepts its comparison");
		auto completions = 0U;
		bool restored    = false;
		workflow.restore([&](auto aCommit) {
			++completions;
			restored = aCommit.has_value();
		});
		if (aCompleteStoreFirst) {
			effects.drain();
		}
		// A refresh adopts a new comparison while the prior restore is either
		// queued or waiting to land on the decision lane. No second caller
		// restore and no repeated user gesture may be needed to become ready.
		if (aClose) {
			workflow.shutdown();
		} else {
			binding.Comparison = private_review_fixture::opaque<core::file_comparison_digest>();
			workflow.observe(binding, { }, std::nullopt);
		}
		drain();
		expect(completions == 1U && restored != aClose, "a retired restore answers once with current drafts, or refuses after shutdown");
	}
}

auto main(int aArgumentCount, char** aArguments) -> int
try {
	QApplication application{ aArgumentCount, aArguments };
	restore_after_comparison_change(false, false);
	restore_after_comparison_change(true, false);
	restore_after_comparison_change(false, true);
	std::cout << "private-draft-restore:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
