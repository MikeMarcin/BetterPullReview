#include "bpr/qt/profile_data_reset_views.hpp"

#include "bpr/qt/destructive_question.hpp"

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
#include <QStyle>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		using reset = core::profile_data_reset;

		constexpr std::array resets{ reset::caches, reset::settings, reset::review_data, reset::everything };

		[[nodiscard]] auto index_of(const reset aReset) -> std::size_t
		{
			return static_cast<std::size_t>(aReset) - 1U;
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

	auto profile_data_reset_wording_for(const core::profile_data_reset aReset, const QString& aProfile) -> profile_data_reset_wording
	{
		const auto name = quoted(aProfile);
		switch (aReset) {
		case reset::caches:
			return { QStringLiteral("Clear caches for %1?").arg(name),
					 QStringLiteral("This profile’s windows close, its caches are removed, and Home reopens. Nothing you wrote is affected."),
					 { QStringLiteral("Repository data downloaded from GitHub"), QStringLiteral("Images in descriptions and comments"),
					   QStringLiteral("Pull request summaries on Home"), QStringLiteral("Context snapshots kept for agents") },
					 { QStringLiteral("Notes, drafts and read marks"), QStringLiteral("Settings"), QStringLiteral("Sign-in"), QStringLiteral("Diagnostic logs") },
					 QStringLiteral("Earlier pushes of a pull request that was force-pushed may no longer be on GitHub. Comparisons with those pushes can’t be rebuilt."),
					 QStringLiteral("Clear caches") };
		case reset::settings:
			return { QStringLiteral("Reset settings for %1?").arg(name),
					 QStringLiteral("This profile’s windows close, every choice in Settings returns to its default, and Home reopens."),
					 { QStringLiteral("Every choice in Settings"), QStringLiteral("Home’s queue filters and sort"), QStringLiteral("Notification choices"),
					   QStringLiteral("Window layout and which windows reopen") },
					 { QStringLiteral("Saved repositories and servers"), QStringLiteral("Observer rules"), QStringLiteral("Review data and caches"),
					   QStringLiteral("Sign-in"), QStringLiteral("Diagnostic logs") },
					 { },
					 QStringLiteral("Reset settings") };
		case reset::review_data:
			return { QStringLiteral("Reset review data for %1?").arg(name),
					 QStringLiteral("This deletes review work that exists only on this computer. Comments and reviews already posted stay on GitHub."),
					 { QStringLiteral("Read and seen marks"), QStringLiteral("Private notes and their threads"), QStringLiteral("Unsent comments, replies and reviews"),
					   QStringLiteral("Review tours"), QStringLiteral("Review Cycles and their rounds") },
					 { QStringLiteral("Everything posted on GitHub"), QStringLiteral("Settings and caches"), QStringLiteral("Sign-in"),
					   QStringLiteral("Diagnostic logs"), QStringLiteral("Files you saved or exported") },
					 { },
					 QStringLiteral("Reset review data") };
		case reset::everything:
			break;
		}
		return { QStringLiteral("Reset everything for %1?").arg(name),
				 QStringLiteral("This profile starts again as on first launch. You stay signed in, and nothing on GitHub changes."),
				 { QStringLiteral("Caches: repository data, images, summaries"), QStringLiteral("Settings: every choice, saved repositories, observer rules"),
				   QStringLiteral("Review data: read marks, notes, unsent comments, tours, Review Cycles"), QStringLiteral("Insights statistics and history") },
				 { QStringLiteral("Sign-in to your accounts and hosts"), QStringLiteral("Everything posted on GitHub"), QStringLiteral("Diagnostic logs"),
				   QStringLiteral("Other profiles"), QStringLiteral("Your Git checkouts and exported files") },
				 { },
				 QStringLiteral("Reset everything") };
	}

	auto profile_data_reset_notice(const core::profile_data_reset_outcome& aOutcome, const QString& aCopyFolder) -> QString
	{
		const auto action = profile_data_reset_wording_for(aOutcome.Reset, { }).Confirm;
		if (aOutcome.Reason != core::release_maintenance_reason::none) {
			return QStringLiteral("%1 did not run: %2. Nothing was removed.").arg(action, refusal(aOutcome.Reason));
		}
		auto notice = QString{ };
		switch (aOutcome.Reset) {
		case reset::caches:
			notice = QStringLiteral("Caches cleared. They download again as you open pull requests.");
			break;
		case reset::settings:
			notice = aOutcome.QueueDisplayRestored
				? QStringLiteral("Settings reset to their defaults.")
				: QStringLiteral("Settings reset to their defaults, except Home’s queue filters, which could not be saved; Reset everything clears them.");
			break;
		case reset::review_data:
			notice = QStringLiteral("Review data reset.");
			break;
		case reset::everything:
			notice = QStringLiteral("This profile was reset. You are still signed in.");
			break;
		}
		if (aOutcome.Copied && !aCopyFolder.isEmpty()) {
			notice += QStringLiteral(" A copy of the removed data is in “%1”.").arg(aCopyFolder);
		}
		return notice;
	}

	profile_reset_confirmation::profile_reset_confirmation(QWidget* const aParent, const core::profile_data_reset aReset, const QString& aProfile)
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
			const auto [copyText, copyDetail] = copy_words();
			Copy                              = option(this, column, copyText, copyDetail, QStringLiteral("profileResetCopy"), true);
			column->addSpacing(2);
			Acknowledge = option(this, column, QStringLiteral("I understand my private notes and unsent comments will be deleted"), { }, QStringLiteral("profileResetAcknowledge"), false);
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
	auto profile_reset_confirmation::reset() const noexcept -> core::profile_data_reset { return Reset; }
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
		: QGroupBox{ QStringLiteral("Troubleshooting"), aParent }
	{
		setObjectName(QStringLiteral("profileResetPanel"));
		auto* const column = new QVBoxLayout{ this };
		auto* const lead   = new QLabel{ QStringLiteral("Each reset keeps you signed in and reopens this profile’s windows."), this };
		lead->setObjectName(QStringLiteral("profileResetLead"));
		lead->setWordWrap(true);
		lead->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(lead);
		auto* const grid = new QGridLayout;
		grid->setHorizontalSpacing(12);
		grid->setVerticalSpacing(6);
		const std::array rows{
			std::pair{ QStringLiteral("Clear caches"), QStringLiteral("Downloaded data. It downloads again when needed.") },
			std::pair{ QStringLiteral("Reset settings"), QStringLiteral("Every choice in Settings returns to its default.") },
			std::pair{ QStringLiteral("Reset review data"), QStringLiteral("Read marks, notes, drafts, tours and Review Cycles.") },
			std::pair{ QStringLiteral("Reset everything"), QStringLiteral("All three at once. You stay signed in.") },
		};
		const std::array names{ QStringLiteral("Caches"), QStringLiteral("Settings"), QStringLiteral("ReviewData"), QStringLiteral("Everything") };
		for (const auto each : resets) {
			const auto  row   = static_cast<int>(index_of(each));
			const auto& words = rows[index_of(each)];
			auto* const name  = new QLabel{ words.first, this };
			name->setObjectName(QStringLiteral("profileReset%1Name").arg(names[index_of(each)]));
			auto* const line = new QLabel{ words.second, this };
			line->setObjectName(QStringLiteral("profileReset%1Line").arg(names[index_of(each)]));
			line->setWordWrap(true);
			line->setForegroundRole(QPalette::PlaceholderText);
			auto* const button = new QPushButton{ words.first + QStringLiteral("…"), this };
			button->setObjectName(QStringLiteral("profileReset%1").arg(names[index_of(each)]));
			button->setAccessibleName(words.first);
			button->setAccessibleDescription(words.second);
			button->setAutoDefault(false);
			name->setBuddy(button);
			Buttons[index_of(each)] = button;
			grid->addWidget(name, row, 0);
			grid->addWidget(line, row, 1);
			grid->addWidget(button, row, 2, Qt::AlignRight);
			connect(button, &QPushButton::clicked, this, [this, each] { static_cast<void>(ask(each)); });
		}
		grid->setColumnMinimumWidth(0, 170);
		grid->setColumnStretch(1, 1);
		column->addLayout(grid);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("profileResetStatus"));
		Status->setWordWrap(true);
		Status->hide();
		column->addWidget(Status);
	}

	void profile_reset_panel::set_dispatch(dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
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

	auto profile_reset_panel::button(const core::profile_data_reset aReset) const noexcept -> QPushButton*
	{
		const auto index = static_cast<std::size_t>(aReset) - 1U;
		return index < Buttons.size() ? Buttons[index] : nullptr;
	}

	auto profile_reset_panel::status_label() const noexcept -> QLabel*
	{
		return Status;
	}

	auto profile_reset_panel::ask(const core::profile_data_reset aReset) -> profile_reset_confirmation*
	{
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
						   QStringLiteral("Clear caches first. If that does not help, reset everything: you stay signed in and can keep a copy of your data.") }
			: QStringList{ QStringLiteral("BetterPullReview couldn’t read this profile’s local data. A crash or a full disk can damage it."),
						   QStringLiteral("Try clearing caches first. If the profile still doesn’t open, reset everything: you stay signed in and can keep a copy of your data.") };
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
		Quit = new QPushButton{ QStringLiteral("Quit"), this };
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
