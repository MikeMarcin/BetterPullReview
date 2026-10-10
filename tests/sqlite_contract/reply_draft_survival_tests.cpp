// [[BPR-1374]]. An unsent reply through the real durable owners: the SQLite
// private review store, review_composition_application_host and
// shared_draft_application_host, read against the accepted snapshot's threads
// through application::observe_reply_threads. A Refresh is a new composition
// over the same store; a restart reopens the store. The reply is opened again
// by its thread and held again after a Refresh, it is presented on its thread
// after a restart, and when its thread is resolved or deleted on the server it
// is kept and flagged; a deleted thread's reply is kept as a comment on its
// line or discarded, durably. No provider or publication port is constructed,
// and nothing is published.

#include "../core/private_review_fixture.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/reply_thread_observation.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/core/reply_draft_standing.hpp"
#include "scratch_root.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace core;
	using namespace application;
	using namespace adapters::sqlite;

	// Every scenario runs, and each failed expectation is reported by name.
	int  Failures = 0;
	auto Scenario = std::string_view{ };

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: [" << Scenario << "] " << aMessage << '\n';
			++Failures;
		}
	}

	struct every_window_current : shared_draft_context_port
	{
		auto current(const bpr_window_context&) const noexcept -> bool override { return true; }
	};

	constexpr auto thread_id = std::string_view{ "PRRT_reply_1" };
	constexpr auto words     = std::string_view{ "question: Is this still needed once the clear runs first?" };

	struct fixture
	{
		tests::sqlite_contract::scratch_root       Scratch{ "reply-draft-survival" };
		private_review_configuration               Config{ { Scratch.path(), "github.com", "octo", "example", 5 }, bpr_profile_id{ 1 }, git_host_id{ 2 }, github_account_id{ 3 }, repository_id{ 4 }, pull_request_id{ 5 } };
		std::unique_ptr<private_review_store_port> Store;
		private_review_binding                     Binding = private_review_fixture::binding();
		diff_semantic_index                        Index   = private_review_fixture::index();
		every_window_current                       Contexts;
		review_draft_scope_id                      Scope{ 9 };
		fixture()
		{
			reopen();
			Binding.Scope      = Store->frontier()->Scope;
			Binding.Comparison = Index.diff().Alignment.Digest;
		}
		/// A restart: the store is closed and opened again from its file.
		void reopen()
		{
			Store.reset();
			auto opened = make_private_review_store(Config);
			private_review_fixture::expect(opened.has_value(), "the private review store opens");
			Store = std::move(*opened);
		}
		[[nodiscard]] auto authority() const -> shared_draft_authority
		{
			return { .BprProfile = Binding.Scope.Profile, .ProfileAuthority = { .Profile = host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = Binding.Scope.Host, .Hostname = github_canonical_hostname{ "github.com" }, .ProbeContractRevision = github_probe_contract_revision{ 1 } }, .HostAuthority = Binding.HostAuthority, .Session = application_session_id{ 1 }, .Generation = github_identity_generation{ 1 }, .Account = Binding.Scope.Account, .Mode = github_access_mode::authenticated, .Target = { Binding.Scope.Host, Binding.Scope.Repository, Binding.Scope.PullRequest, Binding.Revision.HeadOid } };
		}
		/// The changed line on the right side, where the thread is.
		[[nodiscard]] auto line() const -> source_line_ref { return { *Binding.Comparison, diff_side::right, 1U }; }
	};

	/// One window's composition over the store: what a Refresh rebuilds.
	struct composition
	{
		review_composition_application_host Drafts;
		shared_draft_application_host       Shared;
		shared_draft_authority              Authority;
		shared_draft_consumer               Window;
		composition(fixture& aFixture)
			: Drafts(*aFixture.Store)
			, Shared(aFixture.authority(), aFixture.Scope, aFixture.Contexts)
			, Authority(aFixture.authority())
		{
			Window        = *Shared.attach(Authority, { aFixture.Binding.Scope.Profile, bpr_window_id{ 1 }, bpr_window_generation{ 1 } }, [](const revisioned_review_draft&) { });
			auto restored = Drafts.restore(aFixture.Binding.Revision.HeadOid);
			expect(restored && Shared.adopt(Authority, *restored).has_value(), "the saved drafts are restored under the head");
		}
		/// The draft a line, or a thread's Reply on it, opens.
		auto select(const fixture& aFixture, const std::optional<std::string>& aThread) -> std::optional<revisioned_review_draft>
		{
			auto selected = Drafts.select_draft(aFixture.Binding, aFixture.line(), aFixture.Index, aThread);
			if (!selected || selected->records().empty() || !Shared.adopt(Authority, *selected)) {
				return std::nullopt;
			}
			return Shared.read(Window, selected->records().back().Draft.Draft.Id).Accepted;
		}
		auto commit(const revisioned_review_draft_command& aCommand) -> std::optional<revisioned_review_draft>
		{
			auto committed = Drafts.change_draft(aCommand);
			if (!committed || !Shared.adopt(Authority, *committed)) {
				return std::nullopt;
			}
			return Shared.read(Window, aCommand.Command.Id).Accepted;
		}
		[[nodiscard]] auto drafts() const -> std::vector<review_comment_draft>
		{
			auto values = std::vector<review_comment_draft>{ };
			for (const auto& saved : Shared.recovery_snapshot()) {
				values.push_back(saved.Draft);
			}
			return values;
		}
	};

	/// The accepted snapshot's threads: the reply's thread, open or resolved,
	/// with its root's text, or gone; read whole or not at all.
	[[nodiscard]] auto snapshot(const bool aThreadThere, const bool aResolved = false, std::string aRoot = "issue: the clear runs after the draw.", const bool aRead = true) -> github_pull_request_snapshot
	{
		auto value                      = github_pull_request_snapshot{ };
		value.ReviewThreadsAvailability = aRead ? github_collection_availability::available : github_collection_availability::unavailable;
		auto other                      = github_review_thread_record{ };
		other.Thread                    = github_thread_id{ "PRRT_other" };
		value.ReviewThreads.push_back(other);
		if (aThreadThere && aRead) {
			auto thread     = github_review_thread_record{ };
			thread.Thread   = github_thread_id{ std::string{ thread_id } };
			thread.Resolved = aResolved;
			auto root       = github_review_comment_record{ };
			root.Comment    = github_comment_id{ "PRRC_root" };
			root.Body       = std::move(aRoot);
			thread.Comments.push_back(std::move(root));
			value.ReviewThreads.push_back(std::move(thread));
		}
		if (!aRead) {
			value.ReviewThreads.clear();
		}
		return value;
	}

	/// The reviewer selects the thread's line, presses Reply and types: the
	/// line's new comment is made first, then the reply, then its words.
	[[nodiscard]] auto write_reply(fixture& aFixture) -> review_draft_id
	{
		composition app(aFixture);
		const auto  line  = app.select(aFixture, std::nullopt);
		const auto  reply = app.select(aFixture, std::string{ thread_id });
		expect(line && reply && line->Draft.Id != reply->Draft.Id && reply->Draft.ReplyToThread == std::optional<std::string>{ std::string{ thread_id } },
			   "the line's comment and the thread's reply are two drafts");
		if (!reply) {
			return { };
		}
		auto edit             = revisioned_review_draft_command{ };
		edit.Command          = { .Id = reply->Draft.Id, .Kind = review_draft_command_kind::edit, .Body = std::string{ words } };
		edit.ExpectedRevision = reply->Revision;
		edit.Scope            = reply->Scope;
		const auto written    = app.commit(edit);
		expect(written && written->Draft.Body == words, "the reply's words are saved");
		return reply->Draft.Id;
	}

	void refresh_with_no_change()
	{
		fixture    f;
		const auto id = write_reply(f);
		// The Refresh: a new composition over the same store.
		composition app(f);
		const auto  threads = observe_reply_threads(snapshot(true));
		expect(threads.Complete && threads.Threads.size() == 2U, "the snapshot's threads are read whole");
		expect(resume_reply_after_refresh(app.drafts(), id, threads) == std::optional{ id }, "the reply the editor held is held again");
		const auto reopened = app.select(f, std::string{ thread_id });
		expect(reopened && reopened->Draft.Id == id && reopened->Draft.Body == words, "its thread's Reply opens the same reply with its words");
		const auto decisions = decide_reply_drafts(app.drafts(), threads);
		expect(decisions.size() == 1U && decisions.front().Standing == reply_draft_standing::answerable, "it stands on its thread, sendable");
	}

	void refresh_with_an_external_edit()
	{
		fixture     f;
		const auto  id = write_reply(f);
		composition app(f);
		const auto  threads = observe_reply_threads(snapshot(true, false, "issue: edited on GitHub."));
		expect(resume_reply_after_refresh(app.drafts(), id, threads) == std::optional{ id }, "the reply is held again after another client's edit");
		const auto reopened = app.select(f, std::string{ thread_id });
		expect(reopened && reopened->Draft.Id == id && reopened->Draft.Body == words, "the reviewer's words are not the server's and are kept");
		expect(decide_reply_draft(reopened.value_or(revisioned_review_draft{ }).Draft, threads).Standing == reply_draft_standing::answerable, "it still answers its thread");
	}

	void restart()
	{
		fixture    f;
		const auto id = write_reply(f);
		f.reopen();
		composition app(f);
		const auto  threads   = observe_reply_threads(snapshot(true));
		const auto  decisions = decide_reply_drafts(app.drafts(), threads);
		expect(decisions.size() == 1U && decisions.front().Draft == id && decisions.front().Standing == reply_draft_standing::answerable,
			   "after a restart the saved reply is presented on its thread");
		const auto line = app.select(f, std::nullopt);
		expect(line && line->Draft.Id != id && !line->Draft.ReplyToThread, "choosing the line opens its comment, never the reply");
		const auto reopened = app.select(f, std::string{ thread_id });
		expect(reopened && reopened->Draft.Id == id && reopened->Draft.Body == words, "Reply opens the saved reply, not an empty new one");
	}

	void thread_deleted()
	{
		fixture    f;
		const auto id      = write_reply(f);
		const auto threads = observe_reply_threads(snapshot(false));
		{
			composition app(f);
			const auto  decision = decide_reply_draft(app.Shared.read(app.Window, id).Accepted.value_or(revisioned_review_draft{ }).Draft, threads);
			expect(decision.Standing == reply_draft_standing::thread_deleted && !decision.Sendable && decision.KeepableAsComment, "the reply is kept and flagged, never sent");
			expect(!resume_reply_after_refresh(app.drafts(), id, threads).has_value(), "it is not put back into a thread that is gone");
			expect(decide_reply_draft(app.Shared.read(app.Window, id).Accepted.value_or(revisioned_review_draft{ }).Draft, observe_reply_threads(snapshot(false, false, { }, false))).Standing
					   == reply_draft_standing::thread_unknown,
				   "a read that could not see the threads calls nothing deleted");
			const auto saved   = app.Shared.read(app.Window, id).Accepted;
			const auto command = saved ? keep_reply_as_comment(*saved, threads) : std::nullopt;
			expect(command.has_value(), "Keep as comment is offered");
			if (command) {
				expect(app.commit(*command).has_value(), "Keep as comment is stored");
			}
		}
		f.reopen();
		{
			composition app(f);
			const auto  kept = app.Shared.read(app.Window, id).Accepted;
			expect(kept && !kept->Draft.ReplyToThread && kept->Draft.Body == words && kept->Draft.State == review_draft_state::drafting && kept->Draft.Anchor == f.line(),
				   "after a restart the words are a comment on the reply's line, unpublished");
			const auto line = app.select(f, std::nullopt);
			expect(line && line->Draft.Id == id, "choosing the line opens the kept comment");
			// A second reply, discarded: gone for good.
			const auto other = app.select(f, std::string{ "PRRT_gone" });
			auto       edit  = revisioned_review_draft_command{ };
			if (other) {
				edit.Command          = { .Id = other->Draft.Id, .Kind = review_draft_command_kind::edit, .Body = "nit: and this one." };
				edit.ExpectedRevision = other->Revision;
				edit.Scope            = other->Scope;
			}
			const auto written = other ? app.commit(edit) : std::nullopt;
			const auto discard = written ? discard_unsent_reply(*written) : std::nullopt;
			expect(discard.has_value() && app.commit(*discard).has_value(), "Discard is stored");
		}
		f.reopen();
		{
			composition app(f);
			const auto  replies = decide_reply_drafts(app.drafts(), threads);
			expect(replies.empty(), "nothing discarded or kept as a comment is a reply any longer");
		}
	}

	void thread_resolved()
	{
		fixture     f;
		const auto  id = write_reply(f);
		composition app(f);
		const auto  threads  = observe_reply_threads(snapshot(true, true));
		const auto  decision = decide_reply_draft(app.Shared.read(app.Window, id).Accepted.value_or(revisioned_review_draft{ }).Draft, threads);
		expect(decision.Standing == reply_draft_standing::thread_resolved && decision.Sendable && !decision.KeepableAsComment, "the reply is kept and flagged as resolved, and can still be sent");
		expect(resume_reply_after_refresh(app.drafts(), id, threads) == std::optional{ id }, "the reply is held again");
		const auto reopened = app.select(f, std::string{ thread_id });
		expect(reopened && reopened->Draft.Id == id && reopened->Draft.Body == words, "Reply opens it with its words");
	}
}

int main()
{
	const auto run = [](const std::string_view aName, void (*aScenario)()) {
		Scenario = aName;
		aScenario();
	};
	run("refresh with no change", refresh_with_no_change);
	run("refresh with an external edit", refresh_with_an_external_edit);
	run("restart", restart);
	run("thread deleted", thread_deleted);
	run("thread resolved", thread_resolved);
	if (Failures != 0) {
		std::cerr << Failures << " expectations failed\n";
		return 1;
	}
	std::cout << "REPLY_DRAFT_SURVIVAL_OK\n";
	return 0;
}
