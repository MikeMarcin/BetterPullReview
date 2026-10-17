// [[BPR-0169]], [[BPR-1222]]. The shipped composition, offscreen, on the
// owner's designated live target, reading pull requests a second GitHub
// account (MikeMarcinTest) authored or reviews.
//
// Run by hand on macOS, never as a registered row: it signs in through the
// GitHub CLI account `gh` has active and reads live GitHub. It asks for no
// write, and a tests-enabled composition could not make one: its review
// composer is read-only without a fixture. Between steps the course writes
// `at-STEP` into its root and waits for `go-STEP`, so an independent client
// can read GitHub's own state at exactly that point.
//
//   --course observer --root DIR --pull N --author LOGIN --path PREFIX
//       Opens MikeMarcin/BPR-Test#N, then adds and removes a repository, an
//       author and a path observer rule in turn through the queue controls.
//   --course reader --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N and stays a while, asking for nothing.
//   --course epoch --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N in Review and drags each handle of the
//       Updates line onto the oldest update, saying where the handles are and
//       which updates they can take, and rendering the line into DIR
//       ([[BPR-1553]]). Run it again on the same DIR after a push to see the
//       line of a review this install has seen before.
//   --course parity --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N in Review on its default comparison and
//       visits every file, writing one JSON line per file into
//       DIR/parity.jsonl: the path, both blob OIDs the comparison read and
//       every removed (left) and added (right) line with its number and text
//       ([[BPR-1560]]). `tests/end_to_end/bpr1560_live_parity.py` compares
//       those lines with GitHub's own Files changed for the same pull request.
#include "bpr/app/github_application.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_line.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_queue.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QWindow>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	[[noreturn]] void fail(const std::string& aMessage)
	{
		throw std::runtime_error{ aMessage };
	}

	void say(const std::string& aLine)
	{
		std::cout << aLine << '\n'
				  << std::flush;
	}

	auto wait_for(const std::function<bool()>& aDone, const std::string& aWhat, std::chrono::seconds aBudget = std::chrono::seconds{ 180 }) -> bool
	{
		const auto deadline = std::chrono::steady_clock::now() + aBudget;
		while (std::chrono::steady_clock::now() < deadline) {
			QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
			if (aDone()) {
				return true;
			}
			QThread::msleep(20);
		}
		say("timeout:" + aWhat);
		return false;
	}

	void require(const std::function<bool()>& aDone, const std::string& aWhat, std::chrono::seconds aBudget = std::chrono::seconds{ 180 })
	{
		if (!wait_for(aDone, aWhat, aBudget)) {
			fail("timed out waiting for " + aWhat);
		}
	}

	/// Writes `at-STEP` with its facts and waits, events still running, until
	/// the operator's second client has read GitHub and created `go-STEP`.
	void checkpoint(const std::filesystem::path& aRoot, const std::string& aStep, const std::string& aFacts)
	{
		{
			std::ofstream out{ aRoot / ("at-" + aStep) };
			out << aFacts << '\n';
		}
		say("checkpoint:" + aStep + " " + aFacts);
		require([&] { return std::filesystem::exists(aRoot / ("go-" + aStep)); }, "go-" + aStep, std::chrono::seconds{ 1800 });
	}

	auto text(const QString& aText) -> std::string { return aText.toStdString(); }

	struct options
	{
		std::string           Course;
		std::filesystem::path Root;
		int                   Pull = 0;
		std::string           Author;
		std::string           Path;
	};

	auto parse(const QStringList& aArguments) -> options
	{
		options result;
		for (qsizetype i = 1; i + 1 < aArguments.size(); i += 2) {
			const auto& key   = aArguments[i];
			const auto  value = aArguments[i + 1].toStdString();
			if (key == "--course") {
				result.Course = value;
			} else if (key == "--root") {
				result.Root = value;
			} else if (key == "--pull") {
				result.Pull = std::stoi(value);
			} else if (key == "--author") {
				result.Author = value;
			} else if (key == "--path") {
				result.Path = value;
			} else {
				fail("unknown argument " + key.toStdString());
			}
		}
		if ((result.Course != "observer" && result.Course != "reader" && result.Course != "epoch" && result.Course != "parity") || result.Root.empty() || result.Pull <= 0
			|| (result.Course == "observer" && (result.Author.empty() || result.Path.empty()))) {
			fail("usage: --course observer|reader|epoch|parity --root DIR --pull N [--author LOGIN --path PREFIX]");
		}
		return result;
	}

	/// Signs in the way a person does from Home: Use GitHub CLI, then the
	/// github.com card's GitHub CLI offer. Returns the login the card shows.
	auto sign_in(qt::workbench_window& aWindow) -> std::string
	{
		auto* const use = aWindow.use_github_cli_button();
		if (use == nullptr) {
			fail("no Use GitHub CLI control");
		}
		use->click();
		require([&] {
			return aWindow.github_status_text() == "GitHub account connected."
				|| std::ranges::any_of(aWindow.server_cards(), [](qt::server_card* aCard) {
					   return aCard->hostname() == "github.com"
						   && ((aCard->cli_option() != nullptr && aCard->cli_offer_button() != nullptr && aCard->cli_offer_button()->isVisible())
							   || (aCard->cli_accounts() != nullptr && aCard->cli_accounts()->isVisible()));
				   });
		},
				"GitHub CLI discovery");
		say("cli-discovery:" + text(aWindow.github_cli_discovery_status()->text()));
		if (aWindow.github_status_text() != "GitHub account connected.") {
			auto offered = 0;
			for (auto* card : aWindow.server_cards()) {
				if (card->hostname() == "github.com" && card->cli_option() != nullptr && card->cli_offer_button() != nullptr && card->cli_offer_button()->isVisible()) {
					++offered;
					if (offered == 1) {
						say("cli-offer:" + text(card->cli_offer_button()->text()));
						card->cli_offer_button()->click();
					}
				}
			}
			say("cli-offers:" + std::to_string(offered));
			// [[BPR-1373]]. With more than one gh account the card lists each
			// in its own row instead of one offer; the course uses the row of
			// the account gh has active, as the header says.
			for (auto* card : aWindow.server_cards()) {
				if (offered != 0 || card->hostname() != "github.com" || card->cli_accounts() == nullptr) {
					continue;
				}
				for (auto* const row : card->cli_accounts()->findChildren<QWidget*>(QStringLiteral("serverCardCliAccount"))) {
					auto* const sentence = row->findChild<QLabel*>(QStringLiteral("serverCardCliAccountSentence"));
					auto* const choose   = row->findChild<QPushButton*>(QStringLiteral("serverCardCliAccountUse"));
					if (sentence != nullptr && choose != nullptr && row->isVisibleTo(card) && sentence->text().contains(QStringLiteral("gh's active account"))) {
						say("cli-account:" + text(row->property("login").toString()));
						choose->click();
						offered = 1;
						break;
					}
				}
			}
		}
		require([&] { return aWindow.github_status_text() == "GitHub account connected."; }, "GitHub account connected");
		std::string login;
		for (auto* card : aWindow.server_cards()) {
			if (card->hostname() == "github.com" && card->signed_in()) {
				login = text(card->login_label()->text());
				say("signed-in-card:" + login + " kind=" + text(card->kind_label()->text()));
			}
		}
		return login;
	}

	void open_pull(qt::workbench_window& aWindow, int aPull)
	{
		aWindow.home_open_bar()->setText(QStringLiteral("MikeMarcin/BPR-Test#%1").arg(aPull));
		say("open-enabled:" + std::string{ aWindow.home_open_action()->isEnabled() ? "yes" : "no" });
		aWindow.home_open_action()->trigger();
	}

	void dump(qt::workbench_window& aWindow)
	{
		say("dump:status=" + aWindow.github_status_text());
		say("dump:statusbar=" + text(aWindow.statusBar()->currentMessage()));
		auto* const editor = aWindow.review_submission_editor_view();
		say("dump:editor-status=" + text(editor->status_label()->text()) + " draft=" + (editor->shown_draft() ? "yes" : "no"));
		say("dump:home=" + text(aWindow.home_open_bar()->text()));
	}

	/// Opens the pull request and its published tour as a reader would, and
	/// leaves them open for a while; nothing here asks for a write.
	void reader_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		aWindow.choose_modality(qt::review_modality::review);
		wait_for([] { return false; }, "a reader's dwell (expected)", std::chrono::seconds{ 20 });
		dump(aWindow);
		checkpoint(aOptions.Root, "read", "{\"status\":\"" + aWindow.github_status_text() + "\"}");
	}

	/// Drags through the window, as a pointer is delivered, so whatever sits
	/// over the widget gets the events first.
	void window_drag(QWidget& aWidget, const QPoint& aFrom, const QPoint& aTo)
	{
		auto* const top    = aWidget.window();
		auto* const target = top != nullptr ? top->windowHandle() : nullptr;
		if (target == nullptr) {
			fail("the window has no native handle to deliver a drag to");
		}
		const auto send = [&](const QEvent::Type aType, const QPoint& aAt, const Qt::MouseButton aButton, const Qt::MouseButtons aButtons) {
			const auto local = QPointF{ aWidget.mapTo(top, aAt) };
			auto       event = QMouseEvent{ aType, local, local, QPointF{ aWidget.mapToGlobal(aAt) }, aButton, aButtons, Qt::NoModifier };
			QCoreApplication::sendEvent(target, &event);
		};
		send(QEvent::MouseMove, aFrom, Qt::NoButton, Qt::NoButton);
		send(QEvent::MouseButtonPress, aFrom, Qt::LeftButton, Qt::LeftButton);
		for (auto step = 1; step <= 4; ++step) {
			send(QEvent::MouseMove, aFrom + ((aTo - aFrom) * step) / 4, Qt::NoButton, Qt::LeftButton);
		}
		send(QEvent::MouseButtonRelease, aTo, Qt::LeftButton, Qt::NoButton);
	}

	/// Says what the Updates line shows: its nodes, where each handle is, which
	/// nodes a handle can be put on (the node menu's Compare actions), and a
	/// render of the window and the line into the root.
	void epoch_dump(qt::workbench_window& aWindow, const std::filesystem::path& aRoot, const std::string& aTag)
	{
		auto* const control = aWindow.epoch_selectors();
		auto* const line    = control->line();
		const auto  stop    = [](const std::optional<qt::epoch_line_stop>& aStop) {
			if (!aStop) {
				return std::string{ "none" };
			}
			// [[BPR-1557]]. The pull request's base, at the line's start.
			if (aStop->Kind == core::update_line_place_kind::pull_request_base) {
				return std::string{ "prbase" };
			}
			return std::string{ aStop->Kind == core::update_line_place_kind::update_base ? "base" : "head" } + std::to_string(aStop->Update);
		};
		std::string labels;
		for (std::size_t i = 0; i < line->update_count(); ++i) {
			labels += text(line->label_text(i)) + " {" + text(line->node_tooltip(i)) + "} | ";
		}
		say("epoch:" + aTag + " visible=" + (line->isVisible() ? "1" : "0") + " controlVisible=" + (control->isVisible() ? "1" : "0") + " size=" + std::to_string(line->width()) + "x" + std::to_string(line->height())
			+ " updates=" + std::to_string(line->update_count()) + " offered=" + (line->selection_offered() ? "1" : "0") + " left=" + stop(line->left_stop()) + " right=" + stop(line->right_stop())
			+ " readiness=" + text(control->readiness_text()) + " statement=" + text(line->statement()) + " status=" + text(control->status_text()) + " labels=" + labels);
		say("epoch:" + aTag + " name=" + text(line->accessibleName()) + " base=" + text(line->base_tooltip()));
		for (std::size_t i = 0; i < line->update_count(); ++i) {
			const auto at        = line->node_centre(i);
			auto       menuEvent = QContextMenuEvent{ QContextMenuEvent::Mouse, at, line->mapToGlobal(at) };
			QCoreApplication::sendEvent(line, &menuEvent);
			for (auto* const menu : line->findChildren<QMenu*>(QStringLiteral("epochLineMenu"))) {
				std::string states;
				for (auto* const action : menu->actions()) {
					if (!action->text().isEmpty()) {
						states += text(action->text()) + "=" + (action->isEnabled() ? "1" : "0") + ";";
					}
				}
				auto said = "epoch:" + aTag;
				said.append(" node").append(std::to_string(i)).append(" menu ").append(states);
				say(said);
				menu->close();
				delete menu;
			}
		}
		say("epoch:" + aTag + " comparing=" + (control->comparing_button() ? text(control->comparing_button()->text()) : std::string{ "-" }));
		aWindow.grab().save(QString::fromStdString((aRoot / ("window-" + aTag + ".png")).string()));
		line->grab().save(QString::fromStdString((aRoot / ("line-" + aTag + ".png")).string()));
	}

	/// [[BPR-1553]]. Opens the pull request, opens the Updates line from the
	/// review bar if it is folded, moves to Review, and drags the right and
	/// then the left handle onto the oldest update, saying what the line shows
	/// before and after each drag. Nothing here writes to GitHub; a drag onto
	/// an update GitHub recorded imports its head into this run's own cache.
	void epoch_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 820);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 15 });
		epoch_dump(aWindow, aOptions.Root, "overview");
		if (!aWindow.epoch_selectors()->line()->isVisible()) {
			if (auto* const disclosure = aWindow.findChild<QToolButton*>(QStringLiteral("reviewBarUpdates")); disclosure != nullptr) {
				disclosure->click();
			}
		}
		wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 5 });
		epoch_dump(aWindow, aOptions.Root, "overview-open");
		aWindow.choose_modality(qt::review_modality::review);
		wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 20 });
		epoch_dump(aWindow, aOptions.Root, "review");
		auto* const line = aWindow.epoch_selectors()->line();
		if (line->right_stop()) {
			window_drag(*line, line->stop_position(*line->right_stop()), line->node_centre(0U));
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 15 });
			epoch_dump(aWindow, aOptions.Root, "review-right-dragged");
		}
		if (line->left_stop()) {
			window_drag(*line, line->stop_position(*line->left_stop()), line->node_centre(0U));
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 15 });
			epoch_dump(aWindow, aOptions.Root, "review-left-dragged");
		}
		// [[BPR-1557]]. Left steps the left handle back to the pull request's
		// base, the line's first stop.
		if (line->left_stop()) {
			line->setFocus(Qt::OtherFocusReason);
			auto left = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier };
			for (std::size_t step = 0U; step <= line->update_count(); ++step) {
				QCoreApplication::sendEvent(line, &left);
				wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 8 });
			}
			epoch_dump(aWindow, aOptions.Root, "review-left-base");
		}
		dump(aWindow);
	}

	/// JSON string escaping for the parity course's lines.
	auto json_text(const std::string_view aText) -> std::string
	{
		std::string result{ "\"" };
		for (const auto c : aText) {
			switch (c) {
			case '"':  result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20U) {
					constexpr auto digits = std::string_view{ "0123456789abcdef" };
					result += "\\u00";
					result += digits[(static_cast<unsigned char>(c) >> 4U) & 0xFU];
					result += digits[static_cast<unsigned char>(c) & 0xFU];
				} else {
					result += c;
				}
			}
		}
		return result + "\"";
	}

	/// The changed lines of one side: `[number, text]` for every line a
	/// non-equal segment covers, numbered from one as GitHub numbers them.
	auto changed_lines(const core::computed_file_diff& aDiff, const core::diff_side aSide) -> std::string
	{
		const auto& document = aSide == core::diff_side::left ? aDiff.LeftDocument : aDiff.RightDocument;
		std::string result{ "[" };
		auto        first = true;
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == core::alignment_kind::equal) {
				continue;
			}
			const auto& span = aSide == core::diff_side::left ? segment.Left : segment.Right;
			for (auto line = span.Begin; line < span.End; ++line) {
				auto content = std::string{ };
				if (document && line < document->Lines.size()) {
					const auto& bytes = document->Lines[line].Content;
					content.assign(reinterpret_cast<const char*>(document->Bytes.data()) + bytes.Begin, bytes.End - bytes.Begin);
				}
				result += (first ? "" : ",") + std::string{ "[" } + std::to_string(line + 1U) + "," + json_text(content) + "]";
				first = false;
			}
		}
		return result + "]";
	}

	/// [[BPR-1560]]. Opens the pull request in Review on the comparison it
	/// opens with and visits every file in the Files order, writing what each
	/// comparison read and showed. Nothing here writes to GitHub.
	void parity_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 820);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		require([&] { return aWindow.overview_page()->projection().has_value() && !aWindow.overview_page()->projection()->Files.empty(); }, "the overview lists the files");
		const auto files = aWindow.overview_page()->projection()->Files;
		aWindow.choose_modality(qt::review_modality::review);
		auto* const surface = aWindow.comparison_surface();
		const auto  path_of = [&]() -> std::optional<std::string> {
			const auto& context = surface->current_context();
			if (!context) {
				return std::nullopt;
			}
			for (const auto& row : files) {
				if (core::file_identity_of(row.Path) == context->Key.File) {
					return row.Path;
				}
			}
			return std::nullopt;
		};
		const auto settled = [&] {
			const auto& context = surface->current_context();
			const auto& result  = surface->current_result();
			return context && result && result->Alignment.Key == context->Key && path_of().has_value();
		};
		std::ofstream            out{ aOptions.Root / "parity.jsonl" };
		std::vector<std::string> visited;
		for (std::size_t index = 0U; index < files.size(); ++index) {
			if (index == 0U) {
				if (!wait_for(settled, "the first file's comparison (opens it next)", std::chrono::seconds{ 15 })) {
					if (auto* const open = surface->idle_open_first_button(); open != nullptr && open->isVisible()) {
						open->click();
					}
					require(settled, "the first file's comparison", std::chrono::seconds{ 120 });
				}
			} else {
				const auto before = *path_of();
				aWindow.next_file_action()->trigger();
				require([&] { return settled() && *path_of() != before; }, "file " + std::to_string(index + 1U) + "'s comparison", std::chrono::seconds{ 120 });
			}
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 1 });
			require(settled, "the comparison stays settled");
			if (index == 0U) {
				// Read once the first comparison has settled, so it names the
				// comparison that was shown rather than a loading state.
				say("parity:comparing=" + (aWindow.epoch_selectors()->comparing_button() ? text(aWindow.epoch_selectors()->comparing_button()->text()) : std::string{ "-" })
					+ " status=" + text(aWindow.epoch_selectors()->status_text()));
			}
			const auto  path    = *path_of();
			const auto& context = *surface->current_context();
			const auto& result  = *surface->current_result();
			const auto  blob    = [](const std::optional<core::git_blob_oid>& aBlob) {
				return aBlob ? json_text(core::canonical_hex(*aBlob).view()) : std::string{ "null" };
			};
			constexpr auto operations = std::array<std::string_view, 7U>{ "added", "modified", "removed", "renamed", "copied", "changed", "unchanged" };
			const auto     row        = std::ranges::find(files, path, &application::overview_file_row::Path);
			const auto     operation  = static_cast<std::size_t>(row->Operation);
			const auto     line       = "{\"path\":" + json_text(path) + ",\"status\":" + json_text(operation < operations.size() ? operations[operation] : "unknown")
				+ ",\"previous\":" + (row->PreviousPath ? json_text(*row->PreviousPath) : std::string{ "null" }) + ",\"left_blob\":" + blob(context.Key.LeftBlob) + ",\"right_blob\":" + blob(context.Key.RightBlob)
				+ ",\"content\":" + std::to_string(static_cast<int>(result.Alignment.Content)) + ",\"removed\":" + changed_lines(result, core::diff_side::left)
				+ ",\"added\":" + changed_lines(result, core::diff_side::right) + ",\"threads\":[";
			auto threads = std::string{ };
			for (const auto& tick : surface->thread_ticks()) {
				threads += (threads.empty() ? "" : ",") + std::string{ "[" } + json_text(tick.Side == core::diff_side::left ? "LEFT" : "RIGHT") + "," + std::to_string(tick.Line) + ","
					+ json_text(text(tick.Description)) + "]";
			}
			out << line << threads << "]}" << '\n';
			say("parity:file " + path);
			visited.push_back(path);
		}
		out.flush();
		say("parity:files=" + std::to_string(visited.size()) + " of " + std::to_string(files.size()));
		epoch_dump(aWindow, aOptions.Root, "parity");
		// The interdiff from the previous update, when there is one: every
		// file's two blobs, or the state the comparison says it is in.
		auto* const previous = aWindow.epoch_selectors()->previous_upload_action();
		say("parity:previous-upload enabled=" + std::string{ previous != nullptr && previous->isEnabled() ? "1" : "0" });
		if (previous != nullptr && previous->isEnabled()) {
			previous->trigger();
			const auto interdiff = [&] {
				return settled() && surface->current_context()->Key.LeftAnchor.Kind == core::comparison_anchor_kind::epoch_head;
			};
			const auto absent = [&] { return surface->status_text().contains(QStringLiteral("absent from both revisions")); };
			const auto ready  = wait_for([&] { return aWindow.epoch_selectors()->status_text() == QStringLiteral("Review history is ready.") && (interdiff() || absent()); },
										 "the interdiff (may be unavailable)", std::chrono::seconds{ 60 });
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 2 });
			say("parity:interdiff shown=" + std::string{ ready ? "1" : "0" } + " comparing="
				+ (aWindow.epoch_selectors()->comparing_button() ? text(aWindow.epoch_selectors()->comparing_button()->text()) : std::string{ "-" })
				+ " status=" + text(aWindow.epoch_selectors()->status_text()) + " surface=" + text(surface->status_text()));
			std::ofstream between{ aOptions.Root / "parity-interdiff.jsonl" };
			for (std::size_t index = 0U; ready && index < files.size(); ++index) {
				// Each file is chosen in Files, as a reviewer does; a file the
				// two updates both lack is its own state, not a comparison.
				const auto& path = files[index].Path;
				const auto  row  = aWindow.files_drawer()->contents()->index_for_file(core::file_identity_of(path));
				aWindow.files_drawer()->contents_view()->setCurrentIndex(row);
				const auto shownHere  = [&] { return interdiff() && path_of() == path; };
				const auto absentHere = [&] {
					return absent() && aWindow.files_drawer()->contents_view()->currentIndex() == row;
				};
				if (!wait_for([&] { return shownHere() || absentHere(); }, "the file's interdiff", std::chrono::seconds{ 60 })) {
					between << "{\"path\":" << json_text(path) << ",\"state\":\"unavailable\"}\n";
					continue;
				}
				if (!shownHere()) {
					between << "{\"path\":" << json_text(path) << ",\"state\":\"absent-both\"}\n";
					continue;
				}
				const auto& context = *surface->current_context();
				const auto  blob    = [](const std::optional<core::git_blob_oid>& aBlob) {
					return aBlob ? json_text(core::canonical_hex(*aBlob).view()) : std::string{ "null" };
				};
				between << "{\"path\":" << json_text(path) << ",\"state\":\"shown\",\"left_blob\":" << blob(context.Key.LeftBlob) << ",\"right_blob\":" << blob(context.Key.RightBlob)
						<< ",\"removed\":" << changed_lines(*surface->current_result(), core::diff_side::left)
						<< ",\"added\":" << changed_lines(*surface->current_result(), core::diff_side::right) << "}\n";
			}
			epoch_dump(aWindow, aOptions.Root, "parity-interdiff");
		}
		dump(aWindow);
	}

	void observer_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.add_queue_rule_action() != nullptr && aWindow.queue_rule_kind_selector() != nullptr; }, "queue controls exist");
		const std::vector<core::observer_rule> rules{
			{ .Kind = core::observer_rule_kind::repository, .Pattern = "MikeMarcin/BPR-Test" },
			{ .Kind = core::observer_rule_kind::author, .Pattern = aOptions.Author },
			{ .Kind = core::observer_rule_kind::path, .Pattern = aOptions.Path },
		};
		const auto enrolled = [&](const core::observer_rule& aRule) -> std::optional<core::queue_item> {
			auto* const landing = aWindow.queue_landing();
			for (int row = 0; row < landing->rowCount(); ++row) {
				const auto item = landing->item_at(landing->index(row));
				if (item && item->Ref.Number == aOptions.Pull && item->Membership.MatchedRule && item->Membership.Rule == aRule) {
					return item;
				}
			}
			return std::nullopt;
		};
		for (const auto& rule : rules) {
			const auto kind     = std::string{ core::name(rule.Kind) };
			auto*      selector = aWindow.queue_rule_kind_selector();
			selector->setCurrentIndex(selector->findData(static_cast<int>(rule.Kind)));
			aWindow.queue_rule_pattern_editor()->setText(QString::fromStdString(rule.Pattern));
			require([&] { return aWindow.add_queue_rule_action()->isEnabled(); }, kind + " rule can be added");
			aWindow.add_queue_rule_action()->trigger();
			require([&] { return enrolled(rule).has_value(); }, "#" + std::to_string(aOptions.Pull) + " enrolled by the " + kind + " rule");
			const auto item  = *enrolled(rule);
			const auto facts = "{\"rule\":\"" + kind + "\",\"pattern\":\"" + rule.Pattern + "\",\"ref\":\"" + item.Ref.Host + "/" + item.Ref.Repository + "#" + std::to_string(item.Ref.Number)
				+ "\",\"account\":\"" + item.Ref.Account + "\",\"role\":\"" + std::string{ core::name(item.Membership.Role) } + "\",\"authored\":" + (item.Membership.Authored ? "true" : "false")
				+ ",\"requested\":" + (item.Membership.Requested ? "true" : "false") + ",\"rows\":" + std::to_string(aWindow.queue_landing()->rowCount()) + "}";
			checkpoint(aOptions.Root, "enrolled-" + kind, facts);
			aWindow.remove_queue_rule_action()->trigger();
			require([&] { return !enrolled(rule).has_value() && !aWindow.remove_queue_rule_action()->isEnabled(); }, kind + " rule removed");
			checkpoint(aOptions.Root, "removed-" + kind, "{\"rule\":\"" + kind + "\",\"rows\":" + std::to_string(aWindow.queue_landing()->rowCount()) + "}");
		}
	}
}

auto main(int argc, char** argv) -> int
try {
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto chosen = parse(QCoreApplication::arguments());
	std::filesystem::create_directories(chosen.Root / "data");
	std::filesystem::permissions(chosen.Root / "data", std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);

	// A question the application asks is a person's gesture. The one this
	// course meets is the GitHub CLI read consent; the course answers Yes and
	// logs the question word for word, so what was agreed to is on record.
	QTimer answer;
	answer.setInterval(100);
	QObject::connect(&answer, &QTimer::timeout, [] {
		auto* const box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (box == nullptr || !box->isVisible()) {
			return;
		}
		say("question:title=" + text(box->windowTitle()) + "\nquestion:text=" + text(box->text()) + "\nquestion:informative=" + text(box->informativeText()));
		auto* const yes = box->button(QMessageBox::Yes);
		if (yes == nullptr) {
			say("question:no-yes-button; left open");
			return;
		}
		say("question:answer=" + text(yes->text()));
		yes->click();
	});
	answer.start();

	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::profile_desktop::start(app::github_application_config{ .ApplicationDataRoot = chosen.Root / "data" }, [&](app::profile_desktop_startup_result aResult) {
		terminal = true;
		if (aResult) {
			owner = *aResult;
		} else {
			say("startup-failure:" + std::to_string(static_cast<int>(aResult.error())));
		}
	});
	require([&] { return terminal; }, "the profile owner starts", std::chrono::seconds{ 120 });
	if (owner == nullptr) {
		fail("the shipped composition did not start");
	}
	owner->show();
	require([&] { return !owner->windows().empty(); }, "the first window", std::chrono::seconds{ 60 });
	auto* const window = owner->window(owner->windows().front());
	const auto  login  = sign_in(*window);
	say("login:" + login);
	if (chosen.Course == "observer") {
		observer_course(chosen, *window);
	} else if (chosen.Course == "epoch") {
		epoch_course(chosen, *window);
	} else if (chosen.Course == "parity") {
		parity_course(chosen, *window);
	} else {
		reader_course(chosen, *window);
	}
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "shutdown", std::chrono::seconds{ 60 });
	say("live-second-account-course:" + chosen.Course + ":pass");
	return EXIT_SUCCESS;
} catch (const std::exception& error) {
	std::cout << "live-second-account-course:fail " << error.what() << '\n'
			  << std::flush;
	return EXIT_FAILURE;
}
