#include "bpr/application/manual_executor.hpp"
#include "bpr/application/profile_management_host.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using namespace bpr::application;
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	struct store final : profile_registry_port
	{
		profile_registry_snapshot               Snapshot;
		std::optional<profile_registry_failure> Failure;
		std::size_t                             Saves                        = 0U;
		bool                                    CommitWithoutAcknowledgement = false;
		auto                                    load(std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			return Failure ? profile_registry_result<profile_registry_snapshot>{ std::unexpected(*Failure) } : Snapshot;
		}
		auto save(const profile_registry_snapshot& aCandidate, std::uint64_t aRevision, std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			++Saves;
			if (Failure) {
				return std::unexpected(*Failure);
			}
			if (Snapshot.Revision != aRevision) {
				return std::unexpected(profile_registry_failure::conflict);
			}
			expect(valid_profile_registry_update(Snapshot, aCandidate), "host writes a legal forward registry transition");
			Snapshot = aCandidate;
			if (CommitWithoutAcknowledgement) {
				return std::unexpected(profile_registry_failure::unavailable);
			}
			return Snapshot;
		}
	};
	struct projection final : profile_management_projection_port
	{
		std::vector<profile_management_projection>                States;
		std::vector<profile_runtime_command>                      Commands;
		std::function<void(const profile_management_projection&)> OnPublish;
		void                                                      publish(const profile_management_projection& aProjection) override
		{
			States.push_back(aProjection);
			if (OnPublish) {
				OnPublish(aProjection);
			}
		}
		void               perform(const profile_runtime_command& aCommand) override { Commands.push_back(aCommand); }
		[[nodiscard]] auto last() const -> const profile_management_projection& { return States.back(); }
	};
	struct fixture
	{
		store                   Store;
		manual_executor         Effects;
		manual_executor         Decisions;
		projection              Output;
		profile_management_host Host{ Store, Effects, Decisions, Output, core::application_session_id{ 1U } };
		void                    drain()
		{
			Effects.drain();
			Decisions.drain();
		}
		void load()
		{
			Host.reload();
			drain();
			expect(Output.last().Ready, "registry load admits management");
		}
		[[nodiscard]] auto create() -> core::bpr_profile_id
		{
			Host.apply(create_bpr_profile_intent{ "Work" });
			drain();
			return Store.Snapshot.Profiles.back().Profile.Id;
		}
	};

	void test_durable_edit_acceptance_and_failures()
	{
		fixture test;
		test.Host.reload();
		expect(test.Output.last().Busy && !test.Output.last().Ready, "reload projects busy before durable admission");
		test.drain();
		expect(test.Output.last().Ready && test.Output.last().Profiles.size() == 1U, "accepted Default becomes visible after decision completion");
		test.Host.apply(create_bpr_profile_intent{ "Work" });
		expect(test.Output.last().Busy && test.Output.last().Profiles.size() == 1U && !test.Output.last().AcceptedEdit, "create never publishes an optimistic label or edit acknowledgement");
		test.Effects.drain();
		expect(test.Output.last().Profiles.size() == 1U, "worker durable completion cannot update management");
		test.Decisions.drain();
		const auto work = test.Store.Snapshot.Profiles.back().Profile.Id;
		expect(test.Output.last().Profiles.size() == 2U && test.Output.last().AcceptedEdit
				   && test.Output.last().AcceptedEdit->Profile == work && test.Output.Commands.empty(),
			   "durable create acknowledges its exact profile without implicitly opening it");
		test.Host.apply(rename_bpr_profile_intent{ work, "Personal" });
		expect(test.Output.last().Profiles.back().Label == "Work" && !test.Output.last().AcceptedEdit, "rename retains the accepted label until durable completion");
		test.drain();
		expect(test.Output.last().Profiles.back().Profile == work && test.Output.last().Profiles.back().Label == "Personal"
				   && test.Output.last().AcceptedEdit->Operation == profile_management_operation::rename,
			   "rename updates only accepted display data");
		test.Host.apply(rename_bpr_profile_intent{ work, "" });
		expect(test.Output.last().Failure && test.Output.last().Failure->Reason == profile_management_failure::invalid_label
				   && test.Output.last().Profiles.back().Label == "Personal" && !test.Output.last().AcceptedEdit,
			   "rejected label leaves accepted projection intact and emits no candidate-clearing acknowledgement");
		test.Store.Failure = profile_registry_failure::unavailable;
		test.Host.apply(rename_bpr_profile_intent{ work, "Refused candidate" });
		test.drain();
		expect(test.Output.last().Failure && test.Output.last().Failure->Profile == work && !test.Output.last().AcceptedEdit && !test.Output.last().Ready,
			   "persistence failure is scoped and never acknowledges refused candidate text");
		expect(test.Store.Snapshot.Profiles.back().Profile.Label == "Personal", "failed rename leaves durable label unchanged");
		expect(test.Output.last().Profiles.back().Label == "Personal", "failed persistence preserves the last accepted row for candidate retention");
	}

	void test_one_owner_pending_open_and_closing()
	{
		fixture test;
		test.load();
		const auto work = test.create();
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		test.drain();
		const auto initial = test.Output.Commands.back().Context;
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::start && test.Host.complete_open(initial), "already-admitted initial app can bind the first start reservation");
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		const auto context = test.Output.Commands.back().Context;
		const auto count   = test.Output.Commands.size();
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		expect(test.Output.Commands.size() == count && test.Output.last().Failure->Reason == profile_management_failure::busy, "pending startup cannot create another private owner");
		expect(test.Host.complete_open(context), "exact startup completion opens its reserved runtime");
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && test.Output.Commands.back().Context == context,
			   "opening an existing profile activates the same runtime identity");
		test.Host.apply(rename_bpr_profile_intent{ work, "Renamed Work" });
		test.drain();
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		expect(test.Output.Commands.back().Context == context && test.Output.Commands.back().Label == "Renamed Work", "registry synchronization preserves live context and adopts accepted label");
		expect(test.Host.begin_close(context), "closing reserves the runtime until its work drains");
		test.drain();
		expect(!test.Host.complete_open(context, std::unexpected(profile_management_failure::runtime_unavailable)),
			   "duplicate startup failure cannot retire an already-open runtime during close");
		const auto closingCount = test.Output.Commands.size();
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		expect(test.Output.Commands.size() == closingCount && test.Output.last().Failure->Reason == profile_management_failure::busy, "closing cannot race a replacement owner");
		expect(test.Host.complete_close(context), "exact drained close retires its reservation");
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		const auto replacement = test.Output.Commands.back().Context;
		expect(replacement != context && test.Output.Commands.back().Kind == profile_runtime_command_kind::start, "reopening a fully drained profile receives a new context");
		expect(!test.Host.complete_open(context) && !test.Host.complete_close(context), "old completion cannot affect a replacement runtime");
		expect(test.Host.complete_open(replacement), "replacement remains completable after stale callbacks");
		expect(test.Host.begin_close(initial), "initial close queues persistence");
		test.drain();
		expect(test.Host.complete_close(initial), "initial runtime can close independently");
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && test.Output.Commands.back().Context == replacement, "another profile remains live after initial window closes");
	}

	void test_failed_and_foreign_completions()
	{
		fixture test;
		test.load();
		const auto work = test.create();
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		const auto context = test.Output.Commands.back().Context;
		const auto foreign = core::bpr_window_context{ core::default_bpr_profile_id, context.Window, context.Generation };
		expect(!test.Host.complete_open(foreign) && !test.Host.begin_close(foreign) && !test.Host.complete_close(foreign), "wrong-profile callbacks cannot affect the pending owner");
		expect(test.Host.complete_open(context, std::unexpected(profile_management_failure::runtime_unavailable)), "startup failure retires its exact pending reservation");
		expect(test.Output.last().Failure && test.Output.last().Failure->Profile == work && test.Output.last().Profiles.back().Runtime == profile_management_runtime::closed,
			   "failed startup leaves a scoped error and no live owner");
		const auto count = test.Output.Commands.size();
		test.Host.apply(open_bpr_profile_intent{ core::bpr_profile_id{ 99U } });
		test.drain();
		expect(test.Output.Commands.size() == count && test.Output.last().Failure->Reason == profile_management_failure::unknown_profile, "unknown profile cannot produce a fallback start");
	}

	void test_multiple_live_windows_and_exact_retirement()
	{
		fixture test;
		test.load();
		const auto profile = core::default_bpr_profile_id;
		test.Host.apply(open_bpr_profile_intent{ profile });
		test.drain();
		const auto first = test.Output.Commands.back().Context;
		expect(test.Host.current(first) && test.Host.complete_open(first), "opening and open contexts are current");
		test.Host.apply(new_bpr_window_intent{ profile });
		test.drain();
		const auto second = test.Output.Commands.back().Context;
		expect(second.Profile == first.Profile && second.Window != first.Window
				   && test.Host.current(first) && test.Host.current(second),
			   "New Window reserves a distinct current context in the same profile");
		expect(test.Output.last().Profiles.front().Runtime == profile_management_runtime::open, "a live sibling keeps profile status open while another starts");
		expect(test.Host.complete_open(second), "second window opens independently");
		expect(test.Host.focus(first), "native focus updates the core MRU order");
		test.Host.apply(open_bpr_profile_intent{ profile });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && test.Output.Commands.back().Context == first, "ordinary Open activates the focused MRU window without allocation");
		expect(test.Host.begin_close(first) && test.Host.current(first), "pending close preserves live authority");
		test.drain();
		expect(!test.Host.current(first) && test.Host.current(second), "begin close rejects A callbacks while B remains current");
		test.Host.apply(open_bpr_profile_intent{ profile });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && test.Output.Commands.back().Context == second, "ordinary Open skips a closing MRU sibling");
		expect(test.Host.complete_close(first) && test.Host.current(second), "A drainage retires only A");
		test.Host.apply(new_bpr_window_intent{ profile });
		test.drain();
		const auto failed = test.Output.Commands.back().Context;
		expect(test.Host.complete_open(failed, std::unexpected(profile_management_failure::runtime_unavailable)), "failed new-window reservation retires exactly once");
		expect(!test.Host.current(failed) && test.Host.current(second)
				   && test.Output.last().Profiles.front().Runtime == profile_management_runtime::open
				   && test.Output.last().Failure->Operation == profile_management_operation::new_window,
			   "failed new window leaves the sibling open and names its operation");
		auto stale              = second;
		stale.Generation        = core::bpr_window_generation{ second.Generation.value() + 1U };
		auto foreign            = second;
		foreign.Profile         = core::bpr_profile_id{ 99U };
		const auto publications = test.Output.States.size();
		const auto commands     = test.Output.Commands.size();
		for (const auto context : { first, failed, stale, foreign }) {
			expect(!test.Host.current(context) && !test.Host.focus(context) && !test.Host.begin_close(context)
					   && !test.Host.complete_close(context) && !test.Host.complete_open(context),
				   "retired, stale and foreign contexts cannot mutate live siblings");
		}
		expect(test.Output.States.size() == publications && test.Output.Commands.size() == commands, "rejected lifecycle callbacks have no projection or runtime effects");
		expect(test.Store.Saves == 4U, "each allocation and explicit close persists window metadata once");
	}

	/// [[BPR-1342]]. Open Review in New Window is New Window carrying one
	/// destination: it rides to that fresh window's start command and nowhere
	/// else, and an ordinary New Window or Open carries none.
	void test_new_window_carries_its_review_to_the_fresh_window_only()
	{
		fixture test;
		test.load();
		const auto profile = core::default_bpr_profile_id;
		test.Host.apply(open_bpr_profile_intent{ profile });
		test.drain();
		const auto first = test.Output.Commands.back();
		expect(first.Kind == profile_runtime_command_kind::start && !first.Review && test.Host.complete_open(first.Context), "an ordinary open carries no review");
		auto review       = core::window_review_destination{ };
		review.Hostname   = "github.com";
		review.Repository = { "octo-org", "review-repository" };
		review.Number     = core::pull_request_number{ 17U };
		review.Account    = core::github_account_id{ 7U };
		const auto before = test.Output.Commands.size();
		test.Host.apply(new_bpr_window_intent{ profile, review });
		test.drain();
		expect(test.Output.Commands.size() == before + 1U, "one window is asked for");
		const auto carried = test.Output.Commands.back();
		expect(carried.Kind == profile_runtime_command_kind::start && !carried.Restore && carried.Context.Profile == profile && carried.Context.Window != first.Context.Window
				   && carried.Review == review,
			   "the fresh window's start carries the review, in the same profile");
		expect(test.Host.complete_open(carried.Context), "the reviewing window opens like any new window");
		test.Host.apply(new_bpr_window_intent{ profile });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::start && !test.Output.Commands.back().Review, "a plain New Window carries none");
		expect(test.Host.complete_open(test.Output.Commands.back().Context), "and opens");
		test.Host.apply(open_bpr_profile_intent{ profile });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && !test.Output.Commands.back().Review, "activating the profile carries none");
	}

	void test_live_window_cap_and_independent_pending_startups()
	{
		fixture test;
		test.load();
		std::vector<core::bpr_window_context> windows;
		for (std::size_t index = 0U; index < core::maximum_bpr_windows_per_profile; ++index) {
			test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
			test.drain();
			windows.push_back(test.Output.Commands.back().Context);
			expect(test.Host.current(windows.back()), "every bounded pending startup has its own admitted context");
		}
		const auto count = test.Output.Commands.size();
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		test.drain();
		expect(test.Output.Commands.size() == count && test.Output.last().Failure->Reason == profile_management_failure::window_limit,
			   "the thirty-third live reservation fails with the core window cap");
		expect(test.Host.complete_open(windows.back()) && test.Host.complete_open(windows.front()), "out-of-order startup terminals remain independently admissible");
		expect(test.Host.begin_close(windows.front()), "close queues durable admission");
		test.drain();
		const auto drainingCount = test.Output.Commands.size();
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		expect(test.Output.Commands.size() == drainingCount && test.Output.last().Failure->Reason == profile_management_failure::busy,
			   "durably removed record still occupies runtime capacity until exact close drains");
		expect(test.Host.complete_close(windows.front()), "one exact close releases one cap slot");
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		test.drain();
		expect(test.Output.Commands.size() == count + 2U && test.Output.Commands.back().Context.Window.value() > windows.back().Window.value(),
			   "replacement uses a fresh window identity after one slot drains");
		test.Host.shutdown();
		for (const auto context : windows) {
			expect(!test.Host.current(context), "shutdown revokes every sibling context immediately");
			static_cast<void>(test.Host.complete_close(context));
		}
		expect(test.Host.complete_close(test.Output.Commands.back().Context) && test.Host.is_quiescent(), "all closing reservations must drain before host quiescence");
	}

	void test_lifecycle_save_admission_and_refusal()
	{
		fixture test;
		test.load();
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		expect(test.Output.Commands.empty() && test.Output.last().Busy, "start waits for durable lifecycle admission");
		expect(!test.Host.request_quit() && test.Output.last().Failure->Reason == profile_management_failure::busy,
			   "Quit during outstanding allocation does not bypass or cancel admission");
		test.Effects.drain();
		expect(test.Output.Commands.empty() && test.Store.Snapshot.LastWindowId == 1, "committed effect alone cannot start a runtime");
		test.Decisions.drain();
		const auto first = test.Output.Commands.back().Context;
		expect(!test.Output.Commands.back().Restore && test.Host.complete_open(first), "new window starts at Home after decision acceptance");
		test.Store.Failure = profile_registry_failure::unavailable;
		const auto count   = test.Output.Commands.size();
		expect(test.Host.begin_close(first) && test.Host.current(first), "pending close keeps exact live authority usable");
		expect(!test.Host.complete_close(first), "unadmitted close terminal cannot retire runtime");
		test.drain();
		expect(test.Output.Commands.size() == count && test.Host.current(first)
				   && test.Output.last().Failure->Operation == profile_management_operation::close,
			   "failed close save emits no effect and preserves live window");
		test.Store.Failure.reset();
		test.load();
		expect(test.Host.current(first), "reload preserves exact live context without retained duplication");
		test.Store.Failure = profile_registry_failure::unavailable;
		expect(test.Host.request_quit(), "orderly Quit queues one retained-state save");
		test.drain();
		expect(test.Output.Commands.size() == count && test.Host.current(first) && !test.Output.last().Closing,
			   "failed Quit leaves live windows usable and does not enter drain");
		test.Store.Failure.reset();
		test.load();
		expect(test.Host.begin_close(first), "close can retry after reload");
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::close
				   && test.Output.Commands.back().Context == first && !test.Host.current(first)
				   && test.Store.Snapshot.Profiles.front().RetainedWindows.empty(),
			   "accepted close removes only its durable record before runtime teardown");
		expect(test.Host.complete_close(first), "accepted close drains exactly once");
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		test.drain();
		expect(test.Output.Commands.back().Context.Window.value() > first.Window.value(), "closed global identity never repeats");
	}

	void test_restart_restores_every_window_and_quit_order()
	{
		fixture first;
		first.load();
		const auto work = first.create();
		for (const auto profile : { core::default_bpr_profile_id, core::default_bpr_profile_id, work }) {
			first.Host.apply(new_bpr_window_intent{ profile });
			first.drain();
			expect(first.Host.complete_open(first.Output.Commands.back().Context), "each original window starts");
		}
		const auto older = first.Output.Commands[0].Context;
		const auto newer = first.Output.Commands[1].Context;
		const auto other = first.Output.Commands[2].Context;
		expect(first.Host.focus(older), "native focus establishes retained MRU order");
		const auto commands = first.Output.Commands.size();
		expect(first.Host.request_quit() && first.Output.Commands.size() == commands && first.Host.current(older),
			   "Quit retains live authority until durable admission");
		first.drain();
		expect(first.Output.Commands.back().Kind == profile_runtime_command_kind::quit && !first.Host.current(older),
			   "one accepted Quit command authorizes profile drain");
		expect(first.Store.Snapshot.Profiles[0].RetainedWindows[0].Id == newer.Window
				   && first.Store.Snapshot.Profiles[0].RetainedWindows[1].Id == older.Window,
			   "Quit persists least-to-most-recent native focus order");
		for (const auto context : { older, newer, other }) {
			expect(first.Host.complete_close(context), "Quit completes every exact retained context");
		}
		expect(first.Host.is_quiescent(), "orderly Quit drains all windows");
		fixture restarted;
		restarted.Store.Snapshot = first.Store.Snapshot;
		restarted.load();
		restarted.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		expect(restarted.Output.Commands.empty(), "restoration saves fresh generations before runtime effects");
		restarted.drain();
		expect(restarted.Output.Commands.size() == 2 && restarted.Output.Commands[0].Context.Window == newer.Window
				   && restarted.Output.Commands[1].Context.Window == older.Window,
			   "every restored context is emitted in core MRU order");
		for (const auto& command : restarted.Output.Commands) {
			expect(command.Restore && command.Context.Generation.value() == 2 && restarted.Host.complete_open(command.Context),
				   "restored identity receives fresh generation and unavailable-content marker");
		}
		expect(!restarted.Host.current(older) && !restarted.Host.complete_close(newer), "prior-process contexts remain stale");
		restarted.Host.apply(open_bpr_profile_intent{ work });
		restarted.drain();
		expect(restarted.Output.Commands.back().Context.Window == other.Window && restarted.Output.Commands.back().Restore,
			   "independent profile retains its own exact windows");
		expect(restarted.Host.complete_open(restarted.Output.Commands.back().Context), "other profile restored window opens");
		const auto close = restarted.Output.Commands[0].Context;
		expect(restarted.Host.begin_close(close), "one restored window close admitted");
		restarted.drain();
		expect(restarted.Host.complete_close(close), "restored close drains");
		restarted.Host.apply(rename_bpr_profile_intent{ core::default_bpr_profile_id, "Renamed" });
		restarted.drain();
		expect(restarted.Store.Snapshot.Profiles.front().RetainedWindows.size() == 1
				   && restarted.Store.Snapshot.Profiles.front().RetainedWindows.front().Id == older.Window,
			   "registry edit preserves surviving exact durable metadata");
		restarted.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		restarted.drain();
		expect(!restarted.Output.Commands.back().Restore && restarted.Output.Commands.back().Context.Window.value() > other.Window.value(),
			   "new allocation advances global high-water across profiles and restart");
	}

	void test_failed_startup_during_sibling_save_cannot_resurrect()
	{
		fixture test;
		test.load();
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		test.drain();
		const auto first = test.Output.Commands.back().Context;
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		expect(test.Host.complete_open(first, std::unexpected(profile_management_failure::runtime_unavailable)),
			   "first startup fails while sibling candidate still contains its live reservation");
		test.drain();
		const auto second = test.Output.Commands.back().Context;
		expect(test.Host.complete_open(second) && !test.Host.current(first), "sibling admission preserves exact failed-start retirement");
		expect(test.Host.begin_close(second), "surviving sibling close queues");
		test.drain();
		expect(test.Host.complete_close(second), "surviving sibling drains");
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::start
				   && test.Output.Commands.back().Context.Window.value() > second.Window.value()
				   && test.Host.complete_open(test.Output.Commands.back().Context),
			   "activation after sibling close starts a new real window instead of selecting a ghost failed reservation");
	}

	void test_native_focus_survives_pending_save()
	{
		fixture test;
		test.load();
		for (int index = 0; index < 2; ++index) {
			test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
			test.drain();
			expect(test.Host.complete_open(test.Output.Commands.back().Context), "focus fixture opens window");
		}
		const auto first  = test.Output.Commands[0].Context;
		const auto second = test.Output.Commands[1].Context;
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		test.Effects.drain();
		expect(test.Host.focus(first), "native focus remains accepted during pending lifecycle decision");
		test.Decisions.drain();
		expect(test.Host.request_quit(), "Quit captures reconciled native order");
		test.drain();
		const auto& retained = test.Store.Snapshot.Profiles[0].RetainedWindows;
		expect(retained.back().Id == first.Window && retained[0].Id == second.Window,
			   "accepted candidate cannot overwrite native MRU observed during save");
	}

	void test_uncertain_save_reload_and_exact_retry()
	{
		fixture test;
		test.load();
		test.Store.CommitWithoutAcknowledgement = true;
		test.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		test.drain();
		expect(test.Output.Commands.empty() && test.Store.Snapshot.LastWindowId == 1 && !test.Output.last().Ready,
			   "uncertain committed allocation emits no start and requires reload");
		test.Store.CommitWithoutAcknowledgement = false;
		test.load();
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		test.drain();
		const auto context = test.Output.Commands.back().Context;
		expect(context.Window.value() == 1 && context.Generation.value() == 2 && test.Host.complete_open(context),
			   "reload restores uncertain committed identity with fresh generation instead of reusing allocation");
		test.Store.CommitWithoutAcknowledgement = true;
		const auto count                        = test.Output.Commands.size();
		expect(test.Host.begin_close(context), "uncertain close submits");
		test.drain();
		expect(test.Host.current(context) && test.Output.Commands.size() == count && test.Store.Snapshot.Profiles[0].RetainedWindows.empty(),
			   "unacknowledged close commit cannot tear down usable window");
		test.Store.CommitWithoutAcknowledgement = false;
		test.load();
		expect(test.Host.current(context) && test.Host.begin_close(context), "reload preserves exact usable context for close retry");
		test.drain();
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::close && test.Host.complete_close(context),
			   "close retry durably confirms absence before exact teardown");
	}

	void test_duplicate_lifecycle_terminal_cannot_start_again()
	{
		struct replay_decisions final : decision_executor
		{
			std::vector<std::function<void()>> Tasks;
			void                               post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		};
		store                   storage;
		manual_executor         effects;
		replay_decisions        decisions;
		projection              output;
		profile_management_host host{ storage, effects, decisions, output, core::application_session_id{ 1 } };
		host.reload();
		effects.drain();
		decisions.Tasks[0]();
		host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		effects.drain();
		decisions.Tasks[1]();
		const auto first = output.Commands.back().Context;
		expect(host.complete_open(first), "first accepted replay fixture starts");
		host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		decisions.Tasks[1]();
		expect(output.Commands.size() == 1 && host.current(first), "stale terminal cannot satisfy next pending allocation");
		effects.drain();
		decisions.Tasks[2]();
		decisions.Tasks[2]();
		expect(output.Commands.size() == 2 && output.Commands.back().Context.Window != first.Window,
			   "duplicate accepted terminal emits each new runtime exactly once");
	}

	void test_cancelled_lifecycle_and_reentrant_quit()
	{
		fixture cancelled;
		cancelled.load();
		cancelled.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		cancelled.Effects.drain();
		cancelled.Host.shutdown();
		cancelled.Decisions.drain();
		expect(cancelled.Output.Commands.empty() && cancelled.Host.is_quiescent(),
			   "forced shutdown after commit retires pending admission without starting or stranding runtime");
		fixture reentrant;
		reentrant.load();
		bool observed              = false;
		reentrant.Output.OnPublish = [&reentrant, &observed](const auto& aState) {
			if (!observed && aState.Profiles.front().Runtime == profile_management_runtime::opening) {
				observed = true;
				expect(!reentrant.Host.request_quit(), "reentrant Quit cannot interrupt a start command batch");
			}
		};
		reentrant.Host.apply(new_bpr_window_intent{ core::default_bpr_profile_id });
		reentrant.drain();
		expect(observed && reentrant.Output.Commands.size() == 1
				   && reentrant.Host.complete_open(reentrant.Output.Commands.front().Context),
			   "busy reentrant Quit leaves delivered reservation independently completable");
		expect(reentrant.Host.request_quit(), "Quit may retry after command delivery");
		reentrant.drain();
		expect(reentrant.Output.Commands.back().Kind == profile_runtime_command_kind::quit, "retry receives durable Quit command");
		fixture failedLoad;
		failedLoad.Store.Failure = profile_registry_failure::unavailable;
		failedLoad.Host.reload();
		failedLoad.drain();
		expect(!failedLoad.Host.request_quit(), "unaccepted registry cannot claim successful orderly Quit");
		failedLoad.Host.shutdown();
		expect(failedLoad.Host.is_quiescent(), "initial failure can force drain without registry acceptance");
	}

	void test_shutdown_and_reentrant_projection()
	{
		fixture test;
		test.load();
		const auto work = test.create();
		test.Host.apply(open_bpr_profile_intent{ work });
		test.drain();
		const auto opening = test.Output.Commands.back().Context;
		test.Host.apply(create_bpr_profile_intent{ "Cancelled" });
		test.Host.shutdown();
		const auto count = test.Output.Commands.size();
		test.drain();
		expect(!test.Host.complete_open(opening), "late successful startup after shutdown must be disposed by composition");
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		test.drain();
		expect(test.Output.Commands.size() == count && !test.Host.is_quiescent(), "late runtime still reserves its root while closing");
		expect(test.Host.complete_close(opening) && test.Host.is_quiescent(), "quiescence follows exact runtime and registry drainage");
		fixture reentrant;
		reentrant.load();
		reentrant.Output.OnPublish = [&reentrant](const auto& aState) {
			if (!aState.Closing && !aState.Profiles.empty() && aState.Profiles.front().Runtime == profile_management_runtime::opening) {
				reentrant.Host.shutdown();
			}
		};
		reentrant.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		reentrant.drain();
		expect(reentrant.Output.Commands.empty() && reentrant.Host.is_quiescent(), "reentrant shutdown before command delivery cannot start a runtime or strand an undelivered reservation");
	}
}

int main()
{
	test_failed_startup_during_sibling_save_cannot_resurrect();
	test_native_focus_survives_pending_save();
	test_uncertain_save_reload_and_exact_retry();
	test_duplicate_lifecycle_terminal_cannot_start_again();
	test_lifecycle_save_admission_and_refusal();
	test_restart_restores_every_window_and_quit_order();
	test_cancelled_lifecycle_and_reentrant_quit();
	test_durable_edit_acceptance_and_failures();
	test_one_owner_pending_open_and_closing();
	test_failed_and_foreign_completions();
	test_shutdown_and_reentrant_projection();
	test_multiple_live_windows_and_exact_retirement();
	test_new_window_carries_its_review_to_the_fresh_window_only();
	test_live_window_cap_and_independent_pending_startups();
	std::cout << "profile-management-host-tests:pass\n";
	return EXIT_SUCCESS;
}
