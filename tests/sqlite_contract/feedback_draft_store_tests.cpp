// [[BPR-1082]]. Conventional Comments header edits composed through the shared
// durable draft services (RFC-BPR-0048 R09 to R11, R42; RFC-BPR-0051 R41): the
// real SQLite private review store, review_composition_application_host and
// shared_draft_application_host for comment drafts, and the real SQLite tour
// store under review_tour_host for local tour stops. No agent, provider or
// publication port is constructed.

#include "../../src/adapters/sqlite/src/review_tour_codec.hpp"
#include "../core/private_review_fixture.hpp"
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/core/sha256.hpp"
#include "scratch_root.hpp"

#include <iostream>
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

	auto set(feedback_label aLabel, std::optional<feedback_expectation> aExpectation = std::nullopt) -> feedback_header_edit
	{
		return { .Change = feedback_label_change::set, .Label = aLabel, .Expectation = aExpectation };
	}
	auto typed(const revisioned_review_draft& aDraft, std::string aBody) -> revisioned_review_draft_command
	{
		return { .Command = { .Id = aDraft.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = std::move(aBody) }, .ExpectedRevision = aDraft.Revision, .Scope = aDraft.Scope };
	}

	struct fixture
	{
		tests::sqlite_contract::scratch_root       Scratch{ "feedback-draft" };
		private_review_configuration               Config{ { Scratch.path(), "github.com", "octo", "example", 5 }, bpr_profile_id{ 1 }, git_host_id{ 2 }, github_account_id{ 3 }, repository_id{ 4 }, pull_request_id{ 5 } };
		std::unique_ptr<private_review_store_port> Store   = std::move(*make_private_review_store(Config));
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

	/// One desktop process: the durable storage-lane owner and the shared
	/// decision-lane owner, with two windows attached.
	struct desktop
	{
		review_composition_application_host Drafts;
		shared_draft_application_host       Shared;
		shared_draft_authority              Authority;
		std::vector<std::string>            First, Second;
		shared_draft_consumer               Window;
		shared_draft_consumer               Other;
		desktop(fixture& aFixture, private_review_store_port& aStore)
			: Drafts(aStore)
			, Shared(aFixture.authority(), aFixture.Scope, aFixture.Contexts)
			, Authority(aFixture.authority())
		{
			Window = *Shared.attach(Authority, { aFixture.Binding.Scope.Profile, bpr_window_id{ 1 }, bpr_window_generation{ 1 } }, [this](const revisioned_review_draft& aDraft) { First.push_back(aDraft.Draft.Body); });
			Other  = *Shared.attach(Authority, { aFixture.Binding.Scope.Profile, bpr_window_id{ 2 }, bpr_window_generation{ 1 } }, [this](const revisioned_review_draft& aDraft) { Second.push_back(aDraft.Draft.Body); });
		}
		/// Commit through storage, then adopt on the decision lane: the path
		/// `dispatch_review_draft` takes for a durable draft.
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

	auto draft_columns(const std::filesystem::path& aLedger) -> std::vector<std::string>
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aLedger.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "open the ledger read-only");
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(database, "SELECT name FROM pragma_table_info('private_draft') ORDER BY cid", -1, &statement, nullptr) == SQLITE_OK, "read the draft table's columns");
		std::vector<std::string> columns;
		while (sqlite3_step(statement) == SQLITE_ROW) {
			columns.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
		}
		sqlite3_finalize(statement);
		sqlite3_close(database);
		return columns;
	}

	/// R09, R10, R11: write, label, save, restart and make plain again by hand,
	/// with no agent, through the existing durable owners.
	void human_only_compose_save_restart()
	{
		fixture    f;
		const auto original = std::string{ "Rename this.\r\n\r\n```suggestion\r\n\tvalue = 1;\r\n```\r\nα" };
		auto       id       = review_draft_id{ };
		{
			desktop app(f, *f.Store);
			auto    allocated = app.Drafts.allocate_draft(f.line(1), f.Index);
			expect(allocated && app.Shared.adopt(app.Authority, *allocated).has_value(), "an ordinary inline draft is allocated durably");
			id           = allocated->records().front().Draft.Draft.Id;
			auto current = app.commit(typed(*app.Shared.read(app.Window, id).Accepted, original));
			expect(current && current->Draft.Body == original, "the reviewer writes the comment");

			const auto plan = plan_feedback_draft_edit(*current, feedback_draft_basis_of(*current), set(feedback_label::issue, feedback_expectation::blocking));
			expect(plan.Command.has_value(), "the label is planned as an ordinary draft edit");
			expect(app.Shared.apply(app.Window, *plan.Command).Failure == shared_draft_failure::durable_required, "the volatile path cannot bypass the durable store");
			current = app.commit(*plan.Command);
			expect(current && current->Draft.Body == "issue (blocking): " + original, "the labelled draft is committed and adopted");
			expect(app.First.back() == current->Draft.Body && app.Second.back() == current->Draft.Body, "both windows show the same labelled draft");
			expect(f.Store->load_draft(id)->Draft.Revision == current->Revision, "storage holds the revision the windows show");
		}
		expect(draft_columns(f.ledger()) == std::vector<std::string>{ "scope_id", "id", "revision", "record", "body", "content_id" }, "the draft table gains no label or classification column");

		auto reopened = std::move(*make_private_review_store(f.Config));
		{
			desktop    app(f, *reopened);
			auto       restored   = app.restore(id);
			const auto annotation = recognize_feedback_prefix(restored.Draft.Body);
			expect(restored.Draft.Body == "issue (blocking): " + original && annotation.Label == feedback_label::issue && annotation.Expectation == feedback_expectation::blocking, "after a restart the label is read again from the saved bytes");

			// Plain by the control, then the label typed back by hand: both are
			// edits of the one Markdown body.
			const auto plain = plan_feedback_draft_edit(restored, feedback_draft_basis_of(restored), { .Change = feedback_label_change::remove });
			auto       after = app.commit(*plain.Command);
			expect(after && after->Draft.Body == original, "Plain restores the comment byte for byte");
			after = app.commit(typed(*after, "question: " + original));
			expect(after && recognize_feedback_prefix(after->Draft.Body).Label == feedback_label::question, "a label typed by hand counts the same");
		}
		reopened = std::move(*make_private_review_store(f.Config));
		{
			desktop app(f, *reopened);
			expect(app.restore(id).Draft.Body == "question: " + original, "the hand-typed label survives a restart; nothing else remembers the old one");
		}
		expect(reopened->find_publication()->Records.empty() && reopened->list_runs()->Runs.empty(), "compose, label, save and restart publish nothing and run no agent");
	}

	/// R10, R41: two windows and two store connections race; nothing is lost
	/// and the late edit becomes a conflict candidate.
	void conflicting_edits()
	{
		fixture f;
		auto    peerStore = std::move(*make_private_review_store(f.Config));
		desktop app(f, *f.Store);
		auto    allocated = app.Drafts.allocate_draft(f.line(1), f.Index, "Check the bounds.");
		expect(allocated && app.Shared.adopt(app.Authority, *allocated).has_value(), "seed one durable draft");
		const auto id    = allocated->records().front().Draft.Draft.Id;
		const auto shown = *app.Shared.read(app.Window, id).Accepted;

		// The first window chooses a label; the second window's typing lands first.
		const auto label = plan_feedback_draft_edit(shown, feedback_draft_basis_of(shown), set(feedback_label::todo));
		const auto other = app.commit(typed(*app.Shared.read(app.Other, id).Accepted, "Check the bounds and the overflow."));
		expect(other && app.First.back() == other->Draft.Body, "the other window's edit is shared");
		expect(!app.Drafts.change_draft(*label.Command), "the label planned against the older revision is refused by storage");
		expect(f.Store->load_draft(id)->Draft.Draft.Body == "Check the bounds and the overflow.", "the newer typing is kept byte for byte");
		const auto conflict = app.Shared.apply(app.Window, *label.Command);
		expect(conflict.Failure.has_value() && app.Shared.read(app.Window, id).Accepted->Draft.Body == other->Draft.Body, "the shared owner keeps the newer draft");
		const auto replan = plan_feedback_draft_edit(*app.Shared.read(app.Window, id).Accepted, feedback_draft_basis_of(shown), set(feedback_label::todo));
		expect(replan.Refusal == feedback_draft_edit_refusal::stale_draft && !replan.Command && replan.Decision.Annotation.BodyDigest == recognize_feedback_prefix(other->Draft.Body).BodyDigest, "the stale label is reported against the current body, the conflict candidate");

		// Another connection (another process) commits between plan and write.
		const auto                          current = *app.Shared.read(app.Window, id).Accepted;
		const auto                          again   = plan_feedback_draft_edit(current, feedback_draft_basis_of(current), set(feedback_label::todo));
		review_composition_application_host peer(*peerStore);
		expect(peer.change_draft(typed(current, "Check the bounds; written elsewhere.")).has_value(), "a second store connection writes first");
		expect(!app.Drafts.change_draft(*again.Command), "compare-and-swap in storage refuses the label");
		expect(peerStore->load_draft(id)->Draft.Draft.Body == "Check the bounds; written elsewhere.", "the other connection's text survives");
		auto restored = app.restore(id);
		expect(restored.Draft.Body == "Check the bounds; written elsewhere.", "the owner adopts the stored winner");
		const auto final = app.commit(*plan_feedback_draft_edit(restored, feedback_draft_basis_of(restored), set(feedback_label::todo)).Command);
		expect(final && final->Draft.Body == "todo: Check the bounds; written elsewhere.", "planning again from the stored draft applies");
		expect(f.Store->find_publication()->Records.empty(), "no conflict publishes anything");
	}

	/// Drafts saved before label support load intact and take edits; a
	/// publishing draft keeps its prepared bytes across a restart.
	void existing_and_prepared_drafts()
	{
		fixture                        f;
		const std::vector<std::string> bodies{ "plain words\r\n", "**issue (blocking, security):** keep\r\nthis", "Issue: mixed case", "custom: not ours", "issue (blocking: malformed", "> quoted", "nitpick (non-blocking): ```suggestion\n```" };
		std::vector<review_draft_id>   ids;
		{
			review_composition_application_host drafts(*f.Store);
			for (std::uint32_t index = 0; index < bodies.size(); ++index) {
				auto allocated = drafts.allocate_draft(f.line(index % 3U), f.Index);
				expect(allocated.has_value(), "seed a stored draft");
				auto record  = allocated->records().back().Draft;
				record.Scope = f.Scope;
				expect(drafts.change_draft(typed(record, bodies[index])).has_value(), "write it as an ordinary draft");
				ids.push_back(record.Draft.Id);
			}
		}
		auto    reopened = std::move(*make_private_review_store(f.Config));
		desktop app(f, *reopened);
		for (std::size_t index = 0; index < bodies.size(); ++index) {
			const auto restored = app.restore(ids[index]);
			expect(restored.Draft.Body == bodies[index], "a stored draft loads byte for byte");
			const auto plan    = plan_feedback_draft_edit(restored, feedback_draft_basis_of(restored), set(feedback_label::question));
			const bool refused = plan.Refusal == feedback_draft_edit_refusal::header;
			expect(refused == (index == 4U || index == 5U), "only malformed and block-start bodies refuse a new label");
			if (!refused) {
				const auto labelled = app.commit(*plan.Command);
				expect(labelled && recognize_feedback_prefix(labelled->Draft.Body).Label == feedback_label::question, "an existing draft takes a label");
			}
		}

		// A prepared publication: the draft is publishing and stays so across
		// a crash, and its label cannot change under the prepared action.
		auto prepared = app.Shared.read(app.Window, ids.front()).Accepted;
		prepared      = app.commit({ .Command = { .Id = prepared->Draft.Id, .Kind = review_draft_command_kind::begin_publish }, .ExpectedRevision = prepared->Revision, .Scope = prepared->Scope });
		expect(prepared && prepared->Draft.State == review_draft_state::publishing, "publication begins durably");
		const auto proposal = propose_publication(prepared->Draft, review_disposition::comment);
		auto       restart  = std::move(*make_private_review_store(f.Config));
		desktop    after(f, *restart);
		const auto held = after.restore(ids.front());
		expect(held.Draft.State == review_draft_state::publishing && held.Revision == prepared->Revision && propose_publication(held.Draft, review_disposition::comment) == proposal, "after a crash the same prepared proposal is recovered");
		expect(plan_feedback_draft_edit(held, feedback_draft_basis_of(held), set(feedback_label::issue)).Refusal == feedback_draft_edit_refusal::not_editable, "the prepared draft is not relabelled");
		expect(!after.Drafts.change_draft(typed(held, "issue: " + held.Draft.Body)) && restart->load_draft(ids.front())->Draft.Draft == held.Draft, "a hand-made edit is refused by storage and the prepared bytes stay");
		expect(restart->find_publication()->Records.empty(), "the store records no publication of its own");
	}

	void sql(const std::filesystem::path& aPath, const char* aSql)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &database) == SQLITE_OK, "open the tour store");
		expect(sqlite3_exec(database, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "tour store fixture SQL");
		sqlite3_close(database);
	}

	/// R41: a tour stop labelled by hand in a schema-1 store, edited by the
	/// terminal and the desktop through the one tour owner.
	void tour_stop_owner()
	{
		tests::sqlite_contract::scratch_root scratch{ "feedback-tour" };
		auto                                 draft = tests::review_tour_fixture();
		draft.Stops.front().Body                   = "**question:** why keep the **raw** text?\r\n<details>hidden</details>";
		{
			auto store = make_review_tour_store(scratch.path(), draft.Owner.Profile);
			expect(store->save(draft, 0, { }).has_value(), "save the tour");
		}
		const auto                           path = scratch.path() / "state/v1/review-tours.sqlite3";
		adapters::sqlite::detail::tour_codec legacy;
		legacy.Version = 7;
		auto copy      = draft;
		legacy.tour(copy);
		expect(legacy.Good, "encode the pre-journal payload");
		const auto digest   = sha256(std::span<const std::uint8_t>{ legacy.Bytes });
		sqlite3*   database = nullptr;
		expect(sqlite3_open(path.string().c_str(), &database) == SQLITE_OK, "open the store");
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(database, "UPDATE tours SET payload=?1,digest=?2", -1, &statement, nullptr) == SQLITE_OK && sqlite3_bind_blob(statement, 1, legacy.Bytes.data(), static_cast<int>(legacy.Bytes.size()), SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_bind_blob(statement, 2, digest.data(), static_cast<int>(digest.size()), SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(statement) == SQLITE_DONE, "install the legacy payload");
		sqlite3_finalize(statement);
		sqlite3_close(database);
		sql(path, "DROP TABLE targets; PRAGMA user_version=1");

		manual_executor effects, decisions;
		const auto      drain = [&] {
			for (unsigned i = 0; i < 20 && (effects.pending() || decisions.pending()); ++i) {
				decisions.drain();
				effects.drain();
			}
		};
		{
			review_tour_host host{ draft, make_review_tour_store(scratch.path(), draft.Owner.Profile), effects, decisions, { } };
			host.start();
			drain();
			const auto shown = *host.presentation().Draft;
			const auto stop  = shown.Stops.front().Id;
			expect(shown.Stops.front().Body == draft.Stops.front().Body, "the schema-1 tour loads its labelled stop byte for byte");
			expect(recognize_feedback_prefix(shown.Stops.front().Body).Label == feedback_label::question, "and its label is read from the text");

			const auto terminal = plan_feedback_tour_stop_edit(shown, stop, *feedback_tour_stop_basis(shown, stop), set(feedback_label::issue, feedback_expectation::blocking));
			auto       request  = tour_service_request{ .Command = *terminal.Command, .Origin = tour_presentation_origin::terminal, .RequestId = "label-1" };
			request.Digest.fill(1);
			const auto first = host.execute(request);
			expect(first.Status == tour_service_status::accepted, "the terminal's label edit is accepted by the tour owner");
			const auto replay = host.execute(request);
			expect(replay.Replayed && replay.Revision == first.Revision && host.presentation().Draft->Revision == first.Revision, "a retransmitted request is answered from its receipt, not applied twice");
			expect(host.presentation().Draft->Stops.front().Body == "**issue (blocking):** why keep the **raw** text?\r\n<details>hidden</details>", "the stop is relabelled in its own form");

			const auto window = plan_feedback_tour_stop_edit(shown, stop, *feedback_tour_stop_basis(shown, stop), set(feedback_label::note));
			const auto late   = host.execute({ .Command = *window.Command, .Origin = tour_presentation_origin::desktop });
			expect(late.Status == tour_service_status::refused && late.Refusal == tour_refusal::conflict, "the desktop's edit from the older revision is a conflict");
			expect(host.presentation().Draft->Stops.front().Body.starts_with("**issue (blocking):**"), "the terminal's edit is kept");
			expect(plan_feedback_tour_stop_edit(*host.presentation().Draft, stop, *feedback_tour_stop_basis(shown, stop), set(feedback_label::note)).Refusal == feedback_draft_edit_refusal::stale_draft, "planning from the older tour is stale");

			const auto current = *host.presentation().Draft;
			const auto undo    = host.execute({ .Command = { .Draft = current.Id, .Owner = current.Owner, .ExpectedRevision = current.Revision }, .Operation = tour_service_operation::undo, .Origin = tour_presentation_origin::desktop });
			expect(undo.Status == tour_service_status::accepted && host.presentation().Draft->Stops.front().Body == draft.Stops.front().Body, "one undo restores the hand-written label byte for byte");
			host.shutdown();
			while (!host.is_quiescent()) {
				effects.drain();
				decisions.drain();
			}
		}
		auto store  = make_review_tour_store(scratch.path(), draft.Owner.Profile);
		auto stored = store->load(draft.Owner, draft.Id, { });
		expect(stored && *stored && (*stored)->Stops.front().Body == draft.Stops.front().Body && (*stored)->Revision > draft.Revision, "the undone label is what the store keeps, at a newer revision");
	}
}

int main()
{
	human_only_compose_save_restart();
	conflicting_edits();
	existing_and_prepared_drafts();
	tour_stop_owner();
	std::cout << "FEEDBACK_DRAFT_STORE_OK\n";
}
