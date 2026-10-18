#include "bpr/qt/private_notes_panel.hpp"
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
int main(int argc, char** argv)
{
	using namespace bpr;
	QApplication                         app{ argc, argv };
	qt::private_notes_panel              panel;
	std::vector<qt::private_note_intent> intents;
	panel.set_dispatch([&](auto aIntent) { intents.push_back(std::move(aIntent)); });
	panel.present({ .Scope = 7, .Available = true });
	expect(!panel.new_file_action()->isEnabled(), "no selected file offers no file note");
	panel.new_review_action()->trigger();
	panel.body_editor()->setPlainText("My note");
	panel.save_action()->trigger();
	expect(intents.size() == 1 && intents.back().Scope == 7 && intents.back().Id == 0 && !intents.back().Attachment && intents.back().Text == "My note", "new note is one private scoped intent");
	panel.refuse("Store unavailable");
	expect(panel.body_editor()->toPlainText() == "My note", "refusal keeps text");
	auto shown = qt::private_notes_projection{ .Scope = 7, .Available = true, .Notes = { { .Id = 2, .Revision = 5, .Label = "Pull request", .Text = "Accepted elsewhere" } } };
	panel.present(shown);
	expect(panel.body_editor()->toPlainText() == "My note", "refresh keeps unsaved text");
	panel.notes_list()->setCurrentRow(0);
	expect(panel.notes_list()->currentRow() == -1 && panel.body_editor()->toPlainText() == "My note", "selecting another note cannot lose unsaved text");
	panel.discard_action()->trigger();
	panel.notes_list()->setCurrentRow(0);
	panel.body_editor()->setPlainText("Edited privately");
	panel.save_action()->trigger();
	expect(intents.back().Id == 2 && intents.back().Revision == 5 && intents.back().Text == "Edited privately", "edit carries original revision");
	shown.Notes[0].Revision = 6;
	shown.Notes[0].Text     = "Another window won";
	panel.present(shown);
	panel.save_action()->trigger();
	expect(intents.back().Revision == 5 && panel.body_editor()->toPlainText() == "Edited privately", "refresh does not silently grant a newer revision to a rejected edit");
	panel.discard_action()->trigger();
	expect(panel.body_editor()->toPlainText() == "Another window won", "explicit discard loads the accepted revision");
	expect(intents.back().Action == qt::private_note_action::discard && intents.back().Scope == 7, "discard asks the host to dismiss the rejected candidate and reload this review");
	panel.promote_action()->setEnabled(true);
	const auto count = intents.size();
	panel.promote_action()->trigger();
	expect(intents.size() == count, "forced promote has no target and emits nothing");
	panel.set_promotion_target(true, { });
	panel.promote_action()->trigger();
	expect(intents.back().Action == qt::private_note_action::promote && intents.back().Id == 2 && intents.back().Revision == 6 && panel.notes_list()->count() == 1, "promotion keeps exact selected source");
	panel.erase_action()->trigger();
	expect(intents.back().Action == qt::private_note_action::erase && intents.back().Revision == 6, "delete uses accepted revision");
	panel.body_editor()->setPlainText("Unsaved old review");
	panel.present({ .Scope = 8, .Available = true });
	expect(panel.body_editor()->toPlainText().isEmpty() && panel.notes_list()->count() == 0, "different review cannot inherit the prior private editor");
	std::cout << "private-notes-panel-tests:pass\n";
}
