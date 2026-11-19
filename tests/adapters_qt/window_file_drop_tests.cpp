// [[BPR-2282]] (owner, 2026-11-19: "Can we make it so that if you drag and
// drop a setting profile export file onto the app window it brings up the
// import dialog for it"). Drag events are sent to an editor inside a window,
// as Qt delivers them to the widget under the pointer: one local file is the
// window's, shown by its highlight and handed on after the drop; two files
// are refused with a word; text dragged inside the editor stays the
// editor's; while a modal dialog is open a file drag is refused.
#include "bpr/qt/window_file_drop.hpp"

#include <QApplication>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {
	using bpr::qt::dropped_file_admission;
	using bpr::qt::window_file_drop;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// The composition's reading, for the test: local files only.
	[[nodiscard]] auto admit(const QMimeData& aMime) -> dropped_file_admission
	{
		auto files = QStringList{ };
		for (const auto& url : aMime.urls()) {
			if (url.isLocalFile()) {
				files.push_back(url.toLocalFile());
			}
		}
		if (files.isEmpty()) {
			return { };
		}
		if (files.size() > 1) {
			return { .Kind = dropped_file_admission::kind::several_files, .Path = { } };
		}
		return { .Kind = dropped_file_admission::kind::one_file, .Path = files.front() };
	}

	[[nodiscard]] auto files(const QStringList& aPaths) -> QMimeData*
	{
		auto* const mime = new QMimeData;
		auto        urls = QList<QUrl>{ };
		for (const auto& path : aPaths) {
			urls.push_back(QUrl::fromLocalFile(path));
		}
		mime->setUrls(urls);
		return mime;
	}

	/// Sends an enter and a move, as Qt does when a drag arrives; whether the
	/// move was accepted, which sets the cursor.
	[[nodiscard]] auto arrive(QWidget& aTarget, const QMimeData& aMime) -> bool
	{
		const auto at    = aTarget.rect().center();
		auto       enter = QDragEnterEvent{ QPointF{ at }, Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, &aMime, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aTarget, &enter);
		if (!enter.isAccepted()) {
			return false;
		}
		auto move = QDragMoveEvent{ QPointF{ at }, Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, &aMime, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aTarget, &move);
		return move.isAccepted();
	}

	/// Whether the drop was taken, and as what.
	auto drop(QWidget& aTarget, const QMimeData& aMime) -> std::pair<bool, Qt::DropAction>
	{
		auto event = QDropEvent{ QPointF{ aTarget.rect().center() }, Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, &aMime, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aTarget, &event);
		QApplication::processEvents();
		return { event.isAccepted(), event.dropAction() };
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	auto         window = QWidget{ };
	auto* const  layout = new QVBoxLayout{ &window };
	auto* const  editor = new QPlainTextEdit{ &window };
	layout->addWidget(editor);
	window.resize(640, 420);
	window.show();
	QApplication::processEvents();

	auto        dropped = QStringList{ };
	auto        said    = QStringList{ };
	auto* const target  = new window_file_drop{ window, { .Admit = admit, .Dropped = [&](const QString& aPath) { dropped.push_back(aPath); }, .Say = [&](const QString& aText) { said.push_back(aText); } } };
	expect(window.acceptDrops(), "the window accepts drops, so every point of it has a target");
	auto* const surface = editor->viewport();

	// Text dragged inside the editor is the editor's.
	{
		auto text = QMimeData{ };
		text.setText(QStringLiteral("moved words"));
		static_cast<void>(arrive(*surface, text));
		expect(!target->hovering() && !target->highlight()->isVisible(), "a drag with no file draws no highlight");
		static_cast<void>(drop(*surface, text));
		expect(dropped.isEmpty() && said.isEmpty(), "a drag with no file is not the window's");
		expect(editor->toPlainText() == QStringLiteral("moved words"), "the editor still takes its own text drop");
		editor->clear();
	}

	// One local file: highlighted while it hovers, gone when it leaves.
	const auto settings = QStringLiteral("/tmp/bpr-window-file-drop/profile.bprsettings");
	{
		auto* const one = files({ settings });
		expect(arrive(*surface, *one), "one file over the editor is accepted");
		expect(target->hovering() && target->highlight()->isVisible() && target->highlight()->geometry() == window.rect(), "the highlight covers the window while the file hovers");
		expect(target->highlight()->testAttribute(Qt::WA_TransparentForMouseEvents), "the highlight takes no pointer");
		auto leave = QDragLeaveEvent{ };
		QApplication::sendEvent(surface, &leave);
		expect(!target->hovering() && !target->highlight()->isVisible(), "the highlight goes when the file leaves");

		// The drop: accepted as a copy, handed on after the event, the editor untouched.
		expect(arrive(*surface, *one), "the file comes back");
		const auto [taken, action] = drop(*surface, *one);
		expect(taken && action == Qt::CopyAction, "the drop is taken as a copy: the file stays where it is");
		expect(dropped == QStringList{ settings }, "the dropped file's path is handed on once");
		expect(editor->toPlainText().isEmpty(), "the editor does not insert the file's address");
		expect(!target->highlight()->isVisible(), "no highlight after the drop");
		delete one;
	}

	// Two files: refused, with a word, and nothing handed on.
	{
		dropped.clear();
		auto* const two = files({ settings, QStringLiteral("/tmp/bpr-window-file-drop/theme.bprsettings") });
		expect(!arrive(*surface, *two), "two files are refused at the door: the not-accepted cursor");
		expect(said == QStringList{ window_file_drop::several_files_text() } && said.front() == QStringLiteral("Drop one file at a time."), "the window says, once, to drop one file at a time");
		expect(!target->hovering(), "two files draw no highlight");
		static_cast<void>(drop(*surface, *two));
		expect(dropped.isEmpty(), "two files open nothing");
		delete two;
	}

	// A modal dialog open: a file drag is refused, and a drop does nothing.
	{
		said.clear();
		auto dialog = QDialog{ &window };
		dialog.setModal(true);
		dialog.show();
		QApplication::processEvents();
		expect(QApplication::activeModalWidget() == &dialog, "the dialog is modal");
		auto* const one = files({ settings });
		expect(!arrive(*surface, *one), "while a modal dialog is open a file is refused");
		expect(!target->hovering() && said.isEmpty(), "no highlight and no word while a dialog is open");
		static_cast<void>(drop(*surface, *one));
		expect(dropped.isEmpty(), "a drop while a dialog is open opens nothing");
		dialog.hide();
		QApplication::processEvents();
		delete one;
	}

	// Another window's drag is not this window's.
	{
		auto other = QWidget{ };
		other.resize(200, 200);
		other.show();
		QApplication::processEvents();
		auto* const one = files({ settings });
		static_cast<void>(arrive(other, *one));
		expect(!target->hovering(), "a file over another window draws no highlight here");
		static_cast<void>(drop(other, *one));
		expect(dropped.isEmpty(), "a drop on another window is not handed on here");
		delete one;
	}

	std::cout << "window-file-drop:pass\n";
	return EXIT_SUCCESS;
}
