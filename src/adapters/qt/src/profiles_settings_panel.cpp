#include "bpr/qt/profiles_settings_panel.hpp"

#include "list_row_handles.hpp"

#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	auto profile_management_failure_text(const application::profile_management_failure aFailure) -> QString
	{
		using failure = application::profile_management_failure;
		switch (aFailure) {
		case failure::invalid_label:        return QStringLiteral("Choose a nonblank profile name within the supported length. Your text has been kept.");
		case failure::unknown_profile:      return QStringLiteral("That profile is no longer available. Your text has been kept.");
		case failure::profile_limit:        return QStringLiteral("The profile limit has been reached. Your text has been kept.");
		case failure::window_limit:         return QStringLiteral("This profile has reached its window limit. Close a window and try again.");
		case failure::identity_exhausted:   return QStringLiteral("No more profile identities are available. Your text has been kept.");
		case failure::busy:                 return QStringLiteral("Another profile change is in progress. Your text has been kept; try again when it finishes.");
		case failure::closing:              return QStringLiteral("Profiles are closing. Your text has been kept.");
		case failure::runtime_unavailable:  return QStringLiteral("The profile could not be opened. Other profiles remain available.");
		case failure::owner_busy:           return QStringLiteral("This profile is already open in another application process. Use its existing window.");
		case failure::cancelled:            return QStringLiteral("The profile operation was cancelled. Your text has been kept.");
		case failure::conflict:             return QStringLiteral("Profiles changed during this edit. Your text has been kept; review the list and try again.");
		case failure::registry_unavailable:
		case failure::invalid_registry:     return QStringLiteral("Profiles could not be loaded or saved. Your text has been kept.");
		}
		return QStringLiteral("The profile operation failed. Your text has been kept.");
	}

	namespace {
		/// The row's own data. The state is also the row's accessible description.
		namespace profile_row_role {
			enum : int {
				profile = Qt::UserRole,
				runtime = Qt::UserRole + 1,
				state   = Qt::UserRole + 2,
				current = Qt::UserRole + 3,
			};
		}
		constexpr int row_inset = 12;
		/// [[BPR-2318]]. A row's content starts after its grip's gutter.
		constexpr int row_lead     = detail::row_grip_gutter + 4;
		constexpr int row_gap      = 12;
		constexpr int fewest_rows  = 2;
		constexpr int most_rows    = 8;
		constexpr int section_rise = 20;

		auto item_profile(const QListWidgetItem* aItem) -> core::bpr_profile_id
		{
			return core::bpr_profile_id{ aItem == nullptr ? 0U : aItem->data(profile_row_role::profile).toULongLong() };
		}

		/// \brief What a row says about its profile's runtime, in the reviewer's words.
		auto state_text(const application::profile_management_runtime aRuntime, const int aWindows) -> QString
		{
			using runtime = application::profile_management_runtime;
			switch (aRuntime) {
			case runtime::open:
				return aWindows > 1 ? QStringLiteral("Open in %1 windows").arg(aWindows)
					: aWindows == 1 ? QStringLiteral("Open in 1 window")
									: QStringLiteral("Open");
			case runtime::opening: return QStringLiteral("Opening");
			case runtime::closing: return QStringLiteral("Closing");
			case runtime::closed:  break;
			}
			return QStringLiteral("Closed");
		}

		[[nodiscard]] auto heading(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        bold  = label->font();
			bold.setWeight(QFont::DemiBold);
			label->setFont(bold);
			return label;
		}

		[[nodiscard]] auto note(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		/// \brief One profile row: a letter avatar, the name, and the state at the right.
		///
		/// \details Selection, hover and focus painting stay the style's own. The only
		/// ink added is the quiet palette role for the state and one hairline between
		/// rows; the Settings window's own profile is named in the heavier weight.
		class profile_row_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto name  = option.text;
				const auto state = aIndex.data(profile_row_role::state).toString();
				option.text.clear();
				const auto* style = option.widget != nullptr ? option.widget->style() : QApplication::style();
				style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
				const auto selected = (option.state & QStyle::State_Selected) != 0;
				const auto group    = (option.state & QStyle::State_Enabled) != 0 ? QPalette::Normal : QPalette::Disabled;
				const auto ink      = home_chip_ink_for(option.palette, group, selected);
				const auto extent   = home_tile_extent(option.font);
				const auto content  = option.rect.adjusted(row_lead, 0, -row_inset, 0);
				aPainter->save();
				draw_home_tile(*aPainter, QRect{ content.left(), content.center().y() - extent / 2, extent, extent }, home_tile_initial(name), true, option.font, ink);
				const auto ground     = selected ? QPalette::HighlightedText : QPalette::PlaceholderText;
				const auto metrics    = QFontMetrics{ option.font };
				const auto stateWidth = state.isEmpty() ? 0 : metrics.horizontalAdvance(state);
				if (!state.isEmpty()) {
					aPainter->setPen(option.palette.color(group, ground));
					aPainter->drawText(QRect{ content.right() - stateWidth, content.top(), stateWidth, content.height() }, Qt::AlignRight | Qt::AlignVCenter, state);
				}
				auto nameFont = option.font;
				if (aIndex.data(profile_row_role::current).toBool()) {
					nameFont.setWeight(QFont::DemiBold);
				}
				aPainter->setFont(nameFont);
				aPainter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
				const auto nameLeft  = content.left() + extent + row_gap;
				const auto nameWidth = content.right() - (stateWidth == 0 ? 0 : stateWidth + row_gap) - nameLeft;
				aPainter->drawText(QRect{ nameLeft, content.top(), std::max(0, nameWidth), content.height() }, Qt::AlignLeft | Qt::AlignVCenter, name);
				if (aIndex.row() != 0) {
					aPainter->setPen(option.palette.color(group, QPalette::Mid));
					aPainter->drawLine(option.rect.topLeft(), option.rect.topRight());
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				auto size = QStyledItemDelegate::sizeHint(aOption, aIndex);
				size.setHeight(std::max(size.height(), profiles_settings_panel::row_height));
				return size;
			}
		};

		/// \brief The list's and the notice's bordered, rounded surfaces, painted
		/// rather than styled: RFC-BPR-0018 forbids a stylesheet, so the radius
		/// is drawn from the palette's own Base and Mid roles.
		class rounded_surface final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent*) override
			{
				QPainter painter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(palette().color(QPalette::Mid));
				painter.setBrush(palette().color(QPalette::Base));
				const auto radius = static_cast<qreal>(profiles_settings_panel::list_radius);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
			}
		};
	}

	profiles_settings_panel::profiles_settings_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("profilesSettings"));
		setAccessibleName(QStringLiteral("Profiles"));

		auto* const intro = note(QStringLiteral("Each profile keeps its own accounts, repositories, agents and reviews, and opens in its own windows."), this);
		intro->setObjectName(QStringLiteral("profilesIntro"));

		Notice = new rounded_surface{ this };
		Notice->setObjectName(QStringLiteral("profileFailureNotice"));
		NoticeText = new QLabel{ Notice };
		NoticeText->setObjectName(QStringLiteral("profileFailureText"));
		NoticeText->setWordWrap(true);
		NoticeText->setTextFormat(Qt::PlainText);
		NoticeText->setAccessibleName(QStringLiteral("Profile failure"));
		TryAgain = new quiet_push_button{ QStringLiteral("Try again"), Notice };
		TryAgain->setObjectName(QStringLiteral("profileTryAgain"));
		TryAgain->setAccessibleName(QStringLiteral("Try the profile operation again"));
		auto* const noticeRow = new QHBoxLayout{ Notice };
		noticeRow->setContentsMargins(12, 8, 12, 8);
		noticeRow->addWidget(NoticeText, 1);
		noticeRow->addWidget(TryAgain);
		Notice->hide();

		auto* const surface = new rounded_surface{ this };
		surface->setObjectName(QStringLiteral("profileRowSurface"));
		Profiles = new QListWidget{ surface };
		Profiles->setObjectName(QStringLiteral("managedProfiles"));
		Profiles->setAccessibleName(QStringLiteral("Profiles"));
		Profiles->setSelectionMode(QAbstractItemView::SingleSelection);
		Profiles->setFrameShape(QFrame::NoFrame);
		Profiles->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Profiles->viewport()->setAutoFillBackground(false);
		Profiles->setItemDelegate(new profile_row_delegate{ Profiles });
		Profiles->installEventFilter(this);
		// [[BPR-2318]] (owner, 2026-11-20: "Ps profiles needs the same drag
		// handle"). A grip on every row; a drag, Alt+Up or Alt+Down asks the
		// host to move the profile, and the list follows what it saves.
		Profiles->setDragDropMode(QAbstractItemView::InternalMove);
		Profiles->setDefaultDropAction(Qt::MoveAction);
		Profiles->setAccessibleDescription(QStringLiteral("Drag a profile's grip, or press Alt+Up or Alt+Down, to reorder."));
		Handles = new detail::list_row_handles{ *Profiles, { .Name = [this](const int aRow) {
																	 const auto* const item = Profiles->item(aRow);
																	 return item == nullptr ? QString{ } : item->text(); }, .Move = [this](const int aRow, const int aDelta) { move_profile(aRow, aDelta); }, .Remove = { } } };
		connect(Profiles->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex&, const int aStart, int, const QModelIndex&, const int aRow) {
			// Dropped in a new place: the host is asked to move it there.
			const auto destination = aRow > aStart ? aRow - 1 : aRow;
			if (destination != aStart) {
				request_move(item_profile(Profiles->item(destination)), destination - aStart);
			}
		});
		auto* const surfaceColumn = new QVBoxLayout{ surface };
		surfaceColumn->setContentsMargins(1, 1, 1, 1);
		surfaceColumn->addWidget(Profiles);

		// The editor lives over the row it renames, so the name is edited where it is
		// read. It exists only while an edit is open: there is no permanent field.
		Name = new QLineEdit{ Profiles->viewport() };
		Name->setObjectName(QStringLiteral("profileName"));
		Name->setAccessibleName(QStringLiteral("Profile name"));
		Name->setPlaceholderText(QStringLiteral("Profile name"));
		Name->setMaxLength(static_cast<int>(core::maximum_bpr_profile_label_bytes));
		Name->hide();
		Name->installEventFilter(this);

		Create = new quiet_push_button{ QStringLiteral("New profile"), this };
		Create->setObjectName(QStringLiteral("newProfile"));
		Create->setAccessibleName(QStringLiteral("New profile"));
		Rename = new quiet_push_button{ QStringLiteral("Rename"), this };
		Rename->setObjectName(QStringLiteral("renameProfile"));
		Rename->setAccessibleName(QStringLiteral("Rename the selected profile"));
		Open = new QPushButton{ QStringLiteral("Open"), this };
		Open->setObjectName(QStringLiteral("openProfile"));
		Open->setAccessibleName(QStringLiteral("Open the selected profile"));
		Save = new QPushButton{ QStringLiteral("Save"), this };
		Save->setObjectName(QStringLiteral("saveProfileName"));
		Save->setAccessibleName(QStringLiteral("Save the profile name"));
		Cancel = new quiet_push_button{ QStringLiteral("Cancel"), this };
		Cancel->setObjectName(QStringLiteral("cancelProfileName"));
		Cancel->setAccessibleName(QStringLiteral("Cancel the profile name"));
		Save->hide();
		Cancel->hide();

		auto* const actions = new QHBoxLayout;
		actions->addWidget(Create);
		actions->addStretch();
		actions->addWidget(Rename);
		actions->addWidget(Cancel);
		actions->addWidget(Open);
		actions->addWidget(Save);

		// [[BPR-2198]] (owner, 2026-11-15). The file moves this profile's
		// settings; it never carries a sign-in.
		Share = new QWidget{ this };
		Share->setObjectName(QStringLiteral("profileShareSection"));
		auto* const shareColumn = new QVBoxLayout{ Share };
		shareColumn->setContentsMargins(0, section_rise, 0, 0);
		auto* const shareHeading = heading(QStringLiteral("Share profile settings"), Share);
		shareHeading->setObjectName(QStringLiteral("profileShareHeading"));
		shareColumn->addWidget(shareHeading);
		auto* const shareSays = note(QStringLiteral("A file with this profile's agents, prompts and workflows, to move to another computer or share."), Share);
		shareSays->setObjectName(QStringLiteral("profileShareNote"));
		shareColumn->addWidget(shareSays);
		ImportButton = new quiet_push_button{ QStringLiteral("Import Profile Settings…"), Share };
		ImportButton->setObjectName(QStringLiteral("settingsImportProfileSettings"));
		ImportButton->setToolTip(QStringLiteral("Read a Profile Settings file, see what each part would change in this profile and choose what to import. Nothing runs."));
		ExportButton = new quiet_push_button{ QStringLiteral("Export Profile Settings…"), Share };
		ExportButton->setObjectName(QStringLiteral("settingsExportProfileSettings"));
		ExportButton->setToolTip(QStringLiteral("Write the parts of this profile you choose to a file. No sign-in, token or program location is written."));
		auto* const shareRow = new QHBoxLayout;
		shareRow->addWidget(ImportButton);
		shareRow->addWidget(ExportButton);
		shareRow->addStretch();
		shareColumn->addLayout(shareRow);
		ExchangeNote = note({ }, Share);
		ExchangeNote->setObjectName(QStringLiteral("settingsProfileSettingsNote"));
		ExchangeNote->hide();
		shareColumn->addWidget(ExchangeNote);
		Share->hide();

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->addWidget(intro);
		column->addWidget(Notice);
		column->addWidget(surface);
		column->addLayout(actions);
		column->addWidget(Share);

		setTabOrder(Profiles, Create);
		setTabOrder(Create, Rename);
		setTabOrder(Rename, Open);
		setTabOrder(Open, ImportButton);
		setTabOrder(ImportButton, ExportButton);

		connect(TryAgain, &QPushButton::clicked, this, [this] {
			if (Dispatch) {
				Dispatch(application::reload_bpr_profiles_intent{ });
			}
		});
		connect(Profiles, &QListWidget::currentRowChanged, this, [this] {
			if (Mode == edit_mode::none) {
				PreserveCandidate = false;
			}
			update_actions();
		});
		connect(Profiles, &QListWidget::itemDoubleClicked, this, [this] { open_selected(); });
		connect(Create, &QPushButton::clicked, this, [this] { begin_create(); });
		connect(Rename, &QPushButton::clicked, this, [this] { begin_rename(); });
		connect(Open, &QPushButton::clicked, this, [this] { activate_open(); });
		connect(Save, &QPushButton::clicked, this, [this] { commit_edit(); });
		connect(Cancel, &QPushButton::clicked, this, [this] { cancel_edit(); });
		connect(ImportButton, &QPushButton::clicked, this, [this] {
			if (Import) {
				Import();
			}
		});
		connect(ExportButton, &QPushButton::clicked, this, [this] {
			if (Export) {
				Export();
			}
		});
		fit_list();
		update_actions();
	}

	void profiles_settings_panel::set_dispatch(application::profile_management_dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void profiles_settings_panel::set_share_handlers(std::function<void()> aImport, std::function<void()> aExport)
	{
		Import = std::move(aImport);
		Export = std::move(aExport);
		Share->setVisible(Import || Export);
		ImportButton->setEnabled(static_cast<bool>(Import));
		ExportButton->setEnabled(static_cast<bool>(Export));
	}

	void profiles_settings_panel::present_exchange(const QString& aSaid)
	{
		ExchangeNote->setText(aSaid);
		ExchangeNote->setVisible(!aSaid.isEmpty());
	}

	void profiles_settings_panel::set_window_counts(const std::map<core::bpr_profile_id, int>& aCounts)
	{
		WindowCounts = aCounts;
		refresh_states();
	}

	void profiles_settings_panel::set_current_profile(const core::bpr_profile_id aProfile)
	{
		CurrentProfile = aProfile;
		refresh_states();
	}

	void profiles_settings_panel::focus_profiles()
	{
		if (Mode == edit_mode::none) {
			select_current();
		}
		Profiles->setFocus(Qt::OtherFocusReason);
	}

	auto profiles_settings_panel::pending_row() const -> QListWidgetItem*
	{
		for (auto row = 0; row < Profiles->count(); ++row) {
			if (item_profile(Profiles->item(row)).value() == 0U) {
				return Profiles->item(row);
			}
		}
		return nullptr;
	}

	void profiles_settings_panel::select_current()
	{
		for (auto row = 0; row < Profiles->count(); ++row) {
			if (item_profile(Profiles->item(row)) == CurrentProfile) {
				Profiles->setCurrentRow(row);
				return;
			}
		}
		if (Profiles->currentItem() == nullptr && Profiles->count() != 0) {
			Profiles->setCurrentRow(0);
		}
	}

	void profiles_settings_panel::refresh_states()
	{
		for (auto row = 0; row < Profiles->count(); ++row) {
			auto* const item    = Profiles->item(row);
			const auto  profile = item_profile(item);
			if (profile.value() == 0U) {
				continue;
			}
			const auto counted = WindowCounts.find(profile);
			const auto runtime = static_cast<application::profile_management_runtime>(item->data(profile_row_role::runtime).toInt());
			const auto state   = state_text(runtime, counted == WindowCounts.end() ? 0 : counted->second);
			const auto current = profile == CurrentProfile;
			item->setData(profile_row_role::state, state);
			item->setData(profile_row_role::current, current);
			item->setData(Qt::AccessibleDescriptionRole, state);
			item->setToolTip(state);
		}
	}

	void profiles_settings_panel::adopt(const application::profile_management_projection& aProjection)
	{
		if (aProjection.Profiles.size() > core::maximum_bpr_profiles) {
			Ready = false;
			update_actions();
			return;
		}
		const auto selected = selected_profile();
		const auto blocker  = QSignalBlocker{ Profiles };
		for (auto row = Profiles->count() - 1; row >= 0; --row) {
			auto* const item = Profiles->item(row);
			if (item == pending_row()) {
				continue;
			}
			if (std::ranges::none_of(aProjection.Profiles, [item](const auto& aProfile) { return aProfile.Profile == item_profile(item); })) {
				delete Profiles->takeItem(row);
			}
		}
		for (std::size_t index = 0; index < aProjection.Profiles.size(); ++index) {
			const auto& profile = aProjection.Profiles[index];
			auto*       item    = static_cast<QListWidgetItem*>(nullptr);
			for (auto row = 0; row < Profiles->count(); ++row) {
				if (item_profile(Profiles->item(row)) == profile.Profile) {
					item = Profiles->item(row);
					break;
				}
			}
			if (item == nullptr) {
				item = new QListWidgetItem;
				item->setData(profile_row_role::profile, QVariant::fromValue<qulonglong>(profile.Profile.value()));
				Profiles->insertItem(static_cast<int>(index), item);
			} else if (Profiles->row(item) != static_cast<int>(index)) {
				Profiles->takeItem(Profiles->row(item));
				Profiles->insertItem(static_cast<int>(index), item);
			}
			item->setData(profile_row_role::runtime, static_cast<int>(profile.Runtime));
			const auto label = QString::fromStdString(profile.Label);
			if (item->text() != label) {
				item->setText(label);
			}
			if (profile.Profile == selected) {
				Profiles->setCurrentItem(item);
			}
		}
		refresh_states();
		Ready   = aProjection.Ready;
		Busy    = aProjection.Busy;
		Closing = aProjection.Closing;
		// RFC-BPR-0033: the current profile's row is selected when the list is shown.
		if (Profiles->currentItem() == nullptr) {
			select_current();
		}
		if (aProjection.Failure) {
			// The name is kept: a refused edit stays open on exactly what was typed.
			NoticeText->setText(profile_management_failure_text(aProjection.Failure->Reason));
			Notice->show();
			PendingEdit.reset();
		} else if (aProjection.AcceptedEdit && PendingEdit && aProjection.AcceptedEdit->Operation == *PendingEdit
				   && (*PendingEdit == application::profile_management_operation::create || aProjection.AcceptedEdit->Profile == PendingProfile)) {
			// The renamed row is the confirmation, so nothing is announced here.
			if (Name->text() == SubmittedLabel) {
				PreserveCandidate = false;
				end_edit();
				for (auto row = 0; row < Profiles->count(); ++row) {
					if (item_profile(Profiles->item(row)) == aProjection.AcceptedEdit->Profile) {
						Profiles->setCurrentRow(row);
						break;
					}
				}
			}
			PendingEdit.reset();
			Notice->hide();
		} else if (!PreserveCandidate) {
			Notice->hide();
		}
		fit_list();
		update_actions();
		place_editor();
	}

	auto profiles_settings_panel::selected_profile() const -> core::bpr_profile_id { return item_profile(Profiles->currentItem()); }

	auto profiles_settings_panel::profile_grip(const int aRow) const -> QWidget* { return Handles->grip(aRow); }

	void profiles_settings_panel::move_profile(const int aRow, const int aDelta)
	{
		if (aRow < 0 || aRow + aDelta < 0 || aRow + aDelta >= Profiles->count()) {
			return;
		}
		request_move(item_profile(Profiles->item(aRow)), aDelta);
	}

	void profiles_settings_panel::request_move(const core::bpr_profile_id aProfile, const int aDelta)
	{
		// A profile being named has no identity yet and cannot be moved; the
		// host's next projection puts a refused move back.
		if (!Dispatch || Mode != edit_mode::none || aProfile.value() == 0U || aDelta == 0) {
			return;
		}
		Dispatch(application::move_bpr_profile_intent{ aProfile, aDelta });
	}

	auto profiles_settings_panel::selected_is_open() const -> bool
	{
		auto* const item = Profiles->currentItem();
		return item != nullptr && item->data(profile_row_role::runtime).toInt() == static_cast<int>(application::profile_management_runtime::open);
	}

	void profiles_settings_panel::begin_rename()
	{
		if (!Rename->isEnabled() || Profiles->currentItem() == nullptr) {
			return;
		}
		Mode = edit_mode::rename;
		Name->setText(Profiles->currentItem()->text());
		Name->show();
		Name->setFocus(Qt::OtherFocusReason);
		Name->selectAll();
		update_actions();
		place_editor();
	}

	void profiles_settings_panel::begin_create()
	{
		if (!Create->isEnabled()) {
			return;
		}
		Mode = edit_mode::create;
		// The new row exists only while it is being named; it carries no identity
		// until the host accepts one.
		const auto blocker = QSignalBlocker{ Profiles };
		auto*      item    = new QListWidgetItem;
		item->setData(profile_row_role::state, QString{ });
		Profiles->addItem(item);
		Profiles->setCurrentItem(item);
		fit_list();
		Profiles->scrollToItem(item);
		Name->clear();
		Name->show();
		Name->setFocus(Qt::OtherFocusReason);
		update_actions();
		place_editor();
	}

	void profiles_settings_panel::commit_edit()
	{
		if (Mode == edit_mode::none || !Dispatch || !Save->isEnabled()) {
			return;
		}
		PreserveCandidate = true;
		PendingEdit       = Mode == edit_mode::create ? application::profile_management_operation::create : application::profile_management_operation::rename;
		PendingProfile    = selected_profile();
		SubmittedLabel    = Name->text();
		if (Mode == edit_mode::create) {
			Dispatch(application::create_bpr_profile_intent{ SubmittedLabel.toStdString() });
		} else {
			Dispatch(application::rename_bpr_profile_intent{ PendingProfile, SubmittedLabel.toStdString() });
		}
	}

	void profiles_settings_panel::cancel_edit()
	{
		// RFC-BPR-0033: Escape cancels. Nothing is dispatched, so the profile
		// keeps the name it already had.
		PreserveCandidate = false;
		PendingEdit.reset();
		end_edit();
		Notice->hide();
		update_actions();
	}

	void profiles_settings_panel::end_edit()
	{
		if (Mode == edit_mode::create) {
			const auto blocker = QSignalBlocker{ Profiles };
			if (auto* const pending = pending_row(); pending != nullptr) {
				delete Profiles->takeItem(Profiles->row(pending));
			}
		}
		Mode = edit_mode::none;
		Name->hide();
		Name->clear();
		fit_list();
		if (Profiles->isVisible()) {
			Profiles->setFocus(Qt::OtherFocusReason);
		}
	}

	void profiles_settings_panel::open_selected()
	{
		// Return and double-click open the selected profile, and the existing activate
		// intent focuses it when it is already open.
		if (Dispatch && Mode == edit_mode::none && Ready && !Closing && selected_profile().value() != 0U) {
			Dispatch(application::open_bpr_profile_intent{ selected_profile() });
		}
	}

	void profiles_settings_panel::activate_open()
	{
		if (!Dispatch || !Open->isEnabled()) {
			return;
		}
		if (selected_is_open()) {
			Dispatch(application::new_bpr_window_intent{ selected_profile() });
			return;
		}
		Dispatch(application::open_bpr_profile_intent{ selected_profile() });
	}

	void profiles_settings_panel::update_actions()
	{
		const auto editing = Mode != edit_mode::none;
		const auto idle    = Ready && !Busy && !Closing;
		// A profile is not dragged while one is being named.
		Profiles->setDragEnabled(!editing);
		Create->setEnabled(idle && !editing);
		Rename->setVisible(!editing);
		Open->setVisible(!editing);
		Save->setVisible(editing);
		Cancel->setVisible(editing);
		Rename->setEnabled(idle && selected_profile().value() != 0U);
		Open->setText(selected_is_open() ? QStringLiteral("New window") : QStringLiteral("Open"));
		Open->setAccessibleName(selected_is_open() ? QStringLiteral("Open a new window in the selected profile") : QStringLiteral("Open the selected profile"));
		Open->setEnabled(Ready && !Closing && selected_profile().value() != 0U && Profiles->currentItem() != nullptr
						 && Profiles->currentItem()->data(profile_row_role::runtime).toInt() != static_cast<int>(application::profile_management_runtime::closing));
		// A blank name is the host's to refuse, so Save stays reachable and the
		// refusal explains itself in the notice.
		Save->setEnabled(!Busy && !Closing);
		Cancel->setEnabled(true);
		if (editing) {
			setTabOrder(Name, Cancel);
			setTabOrder(Cancel, Save);
		}
	}

	void profiles_settings_panel::fit_list()
	{
		// The list is as tall as its rows, between two and eight, so a page of
		// three profiles does not end in an empty well.
		const auto rows = std::clamp(Profiles->count(), fewest_rows, most_rows);
		Profiles->setFixedHeight((rows * row_height) + (2 * Profiles->frameWidth()));
	}

	void profiles_settings_panel::place_editor()
	{
		if (Mode == edit_mode::none || Profiles->currentItem() == nullptr) {
			return;
		}
		const auto row    = Profiles->visualItemRect(Profiles->currentItem());
		const auto extent = home_tile_extent(Profiles->font());
		const auto left   = row.left() + row_lead + extent + row_gap;
		const auto height = std::min(row.height() - 8, Name->sizeHint().height());
		Name->setGeometry(QRect{ left, row.center().y() - (height / 2), std::max(0, row.right() - row_inset - left), height });
	}

	bool profiles_settings_panel::eventFilter(QObject* aObject, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::KeyPress) {
			const auto* key = static_cast<QKeyEvent*>(aEvent);
			if (aObject == Name) {
				if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
					commit_edit();
					return true;
				}
				if (key->key() == Qt::Key_Escape) {
					cancel_edit();
					return true;
				}
			} else if (aObject == Profiles && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
				open_selected();
				return true;
			}
		}
		if (aEvent->type() == QEvent::Resize && aObject == Profiles) {
			place_editor();
		}
		return QWidget::eventFilter(aObject, aEvent);
	}

	void profiles_settings_panel::keyPressEvent(QKeyEvent* aEvent)
	{
		if (aEvent->key() == Qt::Key_Escape && Mode != edit_mode::none) {
			cancel_edit();
			aEvent->accept();
			return;
		}
		QWidget::keyPressEvent(aEvent);
	}

	auto profiles_settings_panel::status_text() const -> QString { return Notice->isVisible() ? NoticeText->text() : QString{ }; }
}
