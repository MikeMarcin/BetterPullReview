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
#include <string>
#include <string_view>
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

		delete editor;
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

	std::cout << "review-draft-editor-tests:pass\n";
	return EXIT_SUCCESS;
}
