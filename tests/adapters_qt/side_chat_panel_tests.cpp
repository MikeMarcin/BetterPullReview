/// \file
/// \brief Proves the side chat shows what it was given and starts nothing.

#include "bpr/qt/side_chat_panel.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>

#include <array>
#include <cstdlib>
#include <iostream>
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

	class recorder final
	{
	public:
		[[nodiscard]] auto sink() -> qt::side_chat_intent_dispatch
		{
			return [this](qt::side_chat_intent aIntent) {
				Intents.push_back(std::move(aIntent));
			};
		}

		template <typename Alternative>
		[[nodiscard]] auto count() const -> std::size_t
		{
			auto found = std::size_t{ 0U };
			for (const auto& intent : Intents) {
				found += std::holds_alternative<Alternative>(intent) ? 1U : 0U;
			}
			return found;
		}

		[[nodiscard]] auto total() const -> std::size_t
		{
			return Intents.size();
		}

		std::vector<qt::side_chat_intent> Intents;
	};

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 7U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = {
				.Host        = core::git_host_id{ 1U },
				.Repository  = core::repository_id{ 2U },
				.PullRequest = core::pull_request_id{ 3U },
				.BaseOid     = commit(4U),
				.HeadOid     = commit(5U),
				.LeftEpoch   = core::epoch_ordinal{ 6U },
				.RightEpoch  = core::epoch_ordinal{ 7U },
			},
		};
	}

	/// \brief The revision the reviewer is looking at, which is not the one
	/// this session was asked about.
	[[nodiscard]] auto observed() -> core::agent_revision_binding
	{
		auto moved    = session().Binding;
		moved.HeadOid = commit(90U);
		return moved;
	}

	[[nodiscard]] auto selection() -> core::agent_chat_scope
	{
		return { .Kind = core::agent_chat_scope_kind::selected_lines };
	}

	/// \brief One current session, at the reviewer's own selection.
	[[nodiscard]] auto current_projection() -> qt::side_chat_projection
	{
		return {
			.Session       = session(),
			.Continuations = core::offer_agent_continuations(
				session(), session().Binding),
			.Chat           = { .Scope = selection() },
			.PrivateContext = { .Session = session().Id },
			.EntryPoint     = core::agent_entry_point::ask_about_selection,
			.Provider       = "Codex",
			.GitHost        = "github.com",
			.Workspace      = "/tmp/bpr-review-workspace",
			.Chips          = { },
		};
	}

	/// \brief The same session after the pull request's head moved.
	[[nodiscard]] auto stale_projection() -> qt::side_chat_projection
	{
		auto stale          = current_projection();
		stale.Continuations = core::offer_agent_continuations(
			session(), observed());
		return stale;
	}

	struct reading
	{
		std::string_view Name;
		QString          Value;
	};

	/// \brief Every value the panel discloses, read off the panel itself.
	[[nodiscard]] auto read_all(const qt::side_chat_panel& aPanel)
		-> std::array<reading, 10U>
	{
		return { {
			{ "provider", aPanel.provider_text() },
			{ "git host", aPanel.git_host_text() },
			{ "workspace", aPanel.workspace_text() },
			{ "epoch pair", aPanel.epoch_pair_text() },
			{ "head OID", aPanel.head_oid_text() },
			{ "session state", aPanel.session_state_text() },
			{ "scope", aPanel.scope_text() },
			{ "context chips", aPanel.chips_text() },
			{ "private context", aPanel.private_context_text() },
			{ "continuations", aPanel.continuations_text() },
		} };
	}

	struct disclosed_field
	{
		/// What the projection changes.
		std::string_view Name;
		/// The one presented value that must move when it does.
		std::string_view Field;
		void (*Mutate)(qt::side_chat_projection&);
	};

	/// \brief Every disclosed field, swept rather than sampled.
	///
	/// \details Each entry changes exactly one thing in the projection. The
	/// sweep asserts the named field moved *and* that nothing else did, which
	/// is what "shown, not summarized" means: a panel that rendered these
	/// into one sentence would fail every row but its own.
	constexpr auto disclosed_fields = std::array{
		disclosed_field{
			"provider",
			"provider",
			[](qt::side_chat_projection& aShown) {
				aShown.Provider = "Claude Code";
			} },
		disclosed_field{
			"git host",
			"git host",
			[](qt::side_chat_projection& aShown) {
				aShown.GitHost = "ghe.example.invalid";
			} },
		disclosed_field{
			"workspace",
			"workspace",
			[](qt::side_chat_projection& aShown) {
				aShown.Workspace = "/tmp/bpr-other-workspace";
			} },
		disclosed_field{
			"left epoch",
			"epoch pair",
			[](qt::side_chat_projection& aShown) {
				aShown.Session.Binding.LeftEpoch = core::epoch_ordinal{ 11U };
			} },
		disclosed_field{
			"right epoch",
			"epoch pair",
			[](qt::side_chat_projection& aShown) {
				aShown.Session.Binding.RightEpoch = core::epoch_ordinal{ 12U };
			} },
		disclosed_field{
			"head OID",
			"head OID",
			[](qt::side_chat_projection& aShown) {
				aShown.Session.Binding.HeadOid = commit(31U);
			} },
		disclosed_field{
			"staleness",
			"session state",
			[](qt::side_chat_projection& aShown) {
				aShown.Continuations.Status.Staleness
					= core::agent_session_staleness::head_moved;
			} },
		disclosed_field{
			"current flag",
			"session state",
			[](qt::side_chat_projection& aShown) {
				aShown.Continuations.Status.Current = false;
			} },
		disclosed_field{
			"continuation requirement",
			"continuations",
			[](qt::side_chat_projection& aShown) {
				aShown.Continuations.Required = true;
			} },
		disclosed_field{
			"scope kind",
			"scope",
			[](qt::side_chat_projection& aShown) {
				aShown.Chat.Scope.Kind
					= core::agent_chat_scope_kind::repository;
			} },
		disclosed_field{
			"widened from",
			"scope",
			[](qt::side_chat_projection& aShown) {
				aShown.Chat.WidenedFrom = selection();
			} },
		disclosed_field{
			"context chips",
			"context chips",
			[](qt::side_chat_projection& aShown) {
				aShown.Chips.push_back(qt::side_chat_context_chip{
					.Scope = core::agent_chat_scope_kind::file,
					.Label = "src/core/agent_chat_scope.hpp",
				});
			} },
		disclosed_field{
			"private note opt-in",
			"private context",
			[](qt::side_chat_projection& aShown) {
				aShown.PrivateContext.Notes = true;
			} },
		disclosed_field{
			"private draft opt-in",
			"private context",
			[](qt::side_chat_projection& aShown) {
				aShown.PrivateContext.Drafts = true;
			} },
	};

	/// \brief Each disclosed fact is its own field, and moves on its own.
	void test_every_disclosed_field_is_presented_separately()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };
		panel.present(current_projection());
		const auto baseline = read_all(panel);

		const auto head = core::canonical_hex(session().Binding.HeadOid);
		expect(
			baseline[4].Value == QString::fromUtf8(head.view())
				&& baseline[4].Value.size() == 40,
			"the head OID field shows the exact head commit, whole");
		expect(
			baseline[5].Value.contains("current"),
			"the session state field shows a current session as current");

		for (const auto& field : disclosed_fields) {
			auto mutated = current_projection();
			field.Mutate(mutated);
			panel.present(mutated);
			const auto after = read_all(panel);
			for (auto index = std::size_t{ 0U }; index < after.size();
				 ++index) {
				const auto moved = after[index].Value != baseline[index].Value;
				const auto owns  = after[index].Name == field.Field;
				expect(
					moved == owns,
					std::string{ "changing " } + std::string{ field.Name }
						+ " must change the presented " + std::string{ field.Field }
						+ " and nothing else, but the "
						+ std::string{ after[index].Name } + " field "
						+ (moved ? "also moved" : "did not move"));
			}
			panel.present(current_projection());
			expect(
				read_all(panel)[0].Value == baseline[0].Value,
				"and presenting the original projection restores it");
		}

		expect(
			recorded.total() == 0U,
			"presenting a projection emits no intent at all");
		expect(
			panel.statistics().Presentations
				== (disclosed_fields.size() * 2U) + 1U,
			"every presentation is counted");
	}

	/// \brief Nothing the reviewer does moves the display.
	void test_nothing_the_reviewer_does_changes_the_display()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };
		panel.present(current_projection());
		const auto baseline = read_all(panel);

		panel.composer()->setPlainText(QStringLiteral("why is this locked?"));
		panel.send_action()->trigger();
		panel.scope_selector()->setCurrentIndex(3);
		panel.add_context_action()->trigger();
		panel.note_optin()->setChecked(true);
		panel.draft_optin()->setChecked(true);

		const auto after = read_all(panel);
		for (auto index = std::size_t{ 0U }; index < after.size(); ++index) {
			expect(
				after[index].Value == baseline[index].Value,
				std::string{ "the " } + std::string{ after[index].Name }
					+ " field moves only when a projection is presented");
		}
		expect(
			recorded.total() == 4U,
			"and every one of those gestures left as an intent instead");
	}

	/// \brief Send, scope, opt-in, and continuation are four typed intents.
	void test_every_gesture_leaves_as_its_own_typed_intent()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };
		panel.present(current_projection());

		panel.composer()->setPlainText(QStringLiteral("explain this branch"));
		panel.send_action()->trigger();
		expect(
			recorded.count<qt::send_chat_message_intent>() == 1U,
			"sending emits exactly one send intent");
		const auto& sent
			= std::get<qt::send_chat_message_intent>(recorded.Intents.back());
		expect(
			sent.Session == session().Id
				&& sent.Action
					== core::agent_action_intent{
						core::agent_entry_point::ask_about_selection }
				&& sent.Scope == selection() && sent.Text == "explain this branch",
			"and it names the session, the explicit action, the scope shown, "
			"and the text as typed");

		panel.scope_selector()->setCurrentIndex(2);
		panel.add_context_action()->trigger();
		expect(
			recorded.count<qt::widen_chat_scope_intent>() == 1U,
			"asking for wider context emits exactly one widening intent");
		expect(
			std::get<qt::widen_chat_scope_intent>(recorded.Intents.back())
					.Widening
				== core::agent_scope_widening_intent{
					core::agent_chat_scope_kind::pull_request },
			"naming the scope the reviewer selected");

		panel.note_optin()->setChecked(true);
		expect(
			recorded.count<qt::chat_private_context_intent>() == 1U
				&& std::get<qt::chat_private_context_intent>(
					   recorded.Intents.back())
						.OptIn
					== core::agent_private_context_optin{
						.Session = session().Id,
						.Notes   = true,
						.Drafts  = false,
					},
			"opting private notes in is one intent keyed to this session, "
			"with drafts still separately off");

		panel.present(stale_projection());
		panel.new_session_action()->trigger();
		expect(
			recorded.count<qt::choose_chat_continuation_intent>() == 1U,
			"choosing a continuation emits exactly one continuation intent");
		expect(
			recorded.total() == 4U,
			"and the four gestures produced four intents between them");
	}

	/// \brief Refresh, navigation, and comment arrival add nothing at all.
	void test_observations_add_no_context_and_ask_for_nothing()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };
		panel.present(current_projection());
		const auto baseline = read_all(panel);

		// The four the core declares, not a sample of them.
		constexpr auto observations = std::array{
			core::agent_observation::refresh,
			core::agent_observation::navigation,
			core::agent_observation::comment_arrival,
			core::agent_observation::revision_update,
		};
		static_assert(
			observations.size() == core::agent_observation_count,
			"every declared observation must be swept");

		for (const auto observation : observations) {
			panel.observe(observation);
			expect(
				panel.last_observation()
					== core::note_agent_observation(observation),
				"the panel records that it happened");
			expect(
				recorded.total() == 0U,
				std::string{ core::stable_name(observation) }
					+ " must emit no intent");
			expect(
				panel.chip_count() == 0U,
				std::string{ core::stable_name(observation) }
					+ " must add no context chip");
			const auto after = read_all(panel);
			for (auto index = std::size_t{ 0U }; index < after.size();
				 ++index) {
				expect(
					after[index].Value == baseline[index].Value,
					std::string{ core::stable_name(observation) }
						+ " must not move the presented "
						+ std::string{ after[index].Name });
			}
		}
		expect(
			panel.statistics().Observations == observations.size()
				&& panel.statistics().SendIntents == 0U
				&& panel.statistics().ScopeIntents == 0U,
			"four observations happened and nothing was asked of the agent");
	}

	/// \brief A chip appears only from an explicit context action.
	void test_a_chip_appears_only_from_an_explicit_context_action()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };
		panel.present(current_projection());
		expect(
			panel.chip_count() == 0U
				&& panel.chips_text() == QStringLiteral("No added context"),
			"a chat at the reviewer's selection carries no added context");

		panel.scope_selector()->setCurrentIndex(1);
		expect(
			panel.chip_count() == 0U && recorded.total() == 0U
				&& panel.scope_text()
					== QString::fromUtf8(core::stable_name(
						core::agent_chat_scope_kind::selected_lines)),
			"selecting a wider scope is not asking for it");

		panel.add_context_action()->trigger();
		expect(
			recorded.count<qt::widen_chat_scope_intent>() == 1U
				&& panel.chip_count() == 0U
				&& panel.chips_text() == QStringLiteral("No added context"),
			"asking for context emits an intent and adds no chip of its own");

		auto widened             = current_projection();
		widened.Chat.Scope.Kind  = core::agent_chat_scope_kind::file;
		widened.Chat.WidenedFrom = selection();
		widened.Chips.push_back(qt::side_chat_context_chip{
			.Scope = core::agent_chat_scope_kind::file,
			.Label = "src/core/agent_chat_scope.cpp",
		});
		panel.present(widened);
		expect(
			panel.chip_count() == 1U
				&& panel.chips_text().contains("src/core/agent_chat_scope.cpp")
				&& panel.scope_text().contains("widened from selected_lines"),
			"the chip appears when the accepted projection carries it, and "
			"the drawer still says where the chat began");
	}

	/// \brief A stale session says so, offers two continuations, starts neither.
	void test_a_stale_session_offers_two_continuations_and_starts_neither()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };

		panel.present(current_projection());
		expect(
			panel.session_state_text().contains("current"),
			"a current session is shown as current in the session state "
			"field");
		expect(
			panel.offered_continuations().empty()
				&& !panel.new_session_action()->isEnabled()
				&& !panel.continue_old_revision_action()->isEnabled(),
			"a current session asks the reviewer to choose nothing");

		panel.present(stale_projection());
		expect(
			panel.session_state_text().contains("stale")
				&& panel.session_state_text().contains("head_moved"),
			"a session whose head moved is shown as stale, and why");
		expect(
			panel.offered_continuations()
				== std::vector{
					core::agent_continuation::start_new_session,
					core::agent_continuation::continue_against_old_revision },
			"and offers exactly the two continuations the core defines");
		expect(
			panel.continuations_text().contains("start_new_session")
				&& panel.continuations_text().contains(
					"continue_against_old_revision"),
			"and the continuation field names both choices");
		expect(
			recorded.total() == 0U,
			"presenting a stale session starts neither continuation");
		expect(
			!panel.send_action()->isEnabled(),
			"and a stale session is answered before it is talked to again");

		panel.new_session_action()->trigger();
		expect(
			recorded.total() == 1U,
			"starting a new session happens only when it is chosen");
		const auto& fresh = std::get<qt::choose_chat_continuation_intent>(
			recorded.Intents.back());
		expect(
			fresh.Choice == core::agent_continuation::start_new_session
				&& fresh.Session == session().Id
				&& fresh.OfferedBinding == session().Binding
				&& fresh.OfferedBinding != observed(),
			"and the intent names this session's own binding, never the head "
			"the reviewer happens to be looking at");

		panel.continue_old_revision_action()->trigger();
		expect(
			recorded.total() == 2U,
			"continuing against the old revision happens only when chosen");
		const auto& old = std::get<qt::choose_chat_continuation_intent>(
			recorded.Intents.back());
		expect(
			old.Choice
					== core::agent_continuation::continue_against_old_revision
				&& old.OfferedBinding == session().Binding,
			"and it continues against exactly the revision it was asked "
			"about");
		expect(
			panel.session_state_text().contains("stale"),
			"choosing does not move the display; the next projection does");
	}

	/// \brief Presenting an opt-in never looks like the reviewer choosing it.
	void test_presenting_an_optin_emits_nothing()
	{
		auto recorded               = recorder{ };
		auto panel                  = qt::side_chat_panel{ recorded.sink() };
		auto shown                  = current_projection();
		shown.PrivateContext.Notes  = true;
		shown.PrivateContext.Drafts = true;
		panel.present(shown);
		expect(
			panel.note_optin()->isChecked()
				&& panel.draft_optin()->isChecked()
				&& recorded.total() == 0U
				&& panel.private_context_text()
					== QStringLiteral("notes included, drafts included"),
			"an accepted opt-in shows as included without asking for itself");
	}

	/// \brief What the panel would be if it held exactly its declared members.
	struct member_set_probe : QWidget
	{
		qt::side_chat_panel_members Members;
	};

	static_assert(
		sizeof(qt::side_chat_panel) == sizeof(member_set_probe),
		"the side-chat panel holds exactly its declared member set: a member "
		"added to the class outside side_chat_panel_members fails here");

	/// \brief The panel holds no launcher, process, credential, or address.
	///
	/// \details Every member is bound by name and its type is stated, so a
	/// launcher, a process handle, a credential, or a network address added
	/// to the panel fails to compile in this function rather than being
	/// noticed in review - or not noticed.
	void test_the_panel_holds_no_launcher_process_credential_or_address()
	{
		const auto members                = qt::side_chat_panel_members{ };
		const auto& [applying,
					 lastObservation,
					 statistics,
					 dispatch,
					 shown,
					 provider,
					 gitHost,
					 workspace,
					 epochPair,
					 headOid,
					 sessionState,
					 scope,
					 chips,
					 privateContext,
					 continuations,
					 composer,
					 scopeSelector,
					 noteOptIn,
					 draftOptIn,
					 send,
					 addContext,
					 newSession,
					 continueOldRevision] = members;

		static_assert(std::is_same_v<decltype(applying), const bool>);
		static_assert(
			std::is_same_v<
				decltype(lastObservation),
				const core::agent_observation_record>);
		static_assert(
			std::is_same_v<
				decltype(statistics),
				const qt::side_chat_panel_statistics>);
		static_assert(
			std::is_same_v<
				decltype(dispatch),
				const qt::side_chat_intent_dispatch>);
		static_assert(
			std::is_same_v<
				decltype(shown),
				const std::optional<qt::side_chat_projection>>);
		static_assert(std::is_same_v<decltype(provider), QLabel* const>);
		static_assert(std::is_same_v<decltype(gitHost), QLabel* const>);
		static_assert(std::is_same_v<decltype(workspace), QLabel* const>);
		static_assert(std::is_same_v<decltype(epochPair), QLabel* const>);
		static_assert(std::is_same_v<decltype(headOid), QLabel* const>);
		static_assert(std::is_same_v<decltype(sessionState), QLabel* const>);
		static_assert(std::is_same_v<decltype(scope), QLabel* const>);
		static_assert(std::is_same_v<decltype(chips), QLabel* const>);
		static_assert(std::is_same_v<decltype(privateContext), QLabel* const>);
		static_assert(std::is_same_v<decltype(continuations), QLabel* const>);
		static_assert(
			std::is_same_v<decltype(composer), QPlainTextEdit* const>);
		static_assert(
			std::is_same_v<decltype(scopeSelector), QComboBox* const>);
		static_assert(std::is_same_v<decltype(noteOptIn), QCheckBox* const>);
		static_assert(std::is_same_v<decltype(draftOptIn), QCheckBox* const>);
		static_assert(std::is_same_v<decltype(send), QAction* const>);
		static_assert(std::is_same_v<decltype(addContext), QAction* const>);
		static_assert(std::is_same_v<decltype(newSession), QAction* const>);
		static_assert(
			std::is_same_v<decltype(continueOldRevision), QAction* const>);

		expect(
			!applying && lastObservation == core::agent_observation_record{ }
				&& statistics == qt::side_chat_panel_statistics{ }
				&& !dispatch && !shown.has_value() && provider == nullptr
				&& gitHost == nullptr && workspace == nullptr
				&& epochPair == nullptr && headOid == nullptr
				&& sessionState == nullptr && scope == nullptr
				&& chips == nullptr && privateContext == nullptr
				&& continuations == nullptr && composer == nullptr
				&& scopeSelector == nullptr && noteOptIn == nullptr
				&& draftOptIn == nullptr && send == nullptr
				&& addContext == nullptr && newSession == nullptr
				&& continueOldRevision == nullptr,
			"the panel's complete member set is a callback, an accepted "
			"projection, six counters, one observation record, and the "
			"widgets that show them - there is nothing here to start, "
			"nothing to authenticate to, and nowhere to send anything");
	}

	/// \brief A panel with no dispatch and no projection asks for nothing.
	void test_a_panel_without_a_projection_emits_nothing()
	{
		auto recorded = recorder{ };
		auto panel    = qt::side_chat_panel{ recorded.sink() };
		expect(
			!panel.send_action()->isEnabled()
				&& !panel.add_context_action()->isEnabled()
				&& !panel.new_session_action()->isEnabled()
				&& !panel.continue_old_revision_action()->isEnabled(),
			"with nothing presented there is nothing to ask about");
		panel.send_action()->trigger();
		panel.add_context_action()->trigger();
		panel.new_session_action()->trigger();
		panel.continue_old_revision_action()->trigger();
		panel.observe(core::agent_observation::refresh);
		expect(
			recorded.total() == 0U && panel.chip_count() == 0U,
			"and triggering every action anyway emits nothing");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	// The stale path runs first so that a continuation which started itself
	// fails on the claim it broke rather than on a later count of intents.
	test_a_stale_session_offers_two_continuations_and_starts_neither();
	test_every_disclosed_field_is_presented_separately();
	test_nothing_the_reviewer_does_changes_the_display();
	test_every_gesture_leaves_as_its_own_typed_intent();
	test_observations_add_no_context_and_ask_for_nothing();
	test_a_chip_appears_only_from_an_explicit_context_action();
	test_presenting_an_optin_emits_nothing();
	test_the_panel_holds_no_launcher_process_credential_or_address();
	test_a_panel_without_a_projection_emits_nothing();
	std::cout << "side-chat-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
