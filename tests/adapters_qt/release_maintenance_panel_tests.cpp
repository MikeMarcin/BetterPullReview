#include "bpr/application/manual_executor.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"
#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QTextCursor>
#include <QThread>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {
	using namespace bpr;
	using command = core::release_maintenance_command;
	using reason  = core::release_maintenance_reason;
	using phase   = core::release_maintenance_phase;
	struct announcement
	{
		QObject*                            Target;
		QString                             Text;
		QAccessible::AnnouncementPoliteness Politeness;
	};
	std::vector<announcement>* Announcements = nullptr;
	void                       accessible_update(QAccessibleEvent* aEvent)
	{
		if (Announcements && aEvent && aEvent->type() == QAccessible::Announcement) {
			const auto* event = static_cast<const QAccessibleAnnouncementEvent*>(aEvent);
			Announcements->push_back({ event->object(), event->message(), event->politeness() });
		}
	}
	class accessible_capture final
	{
	public:
		explicit accessible_capture(std::vector<announcement>& aEvents)
			: Previous{ QAccessible::installUpdateHandler(accessible_update) }
		{
			Announcements = &aEvents;
		}
		~accessible_capture()
		{
			Announcements = nullptr;
			static_cast<void>(QAccessible::installUpdateHandler(Previous));
		}
		accessible_capture(const accessible_capture&)                    = delete;
		auto operator=(const accessible_capture&) -> accessible_capture& = delete;

	private:
		QAccessible::UpdateHandler Previous;
	};
	void expect(bool aValue, std::string_view aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void key(QWidget& aWidget, int aKey)
	{
		QKeyEvent press{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QKeyEvent release{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}
	void dismissed(const QPointer<QMessageBox>& aBox)
	{
		QElapsedTimer timer;
		timer.start();
		while (aBox && aBox->isVisible() && timer.elapsed() < 2000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(!aBox || !aBox->isVisible(), "dialog keyboard dismissal completes");
	}
	void capture(QWidget& aWidget, const QString& aSuffix)
	{
		const auto prefix = qEnvironmentVariable("BPR_MAINTENANCE_RENDER_PREFIX");
		const auto raster = aWidget.grab();
		expect(!raster.isNull(), "real widget hierarchy renders");
		if (!prefix.isEmpty()) {
			expect(raster.save(prefix + aSuffix + QStringLiteral(".png")), "render artifact saved");
		}
	}
	class preparation final : public application::release_maintenance_preparation
	{
	public:
		application::release_diagnostic_document Document = *application::prepare_release_diagnostics(
			"2026-09-20T00:00:00Z v=1 op=application outcome=failed request=987 http_status=0 summary=sanitized dropped=2 detail=SOURCE_CANARY TOKEN_CANARY COMMENT_CANARY\nlegacy SECRET_CANARY\n", core::release_maintenance_revision{ 7U });
		core::release_maintenance_selection Selection = Document.Descriptor;
		auto                                selection() const -> core::release_maintenance_selection override { return Selection; }
		auto                                preview() const noexcept -> std::string_view override { return Document.Bytes; }
	};
	class storage final : public application::release_maintenance_storage_port
	{
	public:
		std::thread::id                                  Gui      = std::this_thread::get_id();
		unsigned                                         Prepares = 0U;
		unsigned                                         Erases   = 0U;
		bool                                             Fail     = false;
		std::optional<reason>                            Refusal;
		std::optional<core::release_maintenance_context> Erased;
		auto                                             authority() const noexcept -> core::release_maintenance_authority override { return { core::bpr_profile_id{ 31U }, core::application_session_id{ 41U }, core::release_maintenance_root_id{ 51U } }; }
		auto                                             capabilities() const noexcept -> core::release_maintenance_capabilities override { return { true, true, true, true }; }
		auto                                             prepare(const core::release_maintenance_context& aContext, std::stop_token)
			-> std::expected<std::shared_ptr<const application::release_maintenance_preparation>, reason> override
		{
			expect(std::this_thread::get_id() != Gui, "preparation enters storage only on the worker");
			++Prepares;
			if (Refusal) {
				return std::unexpected(*Refusal);
			}
			auto result = std::make_shared<preparation>();
			if (aContext.Operation == command::preview_diagnostics) {
				result->Document = *application::prepare_diagnostic_export(
					"2026-09-20T00:00:00Z v=1 op=application outcome=failed request=987 http_status=0 summary=sanitized dropped=2 detail=SOURCE_CANARY\nlegacy SECRET_CANARY\n", core::release_maintenance_revision{ 7U }, aContext.ExportOptions, { }, "0123456789abcdef0123456789abcdef");
				result->Selection = result->Document.Descriptor;
			}
			if (aContext.Operation != command::preview_diagnostics) {
				core::release_maintenance_inventory inventory{ core::release_maintenance_revision{ 9U }, { }, aContext.Operation == command::prepare_purge ? core::release_profile_purge_categories : 1U, 6U, 0U, true };
				inventory.Digest[0] = 99U;
				result->Selection   = inventory;
			}
			return result;
		}
		auto revalidate(const core::release_maintenance_context&, const application::release_maintenance_preparation&)
			-> application::release_maintenance_result override { return { }; }
		auto erase(const core::release_maintenance_context& aContext, const application::release_maintenance_preparation& aPreparation,
				   std::stop_token, const std::function<bool()>&                                                          aCommit) -> application::release_maintenance_result override
		{
			expect(std::this_thread::get_id() != Gui, "deletion enters storage only on the worker");
			expect(aContext.Selection == aPreparation.selection(), "exact prepared descriptor reaches deletion");
			if (!aCommit()) {
				return std::unexpected(reason::cancelled);
			}
			++Erases;
			Erased = aContext;
			return Fail ? application::release_maintenance_result{ std::unexpected(reason::io_failure) } : application::release_maintenance_result{ };
		}
	};
	class lifecycle final : public application::release_maintenance_lifecycle_port
	{
	public:
		std::function<void(application::release_maintenance_result)> Complete;
		unsigned                                                     Calls = 0U;
		void                                                         quiesce(const core::release_maintenance_context&, std::stop_token,
																			 std::function<void(application::release_maintenance_result)> aComplete) override
		{
			++Calls;
			Complete = std::move(aComplete);
		}
	};
	class projection final : public application::release_maintenance_projection_port
	{
	public:
		qt::release_maintenance_panel&              Panel;
		application::release_maintenance_projection Last;
		explicit projection(qt::release_maintenance_panel& aPanel)
			: Panel{ aPanel }
		{ }
		void publish(const application::release_maintenance_projection& aProjection) override
		{
			expect(QThread::currentThread() == qApp->thread(), "projection returns to GUI lane");
			Last = aProjection;
			Panel.adopt(aProjection);
		}
	};
	struct fixture
	{
		qt::workbench_window                              Window;
		qt::release_maintenance_panel*                    Panel = new qt::release_maintenance_panel;
		storage                                           Storage;
		lifecycle                                         Lifecycle;
		application::manual_executor                      Effects;
		application::manual_executor                      Decisions;
		projection                                        Projection{ *Panel };
		application::release_maintenance_application_host Host{ Storage, Lifecycle, Effects, Decisions, Projection };
		std::vector<core::release_maintenance_intent>     Intents;
		application::diagnostic_export_directory          Directory = QDir::tempPath().toStdString();
		std::optional<core::release_maintenance_intent>   DirectoryRequest;
		bool                                              HoldDirectory = false;
		fixture()
		{
			Panel->set_profile_label(QStringLiteral("<b>Fixture</b>"));
			Panel->set_dispatch({ .Intent = [this](auto aIntent) { Intents.push_back(aIntent); Host.dispatch(aIntent); }, .Export = { }, .ChooseExportDirectory = [this](auto aIntent) {
					DirectoryRequest = aIntent;
					if (!HoldDirectory) {
						Panel->present_export_directory(aIntent, Directory);
					} } });
			Projection.publish({ Host.state(), core::release_maintenance_commands(Host.state()), { }, { }, reason::none });
			Window.settings()->adopt_panel(qt::settings_page::storage_and_privacy, Panel);
			Window.settings()->resize(760, 480);
			Window.settings()->present(qt::settings_page::storage_and_privacy);
			QApplication::processEvents();
		}
		~fixture()
		{
			Panel->set_dispatch({ });
			Host.shutdown();
			if (Lifecycle.Complete) {
				Lifecycle.Complete(std::unexpected(reason::cancelled));
			}
			drain();
		}
		void drain()
		{
			// No simultaneous access to either manual queue; joining is test orchestration only.
			std::thread worker{ [this] { Effects.drain(); } };
			worker.join();
			Decisions.drain();
			QApplication::processEvents();
		}
		auto button(command aCommand) -> QPushButton*
		{
			return Panel->findChild<QPushButton*>(QStringLiteral("releaseMaintenanceCommand%1").arg(static_cast<int>(aCommand)));
		}
		auto confirmation() -> QMessageBox*
		{
			QMessageBox* box = nullptr;
			for (auto* candidate : Panel->findChildren<QMessageBox*>(QStringLiteral("releaseMaintenanceConfirmation"))) {
				if (candidate->isVisible()) {
					box = candidate;
				}
			}
			expect(box && box->isVisible(), "real confirmation is visible");
			expect(box->defaultButton() == box->button(QMessageBox::Cancel) && box->escapeButton() == box->button(QMessageBox::Cancel), "Cancel is default and Escape");
			expect(box->textFormat() == Qt::PlainText && box->text().contains(QStringLiteral("<b>Fixture</b>")), "profile label is literal and identity is explicit");
			return box;
		}
		void proceed()
		{
			auto* box = confirmation();
			for (auto* button : box->buttons()) {
				if (box->buttonRole(button) == QMessageBox::DestructiveRole) {
					button->click();
					QApplication::processEvents();
					return;
				}
			}
			expect(false, "explicit destructive gesture exists");
		}
		void prepare(command aCommand)
		{
			Panel->action(aCommand)->trigger();
			proceed();
			expect(Lifecycle.Complete && Host.state().Phase == phase::preparing, "preparation waits on lifecycle barrier");
			Lifecycle.Complete({ });
			Decisions.drain();
			drain();
			expect(Host.state().Prepared.has_value(), "typed host accepts prepared inventory");
		}
	};
	void preview_keyboard_and_incremental()
	{
		fixture                   test;
		std::vector<announcement> announcements;
		const accessible_capture  accessibility{ announcements };
		auto*                     button = test.button(command::preview_diagnostics);
		button->setFocus();
		expect(button->hasFocus(), "preview is keyboard focusable");
		auto* accessible = QAccessible::queryAccessibleInterface(button);
		expect(accessible && accessible->text(QAccessible::Name) == button->text() && !accessible->state().disabled, "accessible action name and availability");
		key(*button, Qt::Key_Space);
		expect(test.Storage.Prepares == 0U && test.Effects.pending() == 1U, "gesture queues host work without storage entry");
		test.drain();
		auto* status = test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"));
		expect(announcements.size() == 2U && announcements.back().Target == status && announcements.back().Text == status->text()
				   && announcements.back().Politeness == QAccessible::AnnouncementPoliteness::Polite,
			   "preparing and ready each announce one polite exact status");
		auto* preview = test.Panel->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"));
		expect(preview && preview->isReadOnly() && preview->toPlainText().contains(QStringLiteral("bpr-diagnostics-v2")) && !preview->toPlainText().contains(QStringLiteral("CANARY")), "rendered preview contains only classified frozen metadata");
		expect(test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceSummary"))->text().contains(QStringLiteral("1 records omitted")), "omission count visible");
		test.Panel->findChild<QToolButton*>(QStringLiteral("diagnosticExportRecords"))->setChecked(true);
		QApplication::processEvents();
		auto cursor = preview->textCursor();
		cursor.setPosition(2);
		cursor.setPosition(10, QTextCursor::KeepAnchor);
		preview->setTextCursor(cursor);
		preview->setFocus();
		unsigned   changes = 0U;
		const auto changed = QObject::connect(preview, &QPlainTextEdit::textChanged, preview, [&] { ++changes; });
		test.Panel->adopt(test.Projection.Last);
		expect(changes == 0U && announcements.size() == 2U && preview->textCursor().selectedText() == cursor.selectedText() && preview->hasFocus(), "unchanged status retains document, selection and focus without duplicate announcement");
		QObject::disconnect(changed);
		test.button(command::export_diagnostics)->setFocus();
		test.Panel->adopt(test.Projection.Last);
		expect(test.button(command::export_diagnostics)->hasFocus(), "enabled command never toggles off during equivalent projection");
		capture(*test.Window.settings(), QStringLiteral("-preview"));
		auto* scroll = qobject_cast<QScrollArea*>(test.Window.settings()->pages()->currentWidget());
		expect(scroll && scroll->horizontalScrollBar()->maximum() == 0 && test.Window.settings()->size() == QSize(760, 480), "minimum Settings size keeps actions inside horizontal viewport");
		test.Panel->action(command::export_diagnostics)->trigger();
		auto* chooser = test.Panel->findChild<QFileDialog*>(QStringLiteral("releaseMaintenanceSaveDialog"));
		expect(chooser && chooser->isVisible(), "native save gesture opens a selection dialog");
		chooser->reject();
		QApplication::processEvents();
		expect(test.Host.state().Prepared.has_value(), "save cancellation keeps exact preview");
		test.Window.settings()->close();
		expect(!test.Host.state().Prepared && test.Intents.back().Command == command::cancel && test.Intents.back().Target.has_value(), "Settings close sends exact preview cancellation");
	}
	void stale_save_and_unavailable()
	{
		fixture test;
		test.Panel->action(command::preview_diagnostics)->trigger();
		test.drain();
		test.Panel->action(command::export_diagnostics)->trigger();
		QPointer<QFileDialog> chooser = test.Panel->findChild<QFileDialog*>(QStringLiteral("releaseMaintenanceSaveDialog"));
		expect(chooser && chooser->isVisible(), "save dialog captures a live prepared preview");
		test.Host.invalidate();
		expect(!chooser || !chooser->isVisible(), "retiring the preview closes its stale save chooser");
		expect(test.Storage.Erases == 0U && !test.Panel->action(command::export_diagnostics)->isEnabled(), "stale selection grants no effect");
		auto unavailable     = application::release_maintenance_projection{ };
		unavailable.State    = core::make_release_maintenance_state(test.Storage.authority(), { });
		unavailable.Commands = core::release_maintenance_commands(unavailable.State);
		test.Panel->adopt(unavailable);
		expect(!test.Panel->action(command::preview_diagnostics)->isEnabled() && test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"))->text().contains(QStringLiteral("unavailable")), "absent capability is disabled and described truthfully");
		const auto previous = test.Intents.size();
		test.Panel->action(command::preview_diagnostics)->trigger();
		expect(test.Intents.size() == previous, "disabled presentation cannot emit an intent");
	}
	void directory_refusal_retry_and_stale_completion()
	{
		fixture test;
		test.Panel->action(command::preview_diagnostics)->trigger();
		test.drain();
		const auto frozen = test.Host.state().Prepared;
		const auto bytes  = test.Panel->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"))->toPlainText();
		test.Directory    = std::unexpected(reason::unavailable);
		test.Panel->action(command::export_diagnostics)->trigger();
		expect(!test.Panel->findChild<QFileDialog*>() && test.Host.state().Prepared == frozen, "no eligible directory opens no arbitrary picker and keeps exact preview");
		expect(test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"))->text().contains(QStringLiteral("try Save again")), "directory refusal offers retry beside Save");
		test.Directory     = QDir::tempPath().toStdString();
		const auto version = QCoreApplication::applicationVersion();
		QCoreApplication::setApplicationVersion(QStringLiteral("9.8.7"));
		const auto before = QDateTime::currentDateTime();
		test.Panel->action(command::export_diagnostics)->trigger();
		const auto            after   = QDateTime::currentDateTime();
		QPointer<QFileDialog> chooser = test.Panel->findChild<QFileDialog*>();
		QCoreApplication::setApplicationVersion(version);
		expect(chooser && chooser->isVisible() && chooser->directory().absolutePath() == QDir::tempPath(), "retry supplies the admitted explicit directory");
		// [[BPR-1722]]. The suggestion is named for the local minute it
		// was offered and the version, and has no colon.
		const auto suggested = QFileInfo{ chooser->selectedFiles().front() }.fileName();
		const auto named     = [](const QDateTime& aAt) { return QStringLiteral("BetterPullReview-diagnostics-") + aAt.toString(QStringLiteral("yyyy-MM-dd-HHmm")) + QStringLiteral("-9.8.7.zip"); };
		expect(QRegularExpression{ QStringLiteral("^BetterPullReview-diagnostics-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]{4}-9\\.8\\.7\\.zip$") }.match(suggested).hasMatch() && (suggested == named(before) || suggested == named(after)) && !suggested.contains(QLatin1Char{ ':' }),
			   "the suggestion holds the local date, time and version");
		// [[BPR-1725]]. The file is a ZIP archive holding the text.
		expect(chooser->nameFilters() == QStringList{ QStringLiteral("Diagnostic export (*.zip)") } && chooser->defaultSuffix() == QStringLiteral("zip"),
			   "the save offers a .zip archive");
		expect(!chooser->testOption(QFileDialog::DontUseNativeDialog), "native dialog selection remains");
		chooser->reject();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		expect(test.Host.state().Prepared == frozen && test.Panel->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"))->toPlainText() == bytes, "picker cancellation retains frozen bytes");
		test.HoldDirectory = true;
		test.Panel->action(command::export_diagnostics)->trigger();
		const auto oldRequest = *test.DirectoryRequest;
		test.Host.invalidate();
		test.Panel->present_export_directory(oldRequest, test.Directory);
		expect(!test.Panel->findChild<QFileDialog*>(), "late directory observation cannot reopen an invalidated preview");
	}
	void destructive_confirmation_and_stale_selection()
	{
		fixture test;
		test.Panel->action(command::prepare_purge)->trigger();
		QPointer<QMessageBox> initial = test.confirmation();
		key(*initial, Qt::Key_Return);
		dismissed(initial);
		expect(test.Lifecycle.Calls == 0U && test.Storage.Erases == 0U, "Return defaults to Cancel before preparation");
		test.prepare(command::prepare_purge);
		const auto captured = test.Host.state().Prepared;
		test.Panel->action(command::purge_profile)->trigger();
		QPointer<QMessageBox> box = test.confirmation();
		expect(box->text().contains(QStringLiteral("6 exact local entries")) && box->text().contains(QStringLiteral("shared registry")), "confirmation names exact inventory and residuals");
		capture(*box, QStringLiteral("-purge-confirmation"));
		key(*box, Qt::Key_Escape);
		dismissed(box);
		expect(test.Storage.Erases == 0U && test.Intents.back().Target == captured && !test.Host.state().Prepared, "Escape cancels exactly the inventory shown");
		test.prepare(command::prepare_purge);
		test.Panel->action(command::purge_profile)->trigger();
		QPointer<QMessageBox> old = test.confirmation();
		test.Host.invalidate();
		expect(!old || !old->isVisible(), "identity invalidation closes a stale destructive dialog");
		expect(test.Storage.Erases == 0U, "invalidated confirmation performs no deletion");
		test.prepare(command::prepare_purge);
		const auto selected = test.Host.state().Prepared;
		test.Panel->action(command::purge_profile)->trigger();
		test.proceed();
		expect(test.Intents.back().Target == selected && test.Storage.Erases == 0U, "confirmation echoes complete target and queues effect");
		test.drain();
		expect(test.Storage.Erases == 1U && test.Host.state().Stopped && test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"))->text().contains(QStringLiteral("runtime has ended")), "actual purge terminal is presented without admitting another action");
	}
	void cancel_barrier_and_partial_failure()
	{
		fixture test;
		test.Panel->action(command::prepare_clear)->trigger();
		test.proceed();
		test.Panel->action(command::cancel)->trigger();
		test.Lifecycle.Complete({ });
		test.Decisions.drain();
		test.drain();
		expect(test.Storage.Prepares == 0U && test.Host.state().Phase == phase::cancelled, "cancelled barrier never prepares inventory");
		expect(test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"))->text().contains(QStringLiteral("reopen it explicitly")), "cancel does not promise producer restart");
		test.prepare(command::prepare_clear);
		test.Storage.Fail = true;
		test.Panel->action(command::clear_diagnostics)->trigger();
		test.proceed();
		test.drain();
		expect(test.Host.state().RecoveryRequired && !test.Panel->action(command::prepare_purge)->isEnabled(), "irreversible failure disables new admission");
		expect(test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"))->text().contains(QStringLiteral("Cleanup may be partial")), "failure is not presented as cancellation or successful rollback");
		capture(*test.Window.settings(), QStringLiteral("-recovery"));
	}
	/// \brief [[BPR-1038]]. A purge refused because a file or folder is not
	/// private says so, rather than asking for a new preparation of a
	/// selection that never changed.
	void not_private_refusal_is_named()
	{
		fixture test;
		test.Storage.Refusal = reason::storage_not_private;
		test.Panel->action(command::prepare_purge)->trigger();
		test.proceed();
		test.Lifecycle.Complete({ });
		test.Decisions.drain();
		test.drain();
		const auto text = test.Panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"))->text();
		expect(test.Host.state().Phase == phase::failed && test.Host.state().Reason == reason::storage_not_private,
			   "the refusal reaches the core as its own operational failure");
		expect(text.contains(QStringLiteral("not private")) && !text.contains(QStringLiteral("changed")),
			   "the panel names a refusal for privacy, not a changed selection");
	}
	/// [[BPR-1744]]. Each platform's button order, forced on the box.
	class dialog_button_layout final : public QProxyStyle
	{
	public:
		explicit dialog_button_layout(const QDialogButtonBox::ButtonLayout aLayout)
			: Layout{ aLayout }
		{ }
		auto styleHint(const StyleHint aHint, const QStyleOption* aOption, const QWidget* aWidget, QStyleHintReturn* aReturn) const -> int override
		{
			return aHint == QStyle::SH_DialogButtonLayout ? static_cast<int>(Layout) : QProxyStyle::styleHint(aHint, aOption, aWidget, aReturn);
		}

	private:
		QDialogButtonBox::ButtonLayout Layout;
	};
	/// \brief [[BPR-1744]]. The export dialog's Save is the box's accept
	/// button, so on every platform it sits with Cancel at the right end, not
	/// alone at the left as an action button does on macOS.
	void save_sits_beside_cancel()
	{
		fixture test;
		auto*   save = test.button(command::export_diagnostics);
		auto*   box  = save != nullptr ? qobject_cast<QDialogButtonBox*>(save->parentWidget()) : nullptr;
		expect(box != nullptr && box->button(QDialogButtonBox::Cancel) != nullptr && save->text() == QStringLiteral("Save…"),
			   "Save is in the export dialog's button box with Cancel");
		expect(box->buttonRole(save) == QDialogButtonBox::AcceptRole && box->buttonRole(box->button(QDialogButtonBox::Cancel)) == QDialogButtonBox::RejectRole,
			   "Save accepts and Cancel rejects");
		for (const auto layout : { QDialogButtonBox::MacLayout, QDialogButtonBox::WinLayout, QDialogButtonBox::KdeLayout, QDialogButtonBox::GnomeLayout }) {
			dialog_button_layout style{ layout };
			box->setStyle(&style);
			auto       stretch = -1;
			auto       saveAt  = -1;
			auto       cancel  = -1;
			const auto items   = box->layout()->count();
			for (auto index = 0; index < items; ++index) {
				auto* const item = box->layout()->itemAt(index);
				if (item->widget() == save) {
					saveAt = index;
				} else if (item->widget() == box->button(QDialogButtonBox::Cancel)) {
					cancel = index;
				} else if (item->spacerItem() != nullptr && (item->spacerItem()->expandingDirections() & Qt::Horizontal) != 0) {
					stretch = index;
				}
			}
			box->setStyle(nullptr);
			expect(stretch >= 0 && saveAt > stretch && cancel > stretch && (saveAt - cancel == 1 || cancel - saveAt == 1),
				   "Save and Cancel sit together at the right, after the stretch, in every platform's order");
			expect(layout != QDialogButtonBox::MacLayout || cancel < saveAt, "on macOS Save is the rightmost, after Cancel");
		}
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication application{ aArgc, aArgv };
	preview_keyboard_and_incremental();
	stale_save_and_unavailable();
	directory_refusal_retry_and_stale_completion();
	destructive_confirmation_and_stale_selection();
	cancel_barrier_and_partial_failure();
	not_private_refusal_is_named();
	save_sits_beside_cancel();
	std::cout << "release-maintenance-panel:pass keyboard accessibility exact-target default-cancel stale-dialog partial-failure worker-boundary incremental-preview\n";
	return EXIT_SUCCESS;
}
