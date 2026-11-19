/// \file
/// \brief [[BPR-2280]]. The pull request's description in Author's Workflow
/// tab: read, edited and saved by its author, filled by Use as description,
/// and refused, never overwritten, when GitHub changed it meanwhile. Nothing
/// here writes to GitHub: Save leaves as the gesture the composition decides.
#include "bpr/qt/author_description_editor.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "author-description-editor-tests: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	using namespace bpr;
	auto                          saves = std::vector<qt::overview_description_save>{ };
	qt::author_description_editor editor{ nullptr };
	editor.set_save_dispatch([&saves](const qt::overview_description_save& aSave) { saves.push_back(aSave); });
	editor.resize(420, 520);
	editor.show();
	QApplication::processEvents();

	// Before a read: nothing to edit, and it says so.
	expect(!editor.edit_button()->isVisible() && editor.findChild<QLabel*>(QStringLiteral("authorDescriptionEmpty"))->isVisible(), "no Edit before the description is read");
	expect(!editor.fill(QStringLiteral("x")), "nothing fills before a read");

	// Read, not editable: shown, no Edit.
	editor.present_body(std::string{ "Old body.\r\n\r\n- kept CRLF" });
	QApplication::processEvents();
	expect(editor.reader()->isVisible() && editor.reader()->original_body() == "Old body.\r\n\r\n- kept CRLF", "the description reads as Markdown");
	expect(!editor.edit_button()->isVisible() && !editor.begin_edit(), "a viewer who cannot save it gets no Edit");

	// Editable: Edit opens the editor on the body, byte for byte.
	editor.set_editable(true);
	QApplication::processEvents();
	expect(editor.edit_button()->isVisible() && editor.edit_button()->text() == QStringLiteral("Edit"), "the author gets Edit");
	editor.edit_button()->click();
	QApplication::processEvents();
	expect(editor.editing() && editor.editor()->isVisible() && editor.editor()->raw() == "Old body.\r\n\r\n- kept CRLF" && editor.original() == editor.editor()->raw(),
		   "the editor opens on the body as GitHub has it");
	expect(!editor.reader()->isVisible() && !editor.edit_button()->isVisible(), "the editor takes the reader's place");
	expect(saves.empty(), "nothing is sent by opening the editor");

	// Use as description: the draft replaces the text as one edit; nothing sent.
	const auto draft = QStringLiteral("## Summary\n\nDraws each view.\n\n## What changed\n\n- draw()\n\n## How to test\n\n1. Resize.\n");
	expect(editor.fill(draft), "Use as description fills the open editor");
	expect(editor.editor()->raw() == draft.trimmed().toStdString() && editor.original() == "Old body.\r\n\r\n- kept CRLF", "the draft is the editor's text; the read body stays the base");
	expect(editor.status()->text().contains(QStringLiteral("agent's draft")) && saves.empty(), "it says the draft is there to read; nothing is sent");
	editor.editor()->undo();
	expect(editor.editor()->raw() == "Old body.\r\n\r\n- kept CRLF", "Undo takes the draft back in one step");
	editor.editor()->redo();
	expect(editor.editor()->raw() == draft.trimmed().toStdString(), "Redo puts it back");

	// Save leaves as the gesture, from the body editing began with.
	editor.save_button()->click();
	expect(saves.size() == 1U && saves.back().Original == "Old body.\r\n\r\n- kept CRLF" && saves.back().Body == draft.trimmed().toStdString(), "Save sends the original and the text");
	editor.present_status(QStringLiteral("Saving the description to GitHub…"), true);
	expect(editor.saving() && editor.editor()->isReadOnly() && !editor.save_button()->isEnabled() && !editor.cancel_button()->isEnabled(), "the editor holds while GitHub answers");
	editor.save_button()->click();
	expect(saves.size() == 1U, "no second save while one is waiting");

	// GitHub changed it meanwhile: refused, the text kept, Reload offered.
	editor.present_status(QStringLiteral("Someone changed the description on GitHub since you began. Nothing was saved; reload it to edit from GitHub's text."));
	editor.offer_newer("Changed on GitHub.");
	QApplication::processEvents();
	expect(!editor.saving() && editor.editor()->raw() == draft.trimmed().toStdString() && editor.reload_button()->isVisible() && editor.reload_button()->isEnabled(),
		   "a conflict keeps the typed text and offers Reload");
	editor.reload_button()->click();
	QApplication::processEvents();
	expect(editor.editor()->raw() == "Changed on GitHub." && editor.original() == "Changed on GitHub." && !editor.reload_button()->isVisible(), "Reload edits from GitHub's text");
	editor.editor()->undo();
	expect(editor.editor()->raw() == draft.trimmed().toStdString(), "and the author's text is one Undo away");

	// A new read while editing leaves the edit alone.
	editor.present_body(std::string{ "Read again." });
	expect(editor.editing() && editor.original() == "Changed on GitHub.", "a read while editing does not move the base");

	// Saved: the editor closes and the reader shows what the next read brings.
	editor.close_editor();
	QApplication::processEvents();
	expect(!editor.editing() && editor.reader()->isVisible() && editor.reader()->original_body() == "Read again.", "closed, the description reads again");

	// Empty description: Write, not Edit.
	editor.present_body(std::string{ });
	QApplication::processEvents();
	expect(editor.edit_button()->isVisible() && editor.edit_button()->text() == QStringLiteral("Write") && !editor.reader()->isVisible(), "an empty description offers Write");

	// Withdrawn: an open editor closes.
	expect(editor.begin_edit(), "Write opens the editor");
	editor.set_editable(false);
	expect(!editor.editing() && !editor.edit_button()->isVisible(), "withdrawing Edit closes the editor");

	std::cout << "author-description-editor-tests:pass\n";
	return EXIT_SUCCESS;
}
