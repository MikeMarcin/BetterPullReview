/// \file
/// \brief The owner's notes on the local-draft look of 2026-11-13, each held
/// through the real widgets: the agent's commit-message nib sits beside Commit
/// and wears the agent accent, and it is the only agent control in Local
/// Changes (an accent inventory); the tooltips of the draft's additions and of
/// Local Changes are one short sentence; Home selects one row across its lists
/// and Escape in a list clears it; the draft's additions stand in the pull
/// request window's own slots (Local Changes a tab beside Files and Search).

#include "bpr/application/local_changes_host.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/local_changes_panel.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// The colour the icon paints most, ignoring transparent pixels.
	auto icon_ink(const QIcon& aIcon) -> QColor
	{
		const auto image = aIcon.pixmap(QSize{ 32, 32 }).toImage();
		auto       best  = QColor{ };
		auto       alpha = 0;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				if (pixel.alpha() > alpha) {
					alpha = pixel.alpha();
					best  = pixel;
				}
			}
		}
		return best;
	}

	auto close(const QColor& aLeft, const QColor& aRight) -> bool
	{
		return std::abs(aLeft.red() - aRight.red()) <= 24 && std::abs(aLeft.green() - aRight.green()) <= 24 && std::abs(aLeft.blue() - aRight.blue()) <= 24;
	}

	/// One short sentence: at most 60 characters and no second clause.
	void short_tooltips(const QWidget& aRoot, const std::string_view aWhere)
	{
		auto fine = true;
		for (auto* const widget : aRoot.findChildren<QWidget*>()) {
			const auto tip = widget->toolTip();
			if (tip.isEmpty()) {
				continue;
			}
			if (tip.size() > 60 || tip.contains(QLatin1Char{ ';' }) || tip.contains(QLatin1Char{ '\n' })) {
				std::cerr << "long tooltip in " << aWhere << " (" << widget->objectName().toStdString() << "): " << tip.toStdString() << '\n';
				fine = false;
			}
		}
		expect(fine, "every tooltip is one short sentence");
	}

	void the_nib_is_beside_commit_in_the_accent()
	{
		auto host  = QWidget{ };
		auto panel = qt::local_changes_panel{ &host };
		host.resize(360, 700);
		host.show();
		QApplication::processEvents();
		auto* const nib    = panel.suggest_button();
		auto* const commit = panel.commit_button();
		expect(nib->parentWidget() == commit->parentWidget() && nib->geometry().right() < commit->geometry().left()
				   && std::abs(nib->geometry().center().y() - commit->geometry().center().y()) <= 4,
			   "the nib sits immediately left of Commit, on its row");
		expect(std::abs(nib->geometry().center().y() - panel.subject()->geometry().center().y()) > 4, "and not beside the subject");
		// The accent inventory: the nib is Local Changes' one agent control, and it wears the accent.
		auto agents = 0;
		for (auto* const widget : panel.findChildren<QWidget*>()) {
			agents += qt::agent_run_control(*widget) ? 1 : 0;
		}
		expect(agents == 1 && qt::agent_run_control(*nib), "the nib is the one agent control");
		const auto accent = qt::agent_accent(panel.palette());
		expect(nib->palette().color(QPalette::Active, QPalette::ButtonText) == accent, "its ink is the agent accent");
		expect(close(icon_ink(nib->icon()), accent), "the nib glyph itself is painted in the agent accent");
		short_tooltips(panel, "Local Changes");
		std::cout << "nib-beside-commit:pass\n";
	}

	void the_draft_parts_stand_in_the_window()
	{
		auto window = qt::workbench_window{ };
		window.resize(1280, 800);
		window.show();
		QApplication::processEvents();
		auto* const parts = window.local_draft();
		expect(parts != nullptr, "the window builds the draft's additions");
		auto* const tabs = window.file_search_tabs();
		expect(tabs->tabText(0) == QStringLiteral("Files") && tabs->tabText(1) == QStringLiteral("Search") && window.local_changes_tab() == 2 && !tabs->isTabVisible(2),
			   "Local Changes is a tab beside Files and Search, shown only in author state");
		expect(parts->files_footer()->parentWidget() != nullptr && tabs->widget(0)->isAncestorOf(parts->files_footer()), "the working files are the Files tab's footer");
		expect(!parts->working()->isVisible(), "behind a disclosure");
		for (auto* const root : { parts->bar_leading(), parts->bar_trailing(), parts->notice_row(), parts->files_footer(), parts->edit_page(), parts->notes_page(), parts->status_page() }) {
			short_tooltips(*root, "the draft's additions");
		}
		std::cout << "draft-parts-in-window:pass\n";
	}

	void home_selects_one_row()
	{
		auto window = qt::workbench_window{ };
		window.resize(1280, 800);
		window.show();
		QApplication::processEvents();
		window.present_local_drafts({ .InPreparation = { { .Id = core::local_draft_id{ "draft-a" }, .Title = "Port parsing", .Repository = "bpr-fixtures/engine", .Branch = "fix/port-parsing", .Updated = 1 } } });
		auto recent = application::recent_reviews_projection{ };
		recent.Entries.push_back({ .Destination = { .Hostname = "github.com", .Repository = { "octo-org", "tools" }, .Number = core::pull_request_number{ 7U } }, .Title = "Recently read" });
		window.publish(recent);
		QApplication::processEvents();
		auto* const drafts  = window.home_drafts_list();
		auto* const recents = window.home_recent_list();
		expect(drafts->count() == 1 && recents->count() == 1, "a local draft row and a Recently viewed row");
		drafts->setFocus(Qt::OtherFocusReason);
		drafts->setCurrentRow(0);
		QApplication::processEvents();
		recents->setFocus(Qt::OtherFocusReason);
		recents->setCurrentRow(0);
		QApplication::processEvents();
		expect(recents->currentItem() != nullptr && recents->currentItem()->isSelected() && drafts->selectedItems().isEmpty() && window.home_selected_rows() == 1,
			   "choosing a Recently viewed row leaves the draft row unselected");
		drafts->setFocus(Qt::OtherFocusReason);
		drafts->setCurrentRow(0);
		QApplication::processEvents();
		expect(drafts->selectedItems().size() == 1 && recents->selectedItems().isEmpty() && window.home_selected_rows() == 1, "and the other way round");
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(drafts, &escape);
		QApplication::processEvents();
		expect(window.home_selected_rows() == 0 && drafts->selectedItems().isEmpty() && recents->selectedItems().isEmpty(), "Escape clears Home's selection");
		std::cout << "home-selects-one-row:pass\n";
	}
}

auto main(int argc, char** argv) -> int
{
	auto application = QApplication{ argc, argv };
	the_nib_is_beside_commit_in_the_accent();
	the_draft_parts_stand_in_the_window();
	home_selects_one_row();
	std::cout << "local-draft-window:pass\n";
	return EXIT_SUCCESS;
}
