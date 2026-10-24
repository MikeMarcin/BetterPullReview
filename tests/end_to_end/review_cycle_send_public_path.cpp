#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/review_cycle_share_marks.hpp"
#include "bpr/qt/review_cycle_share_sheet.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "product_diff.hpp"
#include "review_cycle_desktop.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QThread>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1180]]. Send to Agent through the real window, the real
/// composition and the real agent adapter, which starts real processes: a
/// stand-in harness (`cycle_send_agent_main.cpp`) copied into a root of the
/// course's own, never a model or the network. It shows the same bytes Copy
/// and Save give reaching the harness under both protocols, a new session
/// said as new and an existing one continued exactly, a corporate host's
/// refusal of a profile, an uncertain send left uncertain and never sent
/// again, a missing harness that sends nothing, and the cycle carried on by
/// hand once no agent is installed. In a graph built without the agent
/// integrations it shows the route absent; given `--reopen DIR` there, it
/// reads back a profile a full build's course left (`--profile-root DIR`).

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 4; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	template <typename T, typename TStart>
	auto settle_store(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject, TStart aStart) -> T
	{
		application::manual_executor      lanes;
		application::review_cycle_runtime runtime{ adapters::sqlite::make_review_cycle_store(aProfileRoot, core::bpr_profile_id{ 1 }), lanes, lanes, nullptr, bpr::tests::product_line_matcher() };
		runtime.select(aSubject);
		auto result = std::optional<application::cycle_store_result<T>>{ };
		aStart(runtime, [&](application::cycle_store_result<T> aValue) { result = std::move(aValue); });
		lanes.drain();
		runtime.shutdown();
		lanes.drain();
		expect(result && *result, "the store answered");
		return std::move(**result);
	}

	auto responses(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject) -> application::cycle_response_entries
	{
		return settle_store<application::cycle_response_entries>(aProfileRoot, aSubject, [](auto& aRuntime, auto aDone) { aRuntime.responses_selected(std::move(aDone)); });
	}

	auto delivery_lines(const qt::review_cycle_share_sheet& aSheet) -> QStringList
	{
		auto lines = QStringList{ };
		for (auto row = 0; row < aSheet.deliveries()->count(); ++row) {
			lines.append(aSheet.deliveries()->item(row)->text());
		}
		return lines;
	}

	auto open_cycle(app::review_cycle_desktop& aDesktop, qt::workbench_window& aWindow, application::manual_executor& aLanes, const std::filesystem::path& aProject) -> qt::review_cycle_view*
	{
		aDesktop.open_project(aProject.string());
		settle(aLanes);
		auto* const page = aWindow.local_cycle();
		expect(page != nullptr && aWindow.showing_local_cycle(), "the cycle is open");
		return page;
	}

	auto compose(qt::review_cycle_view& aPage, application::manual_executor& aLanes) -> qt::review_cycle_share_sheet*
	{
		aPage.compose_action()->trigger();
		settle(aLanes);
		auto* const sheet = aPage.share_page();
		expect(sheet->isVisible() && aPage.projection().Share.has_value(), "Share notes is open");
		return sheet;
	}

	void close_sheet(qt::review_cycle_share_sheet& aSheet, application::manual_executor& aLanes)
	{
		aSheet.cancel_button()->click();
		settle(aLanes);
	}

#if defined(BPR_AGENT_INTEGRATIONS)
	/// \brief [[BPR-1182]]. Keeps a render of the sheet as the reviewer sees
	/// it, for the owner's look at the Send box, when BPR_CYCLE_RENDER_DIR is set.
	void render(QWidget& aSheet, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_CYCLE_RENDER_DIR");
		if (directory.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(directory);
		expect(aSheet.grab().save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
	}

	auto contains(const QString& aText, const QString& aPart) -> bool
	{
		return aText.contains(aPart);
	}

	/// Files a harness copy wrote beside itself.
	auto sends_in(const std::filesystem::path& aFolder) -> std::size_t
	{
		auto count = std::size_t{ 0 };
		for (const auto& entry : std::filesystem::directory_iterator{ aFolder }) {
			count += entry.path().extension() == ".stdin" ? 1U : 0U;
		}
		return count;
	}

	/// Waits for the send under way to be recorded: the adapter runs on its own worker.
	void wait_for_send(app::review_cycle_desktop& aDesktop, application::manual_executor& aLanes)
	{
		for (auto turn = 0; turn < 3000; ++turn) {
			settle(aLanes);
			const auto& share = aDesktop.host().projection().Share;
			if (share && share->Send && !share->Send->Sending && aDesktop.host().is_quiescent()) {
				settle(aLanes);
				return;
			}
			QThread::msleep(10);
		}
		expect(false, "the send ended within 30 seconds");
	}

	/// Chooses an agent and a session in the box, as the reviewer would.
	void choose(qt::review_cycle_share_sheet& aSheet, const core::agent_profile_id aProfile, const QString& aSession)
	{
		auto* const agents = aSheet.send_profile();
		const auto  agent  = agents->findData(QVariant::fromValue(aProfile.value()));
		expect(agent >= 0, "the agent is listed");
		agents->setCurrentIndex(agent);
		QApplication::processEvents();
		auto* const sessions = aSheet.send_session();
		const auto  session  = aSession.isEmpty() ? 0 : sessions->findText(aSession);
		expect(session >= 0, "the session is listed");
		sessions->setCurrentIndex(session);
		QApplication::processEvents();
	}

	/// The text a Claude stream JSON frame carried.
	auto framed_text(const std::string& aFrame) -> QString
	{
		const auto message = QJsonDocument::fromJson(QByteArray::fromStdString(aFrame)).object();
		expect(message.value(QStringLiteral("type")).toString() == QStringLiteral("user"), "one user message");
		return message.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray().at(0).toObject().value(QStringLiteral("text")).toString();
	}

	void sends_through_the_real_adapter(const std::filesystem::path& aFixture, const std::filesystem::path& aProfileRoot, const std::filesystem::path& aScratch)
	{
		const auto project = aScratch / "send-project";
		write_file(project / "README.md", "# Sender\n");
		write_file(project / "src" / "send.cpp", "int send(int aRound)\n{\n\treturn aRound;\n}\n");
		// Each harness in its own folder of the course's root, beside what it writes.
		const auto agents = aScratch / "agents";
		auto       copy   = [&](const std::string& aFolder, const std::string& aName) {
			const auto folder = agents / aFolder;
			std::filesystem::create_directories(folder);
			const auto target = folder / (aName + aFixture.extension().string());
			std::filesystem::copy_file(aFixture, target);
			return target;
		};
		auto profiles = core::built_in_agent_profiles();
		for (auto& profile : profiles) {
			if (profile.Id == core::claude_stream_json_profile_id) {
				profile.Executable = copy("claude", "claude").string();
			} else if (profile.Id == core::external_terminal_profile_id) {
				profile.Executable = copy("terminal", "bpr-agent-terminal").string();
			} else {
				profile.Executable = copy(profile.Id == core::codex_jsonl_profile_id ? "codex-jsonl" : "codex-app-server", "codex").string();
			}
		}

		auto lanes  = application::manual_executor{ };
		auto window = qt::workbench_window{ };
		window.resize(1280, 800);
		window.show();
		auto desktop = app::review_cycle_desktop{ window, aProfileRoot, core::bpr_profile_id{ 1 }, lanes, lanes, { }, &lanes };
		// [[BPR-1285]]. The appearance the application applies, in the platform's
		// own scheme, so the sheet's marks take the role colours it would.
		const auto scheme     = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::os_color_scheme::dark : core::os_color_scheme::light;
		auto       appearance = qt::appearance_profile{ qt::default_appearance_settings(), { .Scheme = scheme } };
		appearance.apply_to(window);
		desktop.adopt_appearance(appearance);
		const auto roles = qt::code_role_colors_of(appearance.palette());
		desktop.set_send_profiles(profiles);
		const auto handoff = aScratch / "handoff";
		std::filesystem::create_directories(handoff);
		desktop.set_save_chooser([&](const std::string& aSuggested, const bool) {
			const auto path = (handoff / aSuggested).u8string();
			return std::string{ path.begin(), path.end() };
		});
		auto* const page = open_cycle(desktop, window, lanes, project);
		desktop.host().comment({ .Primary = core::cycle_whole_target{ }, .Body = "Say what a send claims." });
		settle(lanes);
		desktop.host().comment({ .Primary = core::cycle_whole_target{ }, .Body = "Never send twice." });
		settle(lanes);
		const auto selected = desktop.host().selected();
		expect(selected.has_value(), "the opened project is the selected cycle");
		const auto subject = *selected;

		// Listed, refused, until the round's channel is allowed for this source.
		auto*       sheet = compose(*page, lanes);
		auto* const route = sheet->route_button(core::cycle_share_route::send_to_agent);
		expect(route->isVisible() && !route->isEnabled() && contains(route->toolTip(), QStringLiteral("Not allowed until it is configured")), "Send to Agent… listed, refused until configured");
		close_sheet(*sheet, lanes);
		static_cast<void>(settle_store<application::cycle_write_receipt>(aProfileRoot, subject, [](auto& aRuntime, auto aDone) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = 0xC7;
			aRuntime.set_channel_permission(application::cycle_operation_id{ bytes }, core::cycle_data_classification::unclassified, core::cycle_disclosure_channel::agent_adapter,
											core::cycle_channel_permission::allowed, std::move(aDone));
		}));

		// F1 to Claude in a new session, said before it is sent.
		sheet = compose(*page, lanes);
		expect(route->isEnabled() && !sheet->send_box()->isVisible(), "allowed; the box opens on its gesture");
		route->click();
		settle(lanes);
		expect(sheet->send_box()->isVisible() && sheet->send_profile()->count() == 4 && sheet->send_session()->count() == 1, "four agents, and only a new session yet");
		// [[BPR-1282]]. Each agent by its harness; no built-in label, no session row yet.
		auto names = QStringList{ };
		for (auto index = 0; index < sheet->send_profile()->count(); ++index) {
			names.append(sheet->send_profile()->itemText(index));
		}
		expect(names == QStringList{ QStringLiteral("Codex (app server)"), QStringLiteral("Codex (exec)"), QStringLiteral("Claude"), QStringLiteral("External terminal harness") },
			   "the agents by harness: " + names.join(QStringLiteral(", ")).toStdString());
		expect(sheet->send_session()->isHidden(), "no session to continue, so no session row");
		const auto folder = QString::fromStdString(project.filename().string());
		expect(sheet->send_words()->text() == QStringLiteral("Runs Codex in %1.").arg(folder) && !contains(sheet->send_words()->text(), QStringLiteral("Claude"))
				   && !contains(sheet->send_words()->text(), QStringLiteral("session")),
			   "Codex selected: the words name Codex, never Claude, and no session: " + sheet->send_words()->text().toStdString());
		render(*sheet, QStringLiteral("send-box-codex"));
		choose(*sheet, core::claude_stream_json_profile_id, { });
		expect(sheet->send_words()->text() == QStringLiteral("Starts a new Claude session in %1.").arg(folder), "Claude selected: one line, naming Claude: " + sheet->send_words()->text().toStdString());
		auto* const details = sheet->send_details();
		expect(sheet->send_more()->isVisible() && !details->isVisible(), "the rest waits behind Details");
		const auto shownWords = sheet->send_words()->text().split(QLatin1Char(' '), Qt::SkipEmptyParts).size();
		std::cout << "send box words shown: " << shownWords << " (details closed), " << details->text().split(QLatin1Char(' '), Qt::SkipEmptyParts).size() << " behind Details\n";
		expect(shownWords <= 8, "the Send box says what Send does in at most eight words");
		sheet->send_more()->click();
		QApplication::processEvents();
		expect(details->isVisible() && sheet->send_more()->text() == QStringLiteral("Hide details") && contains(details->text(), QStringLiteral("exactly as Copy gives it")) && contains(details->text(), QStringLiteral("nothing else is tried"))
				   && contains(details->text(), QStringLiteral("Claude uses its own settings and sign-in")) && contains(details->text(), QDir::toNativeSeparators(QString::fromStdString(project.string()))),
			   "Details: the exact text, Claude's own settings, no fallback, the whole folder");
		expect(contains(sheet->send_words()->accessibleDescription(), QStringLiteral("nothing else is tried")), "and a screen reader hears the details with the line");
		expect(details->height() >= details->heightForWidth(details->width()), "the details are not cut");
		render(*sheet, QStringLiteral("send-box-details"));
		sheet->send_more()->click();
		QApplication::processEvents();
		expect(!details->isVisible() && sheet->send_more()->text() == QStringLiteral("Details"), "Hide details closes them again");
		// The box gives back the room the details took once the layout runs.
		for (auto turn = 0; turn < 3; ++turn) {
			QApplication::processEvents();
		}
		expect(sheet->send_box()->height() == sheet->send_box()->heightForWidth(sheet->send_box()->width()), "and the box is its one-line height again");
		expect(sheet->send_button()->isEnabled() && sheet->send_button()->text() == QStringLiteral("Send Round 1"), "Send Round 1");
		// [[BPR-1182]]. What a send will do is said whole: the words keep the
		// height their wrapped lines need, however tall the window leaves the sheet.
		auto* const words = sheet->send_words();
		expect(words->height() >= words->heightForWidth(words->width()), "the Send box's words are not cut: " + std::to_string(words->height()) + " of " + std::to_string(words->heightForWidth(words->width())) + " pixels");
		render(*sheet, QStringLiteral("send-box-before-sending"));
		const auto document = desktop.host().projection().Share->Document;
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		const auto claude = agents / "claude";
		expect(sends_in(claude) == 1U && read_file(claude / "send-1.args") == "-p\n--input-format\nstream-json\n--output-format\nstream-json\n--verbose", "one harness process, a new session");
		expect(contains(sheet->status()->text(), QStringLiteral("Claude finished its turn on F1")) && contains(sheet->status()->text(), QStringLiteral("Its response is added to F1"))
				   && contains(sheet->status()->text(), QStringLiteral("Nothing was captured or verified")),
			   "finished, its structured response added, nothing more");
		expect(words->height() >= words->heightForWidth(words->width()), "after the send, the Send box's words are still whole");
		render(*sheet, QStringLiteral("send-box-finished"));
		// [[BPR-1285]]. Through the real composition: the shared round's chip and
		// the onward observations in the appearance's role colours, each with
		// words; the caption whole behind the info glyph.
		{
			const auto roleColour = [&roles](const core::appearance_role aRole) { return roles.at(static_cast<std::size_t>(aRole)); };
			expect(sheet->round_chip()->text() == QStringLiteral("Shared") && sheet->round_chip()->colour() == roleColour(core::appearance_role::state_accepted), "the shared round's chip in the accepted role");
			auto onward = sheet->deliveries()->count() > 0;
			for (auto row = 0; row < sheet->deliveries()->count(); ++row) {
				onward = onward && sheet->deliveries()->item(row)->data(qt::review_cycle_share_sheet::delivery_colour_role).value<QColor>() == roleColour(core::appearance_role::state_verified);
			}
			expect(onward, "accepted by the adapter and acknowledged, each in the verified role, beside its words");
			expect(sheet->meaning()->consequence() == QStringLiteral("These say only what happened to the bytes. None of them means the round was received, read, acted on or accepted."), "and what none of them means, whole");
		}
		const auto received = framed_text(read_file(claude / "send-1.stdin"));
		expect(received == QString::fromStdString(document), "the harness got the round's readable document");
		auto* const clipboard = QGuiApplication::clipboard();
		sheet->route_button(core::cycle_share_route::copy_document)->click();
		settle(lanes);
		sheet->route_button(core::cycle_share_route::save_document)->click();
		settle(lanes);
		expect(clipboard->text() == received && QString::fromStdString(read_file(handoff / "RC-1-F1.md")) == received, "payload parity: Copy and Save give exactly what was sent");
		auto lines = delivery_lines(*sheet);
		expect(lines.size() == 4 && lines[0].startsWith(QStringLiteral("F1 · Accepted by the agent adapter")) && lines[1].startsWith(QStringLiteral("F1 · The recipient acknowledged receiving it")),
			   "accepted and acknowledged, in their own words");
		auto answered = responses(aProfileRoot, subject);
		expect(answered.size() == 1U && answered[0].first.Channel == core::cycle_response_channel::agent_adapter && answered[0].first.ForeignId == "FIXTURE-F1",
			   "the harness's response, through the response model, from the adapter");
		expect(page->projection().Revisions.size() == 1U, "nothing captured");
		close_sheet(*sheet, lanes);

		// F2 continues that session, exactly; the prose reply is kept by hand.
		desktop.host().compose();
		settle(lanes);
		desktop.host().edit_composition(core::set_round_goal{ "Second round. SEND_PROSE_CANARY" });
		settle(lanes);
		sheet = page->share_page();
		route->click();
		settle(lanes);
		choose(*sheet, core::claude_stream_json_profile_id, QStringLiteral("Continue session fixture-session-1"));
		expect(sheet->send_words()->text() == QStringLiteral("Continues Claude session fixture-session-1 in %1.").arg(folder) && !sheet->send_session()->isHidden(), "the session it continues, said, with the session row: " + sheet->send_words()->text().toStdString() + (sheet->send_session()->isHidden() ? " (row hidden)" : " (row shown)"));
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		expect(sends_in(claude) == 2U && read_file(claude / "send-2.args").ends_with("--verbose\n--resume\nfixture-session-1"), "bound to exactly that session");
		expect(sheet->reply_box()->isVisible() && sheet->reply_text()->toPlainText() == QStringLiteral("I changed it as asked; the tests pass."), "the prose reply, shown");
		render(*sheet, QStringLiteral("send-box-prose-reply"));
		expect(responses(aProfileRoot, subject).size() == 1U, "not appended by itself");
		sheet->add_reply_button()->click();
		settle(lanes);
		answered = responses(aProfileRoot, subject);
		expect(answered.size() == 2U && answered[1].first.Channel == core::cycle_response_channel::manual_entry && answered[1].first.Round == core::cycle_round_id{ 2 }, "added by hand, to F2");
		expect(!sheet->reply_box()->isVisible(), "and no longer waiting");
		close_sheet(*sheet, lanes);

		// A corporate account: the corporate host's refusal of Codex stands, by name.
		desktop.set_publication_account([] { return std::optional{ application::github_publication_account{ .Login = "reviewer", .Host = "ghe.example.com" } }; });
		desktop.host().compose();
		settle(lanes);
		desktop.host().edit_composition(core::set_round_goal{ "Third round." });
		settle(lanes);
		sheet = page->share_page();
		route->click();
		settle(lanes);
		auto* const model = qobject_cast<QStandardItemModel*>(sheet->send_profile()->model());
		const auto  codex = sheet->send_profile()->findData(QVariant::fromValue(core::codex_jsonl_profile_id.value()));
		expect(model != nullptr && codex >= 0 && !model->item(codex)->isEnabled() && sheet->send_profile()->itemText(codex).endsWith(QStringLiteral("not allowed here")),
			   "Codex listed, not choosable, under a corporate host");
		expect(!sheet->send_profile()->itemIcon(codex).isNull() && sheet->send_profile()->itemIcon(sheet->send_profile()->currentIndex()).isNull() && !sheet->send_profile()->itemData(codex, Qt::ToolTipRole).toString().isEmpty(),
			   "marked refused with a glyph, its reason the tooltip; an allowed agent carries none");
		desktop.host().send({ .Profile = core::codex_jsonl_profile_id });
		settle(lanes);
		expect(contains(sheet->status()->text(), QStringLiteral("does not allow Codex (profile_forbidden_on_host)")) && page->projection().Facts.Rounds.size() == 2U,
			   "the denied GHES profile is refused; nothing frozen");
		expect(sends_in(agents / "codex-jsonl") == 0U && sends_in(claude) == 2U && sends_in(agents / "terminal") == 0U, "and nothing started in its place");

		// The external terminal gets the same bytes as plain text.
		desktop.set_publication_account([] { return std::optional<application::github_publication_account>{ }; });
		choose(*sheet, core::external_terminal_profile_id, { });
		expect(sheet->send_session()->count() == 1 && !sheet->send_session()->isEnabled() && sheet->send_session()->isHidden(), "a harness that cannot continue offers no session");
		expect(sheet->send_words()->text() == QStringLiteral("Runs External terminal harness in %1.").arg(folder) && !contains(sheet->send_words()->text(), QStringLiteral("Claude"))
				   && !contains(sheet->send_details()->text(), QStringLiteral("Claude")),
			   "the custom harness by its own name, never Claude: " + sheet->send_words()->text().toStdString());
		render(*sheet, QStringLiteral("send-box-custom-harness"));
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		const auto terminal = agents / "terminal";
		const auto third    = desktop.host().projection().Share->Document;
		expect(sends_in(terminal) == 1U && read_file(terminal / "send-1.args").empty() && read_file(terminal / "send-1.stdin") == third, "the terminal harness reads the document itself");
		expect(contains(sheet->status()->text(), QStringLiteral("Its response is added to F3")), "and its response is F3's");
		close_sheet(*sheet, lanes);

		// Uncertain: the harness acknowledged, then stopped. Never sent again.
		desktop.host().compose();
		settle(lanes);
		desktop.host().edit_composition(core::set_round_goal{ "Fourth round. SEND_STOP_CANARY" });
		settle(lanes);
		sheet = page->share_page();
		route->click();
		settle(lanes);
		choose(*sheet, core::claude_stream_json_profile_id, { });
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		lines = delivery_lines(*sheet);
		expect(lines.size() >= 3 && lines[lines.size() - 3].startsWith(QStringLiteral("F4 · Accepted")) && lines[lines.size() - 2].startsWith(QStringLiteral("F4 · The recipient acknowledged"))
				   && lines.back().startsWith(QStringLiteral("F4 · Delivery not confirmed")),
			   "accepted, acknowledged, then not confirmed");
		expect(contains(sheet->status()->text(), QStringLiteral("stopped before finishing its turn")) && contains(sheet->status()->text(), QStringLiteral("It was not sent again")), "said as uncertain");
		QThread::msleep(500);
		settle(lanes);
		expect(sends_in(claude) == 3U && delivery_lines(*sheet).size() == lines.size(), "no resend, no other way");
		close_sheet(*sheet, lanes);

		// A harness that is not there: failed, nothing left.
		auto missing = profiles;
		for (auto& profile : missing) {
			if (profile.Id == core::external_terminal_profile_id) {
				profile.Executable = (agents / "missing" / "bpr-agent-terminal").string();
			}
		}
		desktop.set_send_profiles(missing);
		desktop.host().compose();
		settle(lanes);
		desktop.host().edit_composition(core::set_round_goal{ "Fifth round." });
		settle(lanes);
		sheet = page->share_page();
		route->click();
		settle(lanes);
		choose(*sheet, core::external_terminal_profile_id, { });
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		expect(delivery_lines(*sheet).back().startsWith(QStringLiteral("F5 · Delivery failed")) && contains(sheet->status()->text(), QStringLiteral("could not be started")), "failed, and said so");
		close_sheet(*sheet, lanes);

		// No agent installed any more: the cycle reads every send and carries on by hand.
		desktop.set_send_profiles({ });
		sheet = compose(*page, lanes);
		expect(!route->isVisible() && !sheet->send_box()->isVisible(), "no agent, no route");
		const auto history = delivery_lines(*sheet);
		expect(history.size() == lines.size() + 1 && history.front().startsWith(QStringLiteral("F1 · Accepted")), "every send still listed");
		desktop.host().attach_reply(core::cycle_round_id{ 4 }, core::cycle_manual_reply{ .From = "Pasted from the terminal", .Body = "Finished it by hand." });
		settle(lanes);
		expect(responses(aProfileRoot, subject).size() == 4U, "a reply entered by hand continues F4");
		close_sheet(*sheet, lanes);
		desktop.shutdown();
		settle(lanes);
		std::cout << "NATIVE: real adapter, real processes: parity, sessions, corporate refusal, uncertain, missing harness, removal\n";
	}
#else
	/// Without an adapter: no route; a profile a full build left reads back whole.
	void reads_without_the_adapter(const std::filesystem::path& aProfileRoot, const std::filesystem::path& aScratch, const bool aReopen)
	{
		auto lanes  = application::manual_executor{ };
		auto window = qt::workbench_window{ };
		window.resize(1280, 800);
		window.show();
		auto desktop = app::review_cycle_desktop{ window, aProfileRoot, core::bpr_profile_id{ 1 }, lanes, lanes, { }, &lanes };
		if (!aReopen) {
			const auto project = aScratch / "absent-project";
			write_file(project / "README.md", "# Absent\n");
			auto* const page = open_cycle(desktop, window, lanes, project);
			desktop.host().comment({ .Primary = core::cycle_whole_target{ }, .Body = "Only Copy and Save here." });
			settle(lanes);
			auto* const sheet = compose(*page, lanes);
			expect(!sheet->route_button(core::cycle_share_route::send_to_agent)->isVisible() && !sheet->send_box()->isVisible() && !page->projection().Share->Send,
				   "no agent integrations: no route, no box");
			close_sheet(*sheet, lanes);
			desktop.shutdown();
			settle(lanes);
			std::cout << "NATIVE: agent-free graph: no Send to Agent route\n";
			return;
		}
		// A full build's course left this profile: every send reads back, and the
		// round carries on by hand.
		const auto listed = [&] {
			application::manual_executor      store;
			application::review_cycle_runtime runtime{ adapters::sqlite::make_review_cycle_store(aProfileRoot, core::bpr_profile_id{ 1 }), store, store, nullptr, bpr::tests::product_line_matcher() };
			auto                              page = std::optional<application::cycle_store_result<application::cycle_page>>{ };
			runtime.list(std::nullopt, [&](application::cycle_store_result<application::cycle_page> aPage) { page = std::move(aPage); });
			store.drain();
			runtime.shutdown();
			store.drain();
			expect(page && *page && !(*page)->Cycles.empty(), "the profile holds the sent cycle");
			return (*page)->Cycles.front().Subject;
		}();
		desktop.host().resume(listed);
		settle(lanes);
		auto* const page  = window.local_cycle();
		auto* const sheet = compose(*page, lanes);
		const auto  lines = delivery_lines(*sheet);
		expect(!sheet->route_button(core::cycle_share_route::send_to_agent)->isVisible() && !sheet->send_box()->isVisible(), "agent-free: no route over a cycle that was sent");
		expect(lines.size() >= 9 && lines.front().startsWith(QStringLiteral("F1 · Accepted by the agent adapter")) && lines[1].startsWith(QStringLiteral("F1 · The recipient acknowledged")),
			   "every observation a full build's sends left reads back");
		const auto before = responses(aProfileRoot, listed);
		expect(before.size() >= 4U && before.front().first.Channel == core::cycle_response_channel::agent_adapter, "and the responses, the adapter's among them");
		sheet->route_button(core::cycle_share_route::copy_document)->click();
		settle(lanes);
		expect(QGuiApplication::clipboard()->text() == QString::fromStdString(page->projection().Share->Document), "Copy shares the round");
		desktop.host().attach_reply(page->projection().Share->Round, core::cycle_manual_reply{ .From = "Reopened without agents", .Body = "Continued by hand." });
		settle(lanes);
		expect(responses(aProfileRoot, listed).size() == before.size() + 1U, "a reply entered by hand continues it");
		close_sheet(*sheet, lanes);
		desktop.shutdown();
		settle(lanes);
		static_cast<void>(aScratch);
		std::cout << "NATIVE: agent-free graph reopened a full build's sent cycle\n";
	}
#endif
}

#if defined(BPR_AGENT_INTEGRATIONS)
namespace {
	/// \brief [[BPR-1301]]. Send to Agent with the shipped profiles, whose
	/// Claude is the bare `claude`, and a Claude process wrapper saved for the
	/// window: the harness copy saved as the wrapper is what runs, with
	/// exactly Claude's arguments and the round on its input.
	void sends_through_a_wrapper(const std::filesystem::path& aFixture, const std::filesystem::path& aProfileRoot, const std::filesystem::path& aScratch)
	{
		const auto project = aScratch / "wrapper-project";
		write_file(project / "README.md", "# Wrapped\n");
		const auto folder = aScratch / "wrapper";
		std::filesystem::create_directories(folder);
		const auto wrapper = folder / ("claude-gateway" + aFixture.extension().string());
		std::filesystem::copy_file(aFixture, wrapper);
		auto source = std::make_shared<application::claude_process_wrapper_source>();
		source->set(wrapper.string());

		auto lanes  = application::manual_executor{ };
		auto window = qt::workbench_window{ };
		window.resize(1280, 800);
		window.show();
		auto desktop = app::review_cycle_desktop{ window, aProfileRoot, core::bpr_profile_id{ 1 }, lanes, lanes, { }, &lanes, source };
		desktop.set_send_profiles(core::built_in_agent_profiles());
		auto* const page = open_cycle(desktop, window, lanes, project);
		desktop.host().comment({ .Primary = core::cycle_whole_target{ }, .Body = "Run what I chose." });
		settle(lanes);
		const auto subject = *desktop.host().selected();
		static_cast<void>(settle_store<application::cycle_write_receipt>(aProfileRoot, subject, [](auto& aRuntime, auto aDone) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = 0xC8;
			aRuntime.set_channel_permission(application::cycle_operation_id{ bytes }, core::cycle_data_classification::unclassified, core::cycle_disclosure_channel::agent_adapter,
											core::cycle_channel_permission::allowed, std::move(aDone));
		}));
		auto* const sheet = compose(*page, lanes);
		sheet->route_button(core::cycle_share_route::send_to_agent)->click();
		settle(lanes);
		choose(*sheet, core::claude_stream_json_profile_id, { });
		const auto document = desktop.host().projection().Share->Document;
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		expect(sends_in(folder) == 1U && read_file(folder / "send-1.args") == "-p\n--input-format\nstream-json\n--output-format\nstream-json\n--verbose",
			   "the wrapper ran in place of the bare claude, with exactly Claude's arguments");
		expect(framed_text(read_file(folder / "send-1.stdin")) == QString::fromStdString(document), "and read the round exactly as Claude would");
		expect(contains(sheet->status()->text(), QStringLiteral("Claude finished its turn on F1")), "the send finished as Claude's: " + sheet->status()->text().toStdString());
		close_sheet(*sheet, lanes);
		desktop.shutdown();
		settle(lanes);
	}

	void sends_through_an_added_agent(const std::filesystem::path& aFixture, const std::filesystem::path& aProfileRoot, const std::filesystem::path& aScratch)
	{
		// [[BPR-1753]], owner decision 2026-10-23: Send to Agent offers
		// the agents added to the list, each run as its entry says.
		const auto project = aScratch / "entry-project";
		write_file(project / "README.md", "# Wrapped\n");
		const auto folder = aScratch / "entry";
		std::filesystem::create_directories(folder);
		const auto wrapper = folder / ("claude-gateway" + aFixture.extension().string());
		std::filesystem::copy_file(aFixture, wrapper);

		auto lanes  = application::manual_executor{ };
		auto window = qt::workbench_window{ };
		window.resize(1280, 800);
		window.show();
		auto       desktop = app::review_cycle_desktop{ window, aProfileRoot, core::bpr_profile_id{ 1 }, lanes, lanes, { }, &lanes };
		const auto added   = *core::add_agent_entry({ }, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = wrapper.string() });
		desktop.offer_agents(added);
		auto* const page = open_cycle(desktop, window, lanes, project);
		desktop.host().comment({ .Primary = core::cycle_whole_target{ }, .Body = "Run what I chose." });
		settle(lanes);
		const auto subject = *desktop.host().selected();
		static_cast<void>(settle_store<application::cycle_write_receipt>(aProfileRoot, subject, [](auto& aRuntime, auto aDone) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = 0xC8;
			aRuntime.set_channel_permission(application::cycle_operation_id{ bytes }, core::cycle_data_classification::unclassified, core::cycle_disclosure_channel::agent_adapter,
											core::cycle_channel_permission::allowed, std::move(aDone));
		}));
		auto* const sheet = compose(*page, lanes);
		sheet->route_button(core::cycle_share_route::send_to_agent)->click();
		settle(lanes);
		choose(*sheet, core::entry_profile_id(added.front()), { });
		const auto document = desktop.host().projection().Share->Document;
		sheet->send_button()->click();
		wait_for_send(desktop, lanes);
		expect(sends_in(folder) == 1U && read_file(folder / "send-1.args") == "-p\n--input-format\nstream-json\n--output-format\nstream-json\n--verbose",
			   "the added agent's own program ran, with exactly Claude's arguments");
		expect(framed_text(read_file(folder / "send-1.stdin")) == QString::fromStdString(document), "and read the round exactly as Claude would");
		expect(contains(sheet->status()->text(), QStringLiteral("Claude finished its turn on F1")), "the send finished as Claude's: " + sheet->status()->text().toStdString());
		close_sheet(*sheet, lanes);
		desktop.shutdown();
		settle(lanes);
	}
}
#endif

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	auto         arguments = std::vector<std::string>{ aArgv + 1, aArgv + aArgc };
	auto         option    = [&](const std::string_view aName) -> std::optional<std::filesystem::path> {
		const auto found = std::ranges::find(arguments, aName);
		return found != arguments.end() && found + 1 != arguments.end() ? std::optional{ std::filesystem::path{ *(found + 1) } } : std::nullopt;
	};
	tests::sqlite_contract::scratch_root scratch{ "cycle-send-course" };
	tests::sqlite_contract::scratch_root profile{ "cycle-send-profile" };
	const auto                           given = option("--profile-root");
	const auto                           root  = given ? *given : profile.path();
#if defined(BPR_AGENT_INTEGRATIONS)
	expect(!arguments.empty(), "the stand-in harness is named");
	sends_through_the_real_adapter(std::filesystem::path{ arguments.front() }, root, scratch.path());
	sends_through_a_wrapper(std::filesystem::path{ arguments.front() }, root, scratch.path());
	sends_through_an_added_agent(std::filesystem::path{ arguments.front() }, root, scratch.path());
#else
	const auto reopen = option("--reopen");
	reads_without_the_adapter(reopen ? *reopen : root, scratch.path(), reopen.has_value());
#endif
	std::cout << "review-cycle-send-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle send public path failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
