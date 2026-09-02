#include "bpr/qt/review_draft_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>

#include <algorithm>
#include <array>
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

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto a_draft(std::string aBody) -> core::review_comment_draft
	{
		return core::review_comment_draft{
			.Id     = core::review_draft_id{ 1U },
			.Anchor = core::source_line_ref{
				.Comparison = opaque<core::file_comparison_digest>(2U),
				.Side       = core::diff_side::right,
				.Line       = 8U },
			.Body = std::move(aBody),
		};
	}

	[[nodiscard]] auto a_receipt() -> core::file_receipt
	{
		return core::file_receipt{
			.File     = opaque<core::file_identity>(3U),
			.Material = opaque<core::file_material_id>(4U),
		};
	}

	/// Collects the intents the editor emits. This is the editor's only
	/// outbound path — there is no provider to intercept, because the editor
	/// was never given one.
	struct intent_log
	{
		std::vector<core::review_draft_command> Drafts;
		std::vector<core::file_receipt_command> Receipts;
	};

	[[nodiscard]] auto editor_for(intent_log& aLog) -> qt::review_draft_editor*
	{
		return new qt::review_draft_editor{
			[&aLog](core::review_draft_command aCommand) {
				aLog.Drafts.push_back(std::move(aCommand));
			},
			[&aLog](core::file_receipt_command aCommand) {
				aLog.Receipts.push_back(aCommand);
			},
		};
	}

	// The rule: an action emits an intent and changes nothing. A widget that
	// updated itself optimistically would show a comment as published while the
	// request was in flight, and keep showing it if the request failed.
	void test_publishing_emits_an_intent_and_changes_nothing()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft("Needs a test."));

		const auto before = editor->shown_draft();
		editor->publish_action()->trigger();

		expect(log.Drafts.size() == 1U, "one draft intent was emitted");
		expect(
			log.Drafts.front().Kind
				== core::review_draft_command_kind::begin_publish,
			"the intent is the publish the action names");
		expect(
			log.Drafts.front().Body == "Needs a test.",
			"the intent carries the text the editor holds");
		expect(
			editor->shown_draft() == before,
			"nothing the editor shows has moved");
		expect(
			editor->publish_action()->isEnabled(),
			"the action is still enabled because no projection has arrived");

		// Only an accepted projection moves it.
		auto publishing  = a_draft("Needs a test.");
		publishing.State = core::review_draft_state::publishing;
		editor->present(publishing);
		expect(
			editor->shown_draft()->State == core::review_draft_state::publishing
				&& !editor->publish_action()->isEnabled(),
			"the accepted projection is what changes the editor");

		delete editor;
	}

	void test_typing_emits_a_semantic_edit_and_waits_for_projection()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft("before"));

		editor->body_editor()->setPlainText(QStringLiteral("after"));
		expect(log.Drafts.size() == 1U
				   && log.Drafts.front().Kind
					   == core::review_draft_command_kind::edit
				   && log.Drafts.front().Body == "after",
			   "typing emits one semantic edit carrying the current body");
		expect(editor->shown_draft()->Body == "before",
			   "typing does not mutate the accepted draft projection");

		editor->present(a_draft("after"));
		expect(editor->shown_draft()->Body == "after" && log.Drafts.size() == 1U,
			   "the accepted edit projection updates without emitting another intent");
		delete editor;
	}

	void test_receipt_actions_emit_intents_and_change_nothing()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_receipt());

		const auto before = editor->shown_receipt();
		editor->mark_read_action()->trigger();
		editor->accept_action()->trigger();
		editor->exclude_action()->trigger();

		expect(log.Receipts.size() == 3U, "three receipt intents were emitted");
		expect(
			log.Receipts.at(0).Kind == core::file_receipt_command_kind::mark_read
				&& log.Receipts.at(1).Kind == core::file_receipt_command_kind::accept
				&& log.Receipts.at(2).Kind
					== core::file_receipt_command_kind::exclude,
			"each action emits the command it names");
		expect(
			log.Receipts.front().File == a_receipt().File
				&& log.Receipts.front().Material == a_receipt().Material,
			"the intent carries the file and the material it was issued against");
		expect(
			editor->shown_receipt() == before,
			"marking, accepting, and excluding changed nothing locally");

		auto accepted     = a_receipt();
		accepted.Accepted = true;
		editor->present(accepted);
		expect(
			editor->shown_receipt()->Accepted
				&& !editor->exclude_action()->isEnabled(),
			"the accepted projection moves the state and the enablement");

		delete editor;
	}

	// Enablement follows the accepted projection, not the local edit. A publish
	// button enabled because the user typed would be the widget deciding
	// legality the core owns.
	void test_enablement_follows_the_projection_not_the_keystrokes()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);

		expect(
			!editor->publish_action()->isEnabled()
				&& !editor->discard_action()->isEnabled()
				&& !editor->mark_read_action()->isEnabled(),
			"with nothing presented, nothing is actionable");

		editor->body_editor()->setPlainText(QStringLiteral("typed but unknown"));
		expect(
			!editor->publish_action()->isEnabled(),
			"typing does not enable publishing");
		editor->publish_action()->trigger();
		expect(
			log.Drafts.empty(),
			"a disabled action with no presented draft emits nothing");

		editor->present(a_draft(""));
		expect(
			editor->publish_action()->isEnabled(),
			"a presented drafting comment is publishable");

		auto published  = a_draft("done");
		published.State = core::review_draft_state::published;
		editor->present(published);
		expect(
			!editor->publish_action()->isEnabled()
				&& !editor->discard_action()->isEnabled()
				&& editor->body_editor()->isReadOnly(),
			"a published comment is GitHub's and the editor stops offering to "
			"change it");

		delete editor;
	}

	// The reviewer is agreeing to a specific pull request at a specific
	// revision under a specific account, and cannot agree to what they were not
	// shown.
	void test_the_publication_target_is_shown_before_publishing()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);

		expect(
			editor->target_label()->text().isEmpty(),
			"no target is claimed before one is described");

		editor->describe_target(
			QStringLiteral("github.com/octo/app#7 at a1b2c3 as @octo-reviewer"));
		expect(
			editor->target_label()->text().contains(QStringLiteral("#7"))
				&& editor->target_label()->text().contains(
					QStringLiteral("a1b2c3"))
				&& editor->target_label()->text().contains(
					QStringLiteral("@octo-reviewer")),
			"the pull request, the revision, and the acting account are all "
			"shown");
		expect(
			!editor->target_label()->accessibleName().isEmpty(),
			"the target is announced to assistive technology, not only drawn");

		delete editor;
	}

	void test_every_action_and_widget_is_distinctly_named()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);

		const auto actions = std::array{
			editor->publish_action(),
			editor->discard_action(),
			editor->mark_read_action(),
			editor->accept_action(),
			editor->exclude_action(),
			editor->accept_revision_action(),
			editor->reject_revision_action(),
		};

		for (auto first = std::size_t{ 0U }; first < actions.size(); ++first) {
			expect(
				!actions[first]->text().isEmpty()
					&& !actions[first]->objectName().isEmpty(),
				"every action carries text and an object name");
			for (auto second = first + 1U; second < actions.size(); ++second) {
				expect(
					actions[first]->text() != actions[second]->text(),
					"two actions never share the text assistive technology "
					"reads");
				expect(
					actions[first]->objectName() != actions[second]->objectName(),
					"two actions never share an object name");
			}
		}

		expect(
			editor->body_editor()->accessibleName()
				!= editor->target_label()->accessibleName(),
			"the body and the target are separately named");
		expect(
			editor->workshop_original_editor()->accessibleName()
					!= editor->workshop_proposal_editor()->accessibleName()
				&& editor->workshop_original_editor()->accessibleName()
					!= editor->body_editor()->accessibleName(),
			"the original, the proposal, and the editable body are three "
			"separately announced surfaces");

		delete editor;
	}

	/// The reviewer's own text, chosen so a comparison is a byte comparison:
	/// two paragraphs, a tab, and a trailing space that a helpful editor
	/// would be tempted to tidy away.
	constexpr auto original_body
		= "Needs a test.\n\n\tThe tab and the trailing space matter. ";
	constexpr auto proposed_body
		= "This needs a test that fails when the check is removed.";

	[[nodiscard]] auto a_proposal() -> qt::comment_workshop_proposal
	{
		return qt::comment_workshop_proposal{
			.Draft    = core::review_draft_id{ 1U },
			.Original = original_body,
			.Proposed = proposed_body,
		};
	}

	// The workshop's whole point: the reviewer compares two texts, and both
	// are still there to compare. A single buffer that showed the proposal in
	// the original's place would have destroyed the thing being compared
	// against, and this is where that fails.
	void test_the_workshop_shows_both_texts_at_once_on_two_surfaces()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft(original_body));
		expect(
			editor->body_editor()->toPlainText().toStdString()
				== std::string{ original_body },
			"the editor holds the reviewer's bytes, whitespace and all");

		expect(
			editor->workshop_original_text().isEmpty()
				&& editor->workshop_proposal_text().isEmpty()
				&& !editor->accept_revision_action()->isEnabled()
				&& !editor->reject_revision_action()->isEnabled(),
			"there is no revision to weigh until one is proposed");

		editor->present(a_proposal());
		expect(
			editor->workshop_original_text().toStdString()
				== std::string{ original_body },
			"the original is presented as its own value");
		expect(
			editor->workshop_proposal_text().toStdString()
				== std::string{ proposed_body },
			"and the proposed revision is presented as a second, separate "
			"value at the same time");
		expect(
			editor->workshop_original_text()
				!= editor->workshop_proposal_text(),
			"the two surfaces are two surfaces: one buffer holding the "
			"proposal would read back as the original here");
		expect(
			editor->workshop_original_editor()
				!= editor->workshop_proposal_editor(),
			"and they are two widgets rather than one read twice");
		expect(
			editor->body_editor()->toPlainText().toStdString()
					== std::string{ original_body }
				&& editor->shown_draft()->Body == std::string{ original_body },
			"presenting a proposal leaves the reviewer's own draft alone");
		expect(
			log.Drafts.empty() && log.Receipts.empty(),
			"and asks for nothing while it does it");
		expect(
			editor->workshop_original_editor()->isReadOnly()
				&& editor->workshop_proposal_editor()->isReadOnly(),
			"neither comparison surface is a place to type; the body is");

		delete editor;
	}

	// Accepting is one semantic command and nothing else. It does not write
	// the revision into the body: the editor moves when an accepted edit
	// projection comes back, and writing it here would both pre-empt that
	// answer and emit a second command through the body's change signal.
	void test_accepting_a_revision_emits_exactly_one_command_and_stores_nothing()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft(original_body));
		editor->present(a_proposal());

		editor->accept_revision_action()->trigger();
		expect(
			log.Drafts.size() == 1U,
			"accepting a revision emits exactly one review draft command");
		expect(
			log.Receipts.empty(),
			"and touches no other command path: there is no store here to "
			"reach and no receipt to issue");
		expect(
			log.Drafts.front().Kind == core::review_draft_command_kind::edit,
			"the command is an edit of a draft that already exists, never a "
			"publication");
		expect(
			log.Drafts.front().Id == a_proposal().Draft,
			"and it names the draft the revision was proposed against");
		expect(
			log.Drafts.front().Body == std::string{ proposed_body },
			"carrying the proposed text byte for byte");
		expect(
			editor->shown_draft()->Body == std::string{ original_body }
				&& editor->body_editor()->toPlainText().toStdString()
					== std::string{ original_body },
			"and the reviewer's original has not moved: only an accepted "
			"projection moves it");

		// Only an accepted projection moves it, and that is one more command
		// from nobody.
		editor->present(a_draft(proposed_body));
		expect(
			editor->shown_draft()->Body == std::string{ proposed_body }
				&& log.Drafts.size() == 1U,
			"the accepted edit projection updates the editor without emitting "
			"another command");

		delete editor;
	}

	// Rejecting is the reviewer declining. It emits nothing, and what they
	// wrote is what they still have - compared byte for byte, not by shape.
	void test_rejecting_a_revision_leaves_the_original_byte_identical()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft(original_body));
		const auto beforeDraft = editor->shown_draft();
		const auto beforeBody
			= editor->body_editor()->toPlainText().toStdString();
		editor->present(a_proposal());

		editor->reject_revision_action()->trigger();
		expect(
			log.Drafts.empty() && log.Receipts.empty(),
			"rejecting a revision asks for nothing at all");
		expect(
			editor->body_editor()->toPlainText().toStdString() == beforeBody
				&& beforeBody == std::string{ original_body },
			"the reviewer's text is byte for byte what it was, tab and "
			"trailing space included");
		expect(
			editor->shown_draft() == beforeDraft,
			"and the accepted draft projection is unchanged");
		expect(
			editor->statistics().RejectedRevisions == 1U,
			"the rejection happened rather than the action being unwired");
		expect(
			!editor->shown_proposal().has_value()
				&& editor->workshop_proposal_text().isEmpty()
				&& editor->workshop_original_text().isEmpty()
				&& !editor->accept_revision_action()->isEnabled(),
			"and the rejected proposal is gone rather than left acceptable");

		editor->accept_revision_action()->trigger();
		editor->reject_revision_action()->trigger();
		expect(
			log.Drafts.empty() && editor->statistics().RejectedRevisions == 1U,
			"a proposal that was rejected cannot be accepted afterwards");

		delete editor;
	}

	// A proposal names one draft. Applying it to whatever the editor happens
	// to be showing would put the agent's revision on the wrong comment.
	void test_a_revision_is_accepted_only_onto_the_draft_it_names()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);

		auto other = a_draft("Another comment entirely.");
		other.Id   = core::review_draft_id{ 2U };
		editor->present(other);
		editor->present(a_proposal());
		expect(
			!editor->accept_revision_action()->isEnabled(),
			"a revision proposed against another draft is not acceptable here");
		editor->accept_revision_action()->trigger();
		expect(
			log.Drafts.empty(),
			"and triggering it anyway emits nothing");

		editor->present(a_draft(original_body));
		expect(
			editor->accept_revision_action()->isEnabled(),
			"presenting the draft it names makes it acceptable");

		auto publishing  = a_draft(original_body);
		publishing.State = core::review_draft_state::publishing;
		editor->present(publishing);
		expect(
			!editor->accept_revision_action()->isEnabled()
				&& editor->reject_revision_action()->isEnabled(),
			"a comment already on its way to GitHub is not the reviewer's to "
			"revise, though the proposal is still theirs to decline");

		delete editor;
	}

	// A proposal is about a draft that is no longer selected, so it goes with
	// it rather than sitting beside a body nobody is editing.
	void test_a_typed_clear_takes_the_proposal_with_it()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft(original_body));
		editor->present(a_proposal());

		editor->present(qt::review_draft_editor_clear_reason::selection_cleared);
		expect(
			!editor->shown_proposal().has_value()
				&& editor->workshop_original_text().isEmpty()
				&& editor->workshop_proposal_text().isEmpty()
				&& !editor->accept_revision_action()->isEnabled()
				&& !editor->reject_revision_action()->isEnabled(),
			"clearing the selection clears the proposal made about it");
		expect(
			log.Drafts.empty() && editor->statistics().RejectedRevisions == 0U,
			"and clearing is not the reviewer rejecting anything");

		delete editor;
	}

	/// \brief What the editor would be if it held exactly its declared members.
	struct member_set_probe : QWidget
	{
		qt::review_draft_editor_members Members;
	};

	static_assert(
		sizeof(qt::review_draft_editor) == sizeof(member_set_probe),
		"the review draft editor holds exactly its declared member set: a "
		"member added to the class outside review_draft_editor_members fails "
		"here");

	/// \brief The editor holds no provider, no endpoint, no store, no process.
	///
	/// \details Every member is bound by name and its type is stated, so a
	/// provider handle, a network address, a ledger store, or a process added
	/// to the editor fails to compile in this function rather than being
	/// noticed in review - or not noticed. This is what makes "the only way
	/// out of this widget is a semantic command" a property of the type.
	void test_the_editor_holds_no_provider_endpoint_store_or_process()
	{
		const auto members       = qt::review_draft_editor_members{ };
		const auto& [applying,
					 draftDispatch,
					 receiptDispatch,
					 body,
					 targetLabel,
					 mutationStatus,
					 workshopOriginal,
					 workshopProposal,
					 publish,
					 discard,
					 markRead,
					 accept,
					 exclude,
					 acceptRevision,
					 rejectRevision,
					 shownDraft,
					 shownReceipt,
					 shownProposal,
					 statistics] = members;

		static_assert(std::is_same_v<decltype(applying), const bool>);
		static_assert(
			std::is_same_v<
				decltype(draftDispatch),
				const qt::review_draft_dispatch>);
		static_assert(
			std::is_same_v<
				decltype(receiptDispatch),
				const qt::file_receipt_dispatch>);
		static_assert(std::is_same_v<decltype(body), QPlainTextEdit* const>);
		static_assert(std::is_same_v<decltype(targetLabel), QLabel* const>);
		static_assert(std::is_same_v<decltype(mutationStatus), QLabel* const>);
		static_assert(
			std::is_same_v<decltype(workshopOriginal), QPlainTextEdit* const>);
		static_assert(
			std::is_same_v<decltype(workshopProposal), QPlainTextEdit* const>);
		static_assert(std::is_same_v<decltype(publish), QAction* const>);
		static_assert(std::is_same_v<decltype(discard), QAction* const>);
		static_assert(std::is_same_v<decltype(markRead), QAction* const>);
		static_assert(std::is_same_v<decltype(accept), QAction* const>);
		static_assert(std::is_same_v<decltype(exclude), QAction* const>);
		static_assert(std::is_same_v<decltype(acceptRevision), QAction* const>);
		static_assert(std::is_same_v<decltype(rejectRevision), QAction* const>);
		static_assert(
			std::is_same_v<
				decltype(shownDraft),
				const std::optional<core::review_comment_draft>>);
		static_assert(
			std::is_same_v<
				decltype(shownReceipt),
				const std::optional<core::file_receipt>>);
		static_assert(
			std::is_same_v<
				decltype(shownProposal),
				const std::optional<qt::comment_workshop_proposal>>);
		static_assert(
			std::is_same_v<
				decltype(statistics),
				const qt::review_draft_editor_statistics>);

		expect(
			!applying && !draftDispatch && !receiptDispatch
				&& body == nullptr && targetLabel == nullptr
				&& mutationStatus == nullptr && workshopOriginal == nullptr
				&& workshopProposal == nullptr && publish == nullptr
				&& discard == nullptr && markRead == nullptr
				&& accept == nullptr && exclude == nullptr
				&& acceptRevision == nullptr && rejectRevision == nullptr
				&& !shownDraft.has_value() && !shownReceipt.has_value()
				&& !shownProposal.has_value()
				&& statistics == qt::review_draft_editor_statistics{ },
			"the editor's complete member set is two dispatch callbacks, the "
			"accepted projections, six counters, and the widgets that show "
			"them - there is no provider to hold, nowhere to send anything, "
			"and nothing to write to");
	}

	void test_typed_clear_removes_stale_selection_bound_state()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft("Stale body"));
		editor->present(a_receipt());
		editor->describe_target(QStringLiteral("github.com/octo/app#7 at deadbeef"));
		editor->present(application::review_mutation_failure::unavailable);

		editor->present(qt::review_draft_editor_clear_reason::selection_invalidated);
		expect(!editor->shown_draft().has_value()
				   && !editor->shown_receipt().has_value()
				   && editor->body_editor()->toPlainText().isEmpty()
				   && editor->target_label()->text().contains(QStringLiteral("#7")),
			   "typed invalidation clears selection state while retaining the host target");
		expect(!editor->publish_action()->isEnabled()
				   && !editor->discard_action()->isEnabled()
				   && !editor->mark_read_action()->isEnabled()
				   && !editor->accept_action()->isEnabled()
				   && !editor->exclude_action()->isEnabled()
				   && editor->body_editor()->isReadOnly(),
			   "typed invalidation disables every stale action");
		expect(log.Drafts.empty() && log.Receipts.empty(),
			   "clearing the body as presentation emits no semantic edit");
		expect(editor->mutation_status_text().contains(
				   QStringLiteral("no longer available")),
			   "the typed invalidation replaces the stale mutation status");
		editor->present(qt::review_draft_editor_clear_reason::composition_retired);
		expect(editor->target_label()->text().isEmpty()
				   && editor->mutation_status_text().isEmpty(),
			   "retiring composition clears its target and status authority");

		delete editor;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);

	test_publishing_emits_an_intent_and_changes_nothing();
	test_typing_emits_a_semantic_edit_and_waits_for_projection();
	test_receipt_actions_emit_intents_and_change_nothing();
	test_enablement_follows_the_projection_not_the_keystrokes();
	test_the_publication_target_is_shown_before_publishing();
	test_every_action_and_widget_is_distinctly_named();
	test_typed_clear_removes_stale_selection_bound_state();
	test_the_workshop_shows_both_texts_at_once_on_two_surfaces();
	test_accepting_a_revision_emits_exactly_one_command_and_stores_nothing();
	test_rejecting_a_revision_leaves_the_original_byte_identical();
	test_a_revision_is_accepted_only_onto_the_draft_it_names();
	test_a_typed_clear_takes_the_proposal_with_it();
	test_the_editor_holds_no_provider_endpoint_store_or_process();

	std::cout << "review-draft-editor-tests:pass\n";
	return EXIT_SUCCESS;
}
