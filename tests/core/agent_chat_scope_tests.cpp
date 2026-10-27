/// \file
/// \brief Proves every AI action is explicit and no observation can start one.

#include "bpr/core/agent_chat_scope.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	constexpr auto configured   = core::agent_profile_id{ 1U };
	constexpr auto alternative  = core::agent_profile_id{ 2U };
	constexpr auto mappedHost   = core::git_host_id{ 10U };
	constexpr auto unmappedHost = core::git_host_id{ 11U };

	[[nodiscard]] auto profile(const core::agent_profile_id aId)
		-> core::agent_profile
	{
		return {
			.Id                 = aId,
			.DisplayName        = "harness",
			.Executable         = "/opt/agents/harness",
			.Protocol           = core::agent_protocol::codex_jsonl_exec,
			.Workspace          = core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = { "HOME", "PATH" },
		};
	}

	[[nodiscard]] auto policy() -> std::optional<core::agent_host_policy>
	{
		return core::agent_host_policy{
			.Host    = mappedHost,
			.Allowed = { configured },
		};
	}

	[[nodiscard]] auto session(const core::agent_profile_id aProfile = configured)
		-> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 7U },
			.Profile = aProfile,
			.Binding = {
				.Host        = mappedHost,
				.Repository  = core::repository_id{ 20U },
				.PullRequest = core::pull_request_id{ 30U },
				.BaseOid     = commit(1U),
				.HeadOid     = commit(2U),
				.LeftEpoch   = core::epoch_ordinal{ 1 },
				.RightEpoch  = core::epoch_ordinal{ 2 },
			},
		};
	}

	[[nodiscard]] auto selection() -> core::agent_selection
	{
		const auto comparison = identity<core::file_comparison_digest>(5U);
		return {
			.File      = identity<core::file_identity>(3U),
			.Selection = {
				.Comparison    = comparison,
				.Kind          = core::semantic_selection_kind::line,
				.PreferredLine = {
					.Comparison = comparison,
					.Side       = core::diff_side::right,
					.Line       = 17U,
				},
			},
		};
	}

	[[nodiscard]] auto selected_scope() -> core::agent_chat_scope
	{
		return {
			.Kind      = core::agent_chat_scope_kind::selected_lines,
			.Selection = selection(),
		};
	}

	[[nodiscard]] auto file_scope() -> core::agent_chat_scope
	{
		return {
			.Kind       = core::agent_chat_scope_kind::file,
			.File       = identity<core::file_identity>(3U),
			.Comparison = identity<core::file_comparison_digest>(5U),
		};
	}

	[[nodiscard]] auto scope_of(const core::agent_chat_scope_kind aKind)
		-> core::agent_chat_scope
	{
		switch (aKind) {
		case core::agent_chat_scope_kind::selected_lines:
			return selected_scope();
		case core::agent_chat_scope_kind::file:
			return file_scope();
		case core::agent_chat_scope_kind::pull_request:
		case core::agent_chat_scope_kind::repository:
			return { .Kind = aKind };
		}
		return { };
	}

	[[nodiscard]] auto decide(
		const core::agent_entry_point                 aEntryPoint,
		const std::optional<core::agent_selection>&   aSelection,
		const std::optional<core::agent_host_policy>& aPolicy  = policy(),
		const core::agent_profile_id                  aProfile = configured,
		const core::agent_session&                    aSession = session())
		-> core::agent_action_verdict
	{
		return core::decide_agent_action(
			profile(aProfile),
			aPolicy,
			aSelection,
			aSession,
			core::agent_action_intent{ aEntryPoint });
	}

	/// \brief One row per declared entry point, tied to the declared count.
	struct entry_point_row
	{
		core::agent_entry_point EntryPoint;
		bool                    ActsOnSelection;
		std::string_view        Name;
	};

	constexpr auto entryPoints = std::array{
		entry_point_row{
			core::agent_entry_point::ask_about_selection,
			true,
			"ask_about_selection",
		},
		entry_point_row{
			core::agent_entry_point::analyze_current_file,
			true,
			"analyze_current_file",
		},
		entry_point_row{
			core::agent_entry_point::analyze_latest_upload,
			false,
			"analyze_latest_upload",
		},
		entry_point_row{
			core::agent_entry_point::evaluate_proposed_fix,
			true,
			"evaluate_proposed_fix",
		},
		entry_point_row{
			core::agent_entry_point::workshop_comment,
			false,
			"workshop_comment",
		},
		entry_point_row{
			core::agent_entry_point::run_ai_review,
			false,
			"run_ai_review",
		},
		entry_point_row{
			core::agent_entry_point::open_full_agent_session,
			false,
			"open_full_agent_session",
		},
		entry_point_row{
			core::agent_entry_point::ask_about_conversation,
			false,
			"ask_about_conversation",
		},
	};

	static_assert(
		entryPoints.size() == core::agent_entry_point_count,
		"every declared entry point needs a row here; a ninth entry point "
		"without one fails this count rather than going untested");

	/// \brief Each of the eight is legal only with the intent naming it.
	void test_every_entry_point_is_legal_only_with_its_own_intent()
	{
		for (const auto& row : entryPoints) {
			const auto verdict = decide(row.EntryPoint, selection());
			expect(
				verdict.Permitted
					&& verdict.Refusal == core::agent_action_refusal::none,
				"every declared entry point is legal when the profile, the host "
				"policy, the session, and the selection all admit it");
			expect(
				verdict.EntryPoint == row.EntryPoint,
				"and the verdict names the action it was asked about, so a "
				"permission is about one action rather than about agents in "
				"general");
			expect(
				verdict.Invocation.has_value() && verdict.Invocation->Permitted,
				"the host-policy half is carried whole rather than restated");
			expect(
				core::stable_name(row.EntryPoint) == row.Name,
				"the stable name is the documented one");
			expect(
				core::acts_on_selection(row.EntryPoint) == row.ActsOnSelection,
				"and what the action acts on is the documented answer");

			for (const auto& other : entryPoints) {
				const auto crossed = decide(other.EntryPoint, selection());
				const auto authorizesRow
					= crossed.Permitted && crossed.EntryPoint == row.EntryPoint;
				expect(
					authorizesRow == (other.EntryPoint == row.EntryPoint),
					"a permission obtained for one action never authorizes a "
					"different one");
			}
		}
	}

	/// \brief An action that acts on a selection is refused without one.
	void test_an_action_that_needs_a_selection_is_refused_without_one()
	{
		auto refused   = std::size_t{ 0U };
		auto permitted = std::size_t{ 0U };
		for (const auto& row : entryPoints) {
			const auto verdict = decide(row.EntryPoint, std::nullopt);
			expect(
				verdict.Permitted == !row.ActsOnSelection,
				"an action with no subject without a selection is refused, and "
				"one whose subject is the session's own binding is not");
			expect(
				verdict.EntryPoint == row.EntryPoint,
				"the refusal names the action too");
			if (row.ActsOnSelection) {
				expect(
					verdict.Refusal
						== core::agent_action_refusal::selection_required,
					"and it says the selection is what is missing, rather than "
					"refusing without a reason a reviewer could act on");
				++refused;
			} else {
				++permitted;
			}

			// A malformed selection is not a selection. Otherwise "there is a
			// selection" would be satisfied by a value nobody could resolve.
			auto malformed = selection();
			malformed.Selection.PreferredLine.Comparison
				= identity<core::file_comparison_digest>(9U);
			expect(
				decide(row.EntryPoint, malformed).Permitted
					== !row.ActsOnSelection,
				"a selection whose preferred line belongs to another comparison "
				"is refused exactly as an absent one is");
		}
		expect(
			refused == 3U && permitted == 5U,
			"three of the eight act on the reviewer's selection; four take "
			"their subject from the session's binding and one from the "
			"conversation the reviewer chose");
	}

	/// \brief An entry point nobody declared is refused before anything else.
	void test_an_undeclared_entry_point_is_refused_first()
	{
		for (const auto raw : { core::agent_entry_point_count, std::size_t{ 200U } }) {
			const auto undeclared = static_cast<core::agent_entry_point>(raw);
			expect(
				!core::declared(undeclared),
				"an enumerator past the declared count is not an entry point");
			const auto verdict = decide(undeclared, selection());
			expect(
				!verdict.Permitted
					&& verdict.Refusal
						== core::agent_action_refusal::unknown_entry_point,
				"an action nobody enumerated is refused by name");
			expect(
				!verdict.Invocation.has_value(),
				"and refused before the host policy was consulted, so a cast "
				"enumerator cannot reach a branch that could permit it");
			expect(
				core::stable_name(undeclared).empty(),
				"an undeclared entry point has no stable name to log");
		}

		for (const auto& row : entryPoints) {
			expect(
				core::declared(row.EntryPoint),
				"every documented entry point is declared, so the refusal above "
				"is about the cast and not about the check refusing everything");
		}
	}

	/// \brief The host policy and the session still decide what they decide.
	void test_the_policy_and_the_session_keep_their_refusals()
	{
		const auto unmapped = decide(
			core::agent_entry_point::run_ai_review,
			selection(),
			core::policy_for_host(
				std::vector<core::agent_host_policy>{ *policy() }, unmappedHost));
		expect(
			!unmapped.Permitted
				&& unmapped.Refusal == core::agent_action_refusal::profile_refused
				&& unmapped.Invocation.has_value()
				&& unmapped.Invocation->Refusal
					== core::agent_invocation_refusal::host_not_mapped,
			"an unmapped host permits no action, and the verdict carries the "
			"invocation refusal that says why rather than inventing a second "
			"vocabulary for it");

		const auto forbidden = decide(
			core::agent_entry_point::run_ai_review,
			selection(),
			policy(),
			alternative,
			session(alternative));
		expect(
			!forbidden.Permitted
				&& forbidden.Refusal
					== core::agent_action_refusal::profile_refused
				&& forbidden.Invocation.has_value()
				&& forbidden.Invocation->Refusal
					== core::agent_invocation_refusal::profile_forbidden_on_host
				&& forbidden.Invocation->RefusedBy == std::optional{ mappedHost },
			"a profile the host forbids is refused for every action, and the "
			"host that forbade it is named");

		auto unusable    = session();
		unusable.Binding = { };
		expect(
			decide(
				core::agent_entry_point::run_ai_review, selection(), policy(),
				configured, unusable)
					.Refusal
				== core::agent_action_refusal::session_invalid,
			"an action inside a session pinned to nothing is refused");

		expect(
			decide(
				core::agent_entry_point::run_ai_review, selection(), policy(),
				configured, session(alternative))
					.Refusal
				== core::agent_action_refusal::session_profile_mismatch,
			"and an action that would switch harness mid-session is refused by "
			"name rather than quietly answering in the other tool");
	}

	/// \brief One row per declared observation, tied to the declared count.
	constexpr auto observations = std::array{
		core::agent_observation::refresh,
		core::agent_observation::navigation,
		core::agent_observation::comment_arrival,
		core::agent_observation::revision_update,
	};

	static_assert(
		observations.size() == core::agent_observation_count,
		"every declared observation needs a row here; a fifth without one fails "
		"this count rather than going unswept");

	/// \brief Refresh, navigation, comment arrival, and revision update invoke nothing.
	void test_no_observation_can_produce_an_invocation()
	{
		// Read off the types first. A sweep alone would prove only that the
		// four values nobody wired up today produce nothing today.
		using record = core::agent_observation_record;
		// The binding is named only inside `decltype`, an unevaluated context;
		// Clang and GCC count that as a use and MSVC does not (C4189 under
		// warnings-as-errors), so the attribute says so for all three
		// (BPR-0512).
		[[maybe_unused]] const auto& [observation] = record{ };
		static_assert(
			std::is_same_v<decltype(observation), const core::agent_observation>);
		static_assert(
			sizeof(record) == sizeof(core::agent_observation),
			"agent_observation_record gained a field. If it can hold an entry "
			"point, an intent, or a verdict, say here why that is not an "
			"invocation waiting for somebody to notice it is free.");

		static_assert(
			!std::is_default_constructible_v<core::agent_action_intent>,
			"there is no default intent, so no caller can mean 'some AI action'");
		static_assert(
			std::is_constructible_v<
				core::agent_action_intent, core::agent_entry_point>,
			"an intent is obtained by naming an entry point");
		static_assert(
			!std::is_convertible_v<
				core::agent_entry_point, core::agent_action_intent>,
			"and only by writing the intent out, never by conversion at a call "
			"site");
		static_assert(
			!std::is_constructible_v<
				core::agent_action_intent, core::agent_observation>,
			"an observation is not an intent");
		static_assert(
			!std::is_constructible_v<
				core::agent_action_intent, core::agent_observation_record>,
			"and neither is a record that one happened");

		// No function in this header takes an observation and returns an
		// invocation. The decision refuses the substitution at the call site.
		using decision = decltype(&core::decide_agent_action);
		static_assert(
			std::is_invocable_r_v<
				core::agent_action_verdict,
				decision,
				const core::agent_profile&,
				const std::optional<core::agent_host_policy>&,
				const std::optional<core::agent_selection>&,
				const core::agent_session&,
				const core::agent_action_intent&>);
		static_assert(
			!std::is_invocable_v<
				decision,
				const core::agent_profile&,
				const std::optional<core::agent_host_policy>&,
				const std::optional<core::agent_selection>&,
				const core::agent_session&,
				core::agent_observation>,
			"an observation cannot be passed where an intent is required");
		static_assert(
			!std::is_invocable_v<
				decision,
				const core::agent_profile&,
				const std::optional<core::agent_host_policy>&,
				const std::optional<core::agent_selection>&,
				const core::agent_session&,
				core::agent_observation_record>,
			"and neither can a record of one");

		for (const auto value : observations) {
			const auto noted = core::note_agent_observation(value);
			expect(
				noted == record{ .Observation = value },
				"an observation records exactly which observation it was, and "
				"nothing else");
			expect(
				!core::intent_for(noted).has_value(),
				"and carries no intent, so it authorizes zero invocations");
			expect(
				!core::stable_name(value).empty(),
				"every declared observation has a stable name");
		}
	}

	/// \brief A chat starts at the reviewer's own selection.
	void test_a_chat_starts_at_the_reviewers_selection()
	{
		const auto started = core::begin_agent_chat(selection());
		expect(started.has_value(), "a well-formed selection starts a chat");
		expect(
			started->Scope.Kind == core::agent_chat_scope_kind::selected_lines
				&& started->Scope.Selection == std::optional{ selection() },
			"at the selection, in the identities the reviewer was pointing at");
		expect(
			!started->Scope.File && !started->Scope.Comparison,
			"and nothing wider is engaged, so the first turn sends the lines "
			"and not the file they are in");
		expect(
			!started->WidenedFrom,
			"a chat that has not been widened records no prior scope");

		auto malformed = selection();
		malformed.File = { };
		expect(
			!core::begin_agent_chat(malformed).has_value(),
			"a chat does not start at a malformed selection, which is what it "
			"would otherwise widen from");
	}

	constexpr auto scopeKinds = std::array{
		core::agent_chat_scope_kind::selected_lines,
		core::agent_chat_scope_kind::file,
		core::agent_chat_scope_kind::pull_request,
		core::agent_chat_scope_kind::repository,
	};

	static_assert(
		scopeKinds.size() == core::agent_chat_scope_kind_count,
		"every declared scope needs a row here; a fifth without one fails this "
		"count");

	/// \brief A turn with no widening intent stays exactly where it was.
	void test_a_turn_without_a_widening_intent_stays_where_it_is()
	{
		for (const auto kind : scopeKinds) {
			const auto state = core::agent_chat_state{ .Scope = scope_of(kind) };
			expect(
				core::valid(state.Scope),
				"the fixture for every scope is well formed, so the assertions "
				"below are about the turn and not about a refused state");
			auto carried = state;
			for (auto turn = std::size_t{ 0U }; turn < 3U; ++turn) {
				const auto next = core::next_agent_chat_turn(
					carried, std::nullopt);
				expect(
					!next.Widened
						&& next.Refusal == core::agent_scope_refusal::none,
					"a turn that carried no widening intent widens nothing");
				expect(
					next.State == carried,
					"and returns the whole state unchanged, so neither the "
					"scope nor the record of how it got there drifts across "
					"turns");
				carried = next.State;
			}
			expect(
				carried == state,
				"three turns of navigation, scrolling, and asking questions "
				"leave the scope exactly where the reviewer put it");
		}

		// The case the RFC names outright: a chat at file scope stays at file
		// scope, rather than following the reviewer into the pull request.
		const auto atFile = core::agent_chat_state{
			.Scope       = file_scope(),
			.WidenedFrom = selected_scope(),
		};
		const auto next = core::next_agent_chat_turn(atFile, std::nullopt);
		expect(
			next.State.Scope.Kind == core::agent_chat_scope_kind::file
				&& next.State.Scope.File == file_scope().File
				&& next.State.WidenedFrom == std::optional{ selected_scope() },
			"a chat turn at file scope with no widening intent stays at file "
			"scope and keeps saying where it came from");
	}

	/// \brief Widening is explicit, one-way, and records the scope it came from.
	void test_widening_is_explicit_and_records_the_prior_scope()
	{
		const auto started = core::begin_agent_chat(selection());
		expect(started.has_value(), "the chat starts");

		const auto widened = core::next_agent_chat_turn(
			*started,
			core::agent_scope_widening_intent{
				core::agent_chat_scope_kind::file });
		expect(
			widened.Widened
				&& widened.Refusal == core::agent_scope_refusal::none
				&& widened.State.Scope.Kind == core::agent_chat_scope_kind::file,
			"an explicit context action widens the chat to the file");
		expect(
			widened.State.Scope.File == std::optional{ selection().File }
				&& widened.State.Scope.Comparison
					== std::optional{ selection().Selection.Comparison },
			"and states the file in the identities the selection already "
			"carried, rather than re-deriving one");
		expect(
			widened.State.WidenedFrom == std::optional{ started->Scope },
			"the widened turn records the scope it came from, so the drawer "
			"can say the agent is seeing a whole file because of a gesture and "
			"name the eleven lines it started at");
		expect(
			!widened.State.Scope.Selection,
			"the file scope is a file, not a file plus the lines it was "
			"widened from wearing the same coat");

		const auto toPullRequest = core::next_agent_chat_turn(
			widened.State,
			core::agent_scope_widening_intent{
				core::agent_chat_scope_kind::pull_request });
		expect(
			toPullRequest.Widened
				&& toPullRequest.State.Scope.Kind
					== core::agent_chat_scope_kind::pull_request
				&& toPullRequest.State.WidenedFrom
					== std::optional{ widened.State.Scope },
			"widening again records the scope it came from, which is the file "
			"and not the selection two steps back");
		expect(
			!toPullRequest.State.Scope.File
				&& !toPullRequest.State.Scope.Comparison
				&& !toPullRequest.State.Scope.Selection,
			"a pull-request scope names nothing of its own: the session's "
			"binding already says which pull request");

		const auto toRepository = core::next_agent_chat_turn(
			toPullRequest.State,
			core::agent_scope_widening_intent{
				core::agent_chat_scope_kind::repository });
		expect(
			toRepository.Widened
				&& toRepository.State.WidenedFrom
					== std::optional{ toPullRequest.State.Scope },
			"and once more to the repository");
	}

	/// \brief Everything that is not a widening leaves the scope alone.
	void test_a_refused_widening_changes_nothing()
	{
		for (const auto from : scopeKinds) {
			const auto state = core::agent_chat_state{ .Scope = scope_of(from) };
			for (const auto target : scopeKinds) {
				const auto turn = core::next_agent_chat_turn(
					state, core::agent_scope_widening_intent{ target });
				expect(
					turn.Widened == core::wider(target, from),
					"a scope widens exactly when the target is wider than the "
					"one the chat is at");
				if (turn.Widened) {
					expect(
						turn.State.Scope.Kind == target
							&& turn.State.WidenedFrom
								== std::optional{ state.Scope },
						"a widening lands at the target and records the prior "
						"scope");
					continue;
				}
				expect(
					turn.Refusal == core::agent_scope_refusal::not_a_widening,
					"the same scope and a narrower one are refused by name: a "
					"chat goes back to a selection by starting at one");
				expect(
					turn.State == state,
					"and the refused turn returns the state it was given, so a "
					"caller that ignores the refusal still does not widen");
			}
		}

		const auto atFile     = core::agent_chat_state{ .Scope = file_scope() };
		const auto undeclared = core::next_agent_chat_turn(
			atFile,
			core::agent_scope_widening_intent{
				static_cast<core::agent_chat_scope_kind>(
					core::agent_chat_scope_kind_count) });
		expect(
			!undeclared.Widened
				&& undeclared.Refusal == core::agent_scope_refusal::unknown_scope
				&& undeclared.State == atFile,
			"a scope nobody declared is refused by name and changes nothing");

		auto broken                  = core::agent_chat_state{ .Scope = selected_scope() };
		broken.Scope.Selection->File = { };
		const auto refused           = core::next_agent_chat_turn(
			broken,
			core::agent_scope_widening_intent{
				core::agent_chat_scope_kind::repository });
		expect(
			!refused.Widened
				&& refused.Refusal == core::agent_scope_refusal::state_invalid
				&& refused.State == broken,
			"a malformed scope is not widened into a well-formed one");

		auto brokenRecord        = core::agent_chat_state{ .Scope = file_scope() };
		brokenRecord.WidenedFrom = core::agent_chat_scope{
			.Kind = core::agent_chat_scope_kind::pull_request,
			.File = identity<core::file_identity>(3U),
		};
		expect(
			core::next_agent_chat_turn(brokenRecord, std::nullopt).Refusal
				== core::agent_scope_refusal::state_invalid,
			"and a malformed record of where the chat came from is refused too, "
			"rather than being carried forward as evidence of a gesture");
	}

	/// \brief A scope is domain identity, and there is no field a coordinate fits.
	void test_a_scope_names_identities_and_never_a_coordinate()
	{
		using scope              = core::agent_chat_scope;
		const auto& [kind,
					 chosen,
					 file,
					 comparison] = scope{ };
		static_assert(
			std::is_same_v<decltype(kind), const core::agent_chat_scope_kind>);
		static_assert(std::is_same_v<
					  decltype(chosen),
					  const std::optional<core::agent_selection>>);
		static_assert(std::is_same_v<
					  decltype(file), const std::optional<core::file_identity>>);
		static_assert(std::is_same_v<
					  decltype(comparison),
					  const std::optional<core::file_comparison_digest>>);
		expect(
			kind == core::agent_chat_scope_kind::selected_lines && !chosen
				&& !file && !comparison,
			"a default scope is the narrowest one with nothing engaged, so the "
			"widest scope is never what a half-filled value means");

		using selected                                        = core::agent_selection;
		[[maybe_unused]] const auto& [selectedFile, semantic] = selected{ };
		static_assert(
			std::is_same_v<decltype(selectedFile), const core::file_identity>);
		static_assert(
			std::is_same_v<decltype(semantic), const core::semantic_selection>);
		static_assert(
			sizeof(selected)
				== sizeof(core::file_identity) + sizeof(core::semantic_selection),
			"agent_selection gained or lost a field. If it gained one, say here "
			"why a y-position, a viewport offset, a widget row, or a ribbon "
			"path cannot occupy it.");
		expect(
			!selectedFile.valid() && !semantic.Comparison.valid(),
			"a selection is a file identity and an RFC-BPR-0004 semantic "
			"selection - no coordinate, and nothing measured");
	}

	/// \brief Exactly one shape is engaged per scope, and the rest are refused.
	void test_a_scope_engages_exactly_what_its_kind_means()
	{
		for (const auto kind : scopeKinds) {
			expect(
				core::valid(scope_of(kind)) && core::declared(kind)
					&& !core::stable_name(kind).empty(),
				"every declared scope has a well-formed shape and a stable name");
		}

		auto doubled       = selected_scope();
		doubled.File       = identity<core::file_identity>(3U);
		doubled.Comparison = identity<core::file_comparison_digest>(5U);
		expect(
			!core::valid(doubled),
			"a selection scope that also names a file is refused: one turn is "
			"about one thing");

		auto narrowed      = file_scope();
		narrowed.Selection = selection();
		expect(
			!core::valid(narrowed),
			"and a file scope carrying the lines it was widened from is refused "
			"for the same reason");

		auto review = core::agent_chat_scope{
			.Kind = core::agent_chat_scope_kind::pull_request,
			.File = identity<core::file_identity>(3U),
		};
		expect(
			!core::valid(review),
			"a pull-request scope naming a file of its own is refused; the "
			"session's binding is what says which review this is");

		auto anonymous = file_scope();
		anonymous.File = { };
		expect(
			!core::valid(anonymous),
			"a file scope with no file identity is refused rather than meaning "
			"whichever file is on screen");

		expect(
			!core::valid(core::agent_chat_scope{
				.Kind = static_cast<core::agent_chat_scope_kind>(
					core::agent_chat_scope_kind_count) }),
			"a scope nobody declared is not a scope");

		auto grouped           = selection();
		grouped.Selection.Kind = core::semantic_selection_kind::group;
		expect(
			!core::valid(grouped),
			"a group selection with neither a segment nor a group identity is "
			"refused");
		grouped.Selection.Segment = identity<core::alignment_segment_id>(6U);
		grouped.Selection.Group   = identity<core::change_group_id>(7U);
		expect(
			core::valid(grouped),
			"and holds once it names both, so the refusal above is about the "
			"missing identities");
	}
	/// \brief The six operations are six, are named, and are named stably.
	///
	/// \details The count, the names, and the round trip are pinned together
	/// because each one alone is escapable: a seventh operation with no name
	/// passes a name test, a renamed operation passes a count test, and a name
	/// that no longer maps back to its operation passes both.
	void test_the_workshop_offers_exactly_the_six_documented_operations()
	{
		constexpr auto expected
			= std::array<std::string_view, core::workshop_operation_count>{
				  "clarify",
				  "shorten",
				  "make_actionable",
				  "phrase_as_a_question",
				  "make_more_direct",
				  "check_the_premise",
			  };

		expect(
			core::workshop_operation_count == 6U,
			"docs/ux/ai-experience.md promises six comment-workshop "
			"operations and the tree declares six");

		for (auto index = std::size_t{ 0U }; index < expected.size(); ++index) {
			const auto operation
				= static_cast<core::workshop_operation>(index);
			expect(
				core::declared(operation),
				"every value below the count is a declared operation");
			expect(
				core::stable_name(operation) == expected[index],
				"each operation carries the documented name, in the "
				"documented order");
			// The round trip: the name maps back to exactly one operation,
			// and to this one.
			auto matches = std::size_t{ 0U };
			auto found   = core::workshop_operation{ };
			for (auto other = std::size_t{ 0U }; other < expected.size();
				 ++other) {
				const auto candidate
					= static_cast<core::workshop_operation>(other);
				if (core::stable_name(candidate)
					== core::stable_name(operation)) {
					++matches;
					found = candidate;
				}
			}
			expect(
				matches == 1U && found == operation,
				"a stable name names one operation and names it back");
		}

		expect(
			!core::declared(static_cast<core::workshop_operation>(
				core::workshop_operation_count))
				&& core::stable_name(static_cast<core::workshop_operation>(
										 core::workshop_operation_count))
					   .empty(),
			"a seventh value is not an operation and has no name to be "
			"mistaken for one");
	}

	/// \brief An operation rides one entry point and no other.
	void test_an_operation_is_carried_only_by_the_workshop_entry_point()
	{
		constexpr auto asked = core::workshop_operation::make_actionable;

		expect(
			core::workshop_operation_for(
				core::agent_entry_point::workshop_comment, asked)
				== std::optional{ asked },
			"the workshop entry point carries the operation it was asked "
			"for");
		expect(
			!core::workshop_operation_for(
				 core::agent_entry_point::workshop_comment, std::nullopt)
				 .has_value(),
			"and carries none when none was asked for");

		for (auto index = std::size_t{ 0U };
			 index < core::agent_entry_point_count;
			 ++index) {
			const auto entry = static_cast<core::agent_entry_point>(index);
			if (entry == core::agent_entry_point::workshop_comment) {
				continue;
			}
			expect(
				!core::workshop_operation_for(entry, asked).has_value(),
				"no other entry point carries an operation, so the six are "
				"not six new entry points wearing one name");
		}

		expect(
			!core::workshop_operation_for(
				 core::agent_entry_point::workshop_comment,
				 static_cast<core::workshop_operation>(
					 core::workshop_operation_count))
				 .has_value(),
			"and an undeclared operation is carried by nothing");
	}
} // namespace

auto main() -> int
{
	test_every_entry_point_is_legal_only_with_its_own_intent();
	test_an_action_that_needs_a_selection_is_refused_without_one();
	test_an_undeclared_entry_point_is_refused_first();
	test_the_policy_and_the_session_keep_their_refusals();
	test_no_observation_can_produce_an_invocation();
	test_a_chat_starts_at_the_reviewers_selection();
	test_a_turn_without_a_widening_intent_stays_where_it_is();
	test_widening_is_explicit_and_records_the_prior_scope();
	test_a_refused_widening_changes_nothing();
	test_a_scope_names_identities_and_never_a_coordinate();
	test_a_scope_engages_exactly_what_its_kind_means();
	test_the_workshop_offers_exactly_the_six_documented_operations();
	test_an_operation_is_carried_only_by_the_workshop_entry_point();
	std::cout << "agent-chat-scope-tests:pass\n";
	return EXIT_SUCCESS;
}
