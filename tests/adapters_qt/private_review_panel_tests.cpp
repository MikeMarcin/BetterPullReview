#include "../core/private_review_fixture.hpp"
#include "bpr/qt/private_composition_editor.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QListView>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextDocument>
#include <iostream>
using namespace bpr;
using private_review_fixture::expect;
int main(int argc, char** argv)
{
	QApplication                                          app(argc, argv);
	qt::private_review_panel                              panel;
	auto                                                  created    = private_review_fixture::created();
	auto                                                  source     = core::source_of(created.Revision);
	auto                                                  projection = application::private_conversation_projection{ .Generation = 1, .Available = true, .Threads = { { created.Thread, core::private_subject_availability::current } }, .SelectedThread = created.Thread.Id, .Turns = { { .Revision = created.Revision, .Selection = { source, source.Key, false }, .Availability = core::private_subject_availability::current, .BodyBytes = source.BodyBytes } } };
	std::vector<application::private_conversation_intent> intents;
	panel.set_dispatch([&](auto aIntent) { intents.push_back(std::move(aIntent)); });
	int resets = 0, inserts = 0, changes = 0;
	QObject::connect(panel.turn_model(), &QAbstractItemModel::modelReset, [&] { ++resets; });
	QObject::connect(panel.turn_model(), &QAbstractItemModel::rowsInserted, [&] { ++inserts; });
	QObject::connect(panel.turn_model(), &QAbstractItemModel::dataChanged, [&] { ++changes; });
	panel.present(projection);
	auto* turns = panel.findChild<QListView*>(QStringLiteral("privateTurnList"));
	turns->setCurrentIndex(panel.turn_model()->index(0));
	QPersistentModelIndex retained{ panel.turn_model()->index(0) };
	projection.Turns.front().Lineage = "Composed locally; published as ordinary comment 22";
	panel.present(projection);
	expect(resets == 0 && inserts == 1 && changes == 1 && retained.isValid(), "bounded changed identity produces dataChanged without reset or index loss");
	expect(intents.empty(), "projection and revision selection have no host or agent effects");
	panel.findChild<QPushButton*>(QStringLiteral("privateSelectSource"))->click();
	expect(panel.selected_sources().size() == 1 && panel.selected_sources().front().Source == source, "exact immutable source selected");
	projection.Turns.front().Selection.Historical     = true;
	projection.Turns.front().Selection.ExpectedLatest = core::private_turn_source{ created.Thread.Id, created.Revision.Identity.Turn, core::private_review_turn_revision_id{ 9 } };
	panel.present(projection);
	panel.findChild<QPlainTextEdit*>(QStringLiteral("privateReviewerText"))->setPlainText(QStringLiteral("Synthetic question"));
	panel.findChild<QPushButton*>(QStringLiteral("privateAskAgent"))->click();
	expect(intents.size() == 1 && intents.front().Sources.front().Source == source && intents.front().Body == "Synthetic question", "one explicit semantic agent gesture carries only selected identities");
	auto run                     = core::agent_run_provenance{ };
	run.Run                      = core::agent_run_id{ 10 };
	run.ProfileDisplayName       = "Synthetic profile";
	run.Metadata.Requested.Model = "requested";
	run.Metadata.ProtocolReported.push_back({ .Basis = "protocol.model", .Settings = { .Model = "reported" } });
	panel.present_provenance(run);
	const auto detail = panel.findChild<QPlainTextEdit*>(QStringLiteral("privateProvenanceDetails"))->toPlainText();
	expect(detail.contains(QStringLiteral("unknown")) && detail.contains(QStringLiteral("Conflicting")) && detail.contains(QStringLiteral("Requested")) && detail.contains(QStringLiteral("Content self-reported")), "unknown and conflicting fact bases stay visible");
	panel.resize(420, 1300);
	panel.show();
	app.processEvents();
	const auto provenanceDirectory = qEnvironmentVariable("BPR_PRIVATE_REVIEW_VARIANTS_DIR");
	if (!provenanceDirectory.isEmpty()) {
		auto* provenanceWidget = panel.findChild<QPlainTextEdit*>(QStringLiteral("privateProvenanceDetails"));
		provenanceWidget->setTextCursor(provenanceWidget->document()->find(QStringLiteral("Conflicting")));
		provenanceWidget->ensureCursorVisible();
		expect(QDir{ }.mkpath(provenanceDirectory) && panel.grab().save(provenanceDirectory + QStringLiteral("/provenance-conflicting.png")), "conflicting provenance capture");
	}
	expect(QAccessible::queryAccessibleInterface(turns)->text(QAccessible::Name).contains(QStringLiteral("revision history")), "history is named for assistive technology");
	for (auto* button : panel.findChildren<QPushButton*>()) {
		expect(button->focusPolicy() != Qt::NoFocus && !button->text().isEmpty(), "every semantic action is named and keyboard focusable");
	}
	projection.Turns.front().BodyContinues = true;
	panel.present(projection);
	auto* pages = panel.findChild<QTabWidget*>(QStringLiteral("privateReviewPages"));
	auto  tabTo = [&](int aPage, const char* aFrom, const char* aTo) {
		pages->setCurrentIndex(aPage);
		app.processEvents();
		auto* from = panel.findChild<QPushButton*>(QString::fromLatin1(aFrom));
		auto* to   = panel.findChild<QPushButton*>(QString::fromLatin1(aTo));
		from->setFocus(Qt::TabFocusReason);
		QKeyEvent tab{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
		QApplication::sendEvent(from, &tab);
		expect(to->hasFocus(), std::string{ "Tab " } + aFrom + " -> " + aTo);
	};
	tabTo(0, "privateMoreBody", "privateProvenance");
	tabTo(1, "privateOpenDraft", "privateCompose");
	tabTo(2, "privateEdit", "privateAskAgent");
	pages->setCurrentIndex(0);
	expect(QAccessible::queryAccessibleInterface(panel.findChild<QPlainTextEdit*>(QStringLiteral("privateTurnBody")))->text(QAccessible::Name).contains(QStringLiteral("private revision")), "revision body is named as private to assistive technology");
	expect(QAccessible::queryAccessibleInterface(panel.findChild<QPlainTextEdit*>(QStringLiteral("privateProvenanceDetails")))->text(QAccessible::Name).contains(QStringLiteral("self-reported")), "provenance bases are named to assistive technology");
	auto renderVariant = [&](const QString& aName) {
		panel.present(projection);
		app.processEvents();
		const auto directory = qEnvironmentVariable("BPR_PRIVATE_REVIEW_VARIANTS_DIR");
		if (!directory.isEmpty()) {
			expect(QDir{ }.mkpath(directory) && panel.grab().save(directory + QLatin1Char('/') + aName + QStringLiteral(".png")), "synthetic variant capture");
		}
		return panel.turn_model()->index(0).data(Qt::AccessibleTextRole).toString();
	};
	projection.Turns.front().Revision.Actor = core::private_agent_actor{ core::agent_run_id{ 10 } };
	expect(renderVariant(QStringLiteral("agent-reported")).contains(QStringLiteral("Agent · run 10")), "agent and exact run remain explicit in rendered history");
	projection.Turns.front().Revision.Actor = core::private_legacy_unknown_actor{ };
	expect(renderVariant(QStringLiteral("legacy-unknown")).contains(QStringLiteral("author unknown")), "legacy author remains unknown in rendered history");
	projection.Turns.front().Revision.Actor      = core::private_reviewer_actor{ };
	projection.Turns.front().Revision.Completion = core::private_revision_completion::incomplete;
	expect(renderVariant(QStringLiteral("incomplete")).contains(QStringLiteral("incomplete")), "incomplete revision stays visibly qualified");
	projection.Turns.front().Revision.Completion = core::private_revision_completion::complete;
	projection.Turns.front().Availability        = core::private_subject_availability::stale;
	expect(renderVariant(QStringLiteral("stale")).contains(QStringLiteral("stale")), "stale subject stays visibly qualified");
	projection.Turns.front().Availability = core::private_subject_availability::unplaceable;
	expect(renderVariant(QStringLiteral("unplaceable")).contains(QStringLiteral("unplaceable")), "unplaceable subject stays visibly qualified");
	projection.Turns.front().Availability = core::private_subject_availability::current;
	projection.Turns.front().Lineage      = "Composed locally; published as ordinary comment 22";
	expect(renderVariant(QStringLiteral("composed-published")).contains(QStringLiteral("published as ordinary comment")), "composition lineage and ordinary publication remain textual");
	expect(resets == 0 && retained.isValid(), "bounded rendered variants retain selection without a full model reset");
	projection.Generation = 2;
	panel.present(projection);
	expect(panel.selected_sources().empty(), "identity replacement clears private selection");
	qt::private_composition_editor                       editor;
	std::vector<application::private_composition_intent> compositionIntents;
	editor.set_dispatch([&](auto aIntent) { compositionIntents.push_back(std::move(aIntent)); });
	auto composition = application::private_composition_projection{ .Generation = 2, .Sources = { { .Source = source, .Latest = source.Key, .Actor = created.Revision.Actor, .Availability = core::private_subject_availability::stale } }, .Status = "Stale source: select current code explicitly; no guessed line." };
	editor.present(composition);
	editor.show();
	app.processEvents();
	auto* accept = editor.findChild<QPushButton*>(QStringLiteral("privateCompositionAccept"));
	expect(!accept->isEnabled() && compositionIntents.empty(), "stale/unplaceable workspace cannot accept or silently dispatch");
	auto* mode = editor.findChild<QComboBox*>(QStringLiteral("privateCompositionMode"));
	expect(mode->count() == 3, "all three explicit composition modes are reachable");
	for (int i = 0; i < 3; ++i) {
		mode->setCurrentIndex(i);
		editor.findChild<QPushButton*>(QStringLiteral("privateCompositionCreate"))->click();
		expect(static_cast<int>(compositionIntents.back().Mode) == i, "mode gesture retains requested mode");
	}
	auto bound       = private_review_fixture::binding();
	auto index       = private_review_fixture::index();
	bound.Comparison = index.diff().Alignment.Digest;
	auto target      = core::make_composition_target(bound, { index.diff().Alignment.Digest, core::diff_side::right, 1 }, index);
	expect(target.has_value(), "exact fixture target");
	composition.Targets.push_back(*target);
	auto otherTarget = core::make_composition_target(bound, { index.diff().Alignment.Digest, core::diff_side::right, 0 }, index);
	expect(otherTarget.has_value() && *otherTarget != *target, "second exact fixture target");
	composition.Targets.push_back(*otherTarget);
	composition.Current                      = core::review_composition_state{ .Id = core::review_composition_id{ 11 }, .Binding = bound, .Target = *target, .Candidate = source };
	composition.Version                      = 3;
	composition.Candidate                    = core::review_composition_revision{ .Identity = source, .Actor = core::private_reviewer_actor{ }, .Body = "Reviewed prose" };
	composition.Sources.front().Availability = core::private_subject_availability::current;
	composition.Status                       = "Two exact targets available; choose one or split. No target is inferred.";
	composition.Current->Target.reset();
	composition.Workspaces.push_back(*composition.Current);
	editor.present(composition);
	expect(editor.findChild<QComboBox*>(QStringLiteral("privateCompositionTarget"))->count() == 3 && !accept->isEnabled(), "two exact targets require an explicit choice or split");
	const auto variantDirectory = qEnvironmentVariable("BPR_PRIVATE_REVIEW_VARIANTS_DIR");
	if (!variantDirectory.isEmpty()) {
		expect(editor.grab().save(variantDirectory + QStringLiteral("/composition-multi-target.png")), "multi-target composition capture");
	}
	editor.findChild<QPushButton*>(QStringLiteral("privateCompositionSplit"))->click();
	expect(compositionIntents.back().Action == application::private_composition_action::split, "split is a separate semantic gesture");
	composition.Current->Target = *target;
	editor.present(composition);
	expect(accept->isEnabled(), "exact candidate and target allow distinct local acceptance");
	accept->click();
	expect(compositionIntents.back().Action == application::private_composition_action::accept && compositionIntents.back().Candidate == source && compositionIntents.back().ExpectedVersion == 3, "Accept carries exact candidate and CAS, with no publish action");
	for (auto* button : editor.findChildren<QPushButton*>()) {
		expect(button->focusPolicy() != Qt::NoFocus && !button->text().startsWith(QStringLiteral("Publish")), "composition controls are keyboard reachable and cannot publish");
	}
	expect(QAccessible::queryAccessibleInterface(accept)->text(QAccessible::Name).contains(QStringLiteral("save locally")), "local accept is distinctly named");
	QWidget paletteOwner;
	auto    dark = paletteOwner.palette();
	dark.setColor(QPalette::Window, QColor{ 24, 28, 34 });
	dark.setColor(QPalette::WindowText, QColor{ 235, 238, 244 });
	paletteOwner.setPalette(dark);
	qt::private_composition_editor themed{ &paletteOwner };
	expect(themed.palette().color(QPalette::Window) == dark.color(QPalette::Window), "composition dialog adopts the current workbench palette");
	auto light = dark;
	light.setColor(QPalette::Window, QColor{ 246, 247, 249 });
	paletteOwner.setPalette(light);
	expect(themed.palette().color(QPalette::Window) == light.color(QPalette::Window), "visible theme changes update the separate composition window");
	std::cout << "private-review-panel:pass\n";
}
