#include "../core/private_review_fixture.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QListView>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
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
	panel.set_dispatch([&](auto intent) { intents.push_back(std::move(intent)); });
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
	expect(QAccessible::queryAccessibleInterface(turns)->text(QAccessible::Name).contains(QStringLiteral("revision history")), "history is named for assistive technology");
	for (auto* button : panel.findChildren<QPushButton*>()) {
		expect(button->focusPolicy() != Qt::NoFocus && !button->text().isEmpty(), "every semantic action is named and keyboard focusable");
	}
	projection.Generation = 2;
	panel.present(projection);
	expect(panel.selected_sources().empty(), "identity replacement clears private selection");
	std::cout << "private-review-panel:pass\n";
}
