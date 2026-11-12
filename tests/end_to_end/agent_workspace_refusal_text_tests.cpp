// [[BPR-2118]]. Each workspace-record refusal check has its own sentence, and
// a refusal names how many records it refused and the first of them.
#include "../support/test_expectation.hpp"
#include "github_application/agent_composition.hpp"

#include <QString>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>

namespace {
	using bpr::tests::expect;
	namespace git = bpr::adapters::git;

	auto text_for(const git::workspace_refusal_check aCheck, const std::size_t aCount,
				  const git::workspace_record_finding aFinding = git::workspace_record_finding::none) -> QString
	{
		auto refusal    = git::workspace_recovery_refusal{ };
		refusal.Check   = aCheck;
		refusal.Folder  = std::filesystem::path{ "/data/agent-workspace-ownership-v1/6b24f4dab44ea68a8e1f41c6709393ea.jsonl" };
		refusal.Count   = aCount;
		refusal.Finding = aFinding;
		return bpr::app::agent_workspace_refusal_text(refusal);
	}

	void check_each_record_check_has_its_own_sentence()
	{
		const auto malformed   = text_for(git::workspace_refusal_check::record_malformed, 1U);
		const auto otherVolume = text_for(git::workspace_refusal_check::record_moved, 1U, git::workspace_record_finding::other_volume);
		const auto replaced    = text_for(git::workspace_refusal_check::record_moved, 1U, git::workspace_record_finding::replaced);
		const auto missing     = text_for(git::workspace_refusal_check::record_moved, 1U, git::workspace_record_finding::missing);
		const auto cleanup     = text_for(git::workspace_refusal_check::record_cleanup_failed, 1U);
		const auto sentences   = std::set<QString>{ malformed, otherVolume, replaced, missing, cleanup };
		expect(sentences.size() == 5U, "malformed, each moved finding and cleanup failure say different things");
		for (const auto& sentence : sentences) {
			expect(sentence.startsWith(QStringLiteral("Disposable agent workspaces are unavailable: ")) && sentence.contains(QStringLiteral("6b24f4dab44ea68a8e1f41c6709393ea.jsonl")),
				   "every record sentence keeps the lead and names the record");
			expect(!sentence.contains(QStringLiteral("could not be read back")), "the retired catch-all sentence is gone");
		}
		expect(malformed.contains(QStringLiteral("cannot be read")), "a malformed record says a line cannot be read");
		expect(otherVolume.contains(QStringLiteral("same inode on a different volume")) && otherVolume.contains(QStringLiteral("a restart alone does not do this")),
			   "the same inode on another volume is said as such, not as a missing folder");
		expect(replaced.contains(QStringLiteral("replaced by a different one")), "a replaced folder is said as such");
		expect(missing.contains(QStringLiteral("is gone")), "a missing folder is said as such");
		expect(cleanup.contains(QStringLiteral("could not be cleaned up exactly")), "a cleanup failure is said as such");
	}

	void check_refusal_counts_every_record()
	{
		const auto many = text_for(git::workspace_refusal_check::record_moved, 14U, git::workspace_record_finding::replaced);
		expect(many.contains(QStringLiteral("14 workspace records could not be used (first: ")) && many.contains(QStringLiteral("6b24f4dab44ea68a8e1f41c6709393ea.jsonl")),
			   "a refusal of fourteen records counts them and names the first");
		expect(many.contains(QStringLiteral("Move each refused record")), "the advice covers every refused record, not only the first");
		const auto one = text_for(git::workspace_refusal_check::record_moved, 1U, git::workspace_record_finding::replaced);
		expect(!one.contains(QStringLiteral("workspace records could not be used")), "a single refused record is not counted as several");
	}
}

auto main() -> int
try {
	check_each_record_check_has_its_own_sentence();
	check_refusal_counts_every_record();
	std::cout << "agent-workspace-refusal-text:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
