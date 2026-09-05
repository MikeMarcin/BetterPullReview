/// \file
/// \brief Proves the finding queue shows what it was given and promotes nothing.

#include "bpr/qt/finding_queue_view.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QListWidget>

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

	class recorder final
	{
	public:
		[[nodiscard]] auto sink() -> qt::finding_queue_intent_dispatch
		{
			return [this](qt::finding_action_intent aIntent) {
				Intents.push_back(std::move(aIntent));
			};
		}

		[[nodiscard]] auto total() const -> std::size_t
		{
			return Intents.size();
		}

		std::vector<qt::finding_action_intent> Intents;
	};

	template <typename Identity>
	[[nodiscard]] auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
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

	[[nodiscard]] auto binding() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 1U },
			.Repository  = core::repository_id{ 2U },
			.PullRequest = core::pull_request_id{ 3U },
			.BaseOid     = commit(4U),
			.HeadOid     = commit(5U),
			.LeftEpoch   = core::epoch_ordinal{ 6U },
			.RightEpoch  = core::epoch_ordinal{ 7U },
		};
	}

	[[nodiscard]] auto session() -> core::agent_session_id
	{
		return core::agent_session_id{ 7U };
	}

	/// \brief The finding every assertion below is about, at ordinal 1.
	[[nodiscard]] auto subject_finding() -> core::agent_finding
	{
		return {
			.Provider         = core::agent_profile_id{ 1U },
			.Severity         = core::defect_severity::major,
			.Confidence       = core::agent_finding_confidence::medium,
			.Category         = core::defect_category::correctness,
			.Path             = "src/core/src/review_ledger.cpp",
			.Lines            = { .Begin = 40U, .End = 44U },
			.Anchor           = { .Line = core::source_line_ref{
									  .Comparison = opaque<core::file_comparison_digest>(2U),
									  .Side       = core::diff_side::right,
									  .Line       = 41U } },
			.Claim            = "The edit path never checks the publishing state.",
			.Evidence         = "review_ledger.cpp:41 applies without reading State.",
			.Consequence      = "An edit races a publication already in flight.",
			.SuggestedComment = "Refuse an edit while this draft is publishing.",
			.RequiredContext  = "The draft state machine.",
			.AnalyzedBaseOid  = commit(4U),
			.AnalyzedHeadOid  = commit(5U),
			.AnalyzedCommits  = { commit(5U) },
		};
	}

	/// \brief A different finding, at ordinal 0, so "exact" means something.
	[[nodiscard]] auto other_finding() -> core::agent_finding
	{
		auto other     = subject_finding();
		other.Path     = "src/core/src/defect_workflow.cpp";
		other.Severity = core::defect_severity::trivial;
		other.Claim    = "This name reads oddly.";
		other.Lines    = { .Begin = 8U, .End = 9U };
		return other;
	}

	[[nodiscard]] auto anchored_verdict() -> core::agent_finding_verdict
	{
		return {
			.State           = core::agent_finding_anchor_state::anchored,
			.Refusal         = core::agent_finding_refusal::none,
			.AnalyzedBaseOid = commit(4U),
			.AnalyzedHeadOid = commit(5U),
		};
	}

	/// \brief One delivery: an unanchored finding, then the anchored subject.
	///
	/// \details Nothing is dropped, which is why the unanchored one is here.
	/// A reviewer who cannot see what the agent got wrong cannot correct it.
	[[nodiscard]] auto a_delivery() -> application::agent_finding_delivery
	{
		return {
			.Session   = session(),
			.Binding   = binding(),
			.Staleness = core::agent_session_staleness::current,
			.Findings  = {
				application::agent_delivered_finding{
					.Finding = other_finding(),
					.Verdict = {
						.State           = core::agent_finding_anchor_state::unanchored,
						.Refusal         = core::agent_finding_refusal::span_past_end,
						.AnalyzedBaseOid = commit(4U),
						.AnalyzedHeadOid = commit(5U) } },
				application::agent_delivered_finding{ .Finding = subject_finding(), .Verdict = anchored_verdict() },
			},
		};
	}

	/// \brief The exact subject an action must name.
	[[nodiscard]] auto expected_subject() -> qt::queued_finding
	{
		return {
			.Session = session(),
			.Ordinal = 1U,
			.Finding = subject_finding(),
			.Verdict = anchored_verdict(),
		};
	}

	struct reading
	{
		std::string_view Name;
		QString          Value;
	};

	/// \brief Every value the queue discloses, read off the view itself.
	[[nodiscard]] auto read_all(const qt::finding_queue_view& aView)
		-> std::array<reading, 8U>
	{
		return { {
			{ "findings list", aView.findings_text() },
			{ "severity", aView.severity_text() },
			{ "confidence", aView.confidence_text() },
			{ "category", aView.category_text() },
			{ "claim", aView.claim_text() },
			{ "evidence", aView.evidence_text() },
			{ "consequence", aView.consequence_text() },
			{ "anchor state", aView.anchor_state_text() },
		} };
	}

	struct disclosed_field
	{
		/// What the delivery changes about the selected finding.
		std::string_view Name;
		/// The one presented value that must move when it does.
		std::string_view Field;
		void (*Mutate)(application::agent_finding_delivery&);
	};

	/// \brief Every disclosed field, swept rather than sampled.
	///
	/// \details Each entry changes exactly one thing about the selected
	/// finding. The sweep asserts the named field moved *and* that nothing
	/// else did, which is what "shown, not summarized" means: a queue that
	/// rendered these into one paragraph would fail every row but its own,
	/// and deleting any one field's presentation fails its own row by name.
	constexpr auto disclosed_fields = std::array{
		disclosed_field{
			"path",
			"findings list",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Path = "src/core/src/epoch_model.cpp";
			} },
		disclosed_field{
			"line span",
			"findings list",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Lines.End = 99U;
			} },
		disclosed_field{
			"severity",
			"severity",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Severity
					= core::defect_severity::critical;
			} },
		disclosed_field{
			"confidence",
			"confidence",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Confidence
					= core::agent_finding_confidence::high;
			} },
		disclosed_field{
			"category",
			"category",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Category
					= core::defect_category::security;
			} },
		disclosed_field{
			"claim",
			"claim",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Claim
					= "The publishing check is absent.";
			} },
		disclosed_field{
			"evidence",
			"evidence",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Evidence
					= "review_ledger.cpp:52 is the second such call.";
			} },
		disclosed_field{
			"consequence",
			"consequence",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Finding.Consequence
					= "The reviewer's edit is silently discarded.";
			} },
		disclosed_field{
			"anchor state",
			"anchor state",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Verdict.State
					= core::agent_finding_anchor_state::stale;
			} },
		disclosed_field{
			"anchor refusal",
			"anchor state",
			[](application::agent_finding_delivery& aShown) {
				aShown.Findings.at(1U).Verdict.Refusal
					= core::agent_finding_refusal::unknown_path;
			} },
	};

	/// \brief Each disclosed fact is its own field, and moves on its own.
	void test_every_disclosed_field_is_presented_separately()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		view.present(a_delivery());
		view.select(1U);
		const auto baseline = read_all(view);

		expect(
			baseline[1].Value == QStringLiteral("major")
				&& baseline[2].Value == QStringLiteral("medium")
				&& baseline[3].Value == QStringLiteral("correctness"),
			"severity, confidence, and category are each shown in the core's "
			"own spelling");
		expect(
			baseline[4].Value
					== QString::fromStdString(subject_finding().Claim)
				&& baseline[5].Value
					== QString::fromStdString(subject_finding().Evidence)
				&& baseline[6].Value
					== QString::fromStdString(subject_finding().Consequence),
			"the claim, the evidence, and the consequence are shown whole and "
			"separately");
		expect(
			baseline[7].Value == QStringLiteral("anchored (none)"),
			"an anchor that held is shown as anchored");
		expect(
			baseline[0].Value.contains(QStringLiteral("review_ledger.cpp:40-44"))
				&& baseline[0].Value.contains(
					QStringLiteral("defect_workflow.cpp:8-9")),
			"every delivered finding is listed, including the unanchored one");

		for (const auto& field : disclosed_fields) {
			auto mutated = a_delivery();
			field.Mutate(mutated);
			view.present(mutated);
			const auto after = read_all(view);
			for (auto index = std::size_t{ 0U }; index < after.size(); ++index) {
				const auto moved = after[index].Value != baseline[index].Value;
				const auto owns  = after[index].Name == field.Field;
				expect(
					moved == owns,
					std::string{ "changing " } + std::string{ field.Name }
						+ " must change the presented "
						+ std::string{ field.Field }
						+ " and nothing else, but the "
						+ std::string{ after[index].Name } + " field "
						+ (moved ? "also moved" : "did not move"));
			}
			view.present(a_delivery());
			expect(
				read_all(view)[0].Value == baseline[0].Value,
				"and presenting the original delivery restores it");
		}

		expect(
			recorded.total() == 0U,
			"presenting a delivery emits no intent at all");
	}

	/// \brief An anchor that did not hold is shown, not hidden or repaired.
	void test_an_unanchored_finding_is_shown_and_still_actionable()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		view.present(a_delivery());
		view.select(0U);

		expect(
			view.anchor_state_text()
				== QStringLiteral("unanchored (span_past_end)"),
			"the queue says that the anchor did not hold, and in which way");
		expect(
			view.claim_text() == QStringLiteral("This name reads oddly."),
			"and the finding is still shown rather than dropped");
		for (const auto action : { qt::finding_queue_action::open_evidence,
								   qt::finding_queue_action::convert_to_draft_comment,
								   qt::finding_queue_action::convert_to_draft_defect,
								   qt::finding_queue_action::add_to_private_notes }) {
			expect(
				view.action(action)->isEnabled(),
				std::string{ qt::stable_name(action) }
					+ " stays offered: whether a promotion is legal is the "
					  "core's answer, and a widget that pre-empted it could "
					  "disagree with it");
		}
		expect(
			recorded.total() == 0U,
			"and showing an unanchored finding asks for nothing");
	}

	struct action_case
	{
		qt::finding_queue_action Action;
		std::string_view         Name;
	};

	/// \brief One case per declared action, and the count is enforced.
	///
	/// \details A seventh `finding_queue_action` added without a case here
	/// fails the static assertion below at compile time rather than being
	/// silently untested.
	constexpr auto action_cases = std::array{
		action_case{ qt::finding_queue_action::open_evidence, "open_evidence" },
		action_case{ qt::finding_queue_action::ask_follow_up, "ask_follow_up" },
		action_case{
			qt::finding_queue_action::convert_to_draft_comment,
			"convert_to_draft_comment" },
		action_case{
			qt::finding_queue_action::convert_to_draft_defect,
			"convert_to_draft_defect" },
		action_case{
			qt::finding_queue_action::add_to_private_notes,
			"add_to_private_notes" },
		action_case{ qt::finding_queue_action::dismiss, "dismiss" },
	};

	static_assert(
		action_cases.size() == qt::finding_queue_action_count,
		"every declared finding action must have a case: a seventh action "
		"added without one fails the count here");

	/// \brief All six actions, each one intent naming the exact finding.
	void test_every_action_emits_one_intent_naming_the_exact_finding()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		view.present(a_delivery());
		view.select(1U);

		for (auto index = std::size_t{ 0U }; index < action_cases.size();
			 ++index) {
			const auto& probe = action_cases.at(index);
			expect(
				qt::stable_name(probe.Action) == probe.Name,
				"each action names itself");
			view.action(probe.Action)->trigger();
			expect(
				recorded.total() == index + 1U,
				std::string{ probe.Name }
					+ " must emit exactly one intent, no more and no fewer");
			const auto& emitted = recorded.Intents.back();
			expect(
				emitted.Action == probe.Action,
				std::string{ probe.Name } + " must emit the action it names");
			expect(
				emitted.Subject == expected_subject(),
				std::string{ probe.Name }
					+ " must name the exact finding: its session, its position "
					  "in the delivery, the finding itself, and the verdict "
					  "reached about it");
			expect(
				emitted.Subject.Finding != other_finding(),
				std::string{ probe.Name }
					+ " must not name the other delivered finding");
		}

		expect(
			recorded.total() == qt::finding_queue_action_count
				&& view.statistics().ActionIntents
					== qt::finding_queue_action_count,
			"six declared actions produced six intents between them");
	}

	/// \brief Nothing the reviewer does changes what a finding says.
	void test_acting_on_a_finding_changes_nothing_it_says()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		view.present(a_delivery());
		view.select(1U);
		const auto baseline = read_all(view);

		for (const auto& probe : action_cases) {
			view.action(probe.Action)->trigger();
		}

		const auto after = read_all(view);
		for (auto index = std::size_t{ 0U }; index < after.size(); ++index) {
			expect(
				after[index].Value == baseline[index].Value,
				std::string{ "the " } + std::string{ after[index].Name }
					+ " field moves only when a delivery is presented");
		}
		expect(
			view.finding_count() == 2U
				&& view.selected_ordinal() == std::optional<std::size_t>{ 1U },
			"dismissing a finding removes nothing here: the queue shows what "
			"it was given, and what the reviewer chose is the core's to record");
		expect(
			recorded.total() == qt::finding_queue_action_count,
			"and every gesture left as an intent instead");
	}

	/// \brief Selecting reads a different row; it asks for nothing.
	void test_selecting_a_row_emits_nothing_and_survives_a_refresh()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		view.present(a_delivery());
		expect(
			!view.selected_ordinal().has_value()
				&& view.claim_text().isEmpty()
				&& !view.action(qt::finding_queue_action::dismiss)->isEnabled(),
			"a delivery selects nothing by itself, and there is nothing to act "
			"on until the reviewer picks a finding");

		view.select(1U);
		expect(
			view.claim_text()
					== QString::fromStdString(subject_finding().Claim)
				&& recorded.total() == 0U,
			"selecting details that finding and asks for nothing");

		view.present(a_delivery());
		expect(
			view.selected_ordinal() == std::optional<std::size_t>{ 1U }
				&& view.claim_text()
					== QString::fromStdString(subject_finding().Claim),
			"an accepted refresh does not move the reviewer off the row they "
			"were reading");

		auto shorter = a_delivery();
		shorter.Findings.pop_back();
		view.present(shorter);
		expect(
			!view.selected_ordinal().has_value()
				&& view.claim_text().isEmpty()
				&& recorded.total() == 0U,
			"and a delivery with no row there details nothing rather than "
			"detailing somebody else's finding");
	}

	/// \brief With nothing selected, every action is inert.
	void test_a_queue_without_a_selection_emits_nothing()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		for (const auto& probe : action_cases) {
			view.action(probe.Action)->trigger();
		}
		view.present(a_delivery());
		for (const auto& probe : action_cases) {
			view.action(probe.Action)->trigger();
		}
		expect(
			recorded.total() == 0U && view.statistics().ActionIntents == 0U,
			"triggering every action against no selected finding emits "
			"nothing at all");
	}

	/// \brief Every action is distinctly named for assistive technology.
	void test_every_action_is_distinctly_named()
	{
		auto recorded = recorder{ };
		auto view     = qt::finding_queue_view{ recorded.sink() };
		for (const auto& first : action_cases) {
			expect(
				!view.action(first.Action)->text().isEmpty()
					&& !view.action(first.Action)->objectName().isEmpty(),
				"every action carries text and an object name");
			for (const auto& second : action_cases) {
				if (first.Action == second.Action) {
					continue;
				}
				expect(
					view.action(first.Action)->text()
						!= view.action(second.Action)->text(),
					"two actions never share the text assistive technology "
					"reads");
			}
		}
		expect(
			!view.findings_list()->accessibleName().isEmpty(),
			"the queue itself is announced, not only drawn");
	}

	/// \brief What the view would be if it held exactly its declared members.
	struct member_set_probe : QWidget
	{
		qt::finding_queue_view_members Members;
	};

	static_assert(
		sizeof(qt::finding_queue_view) == sizeof(member_set_probe),
		"the finding queue holds exactly its declared member set: a member "
		"added to the class outside finding_queue_view_members fails here");

	/// \brief The view holds no provider, no endpoint, no store, no process.
	///
	/// \details Every member is bound by name and its type is stated, so a
	/// provider handle, a network address, a ledger store, or a process added
	/// to the view fails to compile in this function rather than being
	/// noticed in review - or not noticed.
	void test_the_view_holds_no_provider_endpoint_store_or_process()
	{
		const auto members    = qt::finding_queue_view_members{ };
		const auto& [applying,
					 statistics,
					 dispatch,
					 shown,
					 selected,
					 findings,
					 severity,
					 confidence,
					 category,
					 claim,
					 evidence,
					 consequence,
					 anchorState,
					 actions] = members;

		static_assert(std::is_same_v<decltype(applying), const bool>);
		static_assert(
			std::is_same_v<
				decltype(statistics),
				const qt::finding_queue_view_statistics>);
		static_assert(
			std::is_same_v<
				decltype(dispatch),
				const qt::finding_queue_intent_dispatch>);
		static_assert(
			std::is_same_v<
				decltype(shown),
				const std::optional<application::agent_finding_delivery>>);
		static_assert(
			std::is_same_v<
				decltype(selected),
				const std::optional<std::size_t>>);
		static_assert(std::is_same_v<decltype(findings), QListWidget* const>);
		static_assert(std::is_same_v<decltype(severity), QLabel* const>);
		static_assert(std::is_same_v<decltype(confidence), QLabel* const>);
		static_assert(std::is_same_v<decltype(category), QLabel* const>);
		static_assert(std::is_same_v<decltype(claim), QLabel* const>);
		static_assert(std::is_same_v<decltype(evidence), QLabel* const>);
		static_assert(std::is_same_v<decltype(consequence), QLabel* const>);
		static_assert(std::is_same_v<decltype(anchorState), QLabel* const>);
		static_assert(
			std::is_same_v<
				decltype(actions),
				const std::array<QAction*, qt::finding_queue_action_count>>);

		expect(
			!applying
				&& statistics == qt::finding_queue_view_statistics{ }
				&& !dispatch && !shown.has_value() && !selected.has_value()
				&& findings == nullptr && severity == nullptr
				&& confidence == nullptr && category == nullptr
				&& claim == nullptr && evidence == nullptr
				&& consequence == nullptr && anchorState == nullptr
				&& actions[0] == nullptr,
			"the queue's complete member set is a callback, the delivery it "
			"was handed, a local selection, three counters, and the widgets "
			"that show them - there is nothing here to promote with, nothing "
			"to write to, and nowhere to send anything");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);

	test_every_disclosed_field_is_presented_separately();
	test_an_unanchored_finding_is_shown_and_still_actionable();
	test_every_action_emits_one_intent_naming_the_exact_finding();
	test_acting_on_a_finding_changes_nothing_it_says();
	test_selecting_a_row_emits_nothing_and_survives_a_refresh();
	test_a_queue_without_a_selection_emits_nothing();
	test_every_action_is_distinctly_named();
	test_the_view_holds_no_provider_endpoint_store_or_process();

	std::cout << "finding-queue-view-tests:pass\n";
	return EXIT_SUCCESS;
}
