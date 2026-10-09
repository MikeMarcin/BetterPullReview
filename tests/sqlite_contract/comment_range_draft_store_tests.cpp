// [[BPR-1212]]. A pull-request draft on several lines through the real durable
// owners: the SQLite private review store, review_composition_application_host
// and shared_draft_application_host. The range survives a restart, a single
// line keeps its stored bytes, an earlier row reads as one line, a range GitHub
// would refuse is refused by storage, and the publication proposal carries the
// range with a key of its own. No provider or publication port is constructed.

#include "../core/private_review_fixture.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "scratch_root.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using namespace core;
	using namespace application;
	using namespace adapters::sqlite;
	using private_review_fixture::expect;

	struct every_window_current : shared_draft_context_port
	{
		auto current(const bpr_window_context&) const noexcept -> bool override { return true; }
	};

	template <class TResult>
	auto opened(TResult aResult)
	{
		expect(aResult.has_value(), "the private review store opens");
		return std::move(*aResult);
	}

	struct fixture
	{
		tests::sqlite_contract::scratch_root       Scratch{ "comment-range-draft" };
		private_review_configuration               Config{ { Scratch.path(), "github.com", "octo", "example", 5 }, bpr_profile_id{ 1 }, git_host_id{ 2 }, github_account_id{ 3 }, repository_id{ 4 }, pull_request_id{ 5 } };
		std::unique_ptr<private_review_store_port> Store   = opened(make_private_review_store(Config));
		private_review_binding                     Binding = private_review_fixture::binding();
		diff_semantic_index                        Index   = private_review_fixture::index();
		every_window_current                       Contexts;
		review_draft_scope_id                      Scope{ 9 };
		fixture()
		{
			Binding.Scope      = Store->frontier()->Scope;
			Binding.Comparison = Index.diff().Alignment.Digest;
		}
		[[nodiscard]] auto authority() const -> shared_draft_authority
		{
			return { .BprProfile = Binding.Scope.Profile, .ProfileAuthority = { .Profile = host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = Binding.Scope.Host, .Hostname = github_canonical_hostname{ "github.com" }, .ProbeContractRevision = github_probe_contract_revision{ 1 } }, .HostAuthority = Binding.HostAuthority, .Session = application_session_id{ 1 }, .Generation = github_identity_generation{ 1 }, .Account = Binding.Scope.Account, .Mode = github_access_mode::authenticated, .Target = { Binding.Scope.Host, Binding.Scope.Repository, Binding.Scope.PullRequest, Binding.Revision.HeadOid } };
		}
		[[nodiscard]] auto line(std::uint32_t aLine) const -> private_composition_target
		{
			return *make_composition_target(Binding, { *Binding.Comparison, diff_side::right, aLine }, Index);
		}
		[[nodiscard]] auto ledger() const { return Config.Ledger.StorageRoot / "state/v1/review-ledger.sqlite3"; }
	};

	struct desktop
	{
		review_composition_application_host Drafts;
		shared_draft_application_host       Shared;
		shared_draft_authority              Authority;
		shared_draft_consumer               Window;
		desktop(fixture& aFixture, private_review_store_port& aStore)
			: Drafts(aStore)
			, Shared(aFixture.authority(), aFixture.Scope, aFixture.Contexts)
			, Authority(aFixture.authority())
		{
			Window = *Shared.attach(Authority, { aFixture.Binding.Scope.Profile, bpr_window_id{ 1 }, bpr_window_generation{ 1 } }, [](const revisioned_review_draft&) { });
		}
		auto commit(const revisioned_review_draft_command& aCommand) -> std::optional<revisioned_review_draft>
		{
			auto committed = Drafts.change_draft(aCommand);
			if (!committed || !Shared.adopt(Authority, *committed)) {
				return std::nullopt;
			}
			return Shared.read(Window, aCommand.Command.Id).Accepted;
		}
		auto restore(review_draft_id aId) -> revisioned_review_draft
		{
			auto restored = Drafts.restore();
			expect(restored && Shared.adopt(Authority, *restored).has_value(), "the durable drafts are restored and adopted");
			auto accepted = Shared.read(Window, aId).Accepted;
			expect(accepted.has_value(), "the restored draft is shared");
			return *accepted;
		}
	};

	[[nodiscard]] auto ranged(const revisioned_review_draft& aDraft, const std::optional<std::uint32_t> aStart, std::string aBody) -> revisioned_review_draft_command
	{
		auto command             = revisioned_review_draft_command{ };
		command.Command.Id       = aDraft.Draft.Id;
		command.Command.Kind     = review_draft_command_kind::edit;
		command.Command.Body     = std::move(aBody);
		command.ExpectedRevision = aDraft.Revision;
		command.Scope            = aDraft.Scope;
		command.ChangeRange      = true;
		command.StartLine        = aStart;
		return command;
	}

	/// The stored record's codec version: its first eight bytes, big-endian.
	[[nodiscard]] auto record_version(const std::filesystem::path& aLedger, const review_draft_id aId) -> std::uint64_t
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aLedger.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "open the ledger read-only");
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(database, "SELECT record FROM private_draft WHERE id=?1", -1, &statement, nullptr) == SQLITE_OK, "read the draft record");
		sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(aId.value()));
		auto version = std::uint64_t{ 0U };
		if (sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_bytes(statement, 0) >= 8) {
			const auto* const bytes = static_cast<const unsigned char*>(sqlite3_column_blob(statement, 0));
			for (auto index = 0; index < 8; ++index) {
				version = (version << 8U) | bytes[index];
			}
		}
		sqlite3_finalize(statement);
		sqlite3_close(database);
		return version;
	}

	void a_range_survives_a_restart_and_one_line_keeps_its_bytes()
	{
		fixture    f;
		const auto block = std::string{ "```suggestion\nfirst\nnew\nlast\n```" };
		auto       id    = review_draft_id{ };
		auto       other = review_draft_id{ };
		{
			desktop app(f, *f.Store);
			auto    allocated = app.Drafts.allocate_draft(f.line(2), f.Index);
			expect(allocated && app.Shared.adopt(app.Authority, *allocated).has_value(), "a draft is allocated on the right side's last line");
			id         = allocated->records().front().Draft.Draft.Id;
			auto shown = *app.Shared.read(app.Window, id).Accepted;
			expect(!shown.Draft.StartLine && record_version(f.ledger(), id) == 1U, "a new draft is one line, stored as version 1");

			const auto range = app.commit(ranged(shown, 0U, block));
			expect(range && range->Draft.StartLine == 0U && range->Draft.Anchor == shown.Draft.Anchor && range->Draft.Body == block,
				   "one edit sets the first line and the suggestion block, and the anchor stays the last line");
			expect(f.Store->load_draft(id)->Draft.Draft.StartLine == 0U && record_version(f.ledger(), id) == 2U, "storage holds the range as version 2");

			for (const auto start : { 2U, 3U }) {
				expect(!app.Drafts.change_draft(ranged(*range, start, block)), "a first line that does not precede the anchor is refused by storage");
			}
			expect(f.Store->load_draft(id)->Draft.Draft.StartLine == 0U, "a refused range changes nothing stored");

			auto single = app.Drafts.allocate_draft(f.line(1), f.Index, "One line.");
			expect(single && app.Shared.adopt(app.Authority, *single).has_value(), "a second draft on one line");
			other = single->records().front().Draft.Draft.Id;
			expect(record_version(f.ledger(), other) == 1U, "a single-line draft beside a range keeps version 1");
		}
		auto reopened = std::move(*make_private_review_store(f.Config));
		{
			desktop    app(f, *reopened);
			const auto restored = app.restore(id);
			expect(restored.Draft.StartLine == 0U && restored.Draft.Anchor.Line == 2U && restored.Draft.Body == block, "after a restart the range and its block are back");
			expect(!app.restore(other).Draft.StartLine, "the single-line draft reads as one line");

			auto       publishing = apply(restored.Draft, review_draft_command{ .Id = restored.Draft.Id, .Kind = review_draft_command_kind::begin_publish, .Body = { } });
			const auto proposal   = propose_publication(publishing.Draft, review_disposition::comment);
			const auto target     = review_mutation_target{ f.Binding.Scope.Host, f.Binding.Scope.Repository, f.Binding.Scope.PullRequest, f.Binding.Revision.HeadOid };
			expect(proposal && proposal->StartLine == 0U && proposal->Anchor == restored.Draft.Anchor, "the restored range is what the proposal carries");
			auto lastLine      = *proposal;
			lastLine.StartLine = std::nullopt;
			expect(idempotency_key_for(*proposal, target) != idempotency_key_for(lastLine, target), "the range's publication key is not its last line's");

			const auto narrowed = app.commit(ranged(restored, std::nullopt, restored.Draft.Body));
			expect(narrowed && !narrowed->Draft.StartLine && record_version(f.ledger(), id) == 1U, "making it one line again stores version 1 again");
		}
		expect(reopened->find_publication()->Records.empty(), "drafting a range publishes nothing");
	}

	/// [[BPR-1093]]. A label written into a draft through the plan the chip
	/// and `bpr` share (RFC-BPR-0048 R10, R12; RFC-BPR-0049 R41) keeps what
	/// the record already held: a range stays a range in codec version 2 with
	/// its suggestion block byte for byte, a single line stays version 1, the
	/// label is read again from the bytes after a restart, and a label chosen
	/// against an older revision is refused by storage with the newer text
	/// kept. Nothing is published.
	void a_label_edit_keeps_the_range_and_its_record_version()
	{
		fixture    f;
		const auto prose = std::string{ "Bound the copy.\r\n\r\n" };
		const auto block = std::string{ "```suggestion\r\n\tcopy_n(src, checked, dst);\r\n```\r\n" };
		const auto label = [](const revisioned_review_draft& aDraft, const feedback_header_edit& aEdit) {
			const auto plan = plan_feedback_draft_edit(aDraft, feedback_draft_basis_of(aDraft), aEdit);
			expect(plan.Refusal == feedback_draft_edit_refusal::none && plan.Command.has_value() && !plan.Command->ChangeRange, "the label is planned as an ordinary edit that leaves the range alone");
			return *plan.Command;
		};
		auto id     = review_draft_id{ };
		auto single = review_draft_id{ };
		{
			desktop app(f, *f.Store);
			auto    allocated = app.Drafts.allocate_draft(f.line(2), f.Index);
			expect(allocated && app.Shared.adopt(app.Authority, *allocated).has_value(), "a draft is allocated on the right side's last line");
			id               = allocated->records().front().Draft.Draft.Id;
			const auto range = app.commit(ranged(*app.Shared.read(app.Window, id).Accepted, 0U, prose + block));
			expect(range && range->Draft.StartLine == 0U && record_version(f.ledger(), id) == 2U, "a range with a CRLF suggestion block is stored as version 2");

			const auto issue = app.commit(label(*range, { .Change = feedback_label_change::set, .Label = feedback_label::issue, .Expectation = feedback_expectation::blocking }));
			expect(issue && issue->Draft.Body == "issue (blocking): " + prose + block && issue->Draft.StartLine == 0U && issue->Draft.Anchor == range->Draft.Anchor,
				   "issue (blocking) is written before the prose; the range, the anchor and every byte of the block stay");
			expect(record_version(f.ledger(), id) == 2U && f.Store->load_draft(id)->Draft.Draft.StartLine == 0U, "and storage still holds a version 2 range");

			const auto stale = label(*range, { .Change = feedback_label_change::set, .Label = feedback_label::question });
			expect(!app.Drafts.change_draft(stale) && f.Store->load_draft(id)->Draft.Draft.Body == issue->Draft.Body,
				   "a label chosen against the revision before the issue label is refused by storage and the newer text is kept");
			const auto plain = app.commit(label(*issue, { .Change = feedback_label_change::remove }));
			expect(plain && plain->Draft.Body == prose + block && plain->Draft.StartLine == 0U && record_version(f.ledger(), id) == 2U,
				   "Plain restores the range draft byte for byte, still a version 2 range");
			const auto relabelled = app.commit(label(*plain, { .Change = feedback_label_change::set, .Label = feedback_label::suggestion }));
			expect(relabelled && relabelled->Draft.Body == "suggestion: " + prose + block, "and it can be labelled again");

			auto one = app.Drafts.allocate_draft(f.line(1), f.Index, "One line.");
			expect(one && app.Shared.adopt(app.Authority, *one).has_value(), "a single-line draft beside it");
			single           = one->records().front().Draft.Draft.Id;
			const auto typed = app.commit(label(*app.Shared.read(app.Window, single).Accepted, { .Change = feedback_label_change::set, .Label = feedback_label::nitpick }));
			expect(typed && typed->Draft.Body == "nitpick: One line." && !typed->Draft.StartLine && record_version(f.ledger(), single) == 1U,
				   "a labelled single-line draft keeps its version 1 record");
		}
		auto reopened = std::move(*make_private_review_store(f.Config));
		{
			desktop    app(f, *reopened);
			const auto restored = app.restore(id);
			const auto read     = recognize_feedback_prefix(restored.Draft.Body);
			expect(restored.Draft.Body == "suggestion: " + prose + block && restored.Draft.StartLine == 0U && read.Label == feedback_label::suggestion,
				   "after a restart the range, its block and the label read from the bytes are back");
			expect(app.restore(single).Draft.Body == "nitpick: One line." && record_version(f.ledger(), single) == 1U, "and the single line is still version 1");
		}
		expect(reopened->find_publication()->Records.empty(), "labelling drafts publishes nothing");
	}
} // namespace

int main()
{
	a_range_survives_a_restart_and_one_line_keeps_its_bytes();
	a_label_edit_keeps_the_range_and_its_record_version();
	std::cout << "COMMENT_RANGE_DRAFT_STORE_OK\n";
	return EXIT_SUCCESS;
}
