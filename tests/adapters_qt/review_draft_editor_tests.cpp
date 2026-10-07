#include "bpr/application/agent_comment_workshop_host.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/suggestion_mark.hpp"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
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

	void test_empty_panels_wait_for_draft_or_proposal()
	{
		intent_log log;
		auto*      editor = editor_for(log);
		editor->set_revisioned_dispatch([](auto) { });
		expect(editor->body_editor()->isHidden() && editor->workshop_original_editor()->isHidden()
				   && editor->workshop_proposal_editor()->isHidden() && !editor->reload_draft_action()->isVisible()
				   && !editor->reapply_draft_action()->isVisible(),
			   "no draft shows no empty panels or unusable revision controls");
		editor->present(core::revisioned_review_draft{ a_draft("editable"), 1, core::review_draft_scope_id{ 1 } });
		expect(!editor->body_editor()->isHidden() && editor->workshop_original_editor()->isHidden()
				   && editor->workshop_proposal_editor()->isHidden(),
			   "a draft shows its body without empty workshop panels");
		delete editor;
	}

	void test_own_observation_can_precede_edit_acknowledgement()
	{
		intent_log                                         legacy;
		auto*                                              editor = editor_for(legacy);
		std::vector<core::revisioned_review_draft_command> commands;
		editor->set_revisioned_dispatch([&](auto aCommand) { commands.push_back(std::move(aCommand)); });
		const auto initial = core::revisioned_review_draft{ a_draft("before"), 1, core::review_draft_scope_id{ 1 } };
		editor->present(initial);
		editor->body_editor()->setPlainText(QStringLiteral("after"));
		expect(commands.size() == 1, "edit command is pending");
		auto committed       = initial;
		committed.Revision   = 2;
		committed.Draft.Body = "after";
		editor->present(committed);
		expect(editor->rejected_candidate().has_value(), "observation alone retains the candidate");
		editor->acknowledge(commands.front(), committed);
		expect(!editor->rejected_candidate() && editor->shown_draft() && editor->shown_draft()->Body == "after", "explicit acknowledgement resolves own earlier observation");
		delete editor;
	}

	/// \brief [[BPR-1298]]. A CRLF body written elsewhere keeps its line
	/// endings through the editor: unedited, typed in, and relabelled by the
	/// label controls' typed edit and toggle.
	void test_a_crlf_body_keeps_its_line_endings()
	{
		intent_log                                         legacy;
		auto*                                              editor = editor_for(legacy);
		std::vector<core::revisioned_review_draft_command> commands;
		editor->set_revisioned_dispatch([&](auto aCommand) { commands.push_back(std::move(aCommand)); });
		const auto body    = std::string{ "question: Why copy here?\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\ntail\nmixed" };
		const auto initial = core::revisioned_review_draft{ a_draft(body), 1, core::review_draft_scope_id{ 1 } };
		editor->present(initial);
		expect(editor->body_text() == body && commands.empty(), "a presented CRLF body reads back byte for byte, and presenting sends nothing");

		auto* const labels = editor->label_controls();
		expect(labels != nullptr && labels->apply(core::feedback_header_edit{ .Change = core::feedback_label_change::set, .Label = core::feedback_label::issue }),
			   "the label controls relabel the draft");
		expect(!commands.empty() && commands.back().Command.Body == "issue: Why copy here?\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\ntail\nmixed",
			   "a relabel keeps every CR after the header, mixed endings included");

		auto accepted       = initial;
		accepted.Revision   = 2;
		accepted.Draft.Body = commands.back().Command.Body;
		editor->acknowledge(commands.back(), accepted);
		labels->blocking_toggle()->click();
		expect(commands.back().Command.Body == "issue (blocking): Why copy here?\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\ntail\nmixed",
			   "the blocking toggle keeps every CR");

		accepted.Revision   = 3;
		accepted.Draft.Body = commands.back().Command.Body;
		editor->acknowledge(commands.back(), accepted);
		auto cursor = editor->body_editor()->textCursor();
		cursor.movePosition(QTextCursor::End);
		cursor.insertText(QStringLiteral(" text\nnew line"));
		expect(commands.back().Command.Body == "issue (blocking): Why copy here?\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\ntail\nmixed text\r\nnew line",
			   "typed lines keep the unchanged lines' endings and take the body's own");
		delete editor;
	}

	void test_confirmed_publication_retires_only_matching_candidate()
	{
		intent_log legacy;
		auto*      editor = editor_for(legacy);
		editor->set_revisioned_dispatch([](auto) { });
		const auto initial = core::revisioned_review_draft{ a_draft("before"), 1, core::review_draft_scope_id{ 1 } };
		editor->present(initial);
		editor->body_editor()->setPlainText(QStringLiteral("published body"));
		expect(editor->rejected_candidate().has_value(), "edit remains pending");
		auto published       = initial;
		published.Draft.Body = "different body";
		editor->acknowledge_published_body(published);
		expect(editor->rejected_candidate().has_value(), "different published bytes retain the candidate");
		published.Draft.Body = "published body";
		editor->acknowledge_published_body(published);
		expect(!editor->rejected_candidate(), "confirmed exact published body retires its candidate");
		delete editor;
	}

	void test_revision_conflicts_retain_exact_candidate()
	{
		intent_log                                         legacy;
		auto*                                              editor = editor_for(legacy);
		std::vector<core::revisioned_review_draft_command> commands;
		editor->set_revisioned_dispatch([&](auto aCommand) { commands.push_back(std::move(aCommand)); });
		const auto initial = core::revisioned_review_draft{ a_draft("accepted"), 1, core::review_draft_scope_id{ 1 } };
		editor->present(initial);
		const auto candidate = std::string{ "local candidate\n  exact bytes  " };
		editor->body_editor()->setPlainText(QString::fromStdString(candidate));
		expect(commands.size() == 1 && commands.back().ExpectedRevision == 1, "typed edit carries accepted revision");
		auto remote       = initial;
		remote.Revision   = 2;
		remote.Draft.Body = "other window";
		editor->present_conflict({ remote, core::revisioned_draft_refusal::conflict });
		expect(editor->body_editor()->toPlainText().toStdString() == candidate && editor->rejected_candidate()->Command.Body == candidate, "conflict retains exact editor candidate");
		editor->publish_action()->trigger();
		expect(commands.size() == 1, "conflict cannot publish");
		auto foreign       = remote;
		foreign.Scope      = core::review_draft_scope_id{ 2 };
		foreign.Draft.Body = candidate;
		editor->present(foreign);
		expect(editor->rejected_candidate() && editor->rejected_candidate()->Scope == core::review_draft_scope_id{ 1 } && !editor->reapply_draft_action()->isEnabled(), "different owner with same draft ID cannot acknowledge or reapply old candidate");
		editor->reapply_draft_action()->trigger();
		expect(commands.size() == 1, "foreign scope never receives retained candidate command");
		editor->present(qt::review_draft_editor_clear_reason::selection_cleared);
		auto other       = remote;
		other.Draft.Id   = core::review_draft_id{ 2 };
		other.Draft.Body = "different draft";
		editor->present(other);
		editor->present(initial);
		expect(editor->body_editor()->toPlainText().toStdString() == candidate, "selection replacement and late state cannot erase candidate");
		editor->present(remote);
		editor->reapply_draft_action()->trigger();
		expect(commands.size() == 2 && commands.back().ExpectedRevision == 2 && commands.back().Command.Body == candidate, "explicit reapply uses latest revision and exact candidate");
		auto reapplied       = remote;
		reapplied.Revision   = 3;
		reapplied.Draft.Body = candidate;
		editor->present(reapplied);
		editor->acknowledge(commands.back(), reapplied);
		expect(!editor->rejected_candidate() && editor->mutation_status_text().isEmpty() && !editor->reload_draft_action()->isVisible() && !editor->reapply_draft_action()->isVisible(), "exact accepted reapply releases candidate and stale conflict controls");
		editor->body_editor()->setPlainText(QStringLiteral("same text conflict"));
		auto matching       = reapplied;
		matching.Revision   = 4;
		matching.Draft.Body = "same text conflict";
		editor->present(matching);
		editor->reapply_draft_action()->trigger();
		const auto sameText = commands.back();
		editor->present(matching);
		expect(editor->rejected_candidate().has_value(), "unsolicited same-text state cannot acknowledge a command");
		auto wrong             = sameText;
		wrong.ExpectedRevision = 3;
		editor->acknowledge(wrong, matching);
		expect(editor->rejected_candidate().has_value(), "wrong command acknowledgement preserves candidate");
		editor->acknowledge(sameText, matching);
		expect(!editor->rejected_candidate() && editor->publish_action()->isEnabled(), "exact successful no-op reapply resolves conflict at unchanged revision");
		editor->body_editor()->setPlainText(QStringLiteral("another candidate"));
		auto newer       = reapplied;
		newer.Revision   = 5;
		newer.Draft.Body = "new accepted";
		editor->present(newer);
		editor->reload_draft_action()->trigger();
		expect(!editor->rejected_candidate() && editor->body_editor()->toPlainText() == QStringLiteral("new accepted"), "explicit reload alone replaces conflicting text");
		editor->body_editor()->setPlainText(QStringLiteral("old scope text"));
		auto replacement       = newer;
		replacement.Scope      = core::review_draft_scope_id{ 2 };
		replacement.Revision   = 1;
		replacement.Draft.Body = "new scope accepted";
		editor->present(replacement);
		const auto commandCount = commands.size();
		editor->reload_draft_action()->trigger();
		expect(!editor->rejected_candidate() && editor->body_editor()->toPlainText() == QStringLiteral("new scope accepted") && commands.size() == commandCount, "explicit reload can discard old-scope candidate without transferring its text");
		expect(legacy.Drafts.empty(), "revisioned editor never falls through legacy command dispatch");
		delete editor;
	}

	// The rule: an action emits an intent and changes nothing. A widget that
	// updated itself optimistically would show a comment as published while the
	// request was in flight, and keep showing it if the request failed.
	void test_publishing_emits_an_intent_and_changes_nothing()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft("Needs a test."));
		editor->setMaximumHeight(220);
		editor->resize(360, 220);
		editor->show();
		QApplication::processEvents();
		auto* const publish = editor->findChild<QToolButton*>(QStringLiteral("reviewPublishButton"));
		auto* const discard = editor->findChild<QToolButton*>(QStringLiteral("reviewDiscardButton"));
		expect(publish != nullptr && discard != nullptr,
			   "publication actions have real button controls");
		expect(publish->isVisible() && discard->isVisible()
				   && QRect(0, 0, editor->width(), 220).contains(publish->geometry())
				   && QRect(0, 0, editor->width(), 220).contains(discard->geometry()),
			   "publication controls fit the composed editor height");
		expect(publish->text() == QStringLiteral("Publish")
				   && publish->focusPolicy() == Qt::StrongFocus,
			   "Publish has a readable label and keyboard focus");
		expect(editor->grab().save(QStringLiteral("review-draft-publication-controls.png")),
			   "public editor rendering is available for inspection");

		const auto before = editor->shown_draft();
		publish->click();

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
		expect(!publish->isEnabled() && !discard->isEnabled(),
			   "visible controls follow the publishing projection");
		publish->click();
		expect(log.Drafts.size() == 1U, "disabled Publish cannot emit a second intent");

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

	// [[BPR-0657]]. The thread a draft answers is named beside the target,
	// and only while the shown draft answers one.
	void test_the_thread_a_reply_answers_is_named_only_while_it_does()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		expect(!editor->reply_target_label()->isVisibleTo(editor), "no thread is named before one is described");
		editor->describe_reply_target(QStringLiteral("Replying to octo-reviewer's thread"));
		expect(!editor->reply_target_label()->isVisibleTo(editor),
			   "a description alone names nothing: the shown draft answers no thread yet");
		auto root = core::review_comment_draft{ .Id = core::review_draft_id{ 5U } };
		editor->present(root);
		expect(!editor->reply_target_label()->isVisibleTo(editor), "a root draft names no thread");
		auto reply          = root;
		reply.ReplyToThread = "PRRT_fixture_1";
		editor->present(reply);
		expect(editor->reply_target_label()->isVisibleTo(editor)
				   && editor->reply_target_label()->text() == QStringLiteral("Replying to octo-reviewer's thread")
				   && !editor->reply_target_label()->accessibleName().isEmpty(),
			   "a draft answering a thread shows the thread it answers, announced too");
		editor->present(root);
		expect(!editor->reply_target_label()->isVisibleTo(editor), "back on a root draft the thread is gone");
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
			editor->clarify_action(),
			editor->shorten_action(),
			editor->make_actionable_action(),
			editor->phrase_as_a_question_action(),
			editor->make_more_direct_action(),
			editor->check_the_premise_action(),
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

	void test_workshop_acceptance_is_explicit_during_its_dispatch()
	{
		auto  legacy             = intent_log{ };
		auto* editor             = editor_for(legacy);
		auto  explicitAcceptance = false;
		auto  ordinaryEdit       = false;
		editor->set_revisioned_dispatch(
			[&](const core::revisioned_review_draft_command& aCommand) {
				if (editor->workshop_accepting(aCommand)) {
					explicitAcceptance = true;
				} else {
					ordinaryEdit = true;
				}
			});
		editor->present(core::revisioned_review_draft{
			.Draft    = a_draft(original_body),
			.Revision = 4U,
			.Scope    = core::review_draft_scope_id{ 8U },
		});
		editor->present(a_proposal());
		editor->accept_revision_action()->trigger();
		expect(explicitAcceptance && !ordinaryEdit,
			   "the accept-revision dispatch is explicitly identifiable only while it runs");

		explicitAcceptance = false;
		editor->present(a_proposal());
		editor->reject_revision_action()->trigger();
		editor->body_editor()->setPlainText(QString::fromUtf8(proposed_body));
		expect(!explicitAcceptance && ordinaryEdit,
			   "typing the rejected proposal body remains an ordinary edit");

		delete editor;
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

	/// \brief The one shipped path, unfaked at both ends.
	///
	/// \details The editor's control emits a request; the composed
	/// `application::agent_comment_workshop_host` turns it into a proposal;
	/// the editor shows it. Nothing here stands in for the host - a stub that
	/// answered the request itself would prove the button works and leave the
	/// path it is supposed to drive untested.
	struct workshop_path
	{
		struct recording_projection final
			: application::agent_comment_workshop_projection_port
		{
			qt::review_draft_editor* Editor   = nullptr;
			std::size_t              Refusals = 0U;

			void present(
				const application::agent_comment_workshop_proposal& aProposal)
				override
			{
				Editor->present(qt::comment_workshop_proposal{
					.Draft     = aProposal.Draft,
					.Original  = aProposal.Original,
					.Proposed  = aProposal.Proposed,
					.Operation = aProposal.Operation,
				});
			}

			void refuse(application::agent_comment_workshop_refusal) override
			{
				++Refusals;
			}
		};

		recording_projection                      Projection;
		application::agent_comment_workshop_host  Host{ Projection };
		std::vector<qt::comment_workshop_request> Requests;
	};

	/// The revision the agent would have sent back, made distinct per
	/// operation so a proposal shown under the wrong one is visible.
	[[nodiscard]] auto revision_for(const core::workshop_operation aOperation)
		-> std::string
	{
		return "Revised by " + std::string{ core::stable_name(aOperation) };
	}

	/// \brief Each of the six drives one proposal through the one path.
	void test_each_operation_drives_one_proposal_through_the_one_path()
	{
		auto  log              = intent_log{ };
		auto* editor           = editor_for(log);
		auto  path             = workshop_path{ };
		path.Projection.Editor = editor;
		editor->set_workshop_dispatch(
			[&path, editor](qt::comment_workshop_request aRequest) {
				path.Requests.push_back(aRequest);
				static_cast<void>(path.Host.propose(
					*editor->shown_draft(),
					core::review_draft_command{
						.Id   = aRequest.Draft,
						.Kind = core::review_draft_command_kind::edit,
						.Body = revision_for(aRequest.Operation),
					},
					aRequest.Operation));
			});
		editor->present(a_draft(original_body));

		const auto controls = std::array{
			editor->clarify_action(),
			editor->shorten_action(),
			editor->make_actionable_action(),
			editor->phrase_as_a_question_action(),
			editor->make_more_direct_action(),
			editor->check_the_premise_action(),
		};
		static_assert(controls.size() == core::workshop_operation_count);

		for (auto index = std::size_t{ 0U }; index < controls.size(); ++index) {
			const auto asked = static_cast<core::workshop_operation>(index);
			expect(
				controls[index] == editor->workshop_action(asked)
					&& controls[index]->isEnabled(),
				"the control for each declared operation is the one indexed by "
				"it, and it is offered while there is a draft to revise");

			controls[index]->trigger();

			expect(
				path.Requests.size() == index + 1U
					&& path.Requests.back().Operation == asked
					&& path.Requests.back().Draft == editor->shown_draft()->Id,
				"pressing it asks for exactly that operation, on the draft the "
				"editor is showing");
			expect(
				editor->shown_proposal().has_value()
					&& editor->shown_proposal()->Operation
						== std::optional{ asked }
					&& editor->workshop_proposal_text().toStdString()
						== revision_for(asked)
					&& editor->workshop_original_text().toStdString()
						== original_body,
				"and one proposal comes back through the host, naming the "
				"operation, with the reviewer's own text still beside it");
			expect(
				editor->workshop_operation_text().contains(
					controls[index]->text()),
				"the surface says which operation produced what is shown");
			expect(
				editor->statistics().WorkshopRequests == index + 1U
					&& editor->statistics().WorkshopPresentations == index + 1U,
				"one request, one presentation, per press");
		}

		expect(
			path.Projection.Refusals == 0U
				&& path.Host.statistics().Proposals
					== core::workshop_operation_count,
			"six presses reached the one host six times");
		expect(
			log.Drafts.empty()
				&& editor->body_editor()->toPlainText().toStdString()
					== original_body,
			"and none of the six wrote a comment, published one, or touched "
			"the reviewer's own text - accepting is still the only gesture "
			"that emits an edit");

		delete editor;
	}

	/// \brief One press, one turn: the six close until that turn answers.
	///
	/// \details BPR-0533 part two. A press now starts an agent turn, and an
	/// agent turn takes time. What the reviewer must not be able to do in that
	/// window is ask again and be answered later, or twice: the controls shut,
	/// say why on themselves, and reopen when the answer arrives. Refused on
	/// the surface that asked, because the launch panel deliberately keeps a
	/// running invocation's own status while it runs.
	void test_a_press_closes_the_six_until_that_press_is_answered()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		auto  asked  = std::vector<qt::comment_workshop_request>{ };
		// A dispatch that starts a turn and does not answer inside it, which is
		// what a real invocation does.
		editor->set_workshop_dispatch(
			[&asked](qt::comment_workshop_request aRequest) {
				asked.push_back(aRequest);
			});
		editor->present(a_draft(original_body));

		editor->shorten_action()->trigger();
		expect(
			asked.size() == 1U
				&& asked.front().Operation == core::workshop_operation::shorten
				&& editor->statistics().WorkshopRequests == 1U,
			"the press asks once, for the operation pressed");
		expect(
			editor->workshop_operation_text().contains(
				editor->shorten_action()->text()),
			"and the surface says what was asked while it waits");

		for (auto index = std::size_t{ 0U };
			 index < core::workshop_operation_count;
			 ++index) {
			auto* const control = editor->workshop_action(
				static_cast<core::workshop_operation>(index));
			expect(
				control->isVisible() && !control->isEnabled()
					&& control->toolTip().contains(QStringLiteral("already running")),
				"every operation is shut while that turn is unanswered, and each "
				"one says why on itself rather than somewhere else");
			control->trigger();
		}
		expect(
			asked.size() == 1U && editor->statistics().WorkshopRequests == 1U,
			"and nothing pressed in that window is asked, then or later - a "
			"second question is refused, never queued");

		// The answer to that turn, through the one path.
		editor->present(qt::comment_workshop_proposal{
			.Draft     = editor->shown_draft()->Id,
			.Original  = original_body,
			.Proposed  = revision_for(core::workshop_operation::shorten),
			.Operation = core::workshop_operation::shorten,
		});
		expect(
			editor->clarify_action()->isEnabled()
				&& editor->shorten_action()->isEnabled()
				&& editor->workshop_operation_text().contains(
					editor->shorten_action()->text()),
			"the answer reopens the offer and names which operation produced it");

		editor->shorten_action()->trigger();
		expect(
			asked.size() == 2U,
			"and the next question may be asked");

		// Rejecting an outstanding proposal is the other way out, and it must
		// leave the controls open rather than shut on a turn nobody is running.
		editor->reject_revision_action()->trigger();
		expect(
			editor->clarify_action()->isEnabled(),
			"clearing the workshop reopens it too");

		delete editor;
	}

	/// \brief A turn that ends without a revision reopens the six.
	///
	/// \details BPR-0538. The press shuts the six and says a turn is running.
	/// When that turn ends having proposed nothing, the statement stops being
	/// true, and until this settlement existed the six stayed shut behind it
	/// until the reviewer edited the draft. The reason is read where the
	/// proposal would have been read, because that is where the reviewer is
	/// looking after pressing.
	void test_a_turn_that_ends_without_a_revision_reopens_the_six()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		auto  asked  = std::vector<qt::comment_workshop_request>{ };
		editor->set_workshop_dispatch(
			[&asked](qt::comment_workshop_request aRequest) {
				asked.push_back(aRequest);
			});
		editor->present(a_draft(original_body));

		editor->clarify_action()->trigger();
		for (auto index = std::size_t{ 0U };
			 index < core::workshop_operation_count;
			 ++index) {
			expect(
				!editor->workshop_action(
						   static_cast<core::workshop_operation>(index))
					 ->isEnabled(),
				"the press shuts all six while its turn runs");
		}

		// The turn ended, and it produced nothing.
		editor->present(qt::comment_workshop_settlement{
			.Draft  = editor->shown_draft()->Id,
			.Reason = "the turn proposed no revision",
		});
		for (auto index = std::size_t{ 0U };
			 index < core::workshop_operation_count;
			 ++index) {
			auto* const control = editor->workshop_action(
				static_cast<core::workshop_operation>(index));
			expect(
				control->isEnabled() && control->isVisible(),
				"and all six reopen when it ends, without waiting for a "
				"proposal that is not coming");
		}
		expect(
			editor->workshop_operation_text().toStdString()
					== "No revision: the turn proposed no revision"
				&& !editor->shown_proposal().has_value()
				&& editor->workshop_proposal_text().isEmpty(),
			"the surface says why there is nothing to read, where the "
			"before/after would have been, and proposes nothing");

		editor->clarify_action()->trigger();
		expect(
			asked.size() == 2U,
			"and the next question may be asked");

		// A settlement for a comment the reviewer has moved away from is not
		// this comment's, and must not reopen or overwrite anything here.
		editor->present(qt::comment_workshop_proposal{
			.Draft     = editor->shown_draft()->Id,
			.Original  = original_body,
			.Proposed  = revision_for(core::workshop_operation::clarify),
			.Operation = core::workshop_operation::clarify,
		});
		editor->present(qt::comment_workshop_settlement{
			.Draft  = core::review_draft_id{ editor->shown_draft()->Id.value() + 1U },
			.Reason = "another comment's turn ended",
		});
		expect(
			editor->workshop_operation_text().contains(
				editor->clarify_action()->text())
				&& editor->shown_proposal().has_value(),
			"a settlement naming another draft says nothing here");

		delete editor;
	}

	/// \brief Without a draft the six refuse in the open.
	void test_the_six_operations_refuse_visibly_without_a_draft()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		auto  asked  = std::vector<qt::comment_workshop_request>{ };
		editor->set_workshop_dispatch(
			[&asked](qt::comment_workshop_request aRequest) {
				asked.push_back(aRequest);
			});

		for (auto index = std::size_t{ 0U };
			 index < core::workshop_operation_count;
			 ++index) {
			auto* const control = editor->workshop_action(
				static_cast<core::workshop_operation>(index));
			expect(
				control != nullptr && control->isVisible()
					&& !control->isEnabled() && !control->toolTip().isEmpty(),
				"with no draft selected every operation is shown, disabled, "
				"and says why - a control that vanished would read as a "
				"workshop that does not exist");
			control->trigger();
		}
		expect(
			asked.empty() && editor->statistics().WorkshopRequests == 0U
				&& editor->workshop_operation_text().isEmpty(),
			"and pressing them asks nobody anything");

		// A published comment is not the reviewer's to revise either, and the
		// refusal is the same one, said the same way.
		auto published  = a_draft(original_body);
		published.State = core::review_draft_state::published;
		editor->present(published);
		expect(
			!editor->clarify_action()->isEnabled()
				&& !editor->check_the_premise_action()->isEnabled(),
			"a draft that is no longer the reviewer's to edit is not one the "
			"workshop offers to revise");

		editor->present(a_draft(original_body));
		expect(
			editor->clarify_action()->isEnabled()
				&& !editor->clarify_action()->toolTip().isEmpty(),
			"and the offer returns with the draft, still saying what it is");

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
		const auto members                                    = qt::review_draft_editor_members{ };
		const auto& [applying,
					 loadedBody,
					 workshopAsked,
					 draftDispatch,
					 receiptDispatch,
					 workshopDispatch,
					 presentationObserver,
					 body,
					 targetLabel,
					 replyTarget,
					 rangeTarget,
					 mutationStatus,
					 publicationStatus,
					 workshopOriginal,
					 workshopProposal,
					 workshopOperation,
					 publish, retryPublication,
					 discard,
					 markRead,
					 accept,
					 exclude,
					 acceptRevision,
					 rejectRevision,
					 workshopOperations,
					 labels,
					 shownDraft,
					 shownReceipt,
					 shownProposal,
					 statistics, revisioned,
					 suggestion, replyPlacement, refusedLine] = members;
		// [[BPR-1084]]. The suggested-change mark, the reply's projected
		// placement and the line a refusal named: presentation and facts.
		static_assert(std::is_same_v<decltype(suggestion), qt::suggestion_mark* const>);
		static_assert(std::is_same_v<decltype(replyPlacement), const std::optional<core::suggestion_placement_observation>>);
		static_assert(std::is_same_v<decltype(refusedLine), const std::optional<core::source_line_ref>>);
		static_assert(std::is_same_v<decltype(revisioned), const qt::revisioned_draft_editor_state>);
		static_assert(std::is_same_v<decltype(retryPublication), const bool>);
		// [[BPR-1212]]. The shown draft's lines when it covers several: text.
		static_assert(std::is_same_v<decltype(rangeTarget), QLabel* const>);
		static_assert(std::is_same_v<decltype(presentationObserver), const std::function<void()>>);
		// [[BPR-1329]]. What the last Publish came to, in words: presentation.
		static_assert(std::is_same_v<decltype(publicationStatus), const QString>);

		static_assert(std::is_same_v<decltype(applying), const bool>);
		// [[BPR-1298]]. The body last presented, for its line endings: text.
		static_assert(std::is_same_v<decltype(loadedBody), const std::string>);
		static_assert(std::is_same_v<decltype(workshopAsked), const bool>);
		static_assert(
			std::is_same_v<
				decltype(draftDispatch),
				const qt::review_draft_dispatch>);
		static_assert(
			std::is_same_v<
				decltype(receiptDispatch),
				const qt::file_receipt_dispatch>);
		static_assert(
			std::is_same_v<
				decltype(workshopDispatch),
				const qt::comment_workshop_dispatch>);
		static_assert(std::is_same_v<decltype(body), QPlainTextEdit* const>);
		static_assert(std::is_same_v<decltype(targetLabel), QLabel* const>);
		static_assert(std::is_same_v<decltype(mutationStatus), QLabel* const>);
		static_assert(
			std::is_same_v<decltype(workshopOriginal), QPlainTextEdit* const>);
		static_assert(
			std::is_same_v<decltype(workshopProposal), QPlainTextEdit* const>);
		static_assert(
			std::is_same_v<decltype(workshopOperation), QLabel* const>);
		static_assert(std::is_same_v<decltype(publish), QAction* const>);
		static_assert(std::is_same_v<decltype(discard), QAction* const>);
		static_assert(std::is_same_v<decltype(markRead), QAction* const>);
		static_assert(std::is_same_v<decltype(accept), QAction* const>);
		static_assert(std::is_same_v<decltype(exclude), QAction* const>);
		static_assert(std::is_same_v<decltype(acceptRevision), QAction* const>);
		static_assert(std::is_same_v<decltype(rejectRevision), QAction* const>);
		static_assert(std::is_same_v<decltype(replyTarget), QLabel* const>);
		// [[BPR-1083]]. The label controls edit the body's text and hold no
		// provider, endpoint, store or process of their own.
		static_assert(std::is_same_v<decltype(labels), qt::feedback_label_controls* const>);
		// One control per declared operation and no more: the array's own type
		// says there are six, so a seventh operation offered from somewhere
		// else fails here rather than appearing beside them.
		static_assert(std::is_same_v<
					  decltype(workshopOperations),
					  const std::array<QAction*, core::workshop_operation_count>>);
		static_assert(core::workshop_operation_count == 6U);
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
			!applying && !draftDispatch && !receiptDispatch && !presentationObserver
				&& body == nullptr && targetLabel == nullptr
				&& mutationStatus == nullptr && workshopOriginal == nullptr
				&& workshopProposal == nullptr && publish == nullptr
				&& discard == nullptr && markRead == nullptr
				&& accept == nullptr && exclude == nullptr
				&& acceptRevision == nullptr && rejectRevision == nullptr
				&& !workshopDispatch && workshopOperation == nullptr
				&& std::ranges::all_of(
					workshopOperations,
					[](const QAction* aControl) { return aControl == nullptr; })
				&& !shownDraft.has_value() && !shownReceipt.has_value()
				&& !shownProposal.has_value()
				&& statistics == qt::review_draft_editor_statistics{ }
				&& suggestion == nullptr && !replyPlacement && !refusedLine,
			"the editor's complete member set is three dispatch callbacks, the "
			"accepted projections, seven counters, and the widgets that show "
			"them - there is no provider to hold, no agent to ask, nowhere to "
			"send anything, and nothing to write to");
	}

	void test_left_new_comment_keeps_text_without_offering_publish()
	{
		auto  log         = intent_log{ };
		auto* editor      = editor_for(log);
		auto  draft       = a_draft("Keep these exact bytes.");
		draft.Anchor.Side = core::diff_side::left;
		editor->present(draft);
		expect(!editor->publish_action()->isEnabled()
				   && editor->publish_action()->toolTip().contains(QStringLiteral("right, current-head"))
				   && editor->body_editor()->toPlainText() == QStringLiteral("Keep these exact bytes.")
				   && !editor->body_editor()->isReadOnly() && editor->discard_action()->isEnabled(),
			   "an unsupported root retains editable text and explains why Publish is unavailable");
		editor->publish_action()->trigger();
		expect(log.Drafts.empty(), "a left-side root emits no publication intent");
		draft.ReplyToThread = "PRRT_fixture_1";
		editor->present(draft);
		expect(editor->publish_action()->isEnabled(), "a left-side reply still offers explicit Publish");
		editor->publish_action()->trigger();
		expect(log.Drafts.size() == 1U && log.Drafts.back().Kind == core::review_draft_command_kind::begin_publish,
			   "a left-side reply emits the same explicit publication intent");
		draft.ReplyToThread.reset();
		draft.Anchor.Side = core::diff_side::right;
		editor->present(draft);
		expect(editor->publish_action()->isEnabled(), "a right-side root remains publishable");
		delete editor;
	}

	void test_target_and_server_failures_are_distinct()
	{
		auto  log    = intent_log{ };
		auto* editor = editor_for(log);
		editor->present(a_draft("Retain this reply."));
		editor->present(application::review_mutation_failure::target_unavailable);
		const auto invisible = editor->mutation_status_text();
		expect(invisible.contains(QStringLiteral("account cannot see")) && invisible.contains(QStringLiteral("check access")),
			   "an invisible target names the account access problem");
		editor->present(application::review_mutation_failure::malformed_response);
		const auto malformed = editor->mutation_status_text();
		expect(malformed != invisible && malformed.contains(QStringLiteral("invalid publication response")),
			   "a malformed server response is visibly distinct from an invisible target");
		expect(editor->shown_draft()->Body == "Retain this reply." && log.Drafts.empty(),
			   "presenting either diagnostic preserves the draft and emits no action");
		editor->present(application::review_mutation_failure::incompatible_body);
		expect(editor->mutation_status_text().contains(QStringLiteral("needs a supported inline line anchor"))
				   && !editor->mutation_status_text().contains(QStringLiteral("file comment"))
				   && editor->shown_draft()->Body == "Retain this reply." && log.Drafts.empty(),
			   "line-only body refusal explains the limitation while retaining text and dispatching no operation");
		delete editor;
	}

	void test_retry_preserves_the_frozen_publication_revision()
	{
		intent_log                                         legacy;
		auto*                                              editor = editor_for(legacy);
		std::vector<core::revisioned_review_draft_command> commands;
		editor->set_revisioned_dispatch([&](auto aCommand) { commands.push_back(std::move(aCommand)); });
		auto draft  = a_draft("The exact previously submitted body.");
		draft.State = core::review_draft_state::publishing;
		editor->present(core::revisioned_review_draft{ draft, 7, core::review_draft_scope_id{ 4 } });
		expect(!editor->publish_action()->isEnabled(), "an in-flight publication cannot be repeated");
		editor->allow_publication_retry(true);
		expect(editor->publish_action()->isEnabled() && editor->body_editor()->isReadOnly()
				   && !editor->discard_action()->isEnabled(),
			   "an uncertain publication can retry without editing its frozen body");
		editor->publish_action()->trigger();
		expect(commands.size() == 1 && commands.front().ExpectedRevision == 7
				   && commands.front().Scope == core::review_draft_scope_id{ 4 }
				   && commands.front().Command.Kind == core::review_draft_command_kind::begin_publish
				   && editor->shown_draft()->Body == draft.Body,
			   "retry emits the original revision and leaves the body untouched");
		editor->allow_publication_retry(false);
		expect(!editor->publish_action()->isEnabled(), "the host closes retry while its request is in flight");
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

	// [[BPR-1084]]. Before Publish, a small mark beside it says whether the
	// body's suggested change is GitHub's native suggestion on this line or
	// text only, from `core::decide_suggestion_applicability`. Prose shows
	// nothing; the mark gates nothing, emits nothing and changes no byte.
	void test_a_suggested_change_is_marked_before_publish()
	{
		auto        log    = intent_log{ };
		auto* const editor = editor_for(log);
		auto* const mark   = editor->suggestion_mark_widget();
		const auto  state  = [mark] { return mark->property("suggestionApplicability").toString().toStdString(); };
		const auto  shown  = [editor, mark] { return mark->isVisibleTo(editor); };
		expect(mark != nullptr && !shown(), "no draft, no mark");

		const auto patch = std::string{ "issue (blocking): Clamp the width.\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n```\n" };
		const auto prose = std::string{ "suggestion: Consider clamping the width to one pixel." };
		editor->present(a_draft(prose));
		expect(!shown(), "a suggestion label with no patch shows no mark");
		editor->present(a_draft(patch));
		expect(shown() && state() == "native" && mark->accessibleName() == QStringLiteral("Suggested change, native")
				   && mark->toolTip().contains(QStringLiteral("GitHub's own suggestion")),
			   "a patch on a new right-side line is native, said in the tooltip");
		expect(editor->publish_action()->isEnabled() && editor->body_editor()->toPlainText().toStdString() == patch, "the mark gates nothing and keeps every byte");

		// Typing takes the mark with the text, before any projection.
		const auto drafts = log.Drafts.size();
		editor->body_editor()->setPlainText(QString::fromStdString(prose));
		expect(!shown(), "removing the fence removes the mark");
		editor->body_editor()->setPlainText(QString::fromStdString(patch));
		expect(shown() && state() == "native", "typing it back brings the mark back");
		expect(log.Drafts.size() == drafts + 2U && std::ranges::all_of(log.Drafts, [](const auto& aCommand) { return aCommand.Kind == core::review_draft_command_kind::edit; }),
			   "the mark emits nothing of its own: only the typed edits went out");

		// GitHub refused an inline comment on this line: the next Publish is
		// refused for a suggestion, and the mark says so first.
		editor->present(application::review_mutation_failure::incompatible_body);
		expect(shown() && state() == "textual_only" && mark->property("suggestionReason").toString() == QStringLiteral("inline_comment_unsupported")
				   && mark->accessibleName() == QStringLiteral("Suggested change, text only") && mark->toolTip().contains(QStringLiteral("refused an inline comment")),
			   "after a refused inline comment the same line is text only");
		expect(editor->body_editor()->toPlainText().toStdString() == patch, "the refused text is kept as written");
		auto other        = a_draft(patch);
		other.Anchor.Line = 9U;
		editor->present(other);
		expect(state() == "native", "another line is asked again; the refusal named one line");
		editor->present(a_draft(patch));
		expect(state() == "textual_only", "back on the refused line it is still text only");

		auto base        = a_draft(patch);
		base.Anchor.Side = core::diff_side::left;
		base.Anchor.Line = 3U;
		editor->present(base);
		expect(shown() && state() == "textual_only" && mark->property("suggestionReason").toString() == QStringLiteral("base_side"), "the base side is text only");

		// A reply goes where its thread is; with no projected fact nothing is claimed.
		auto reply          = a_draft(patch);
		reply.Anchor.Line   = 20U;
		reply.ReplyToThread = "PRRT_fixture_1";
		editor->present(reply);
		expect(!shown(), "a reply with no projected thread facts claims nothing");
		editor->describe_reply_placement(core::suggestion_placement_observation{ .Subject = core::suggestion_subject::outdated_line });
		expect(shown() && state() == "textual_only" && mark->toolTip().contains(QStringLiteral("outdated")), "a reply in an outdated thread is text only");
		editor->describe_reply_placement(core::suggestion_placement_observation{ .Subject = core::suggestion_subject::file });
		expect(state() == "textual_only" && mark->property("suggestionReason").toString() == QStringLiteral("file_subject"), "a reply to a file comment is text only");
		editor->describe_reply_placement(core::suggestion_placement_observation{ .Subject = core::suggestion_subject::current_line, .Side = core::diff_side::right });
		expect(state() == "native", "a reply in a thread on a current line is native");
		expect(editor->body_editor()->toPlainText().toStdString() == patch, "no presentation changed the text");

		auto published  = reply;
		published.State = core::review_draft_state::published;
		editor->present(published);
		expect(!shown(), "a published comment shows no pre-publish mark");
		editor->present(qt::review_draft_editor_clear_reason::selection_cleared);
		expect(!shown(), "cleared, no mark");
		delete editor;
	}

	/// \brief [[BPR-1084]]. The mark is a state, so it is round beside the
	/// square label marks (BPR-1083): a ring around a plus over a minus, solid
	/// for native and broken into dashes for text only. The glyph inside is the
	/// same in both, with no strike across it, and it holds at 1x and 2x.
	void test_the_suggestion_mark_is_a_ring_solid_or_dashed()
	{
		const auto ink    = QColor{ 0x11, 0x14, 0x18 };
		const auto render = [&ink](const core::suggestion_applicability aApplicability, const qreal aRatio) {
			const auto pixels = static_cast<int>(16.0 * aRatio);
			auto       image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
			image.fill(Qt::transparent);
			{
				auto painter = QPainter{ &image };
				qt::paint_suggestion_mark(painter, QRectF{ 0.0, 0.0, static_cast<qreal>(pixels), static_cast<qreal>(pixels) }, aApplicability, ink);
			}
			return image;
		};
		for (const auto ratio : { 1.0, 2.0 }) {
			const auto native = render(core::suggestion_applicability::native, ratio);
			const auto text   = render(core::suggestion_applicability::textual_only, ratio);
			const auto none   = render(core::suggestion_applicability::no_patch, ratio);
			const auto at     = [ratio](const QImage& aImage, const qreal aX, const qreal aY) {
				return aImage.pixelColor(static_cast<int>(aX * ratio), static_cast<int>(aY * ratio)).alpha();
			};
			// Round: the corners a square mark would ink stay empty.
			for (const auto& image : { native, text }) {
				expect(at(image, 0.5, 0.5) == 0 && at(image, 15.5, 0.5) == 0 && at(image, 0.5, 15.5) == 0 && at(image, 15.5, 15.5) == 0,
					   "the mark is round: a square label mark's corners stay empty");
			}
			// The ring at its own radius: unbroken for native, dashed for text only.
			auto nativeGaps = 0;
			auto textGaps   = 0;
			auto textInked  = 0;
			for (auto step = 0; step < 72; ++step) {
				const auto angle = step * 5.0 * 3.14159265358979 / 180.0;
				const auto x     = 8.0 + 7.25 * std::cos(angle);
				const auto y     = 8.0 + 7.25 * std::sin(angle);
				nativeGaps += at(native, x, y) < 96 ? 1 : 0;
				textGaps += at(text, x, y) < 96 ? 1 : 0;
				textInked += at(text, x, y) >= 96 ? 1 : 0;
			}
			expect(nativeGaps == 0, "native: the ring is unbroken");
			expect(textGaps >= 12 && textInked >= 24, "text only: the ring is dashed, gaps and dashes both");
			// The glyph: a plus over a minus, the same in both, with no strike.
			expect(at(native, 8.0, 6.5) >= 128 && at(native, 8.0, 4.5) >= 128 && at(native, 6.0, 11.5) >= 200 && at(native, 10.0, 11.5) >= 200
					   && at(native, 6.0, 6.5) >= 200 && at(native, 8.0, 10.2) < 64,
				   "a plus over a minus inside the ring, apart");
			auto same = true;
			for (auto y = 0; y < native.height(); ++y) {
				for (auto x = 0; x < native.width(); ++x) {
					const auto dx = (x + 0.5) / ratio - 8.0;
					const auto dy = (y + 0.5) / ratio - 8.0;
					if (dx * dx + dy * dy < 5.5 * 5.5) {
						same = same && native.pixel(x, y) == text.pixel(x, y);
					}
				}
			}
			expect(same, "inside the ring both are the same plus over a minus, with nothing struck across it");
			expect(none.isNull() || std::ranges::all_of(std::span{ reinterpret_cast<const QRgb*>(none.constBits()), static_cast<std::size_t>(none.width() * none.height()) }, [](const QRgb aPixel) { return qAlpha(aPixel) == 0; }),
				   "no patch draws nothing");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	test_empty_panels_wait_for_draft_or_proposal();
	test_own_observation_can_precede_edit_acknowledgement();
	test_a_crlf_body_keeps_its_line_endings();
	test_confirmed_publication_retires_only_matching_candidate();
	test_revision_conflicts_retain_exact_candidate();

	test_publishing_emits_an_intent_and_changes_nothing();
	test_typing_emits_a_semantic_edit_and_waits_for_projection();
	test_receipt_actions_emit_intents_and_change_nothing();
	test_enablement_follows_the_projection_not_the_keystrokes();
	test_the_publication_target_is_shown_before_publishing();
	test_the_thread_a_reply_answers_is_named_only_while_it_does();
	test_every_action_and_widget_is_distinctly_named();
	test_left_new_comment_keeps_text_without_offering_publish();
	test_target_and_server_failures_are_distinct();
	test_a_suggested_change_is_marked_before_publish();
	test_the_suggestion_mark_is_a_ring_solid_or_dashed();
	test_typed_clear_removes_stale_selection_bound_state();
	test_retry_preserves_the_frozen_publication_revision();
	test_the_workshop_shows_both_texts_at_once_on_two_surfaces();
	test_workshop_acceptance_is_explicit_during_its_dispatch();
	test_accepting_a_revision_emits_exactly_one_command_and_stores_nothing();
	test_rejecting_a_revision_leaves_the_original_byte_identical();
	test_a_revision_is_accepted_only_onto_the_draft_it_names();
	test_a_typed_clear_takes_the_proposal_with_it();
	test_each_operation_drives_one_proposal_through_the_one_path();
	test_a_press_closes_the_six_until_that_press_is_answered();
	test_a_turn_that_ends_without_a_revision_reopens_the_six();
	test_the_six_operations_refuse_visibly_without_a_draft();
	test_the_editor_holds_no_provider_endpoint_store_or_process();

	std::cout << "review-draft-editor-tests:pass\n";
	return EXIT_SUCCESS;
}
