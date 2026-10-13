// RFC-BPR-0072: real Git-backed comparison, palette controls and fresh processes.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/theme_exchange.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QThread>
#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;
	using access            = app::detail::github_application_fixture_access;
	using role              = core::appearance_role;
	constexpr auto variants = std::array{ core::appearance_variant::light, core::appearance_variant::dark, core::appearance_variant::high_contrast_light, core::appearance_variant::high_contrast_dark };
	constexpr auto modes    = std::array{ core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark };

	constexpr auto at(const role aRole) -> std::size_t
	{
		return static_cast<std::size_t>(aRole);
	}

	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20'000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	auto legacy_palette(const core::appearance_variant aVariant) -> core::appearance_palette
	{
		auto palette = qt::builtin_appearance_palette(aVariant);
		// Existing customized values that pass the same text contrast checks.
		palette[at(role::syntax_comment)]  = palette[at(role::syntax_normal)];
		palette[at(role::syntax_function)] = palette[at(role::syntax_normal)];
		return palette;
	}

	constexpr auto color(const unsigned aRgb) -> core::appearance_color
	{
		return { .Red = static_cast<std::uint8_t>(aRgb >> 16U), .Green = static_cast<std::uint8_t>(aRgb >> 8U), .Blue = static_cast<std::uint8_t>(aRgb), .Alpha = 255U };
	}

	auto imported_palette(const core::appearance_variant aVariant) -> core::appearance_palette
	{
		const auto light   = aVariant == core::appearance_variant::light || aVariant == core::appearance_variant::high_contrast_light;
		auto       palette = core::appearance_palette{ };
		palette.fill(color(light ? 0x000000U : 0xFFFFFFU));
		for (const auto& pair : core::required_contrast_pairs()) {
			palette[at(pair.Background)] = color(light ? 0xFFFFFFU : 0x000000U);
			if (pair.Wash) {
				palette[at(*pair.Wash)] = color(light ? 0xFFFFFFU : 0x000000U);
			}
		}
		// A valid custom theme under the existing all-variant import floor.
		// These pre-existing overrides remain distinguishable from plain ink.
		palette[at(role::syntax_comment)]  = color(light ? 0x333333U : 0xDDDDDDU);
		palette[at(role::syntax_function)] = color(light ? 0x004040U : 0xCCFFFFU);
		for (const auto category : { role::syntax_namespace, role::syntax_enum_member }) {
			palette[at(category)] = qt::builtin_appearance_palette(aVariant)[at(category)];
		}
		return palette;
	}

	auto customized_palette(const core::appearance_variant aVariant) -> core::appearance_palette
	{
		const auto     index                  = static_cast<std::size_t>(aVariant);
		constexpr auto namespaces             = std::array{ 0x102030U, 0xE0EBFFU, 0x301020U, 0xFFECD0U };
		constexpr auto members                = std::array{ 0x303010U, 0xFFF0C0U, 0x103040U, 0xD0FFF0U };
		auto           palette                = imported_palette(aVariant);
		palette[at(role::syntax_namespace)]   = color(namespaces[index]);
		palette[at(role::syntax_enum_member)] = color(members[index]);
		return palette;
	}

	auto baseline() -> application::appearance_settings
	{
		auto settings                                      = application::appearance_settings{ };
		settings.Mode                                      = core::appearance_mode::light;
		settings.Typography.Ui.Font.Family                 = "Arial";
		settings.Typography.Ui.Font.PointSizeCentipoints   = 1200U;
		settings.Typography.Code.Font.Family               = "Courier";
		settings.Typography.Code.Font.PointSizeCentipoints = 1100U;
		settings.Typography.Code.TabWidth                  = 8U;
		settings.Typography.Code.SyntaxBold                = false;
		settings.Typography.Code.SyntaxItalic              = false;
		for (std::size_t index = 0U; index < variants.size(); ++index) {
			settings.Palettes[index] = { .Palette = legacy_palette(variants[index]), .Stored = index % 2U == 0U };
		}
		return settings;
	}

	auto old_document(const core::appearance_palette& aPalette) -> std::string
	{
		const auto document = core::export_theme(aPalette);
		auto       result   = std::string{ };
		for (const auto& line : std::views::split(document, '\n')) {
			const auto text = std::string_view{ line.begin(), line.end() };
			if (!text.empty() && !text.starts_with("syntax_namespace ") && !text.starts_with("syntax_enum_member ")) {
				result.append(text);
				result.push_back('\n');
			}
		}
		return result;
	}

	void write_document(const std::filesystem::path& aPath, const std::string& aDocument)
	{
		QFile file{ QString::fromStdString(aPath.string()) };
		expect(file.open(QIODevice::WriteOnly | QIODevice::NewOnly), "theme fixture is written to a new owned file");
		expect(file.write(QByteArray::fromStdString(aDocument)) == static_cast<qint64>(aDocument.size()), "complete theme fixture written");
	}

	auto read_document(const std::filesystem::path& aPath) -> std::string
	{
		QFile file{ QString::fromStdString(aPath.string()) };
		expect(file.open(QIODevice::ReadOnly), "own exported theme opens");
		return file.readAll().toStdString();
	}

	void seed_legacy(const std::filesystem::path& aRoot)
	{
		const auto settings = baseline();
		{
			auto store = adapters::sqlite::make_appearance_settings_store({ .StorageRoot = aRoot });
			expect(store.has_value() && (*store)->store(settings, { }).has_value(), "real appearance store seeds existing four-variant preferences");
		}
		::sqlite3* database = nullptr;
		expect(::sqlite3_open_v2((aRoot / "state/v1/appearance-settings.sqlite3").string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "course opens its own old-palette database");
		for (std::size_t index = 0U; index < variants.size(); ++index) {
			const auto      document  = old_document(settings.Palettes[index].Palette);
			const auto      sql       = "UPDATE appearance_settings SET " + std::string{ core::stable_name(variants[index]) } + "_palette_document=?1 WHERE singleton=1";
			::sqlite3_stmt* statement = nullptr;
			expect(::sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK, "old palette fixture update prepares");
			expect(::sqlite3_bind_text(statement, 1, document.data(), static_cast<int>(document.size()), SQLITE_TRANSIENT) == SQLITE_OK && ::sqlite3_step(statement) == SQLITE_DONE, "only new role lines are omitted, all old columns and flags retained");
			::sqlite3_finalize(statement);
			write_document(aRoot / (std::string{ core::stable_name(variants[index]) } + "-old.bprtheme"), old_document(imported_palette(variants[index])));
		}
		expect(::sqlite3_close(database) == SQLITE_OK, "seed database closes before next OS process");
	}

	void check_stored(const application::appearance_settings& aSettings, const bool aCustomized)
	{
		const auto expected = baseline();
		expect(aSettings.Typography == expected.Typography && aSettings.Dif == expected.Dif, "restart preserves unrelated typography and mascot state");
		for (std::size_t index = 0U; index < variants.size(); ++index) {
			const auto& stored = aSettings.Palettes[index];
			expect(stored.Stored == (aCustomized || expected.Palettes[index].Stored), "restart preserves each independent stored flag");
			if (aCustomized) {
				expect(stored.Palette == customized_palette(variants[index]), "new-role colors and every existing value survive restart independently per variant");
			} else {
				for (std::size_t colour = 0U; colour < at(role::syntax_namespace); ++colour) {
					expect(stored.Palette[colour] == expected.Palettes[index].Palette[colour], "loading old palette preserves every pre-existing value, even in an unstored slot");
				}
				expect(stored.Palette[at(role::syntax_namespace)] == core::unassigned_appearance_color && stored.Palette[at(role::syntax_enum_member)] == core::unassigned_appearance_color, "old palettes defer both new defaults to their own presentation variant");
			}
		}
	}

	void open_comparison(app::github_application& aApplication)
	{
		auto& window = aApplication.window();
		window.resize(1440, 1100);
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/cpp-identifiers#72"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "synthetic pull request opens on Overview");
		auto step  = app::detail::course_modality_step{ };
		step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
		wait_for([&] { return access::enter_course_modality(aApplication, step) != app::detail::course_modality_result::pending; }, "course enters visible Review comparison");
		window.next_file_action()->trigger();
		wait_for([&] {
			const auto* surface = window.comparison_surface();
			return surface->current_result() && !surface->geometry_pending() && surface->token_spans(core::diff_side::left) && surface->token_spans(core::diff_side::right);
		},
				 "installed Git/cache actor supplies both blobs, classification and measured comparison");
		expect(window.comparison_surface()->right_pane()->toPlainText().contains(QStringLiteral("changed = 2")) && window.comparison_surface()->left_pane()->toPlainText().contains(QStringLiteral("changed = 1")), "both actual Git endpoints are displayed");
		for (auto* action : { window.repositories_drawer_toggle_action(), window.files_drawer_toggle_action(), window.conversations_drawer_toggle_action(), window.line_details_action() }) {
			if (action->isChecked()) {
				action->trigger();
			}
		}
		wait_for([&] { return !window.comparison_surface()->geometry_pending() && window.comparison_surface()->left_pane()->viewport()->width() > 500; }, "public drawer controls give both panes room for qualified names");
	}

	void check_token(QPlainTextEdit& aPane, const QString& aLine, const QString& aToken, const QColor& aColour)
	{
		auto block = aPane.document()->begin();
		while (block.isValid() && !block.text().contains(aLine)) {
			block = block.next();
		}
		expect(block.isValid() && block.layout(), "expected declaration or qualified use is in the displayed document");
		const auto begin = block.text().indexOf(aToken, block.text().indexOf(aLine));
		expect(begin >= 0, "exact token occurs at expected site");
		for (qsizetype offset = 0; offset < aToken.size(); ++offset) {
			const auto formats = block.layout()->formats();
			expect(std::ranges::any_of(formats, [&](const auto& aFormat) { return begin + offset >= aFormat.start && begin + offset < aFormat.start + aFormat.length && aFormat.format.foreground().color() == aColour; }), "every token character receives its required color on the actual pane layout");
		}
		auto cursor = QTextCursor{ block };
		cursor.setPosition(block.position() + static_cast<int>(begin));
		const auto start = aPane.cursorRect(cursor);
		cursor.setPosition(block.position() + static_cast<int>(begin + aToken.size()));
		const auto finish = aPane.cursorRect(cursor);
		const auto area   = QRect{ start.topLeft(), QPoint{ finish.left(), start.bottom() } }.intersected(aPane.viewport()->rect());
		if (area.isEmpty()) {
			std::cerr << "token-not-visible:" << aLine.toStdString() << ":" << aToken.toStdString() << ":line=" << block.blockNumber() << ":origin=" << start.x() << "," << start.y() << ":viewport=" << aPane.viewport()->width() << "," << aPane.viewport()->height() << '\n';
		}
		expect(!area.isEmpty(), "verified token is visible in the offscreen raster");
		const auto image   = aPane.viewport()->grab(area).toImage();
		bool       colored = false;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				colored          = colored || (std::abs(pixel.red() - aColour.red()) <= 3 && std::abs(pixel.green() - aColour.green()) <= 3 && std::abs(pixel.blue() - aColour.blue()) <= 3);
			}
		}
		expect(colored, "token's required foreground appears in its visible painted pixels");
	}

	void check_render(app::github_application& aApplication, const core::appearance_variant aVariant)
	{
		auto*       surface = aApplication.window().comparison_surface();
		const auto& colors  = qt::builtin_appearance_palette(aVariant);
		const auto  root    = qEnvironmentVariable("BPR_CPP_IDENTIFIER_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "render evidence directory exists");
			expect(aApplication.window().grab().save(root + QLatin1Char{ '/' } + QString::fromUtf8(core::stable_name(aVariant).data(), static_cast<qsizetype>(core::stable_name(aVariant).size())) + QStringLiteral(".png")), "full composed Light/Dark window saved offscreen");
		}
		const auto check = [&](QPlainTextEdit& aPane, const QString& aLine, const QString& aToken, const role aRole) { check_token(aPane, aLine, aToken, qt::to_presentation_color(colors[at(aRole)])); };
		for (auto* pane : { surface->left_pane(), surface->right_pane() }) {
			check(*pane, QStringLiteral("namespace GCT"), QStringLiteral("GCT"), role::syntax_namespace);
			check(*pane, QStringLiteral("namespace GCT"), QStringLiteral("Network"), role::syntax_namespace);
			check(*pane, QStringLiteral("namespace MixedCase"), QStringLiteral("MixedCase"), role::syntax_namespace);
			for (const auto& token : { QStringLiteral("Node"), QStringLiteral("ID"), QStringLiteral("Player"), QStringLiteral("DNA") }) {
				check(*pane, QStringLiteral("    ") + token, token, role::syntax_enum_member);
			}
			check(*pane, QStringLiteral("Reason::ID;"), QStringLiteral("ID"), role::syntax_enum_member);
			check(*pane, QStringLiteral("preview_emit_mode::Full"), QStringLiteral("Full"), role::syntax_enum_member);
			check(*pane, QStringLiteral("selected = GCT"), QStringLiteral("GCT"), role::syntax_namespace);
			check(*pane, QStringLiteral("selected = GCT"), QStringLiteral("DNA"), role::syntax_enum_member);
			check(*pane, QStringLiteral("colour = MixedCase"), QStringLiteral("MixedCase"), role::syntax_namespace);
			check(*pane, QStringLiteral("colour = MixedCase"), QStringLiteral("Red"), role::syntax_enum_member);
			check(*pane, QStringLiteral("void verify()"), QStringLiteral("GCT_ASSERT"), role::syntax_macro);
		}
		expect(colors[at(role::syntax_namespace)] != colors[at(role::syntax_macro)] && colors[at(role::syntax_enum_member)] != colors[at(role::syntax_macro)], "macro pixels cannot satisfy new-category color assertions");
	}

	void commit_preview(app::github_application& aApplication, const core::appearance_palette& aPalette)
	{
		auto* panel = aApplication.window().appearance_settings();
		try {
			wait_for([&] { return panel->preview_palette() && *panel->preview_palette() == aPalette && panel->keep_changes_button(); }, "real host validates and previews exact palette");
		} catch (...) {
			std::cerr << "preview-status:" << panel->status_text().toStdString() << ":variant=" << static_cast<int>(*panel->resolved_variant()) << ":has-preview=" << panel->preview_palette().has_value() << '\n';
			if (panel->preview_palette()) {
				for (std::size_t index = 0; index < core::appearance_role_count; ++index) {
					if ((*panel->preview_palette())[index] != aPalette[index]) {
						std::cerr << "preview-diff:" << index << '\n';
					}
				}
			}
			throw;
		}

		panel->keep_changes_button()->click();
		wait_for([&] { return !panel->preview_palette() && panel->adopted().Palettes[static_cast<std::size_t>(*panel->resolved_variant())].Palette == aPalette; }, "Keep changes commits exact palette through host");
	}

	void edit_role(app::github_application& aApplication, const role aRole, const core::appearance_palette& aExpected)
	{
		auto*      panel   = aApplication.window().appearance_settings();
		const auto before  = panel->adopted();
		const auto variant = static_cast<std::size_t>(*panel->resolved_variant());
		const auto wanted  = qt::to_presentation_color(aExpected[at(aRole)]);
		panel->set_colour_picker([&](const qt::colour_pick_request& aRequest) -> std::optional<QColor> { aRequest.Changed(wanted); return wanted; });
		panel->colours()->swatch_of(aRole)->click();
		commit_preview(aApplication, aExpected);
		panel->set_colour_picker(qt::platform_colour_picker());
		for (std::size_t index = 0; index < variants.size(); ++index) {
			if (index != variant) {
				expect(panel->adopted().Palettes[index] == before.Palettes[index], "editing one role leaves every other variant's values and stored state unchanged");
			}
		}
	}

	void exchange(app::github_application& aApplication, const std::filesystem::path& aRoot, const bool aImportNew)
	{
		auto* panel = aApplication.window().appearance_settings();
		for (std::size_t index = 0U; index < variants.size(); ++index) {
			panel->choose_mode(modes[index]);
			wait_for([&] { return panel->resolved_variant() == variants[index]; }, "selected theme owns colors and exchange");
			const auto beforeImport = panel->adopted();
			const auto stem         = std::string{ core::stable_name(variants[index]) };
			panel->request_theme_import(QString::fromStdString((aRoot / (stem + (aImportNew ? "-new.bprtheme" : "-old.bprtheme"))).string()));
			commit_preview(aApplication, aImportNew ? customized_palette(variants[index]) : imported_palette(variants[index]));
			for (std::size_t other = 0; other < variants.size(); ++other) {
				if (other != index) {
					expect(panel->adopted().Palettes[other] == beforeImport.Palettes[other], "old and new theme import preserve every other variant's customized values and stored flag");
				}
			}
			if (!aImportNew) {
				auto namespaceOnly                        = imported_palette(variants[index]);
				namespaceOnly[at(role::syntax_namespace)] = customized_palette(variants[index])[at(role::syntax_namespace)];
				edit_role(aApplication, role::syntax_namespace, namespaceOnly);
				expect(panel->adopted().Palettes[index].Palette[at(role::syntax_enum_member)] == imported_palette(variants[index])[at(role::syntax_enum_member)], "namespace edit does not borrow or change enum-member slot");
				edit_role(aApplication, role::syntax_enum_member, customized_palette(variants[index]));
				const auto path = aRoot / (stem + "-new.bprtheme");
				panel->request_theme_export(QString::fromStdString(path.string()));
				wait_for([&] { return std::filesystem::exists(path); }, "real host atomically exports selected committed palette");
				const auto document = read_document(path);
				expect(document.find("syntax_namespace ") != std::string::npos && document.find("syntax_enum_member ") != std::string::npos, "new export includes both stable identities");
				expect(core::import_theme(document).Palette == customized_palette(variants[index]), "new export/import preserves all 62 role values exactly");
			}
		}
	}

	void child(const std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = aRoot;
		std::unique_ptr<app::github_application> application;
		bool                                     terminal = false;
		auto                                     startup  = access::start(app::detail::github_public_path_fixture::cpp_identifiers_v1, options, [&](auto aResult) { terminal = true; if (aResult) { application = std::move(*aResult); } });
		wait_for([&] { return terminal && startup->is_quiescent(); }, "public composition startup completes");
		expect(application && application->status().AppearancePersistenceAvailable && application->status().AppearanceDurableWrites == 0U, "new OS process reads durable preferences without rewriting them");
		application->show();
		auto* panel = application->window().appearance_settings();
		if (aPhase == "render") {
			open_comparison(*application);
			const auto  initial = application->status().ClassificationSubmissions;
			const auto* result  = application->window().comparison_surface()->current_result().get();
			const auto  left    = application->window().comparison_surface()->token_spans(core::diff_side::left);
			const auto  right   = application->window().comparison_surface()->token_spans(core::diff_side::right);
			for (std::size_t index = 0; index < 2U; ++index) {
				panel->choose_mode(modes[index]);
				wait_for([&] { return panel->resolved_variant() == variants[index]; }, "composed theme changes to exact Light or Dark variant");
				QApplication::processEvents();
				check_render(*application, variants[index]);
			}
			expect(application->status().ClassificationSubmissions == initial && application->window().comparison_surface()->current_result().get() == result && application->window().comparison_surface()->token_spans(core::diff_side::left) == left && application->window().comparison_surface()->token_spans(core::diff_side::right) == right, "theme changes repaint both panes without reclassification or comparison replacement");
		} else {
			wait_for([&] { return panel->adopted().Typography.Code.TabWidth == 8U; }, "asynchronous appearance restore publishes the durable record before it is judged");
			check_stored(panel->adopted(), aPhase != "legacy");
			if (aPhase == "legacy") {
				for (std::size_t index = 0; index < variants.size(); ++index) {
					panel->choose_mode(modes[index]);
					wait_for([&] { return panel->resolved_variant() == variants[index]; }, "old palette resolves its own variant");
					expect(panel->colours()->value_text(role::syntax_namespace).contains(qt::to_presentation_color(qt::builtin_appearance_palette(variants[index])[at(role::syntax_namespace)]).name(), Qt::CaseInsensitive) && panel->colours()->value_text(role::syntax_enum_member).contains(qt::to_presentation_color(qt::builtin_appearance_palette(variants[index])[at(role::syntax_enum_member)]).name(), Qt::CaseInsensitive), "old stored/unstored palette displays each new built-in default without resetting old values");
				}
				check_stored(panel->adopted(), false);
				exchange(*application, aRoot, false);
			} else if (aPhase == "new-import") {
				exchange(*application, aRoot, true);
			}
		}
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "all host effects and durable writes drain before process exit");
		application.reset();
		std::cout << "cpp-identifier-phase:" << aPhase << ":pass\n";
	}

	void phase(const QString& aPhase, const std::filesystem::path& aRoot)
	{
		QProcess process;
		process.setProgram(QFileInfo{ QCoreApplication::applicationFilePath() }.canonicalFilePath());
		process.setArguments({ QStringLiteral("--phase"), aPhase, QString::fromStdString(aRoot.string()) });
		process.setWorkingDirectory(QString::fromStdString(aRoot.parent_path().string()));
		process.start();
		expect(process.waitForStarted(5000), "owned fresh-process course starts");
		const auto finished = process.waitForFinished(90'000);
		if (!finished) {
			process.kill();
			process.waitForFinished(5000);
		}
		const auto output = process.readAllStandardOutput();
		const auto errors = process.readAllStandardError();
		if (!finished || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
			std::cerr << aPhase.toStdString() << ':' << output.toStdString() << errors.toStdString();
		}
		expect(finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && output.contains((QStringLiteral("cpp-identifier-phase:") + aPhase + QStringLiteral(":pass")).toUtf8()), "exact fresh-process course passes completely");
		expect(!errors.contains("AGENT_WORKSPACE_RECOVERY_REFUSED"), "private workspace startup recovery stays available");
		std::cout << output.toStdString();
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	if (aArgc == 4 && std::string_view{ aArgv[1] } == "--phase") {
		child(aArgv[2], app::native_filesystem_path(QString::fromLocal8Bit(aArgv[3])));
		return 0;
	}
	for (const auto variant : variants) {
		const auto findings = core::measure_required_contrast(qt::builtin_appearance_palette(variant), variant);
		expect(findings.size() == core::required_contrast_pairs().size() && std::ranges::all_of(findings, [](const auto& aFinding) { return aFinding.Meets; }), "every published text, diff and selection contrast pair passes in all four built-in variants");
		for (const auto auditedVariant : variants) {
			expect(core::failing_contrast(imported_palette(variant), auditedVariant).empty() && core::failing_contrast(customized_palette(variant), auditedVariant).empty(), "custom import and edited fixtures satisfy the existing all-variant admission floor, including selection washes");
		}
	}
	std::cout << "CPP_IDENTIFIER_CONTRAST_OK pairs=" << core::required_contrast_pairs().size() << " variants=4\n";
	QTemporaryDir rendering;
	const auto    renderedRoot = e2e::private_data_root(rendering);
	expect(renderedRoot.has_value(), "render course owns its private data root");
	phase(QStringLiteral("render"), *renderedRoot);
	expect(e2e::remove_course_root(rendering), "render course removes only its own data");
	QTemporaryDir legacy;
	const auto    root = e2e::private_data_root(legacy);
	expect(root.has_value(), "exchange course owns its private data root");
	seed_legacy(*root);
	phase(QStringLiteral("legacy"), *root);
	phase(QStringLiteral("restart"), *root);
	phase(QStringLiteral("new-import"), *root);
	phase(QStringLiteral("restart"), *root);
	expect(e2e::remove_course_root(legacy), "all exchange data removed after child processes exit");
	std::cout << "CPP_IDENTIFIER_APPEARANCE_OK variants=4 panes=2 roles=62 qt=" << qVersion() << '\n';
}
