#include "bpr/qt/profile_data_reset_views.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include "bpr/qt/destructive_question.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		using reset = core::profile_data_reset;

		/// A category's place in the Clear list; past its end for `everything`.
		[[nodiscard]] auto category_index(const reset aReset) noexcept -> std::size_t
		{
			const auto* const found = std::ranges::find(core::profile_data_reset_categories, aReset);
			return static_cast<std::size_t>(found - core::profile_data_reset_categories.begin());
		}

		/// The object-name stem of a category's controls.
		[[nodiscard]] auto object_key(const reset aReset) -> QString
		{
			switch (aReset) {
			case reset::caches:
				return QStringLiteral("Caches");
			case reset::settings:
				return QStringLiteral("Settings");
			case reset::review_data:
				return QStringLiteral("ReviewData");
			case reset::connections:
				return QStringLiteral("Connections");
			case reset::agent_sessions:
				return QStringLiteral("AgentSessions");
			case reset::saved_sources:
				return QStringLiteral("SavedSources");
			case reset::statistics:
				return QStringLiteral("Statistics");
			case reset::everything:
				break;
			}
			return QStringLiteral("Everything");
		}

		[[nodiscard]] auto quoted(const QString& aProfile) -> QString
		{
			return QStringLiteral("“%1”").arg(aProfile.isEmpty() ? QStringLiteral("this profile") : aProfile);
		}

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount),
									static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount));
		}

		/// The Keeps heading's green: the brightest that still reads at 4.5:1
		/// on the ledger, moved toward the text colour until it does.
		[[nodiscard]] auto keeps_ink(const QPalette& aPalette, const QColor& aGround) -> QColor
		{
			const auto dark  = aGround.lightnessF() < 0.5;
			const auto green = dark ? QColor{ 0x8F, 0xD1, 0x9E } : QColor{ 0x2D, 0x6A, 0x3E };
			const auto text  = aPalette.color(QPalette::Active, QPalette::WindowText);
			for (auto step = 0; step <= 20; ++step) {
				if (const auto ink = blend(green, text, step / 20.0); destructive_contrast(ink, aGround) >= 4.5) {
					return ink;
				}
			}
			return text;
		}

		[[nodiscard]] auto copy_words() -> std::pair<QString, QString>
		{
			return { QStringLiteral("Save a copy of the removed data in Downloads first"),
					 QStringLiteral("A folder named “BetterPullReview reset” and the date and time. Delete it when you no longer need it.") };
		}

		[[nodiscard]] auto refusal(const core::release_maintenance_reason aReason) -> QString
		{
			using reason = core::release_maintenance_reason;
			switch (aReason) {
			case reason::incomplete_inventory:
				return QStringLiteral("a file it does not recognise is in the way");
			case reason::invalid_authority:
			case reason::storage_not_private:
				return QStringLiteral("a file is not private to your account");
			case reason::unavailable:
				return QStringLiteral("no Downloads, Documents or home folder could take the copy");
			case reason::limit_exceeded:
				return QStringLiteral("there are too many files for one reset");
			case reason::selection_changed:
				return QStringLiteral("files changed while it ran");
			case reason::recovery_required:
				return QStringLiteral("an earlier reset has not finished");
			default:
				return QStringLiteral("a local error stopped it");
			}
		}

		/// A quiet second line under a check box, indented to its words.
		[[nodiscard]] auto option(QWidget* aParent, QVBoxLayout* aColumn, const QString& aText, const QString& aDetail, const QString& aName, const bool aChecked) -> QCheckBox*
		{
			auto* const box = new QCheckBox{ aText, aParent };
			box->setObjectName(aName);
			box->setChecked(aChecked);
			aColumn->addWidget(box);
			if (!aDetail.isEmpty()) {
				auto* const detail = new QLabel{ aDetail, aParent };
				detail->setObjectName(aName + QStringLiteral("Detail"));
				detail->setWordWrap(true);
				detail->setForegroundRole(QPalette::PlaceholderText);
				auto font = detail->font();
				font.setPointSizeF(font.pointSizeF() * 0.92);
				detail->setFont(font);
				detail->setContentsMargins(box->style()->pixelMetric(QStyle::PM_IndicatorWidth) + 6, 0, 0, 0);
				box->setAccessibleDescription(aDetail);
				aColumn->addWidget(detail);
			}
			return box;
		}
	}

	auto profile_data_reset_category_words_for(const core::profile_data_reset aReset) -> profile_data_reset_category_words
	{
		switch (aReset) {
		case reset::caches:
			return { QStringLiteral("Caches"),
					 QStringLiteral("Repository data downloaded from GitHub, images, Home’s summaries and context snapshots kept for agents. They download again when needed."),
					 { QStringLiteral("Repository data downloaded from GitHub"), QStringLiteral("Images in descriptions and comments"),
					   QStringLiteral("Pull request summaries on Home"), QStringLiteral("Context snapshots kept for agents") } };
		case reset::settings:
			return { QStringLiteral("Settings"),
					 QStringLiteral("Every choice in Settings, agent setup and prompts, notifications, filters and window layout. Includes Agent sessions and workspaces, which share the agent setup."),
					 { QStringLiteral("Every choice in Settings"), QStringLiteral("Agents you added, their models and checks, and your prompts"),
					   QStringLiteral("Home’s queue filters and sort"), QStringLiteral("Notification choices"), QStringLiteral("Window layout and which windows reopen") } };
		case reset::agent_sessions:
			return { QStringLiteral("Agent sessions and workspaces"),
					 QStringLiteral("BetterPullReview’s agent conversations with their transcripts, names and costs, and the agent workspaces they keep. Claude Code’s and Codex’s own session files stay."),
					 { QStringLiteral("Agent conversations: transcripts, names, costs and finding cards"), QStringLiteral("Retained agent workspaces, except one another app holds") } };
		case reset::review_data:
			return { QStringLiteral("Review data"),
					 QStringLiteral("Read and seen marks, private notes, unsent comments and reviews, tours and Review Cycles."),
					 { QStringLiteral("Read and seen marks"), QStringLiteral("Private notes and their threads"), QStringLiteral("Unsent comments, replies and reviews"),
					   QStringLiteral("Review tours"), QStringLiteral("Review Cycles and their rounds") } };
		case reset::saved_sources:
			return { QStringLiteral("Saved repositories and servers"),
					 QStringLiteral("The repositories and servers you saved, observer rules and Home’s queue."),
					 { QStringLiteral("Saved repositories and servers"), QStringLiteral("Observer rules"), QStringLiteral("Home’s queue") } };
		case reset::statistics:
			return { QStringLiteral("Insights statistics"),
					 QStringLiteral("Insights’ lifetime statistics and history."),
					 { QStringLiteral("Insights statistics and history") } };
		case reset::connections:
			return { QStringLiteral("Connections"),
					 QStringLiteral("Sign-ins made in this app and their saved tokens, and this profile’s GitHub CLI connection. Signs you out; GitHub CLI itself stays signed in."),
					 { QStringLiteral("Sign-ins made in this app, and their saved tokens"), QStringLiteral("This profile’s GitHub CLI connection"),
					   QStringLiteral("Which account each server uses") } };
		case reset::everything:
			break;
		}
		return { QStringLiteral("Everything"), QStringLiteral("Every category above, as on first launch."), { } };
	}

	auto profile_data_reset_wording_for(const core::profile_data_reset_selection aReset, const QString& aProfile) -> profile_data_reset_wording
	{
		const auto name  = quoted(aProfile);
		const auto plan  = core::plan_profile_data_reset(aReset);
		auto       words = profile_data_reset_wording{ };
		const auto many  = std::ranges::count_if(core::profile_data_reset_categories, [&](const auto aEach) { return plan.Selection.contains(aEach); }) > 1;
		for (const auto each : core::profile_data_reset_categories) {
			const auto category = profile_data_reset_category_words_for(each);
			if (!plan.Selection.contains(each)) {
				words.Keeps.push_back(category.Name);
				continue;
			}
			for (const auto& item : category.Removes) {
				words.Removes.push_back(many ? QStringLiteral("%1: %2").arg(category.Name, item.at(0).toLower() + item.mid(1)) : item);
			}
		}
		words.Keeps.append({ QStringLiteral("Everything posted on GitHub"), QStringLiteral("GitHub CLI’s own sign-in, which other tools use"),
							 QStringLiteral("Claude Code’s and Codex’s own session files"), QStringLiteral("Your Git checkouts and exported files"),
							 QStringLiteral("Diagnostic logs"), QStringLiteral("Other profiles") });
		const auto single = plan.Selection.single();
		const auto action = single == reset::everything ? QStringLiteral("Reset everything")
			: single                                    ? QStringLiteral("Clear %1").arg(profile_data_reset_category_words_for(*single).Name.toLower())
														: QStringLiteral("Clear");
		words.Confirm     = action;
		words.Title       = single == reset::everything ? QStringLiteral("Reset everything for %1?").arg(name)
			: single                                    ? QStringLiteral("Clear %1 for %2?").arg(profile_data_reset_category_words_for(*single).Name.toLower(), name)
														: QStringLiteral("Clear the checked data for %1?").arg(name);
		auto lead         = QStringLiteral("This profile’s windows close, the data below is removed, and Home reopens.");
		if (!plan.Implied.empty()) {
			lead += QStringLiteral(" Settings includes Agent sessions and workspaces: they share the agent setup.");
		}
		if (core::profile_data_reset_signs_out(plan.Selection)) {
			lead += QStringLiteral(" It signs you out of GitHub here; it doesn’t sign you out of GitHub CLI itself.");
		}
		if (core::profile_data_reset_destructive(plan.Selection)) {
			lead += QStringLiteral(" Some of it exists only on this computer. Nothing on GitHub changes.");
		}
		words.Lead = lead;
		if (plan.Selection.contains(reset::caches)) {
			words.Note = QStringLiteral("Earlier pushes of a pull request that was force-pushed may no longer be on GitHub. Comparisons with those pushes can’t be rebuilt.");
		}
		return words;
	}

	auto profile_data_reset_acknowledgement(const core::profile_data_reset_selection aReset) -> QString
	{
		const auto plan  = core::plan_profile_data_reset(aReset);
		auto       parts = QStringList{ };
		if (plan.Selection.contains(reset::review_data)) {
			parts << QStringLiteral("my private notes and unsent comments");
		}
		if (plan.Selection.contains(reset::agent_sessions)) {
			parts << QStringLiteral("my agent conversations");
		}
		if (plan.Selection.contains(reset::saved_sources)) {
			parts << QStringLiteral("my saved repositories");
		}
		if (plan.Selection.contains(reset::statistics)) {
			parts << QStringLiteral("my Insights history");
		}
		if (parts.isEmpty()) {
			return { };
		}
		const auto last = parts.takeLast();
		return QStringLiteral("I understand %1 will be deleted").arg(parts.isEmpty() ? last : parts.join(QStringLiteral(", ")) + QStringLiteral(" and ") + last);
	}

	auto profile_data_reset_notice(const core::profile_data_reset_outcome& aOutcome, const QString& aCopyFolder) -> QString
	{
		const auto action = profile_data_reset_wording_for(aOutcome.Reset, { }).Confirm;
		if (aOutcome.Reason != core::release_maintenance_reason::none) {
			return QStringLiteral("%1 did not run: %2. Nothing was removed.").arg(action, refusal(aOutcome.Reason));
		}
		const auto cleared = aOutcome.Reset.without(aOutcome.Failed);
		auto       notice  = QString{ };
		if (cleared == core::profile_data_reset_selection{ reset::caches }) {
			notice = QStringLiteral("Caches cleared. They download again as you open pull requests.");
		} else if (cleared == core::profile_data_reset_selection::all()) {
			notice = QStringLiteral("This profile was reset");
		} else if (!cleared.empty()) {
			auto names = QStringList{ };
			for (const auto each : core::profile_data_reset_categories) {
				if (cleared.contains(each)) {
					names << profile_data_reset_category_words_for(each).Name;
				}
			}
			notice = QStringLiteral("Cleared: %1").arg(names.join(QStringLiteral(", ")));
		}
		if (core::profile_data_reset_signs_out(cleared)) {
			notice += aOutcome.ConnectionsRetained
				? QStringLiteral(", but its remembered connections could not be forgotten. Sign out in Accounts & hosts.")
				: aOutcome.SignedOut ? QStringLiteral(" and signed out. GitHub CLI itself is still signed in; choose Sign in or Use GitHub CLI to connect again.")
									 : QStringLiteral(".");
		} else if (!notice.isEmpty() && !notice.endsWith(QLatin1Char{ '.' })) {
			notice += QStringLiteral(".");
		}
		if (cleared.contains(reset::agent_sessions) && aOutcome.WorkspacesRemoved != 0U) {
			notice += aOutcome.WorkspacesRemoved == 1U ? QStringLiteral(" 1 agent workspace was removed.") : QStringLiteral(" %1 agent workspaces were removed.").arg(aOutcome.WorkspacesRemoved);
		}
		if (cleared.contains(reset::settings) && !aOutcome.QueueDisplayRestored) {
			notice += QStringLiteral(" Home’s queue filters could not be saved; clear Saved repositories and servers to remove them.");
		}
		if (!aOutcome.Failed.empty()) {
			auto names = QStringList{ };
			for (const auto each : core::profile_data_reset_categories) {
				if (aOutcome.Failed.contains(each)) {
					names << profile_data_reset_category_words_for(each).Name;
				}
			}
			notice += QStringLiteral(" Not cleared: %1.").arg(names.join(QStringLiteral(", ")));
			if (aOutcome.Failed.contains(reset::agent_sessions)) {
				notice += aOutcome.WorkspacesLeft == 0U
					? QStringLiteral(" The agent conversations could not be read.")
					: QStringLiteral(" %1 agent workspace(s) are in use by another app or could not be verified, so the conversations and Settings that record them were kept.").arg(aOutcome.WorkspacesLeft);
			}
		}
		if (aOutcome.Copied && !aCopyFolder.isEmpty()) {
			notice += QStringLiteral(" A copy of the removed data is in “%1”.").arg(aCopyFolder);
		}
		return notice.trimmed();
	}

	profile_reset_confirmation::profile_reset_confirmation(QWidget* const aParent, const core::profile_data_reset_selection aReset, const QString& aProfile)
		: QDialog{ aParent }
		, Reset{ aReset }
		, Words{ profile_data_reset_wording_for(aReset, aProfile) }
	{
		setObjectName(QStringLiteral("profileResetConfirmation"));
		setAttribute(Qt::WA_DeleteOnClose);
		setWindowModality(Qt::WindowModal);
		setWindowTitle(Words.Title);
		setAccessibleName(Words.Title);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(22, 20, 22, 18);
		column->setSpacing(6);

		auto* const title = new QLabel{ Words.Title, this };
		title->setObjectName(QStringLiteral("profileResetTitle"));
		auto bold = title->font();
		bold.setBold(true);
		bold.setPointSizeF(bold.pointSizeF() * 1.08);
		title->setFont(bold);
		title->setWordWrap(true);
		column->addWidget(title);
		auto* const lead = new QLabel{ Words.Lead, this };
		lead->setObjectName(QStringLiteral("profileResetLead"));
		lead->setWordWrap(true);
		column->addWidget(lead);
		column->addSpacing(8);

		// The ledger: what goes on the left, what stays on the right.
		Ledger = new QFrame{ this };
		Ledger->setObjectName(QStringLiteral("profileResetLedger"));
		static_cast<QFrame*>(Ledger)->setFrameShape(QFrame::StyledPanel);
		Ledger->setAutoFillBackground(true);
		Ledger->setBackgroundRole(QPalette::Base);
		auto* const ledger = new QHBoxLayout{ Ledger };
		ledger->setContentsMargins(0, 0, 0, 0);
		ledger->setSpacing(0);
		const auto list = [this, ledger](const QString& aHeading, const QStringList& aItems, const QString& aMark, QLabel*& aHeadingLabel, QList<QLabel*>& aMarks, const QString& aName) {
			auto* const side   = new QWidget{ Ledger };
			auto* const layout = new QGridLayout{ side };
			layout->setContentsMargins(14, 10, 14, 12);
			layout->setHorizontalSpacing(6);
			layout->setVerticalSpacing(2);
			aHeadingLabel = new QLabel{ aHeading, side };
			aHeadingLabel->setObjectName(aName + QStringLiteral("Heading"));
			auto heading = aHeadingLabel->font();
			heading.setBold(true);
			aHeadingLabel->setFont(heading);
			layout->addWidget(aHeadingLabel, 0, 0, 1, 2);
			auto row = 1;
			for (const auto& item : aItems) {
				auto* const mark = new QLabel{ aMark, side };
				aMarks.push_back(mark);
				auto* const words = new QLabel{ item, side };
				words->setObjectName(aName + QStringLiteral("Item"));
				words->setWordWrap(true);
				layout->addWidget(mark, row, 0, Qt::AlignTop);
				layout->addWidget(words, row, 1);
				++row;
			}
			layout->setColumnStretch(1, 1);
			layout->setRowStretch(row, 1);
			side->setAccessibleName(aHeading + QStringLiteral(": ") + aItems.join(QStringLiteral("; ")));
			ledger->addWidget(side, 1);
			return side;
		};
		list(QStringLiteral("Removes"), Words.Removes, QStringLiteral("−"), RemovesHeading, RemovesMarks, QStringLiteral("profileResetRemoves"));
		auto* const divider = new QFrame{ Ledger };
		divider->setFrameShape(QFrame::VLine);
		divider->setFrameShadow(QFrame::Plain);
		divider->setForegroundRole(QPalette::Mid);
		ledger->addWidget(divider);
		list(QStringLiteral("Keeps"), Words.Keeps, QStringLiteral("✓"), KeepsHeading, KeepsMarks, QStringLiteral("profileResetKeeps"));
		column->addWidget(Ledger);

		if (!Words.Note.isEmpty()) {
			column->addSpacing(4);
			auto* const note = new QLabel{ Words.Note, this };
			note->setObjectName(QStringLiteral("profileResetNote"));
			note->setWordWrap(true);
			note->setForegroundRole(QPalette::PlaceholderText);
			column->addWidget(note);
		}
		if (core::profile_data_reset_destructive(Reset)) {
			column->addSpacing(6);
			// [[BPR-NNNN]]. The copy holds removed store files; agent
			// conversations cleared alone remove none, so none is offered.
			if (core::profile_data_reset_copyable(Reset)) {
				const auto [copyText, copyDetail] = copy_words();
				Copy                              = option(this, column, copyText, copyDetail, QStringLiteral("profileResetCopy"), true);
				column->addSpacing(2);
			}
			Acknowledge = option(this, column, profile_data_reset_acknowledgement(Reset), { }, QStringLiteral("profileResetAcknowledge"), false);
			connect(Acknowledge, &QCheckBox::toggled, this, [this] { refresh_confirm(); });
		}

		auto* const buttons = new QHBoxLayout;
		buttons->setContentsMargins(0, 12, 0, 0);
		buttons->addStretch(1);
		Cancel = new QPushButton{ QStringLiteral("Cancel"), this };
		Cancel->setObjectName(QStringLiteral("profileResetCancel"));
		Cancel->setDefault(true);
		Cancel->setAutoDefault(true);
		connect(Cancel, &QPushButton::clicked, this, &QDialog::reject);
		Confirm = new question_button{ Words.Confirm, question_button_role::destructive, this };
		Confirm->setObjectName(QStringLiteral("profileResetConfirm"));
		Confirm->setAutoDefault(false);
		connect(Confirm, &QPushButton::clicked, this, [this] {
			if (core::admit_profile_data_reset(request()) == core::release_maintenance_reason::none) {
				accept();
			}
		});
		buttons->addWidget(Cancel);
		buttons->addWidget(Confirm);
		column->addLayout(buttons);
		setMinimumWidth(560);
		setMaximumWidth(640);
		refresh_colours();
		refresh_confirm();
		Cancel->setFocus();
	}

	auto profile_reset_confirmation::request() const -> core::profile_data_reset_request
	{
		return { Reset, Acknowledge != nullptr && Acknowledge->isChecked(), Copy != nullptr && Copy->isChecked() };
	}
	auto profile_reset_confirmation::reset() const noexcept -> core::profile_data_reset_selection { return Reset; }
	auto profile_reset_confirmation::wording() const noexcept -> const profile_data_reset_wording& { return Words; }
	auto profile_reset_confirmation::confirm_button() const noexcept -> QPushButton* { return Confirm; }
	auto profile_reset_confirmation::cancel_button() const noexcept -> QPushButton* { return Cancel; }
	auto profile_reset_confirmation::copy_box() const noexcept -> QCheckBox* { return Copy; }
	auto profile_reset_confirmation::acknowledge_box() const noexcept -> QCheckBox* { return Acknowledge; }

	void profile_reset_confirmation::changeEvent(QEvent* const aEvent)
	{
		QDialog::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::ApplicationPaletteChange) {
			refresh_colours();
		}
	}

	void profile_reset_confirmation::refresh_colours()
	{
		const auto ground = palette().color(QPalette::Active, QPalette::Base);
		const auto danger = destructive_colours_of(palette(), ground).Danger;
		const auto keeps  = keeps_ink(palette(), ground);
		const auto colour = [](QLabel* aLabel, const QColor& aColour) {
            auto colours = aLabel->palette();
            colours.setColor(QPalette::WindowText, aColour);
            colours.setColor(QPalette::Text, aColour);
            aLabel->setPalette(colours); };
		colour(RemovesHeading, danger);
		colour(KeepsHeading, keeps);
		for (auto* const mark : RemovesMarks) {
			colour(mark, danger);
		}
		for (auto* const mark : KeepsMarks) {
			colour(mark, keeps);
		}
	}

	void profile_reset_confirmation::refresh_confirm()
	{
		const auto admitted = core::admit_profile_data_reset(request()) == core::release_maintenance_reason::none;
		Confirm->setEnabled(admitted);
		// The question's button paints its own red; a dimmed one says it is
		// waiting for the acknowledgement.
		if (admitted) {
			Confirm->setGraphicsEffect(nullptr);
		} else {
			auto* const dimmed = new QGraphicsOpacityEffect{ Confirm };
			dimmed->setOpacity(0.45);
			Confirm->setGraphicsEffect(dimmed);
		}
	}

	profile_reset_panel::profile_reset_panel(QWidget* const aParent)
		: QGroupBox{ QStringLiteral("Clear local data"), aParent }
	{
		// [[BPR-NNNN]], owner amendment 2026-11-24: one checklist and one
		// Clear in place of a button per reset.
		setObjectName(QStringLiteral("profileResetPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setSpacing(4);
		auto* const lead = new QLabel{ QStringLiteral("Check what to clear, then Clear. It asks first, closes this profile’s windows and reopens Home. Nothing on GitHub changes."), this };
		lead->setObjectName(QStringLiteral("profileResetLead"));
		lead->setWordWrap(true);
		lead->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(lead);
		column->addSpacing(6);
		for (const auto each : core::profile_data_reset_categories) {
			const auto  words = profile_data_reset_category_words_for(each);
			const auto  key   = object_key(each);
			auto* const row   = new QHBoxLayout;
			row->setSpacing(8);
			auto* const box = new QCheckBox{ words.Name, this };
			box->setObjectName(QStringLiteral("profileReset%1").arg(key));
			box->setAccessibleDescription(words.Line);
			row->addWidget(box, 1);
			auto* const size = new QLabel{ this };
			size->setObjectName(QStringLiteral("profileReset%1Size").arg(key));
			size->setForegroundRole(QPalette::PlaceholderText);
			size->hide();
			row->addWidget(size, 0, Qt::AlignRight);
			column->addLayout(row);
			auto* const line = new QLabel{ words.Line, this };
			line->setObjectName(QStringLiteral("profileReset%1Line").arg(key));
			line->setWordWrap(true);
			line->setForegroundRole(QPalette::PlaceholderText);
			auto font = line->font();
			font.setPointSizeF(font.pointSizeF() * 0.92);
			line->setFont(font);
			line->setContentsMargins(box->style()->pixelMetric(QStyle::PM_IndicatorWidth) + 6, 0, 0, 4);
			column->addWidget(line);
			const auto index = category_index(each);
			Boxes[index]     = box;
			Sizes[index]     = size;
			Lines[index]     = line;
			connect(box, &QCheckBox::toggled, this, [this, index](const bool aOn) {
				Own[index] = aOn;
				refresh();
			});
		}
		auto* const buttons = new QHBoxLayout;
		buttons->setContentsMargins(0, 8, 0, 0);
		SelectAll = new quiet_push_button{ QStringLiteral("Check all"), this };
		SelectAll->setObjectName(QStringLiteral("profileResetSelectAll"));
		SelectAll->setAutoDefault(false);
		connect(SelectAll, &QPushButton::clicked, this, [this] {
			const auto all = checked() == core::profile_data_reset_selection::all();
			set_checked(all ? core::profile_data_reset_selection{ } : core::profile_data_reset_selection::all());
		});
		buttons->addWidget(SelectAll);
		buttons->addStretch(1);
		Clear = new question_button{ QStringLiteral("Clear…"), question_button_role::destructive, this };
		Clear->setObjectName(QStringLiteral("profileResetClear"));
		Clear->setAutoDefault(false);
		connect(Clear, &QPushButton::clicked, this, [this] { static_cast<void>(ask(chosen())); });
		buttons->addWidget(Clear);
		column->addLayout(buttons);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("profileResetStatus"));
		Status->setWordWrap(true);
		Status->hide();
		column->addWidget(Status);
		refresh();
	}

	auto profile_reset_panel::chosen() const -> core::profile_data_reset_selection
	{
		auto selection = core::profile_data_reset_selection{ };
		for (const auto each : core::profile_data_reset_categories) {
			if (Own[category_index(each)]) {
				selection = selection.with(each);
			}
		}
		return selection;
	}

	void profile_reset_panel::refresh()
	{
		// What the person checked, and what that implies shown checked and
		// locked, with why. Unchecking Settings gives the box its own choice back.
		const auto implied = core::profile_data_reset_implied(chosen());
		for (const auto each : core::profile_data_reset_categories) {
			const auto           index = category_index(each);
			auto* const          box   = Boxes[index];
			const auto           held  = implied.contains(each);
			const QSignalBlocker quiet{ box };
			box->setEnabled(!held);
			box->setChecked(held || Own[index]);
			const auto words = profile_data_reset_category_words_for(each);
			Lines[index]->setText(held ? QStringLiteral("Included with Settings, which holds the agent setup these live in. %1").arg(words.Line) : words.Line);
		}
		const auto any = !checked().empty();
		Clear->setEnabled(any);
		Clear->setToolTip(any ? QString{ } : QStringLiteral("Check what to clear first"));
		SelectAll->setText(chosen() == core::profile_data_reset_selection::all() ? QStringLiteral("Uncheck all") : QStringLiteral("Check all"));
	}

	void profile_reset_panel::set_dispatch(dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void profile_reset_panel::set_saved_update_drafts_action(QAction& aAction)
	{
		auto* const button = new QPushButton{ this };
		button->setObjectName(QStringLiteral("settingsSavedUpdateDrafts"));
		button->setAutoDefault(false);
		static_cast<QVBoxLayout*>(layout())->insertWidget(0, button, 0, Qt::AlignLeft);
		const auto render = [&aAction, button] {
			button->setEnabled(aAction.isEnabled());
			button->setVisible(aAction.isVisible());
			button->setText(aAction.text());
		};
		connect(&aAction, &QAction::changed, button, render);
		connect(button, &QPushButton::clicked, &aAction, &QAction::trigger);
		render();
	}

	void profile_reset_panel::set_profile_label(const QString& aProfile)
	{
		Profile = aProfile;
	}

	void profile_reset_panel::present_status(const QString& aStatus)
	{
		Status->setText(aStatus);
		Status->setVisible(!aStatus.isEmpty());
	}

	void profile_reset_panel::present_size(const core::profile_data_reset aCategory, const QString& aSize)
	{
		if (const auto index = category_index(aCategory); index < Sizes.size()) {
			Sizes[index]->setText(aSize);
			Sizes[index]->setVisible(!aSize.isEmpty());
			Boxes[index]->setAccessibleName(aSize.isEmpty() ? profile_data_reset_category_words_for(aCategory).Name
															: QStringLiteral("%1, %2").arg(profile_data_reset_category_words_for(aCategory).Name, aSize));
		}
	}

	auto profile_reset_panel::check_box(const core::profile_data_reset aCategory) const noexcept -> QCheckBox*
	{
		const auto index = category_index(aCategory);
		return index < Boxes.size() ? Boxes[index] : nullptr;
	}

	auto profile_reset_panel::size_label(const core::profile_data_reset aCategory) const noexcept -> QLabel*
	{
		const auto index = category_index(aCategory);
		return index < Sizes.size() ? Sizes[index] : nullptr;
	}

	auto profile_reset_panel::clear_button() const noexcept -> QPushButton* { return Clear; }

	auto profile_reset_panel::select_all_button() const noexcept -> QPushButton* { return SelectAll; }

	auto profile_reset_panel::checked() const -> core::profile_data_reset_selection
	{
		auto selection = core::profile_data_reset_selection{ };
		for (const auto each : core::profile_data_reset_categories) {
			if (Boxes[category_index(each)]->isChecked()) {
				selection = selection.with(each);
			}
		}
		return selection;
	}

	void profile_reset_panel::set_checked(const core::profile_data_reset_selection aSelection)
	{
		for (const auto each : core::profile_data_reset_categories) {
			Own[category_index(each)] = aSelection.contains(each);
		}
		refresh();
	}

	auto profile_reset_panel::status_label() const noexcept -> QLabel*
	{
		return Status;
	}

	auto profile_reset_panel::ask(const core::profile_data_reset_selection aReset) -> profile_reset_confirmation*
	{
		// An entry point that names one category shows it checked here too.
		if (chosen() != aReset && checked() != aReset) {
			set_checked(aReset);
		}
		if (aReset.empty()) {
			return nullptr;
		}
		auto* const question = new profile_reset_confirmation{ window(), aReset, Profile };
		connect(question, &QDialog::accepted, this, [this, question] {
			present_status({ });
			if (Dispatch) {
				Dispatch(question->request());
			}
		});
		question->open();
		return question;
	}

	profile_recovery_dialog::profile_recovery_dialog(const QString& aProfile, const QString& aCause, QWidget* const aParent)
		: QDialog{ aParent }
		, Profile{ aProfile }
	{
		setObjectName(QStringLiteral("profileRecoveryDialog"));
		const auto title = aCause.isEmpty() ? QStringLiteral("Reset local data for %1?").arg(quoted(aProfile))
											: QStringLiteral("Profile %1 couldn’t open").arg(quoted(aProfile));
		setWindowTitle(title);
		setAccessibleName(title);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(22, 22, 22, 16);
		column->setSpacing(8);
		auto* const icon = new QLabel{ this };
		icon->setObjectName(QStringLiteral("profileRecoveryIcon"));
		icon->setPixmap(QApplication::windowIcon().pixmap(64, 64));
		icon->setAlignment(Qt::AlignHCenter);
		column->addWidget(icon);
		auto* const heading = new QLabel{ title, this };
		heading->setObjectName(QStringLiteral("profileRecoveryTitle"));
		auto bold = heading->font();
		bold.setBold(true);
		heading->setFont(bold);
		heading->setAlignment(Qt::AlignHCenter);
		heading->setWordWrap(true);
		column->addWidget(heading);
		const auto lines = aCause.isEmpty()
			? QStringList{ QStringLiteral("BetterPullReview started with --reset-profile-data, so nothing in this profile has opened yet."),
						   QStringLiteral("Clear caches first. If that does not help, reset everything: it signs you out, and you can keep a copy of your data.") }
			: QStringList{ QStringLiteral("BetterPullReview couldn’t read this profile’s local data. A crash or a full disk can damage it."),
						   QStringLiteral("Try clearing caches first. If the profile still doesn’t open, reset everything: it signs you out, and you can keep a copy of your data.") };
		for (const auto& line : lines) {
			auto* const words = new QLabel{ line, this };
			words->setObjectName(QStringLiteral("profileRecoveryText"));
			words->setWordWrap(true);
			words->setAlignment(Qt::AlignHCenter);
			column->addWidget(words);
		}
		column->addSpacing(6);
		Clear = new QPushButton{ aCause.isEmpty() ? QStringLiteral("Clear caches and open") : QStringLiteral("Clear caches and try again"), this };
		Clear->setObjectName(QStringLiteral("profileRecoveryClear"));
		Clear->setDefault(true);
		Everything = new QPushButton{ QStringLiteral("Reset everything…"), this };
		Everything->setObjectName(QStringLiteral("profileRecoveryEverything"));
		Quit = new quiet_push_button{ QStringLiteral("Quit"), this };
		Quit->setObjectName(QStringLiteral("profileRecoveryQuit"));
		for (auto* const button : { Clear, Everything, Quit }) {
			button->setMinimumHeight(28);
			column->addWidget(button);
		}
		const auto details = aCause.isEmpty()
			? QStringLiteral("Details: started with --reset-profile-data.")
			: QStringLiteral("Details: %1\nTo open this window yourself, start BetterPullReview from a terminal with --reset-profile-data.").arg(aCause);
		Details            = new QLabel{ details, this };
		Details->setObjectName(QStringLiteral("profileRecoveryDetails"));
		Details->setWordWrap(true);
		Details->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Details->setForegroundRole(QPalette::PlaceholderText);
		column->addSpacing(6);
		column->addWidget(Details);
		setMinimumWidth(420);
		setMaximumWidth(460);
		connect(Clear, &QPushButton::clicked, this, [this] { choose(profile_recovery_choice::clear_caches); });
		connect(Everything, &QPushButton::clicked, this, [this] { choose(profile_recovery_choice::reset_everything); });
		connect(Quit, &QPushButton::clicked, this, [this] { choose(profile_recovery_choice::quit); });
	}

	auto profile_recovery_dialog::choice() const noexcept -> profile_recovery_choice { return Choice; }
	auto profile_recovery_dialog::request() const -> std::optional<core::profile_data_reset_request> { return Request; }
	auto profile_recovery_dialog::clear_button() const noexcept -> QPushButton* { return Clear; }
	auto profile_recovery_dialog::everything_button() const noexcept -> QPushButton* { return Everything; }
	auto profile_recovery_dialog::quit_button() const noexcept -> QPushButton* { return Quit; }
	auto profile_recovery_dialog::details_label() const noexcept -> QLabel* { return Details; }

	void profile_recovery_dialog::choose(const profile_recovery_choice aChoice, std::optional<core::profile_data_reset_request> aEverything)
	{
		switch (aChoice) {
		case profile_recovery_choice::quit:
			Choice = aChoice;
			Request.reset();
			reject();
			return;
		case profile_recovery_choice::clear_caches:
			Choice  = aChoice;
			Request = core::profile_data_reset_request{ core::profile_data_reset::caches };
			accept();
			return;
		case profile_recovery_choice::reset_everything:
			break;
		}
		if (aEverything) {
			if (aEverything->Reset == core::profile_data_reset::everything && core::admit_profile_data_reset(*aEverything) == core::release_maintenance_reason::none) {
				Choice  = aChoice;
				Request = aEverything;
				accept();
			}
			return;
		}
		auto* const question = new profile_reset_confirmation{ this, core::profile_data_reset::everything, Profile };
		connect(question, &QDialog::accepted, this, [this, question] {
			Choice  = profile_recovery_choice::reset_everything;
			Request = question->request();
			accept();
		});
		question->open();
	}
}
