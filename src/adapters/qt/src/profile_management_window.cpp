#include "bpr/qt/profile_management_window.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include "bpr/qt/home_row_delegate.hpp"

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
			};
		}
		constexpr int row_inset = 12;
		constexpr int row_gap   = 12;

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

		/// \brief One profile row: a letter avatar, the name, and the state at the right.
		///
		/// \details Selection, hover and focus painting stay the style's own. The only
		/// ink added is the quiet palette role for the state and one hairline between
		/// rows; remote literal text is not elided into an ambiguous prefix.
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
				const auto content  = option.rect.adjusted(row_inset, 0, -row_inset, 0);
				aPainter->save();
				draw_home_tile(*aPainter,
							   QRect{ content.left(), content.center().y() - extent / 2, extent, extent },
							   home_tile_initial(name), true, option.font, ink);
				const auto ground     = selected ? QPalette::HighlightedText : QPalette::PlaceholderText;
				const auto metrics    = QFontMetrics{ option.font };
				const auto stateWidth = state.isEmpty() ? 0 : metrics.horizontalAdvance(state);
				if (!state.isEmpty()) {
					aPainter->setPen(option.palette.color(group, ground));
					aPainter->drawText(QRect{ content.right() - stateWidth, content.top(), stateWidth, content.height() },
									   Qt::AlignRight | Qt::AlignVCenter, state);
				}
				aPainter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
				const auto nameLeft  = content.left() + extent + row_gap;
				const auto nameWidth = content.right() - (stateWidth == 0 ? 0 : stateWidth + row_gap) - nameLeft;
				aPainter->drawText(QRect{ nameLeft, content.top(), std::max(0, nameWidth), content.height() },
								   Qt::AlignLeft | Qt::AlignVCenter, name);
				if (aIndex.row() != 0) {
					aPainter->setPen(option.palette.color(group, QPalette::Mid));
					aPainter->drawLine(option.rect.topLeft(), option.rect.topRight());
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
				-> QSize override
			{
				auto size = QStyledItemDelegate::sizeHint(aOption, aIndex);
				size.setHeight(std::max(size.height(), profile_management_window::row_height));
				return size;
			}
		};

		/// \brief The sheet's bordered, rounded surfaces, painted rather than styled.
		///
		/// \details RFC-BPR-0018 forbids a stylesheet, so the one 8-point radius the
		/// design asks for is drawn from the palette's own Base and Mid roles.
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
				const auto radius = static_cast<qreal>(profile_management_window::list_radius);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
			}
		};
	}

	profile_management_window::profile_management_window(QWidget* aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("profileManagementWindow"));
		setWindowTitle(QStringLiteral("Profiles"));
		setAccessibleName(QStringLiteral("Profiles"));
		setMinimumSize(360, 280);
		resize(sheet_width, sheet_height);

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
		auto* noticeRow = new QHBoxLayout{ Notice };
		noticeRow->setContentsMargins(12, 8, 12, 8);
		noticeRow->addWidget(NoticeText, 1);
		noticeRow->addWidget(TryAgain);
		Notice->hide();

		auto* surface = new rounded_surface{ this };
		surface->setObjectName(QStringLiteral("profileRowSurface"));
		Profiles = new QListWidget{ surface };
		Profiles->setObjectName(QStringLiteral("managedProfiles"));
		Profiles->setAccessibleName(QStringLiteral("Profiles"));
		Profiles->setSelectionMode(QAbstractItemView::SingleSelection);
		Profiles->setFrameShape(QFrame::NoFrame);
		Profiles->viewport()->setAutoFillBackground(false);
		Profiles->setItemDelegate(new profile_row_delegate{ Profiles });
		Profiles->installEventFilter(this);
		auto* surfaceColumn = new QVBoxLayout{ surface };
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
		Create->setFlat(true);
		Create->setAccessibleName(QStringLiteral("New profile"));
		Rename = new quiet_push_button{ QStringLiteral("Rename"), this };
		Rename->setObjectName(QStringLiteral("renameProfile"));
		Rename->setAccessibleName(QStringLiteral("Rename the selected profile"));
		Open = new QPushButton{ QStringLiteral("Open"), this };
		Open->setObjectName(QStringLiteral("openProfile"));
		Open->setAccessibleName(QStringLiteral("Open the selected profile"));
		Open->setDefault(true);
		Save = new QPushButton{ QStringLiteral("Save"), this };
		Save->setObjectName(QStringLiteral("saveProfileName"));
		Save->setAccessibleName(QStringLiteral("Save the profile name"));
		Save->setDefault(true);
		Cancel = new quiet_push_button{ QStringLiteral("Cancel"), this };
		Cancel->setObjectName(QStringLiteral("cancelProfileName"));
		Cancel->setAccessibleName(QStringLiteral("Cancel the profile name"));
		Save->hide();
		Cancel->hide();

		auto* footer = new QHBoxLayout;
		footer->addWidget(Create);
		footer->addStretch();
		footer->addWidget(Rename);
		footer->addWidget(Cancel);
		footer->addWidget(Open);
		footer->addWidget(Save);
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(12, 12, 12, 12);
		column->addWidget(Notice);
		column->addWidget(surface, 1);
		column->addLayout(footer);

		setTabOrder(Profiles, Create);
		setTabOrder(Create, Rename);
		setTabOrder(Rename, Open);

		connect(TryAgain, &QPushButton::clicked, this, [this] {
			if (Dispatch) {
				Dispatch(application::reload_bpr_profiles_intent{ });
			}
		});
		connect(Profiles, &QListWidget::currentRowChanged, this, [this] { select_profile(); });
		connect(Profiles, &QListWidget::itemDoubleClicked, this, [this] { open_selected(); });
		connect(Create, &QPushButton::clicked, this, [this] { begin_create(); });
		connect(Rename, &QPushButton::clicked, this, [this] { begin_rename(); });
		connect(Open, &QPushButton::clicked, this, [this] { activate_footer(); });
		connect(Save, &QPushButton::clicked, this, [this] { commit_edit(); });
		connect(Cancel, &QPushButton::clicked, this, [this] { cancel_edit(); });
		update_actions();
	}

	void profile_management_window::set_dispatch(application::profile_management_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void profile_management_window::set_window_counts(const std::map<core::bpr_profile_id, int>& aCounts)
	{
		WindowCounts = aCounts;
	}

	void profile_management_window::set_current_profile(const core::bpr_profile_id aProfile)
	{
		CurrentProfile = aProfile;
	}

	auto profile_management_window::pending_row() const -> QListWidgetItem*
	{
		for (auto row = 0; row < Profiles->count(); ++row) {
			if (item_profile(Profiles->item(row)).value() == 0U) {
				return Profiles->item(row);
			}
		}
		return nullptr;
	}

	void profile_management_window::adopt(const application::profile_management_projection& aProjection)
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
			if (std::ranges::none_of(aProjection.Profiles, [item](const auto& aProfile) {
					return aProfile.Profile == item_profile(item);
				})) {
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
			const auto counted = WindowCounts.find(profile.Profile);
			const auto state   = state_text(profile.Runtime, counted == WindowCounts.end() ? 0 : counted->second);
			item->setData(profile_row_role::runtime, static_cast<int>(profile.Runtime));
			item->setData(profile_row_role::state, state);
			item->setData(Qt::AccessibleDescriptionRole, state);
			item->setToolTip(state);
			const auto label = QString::fromStdString(profile.Label);
			if (item->text() != label) {
				item->setText(label);
			}
			if (profile.Profile == selected) {
				Profiles->setCurrentItem(item);
			}
		}
		Ready   = aProjection.Ready;
		Busy    = aProjection.Busy;
		Closing = aProjection.Closing;
		// RFC-BPR-0033: the current profile's row is selected when the sheet opens.
		if (Profiles->currentItem() == nullptr && Profiles->count() != 0) {
			auto* current = static_cast<QListWidgetItem*>(nullptr);
			for (auto row = 0; row < Profiles->count(); ++row) {
				if (item_profile(Profiles->item(row)) == CurrentProfile) {
					current = Profiles->item(row);
					break;
				}
			}
			Profiles->setCurrentItem(current != nullptr ? current : Profiles->item(0));
		}
		if (aProjection.Failure) {
			// The name is kept: a refused edit stays open on exactly what was typed.
			NoticeText->setText(profile_management_failure_text(aProjection.Failure->Reason));
			Notice->show();
			PendingEdit.reset();
		} else if (aProjection.AcceptedEdit && PendingEdit
				   && aProjection.AcceptedEdit->Operation == *PendingEdit
				   && (*PendingEdit == application::profile_management_operation::create
					   || aProjection.AcceptedEdit->Profile == PendingProfile)) {
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
		update_actions();
		place_editor();
	}

	void profile_management_window::present()
	{
		show();
		raise();
		activateWindow();
		Profiles->setFocus(Qt::OtherFocusReason);
	}

	auto profile_management_window::selected_profile() const -> core::bpr_profile_id
	{
		return item_profile(Profiles->currentItem());
	}

	auto profile_management_window::selected_is_open() const -> bool
	{
		auto* const item = Profiles->currentItem();
		return item != nullptr
			&& item->data(profile_row_role::runtime).toInt() == static_cast<int>(application::profile_management_runtime::open);
	}

	void profile_management_window::select_profile()
	{
		if (Mode == edit_mode::none) {
			PreserveCandidate = false;
		}
		update_actions();
	}

	void profile_management_window::begin_rename()
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

	void profile_management_window::begin_create()
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
		Name->clear();
		Name->show();
		Name->setFocus(Qt::OtherFocusReason);
		update_actions();
		place_editor();
	}

	void profile_management_window::commit_edit()
	{
		if (Mode == edit_mode::none || !Dispatch || !Save->isEnabled()) {
			return;
		}
		PreserveCandidate = true;
		PendingEdit       = Mode == edit_mode::create
			? application::profile_management_operation::create
			: application::profile_management_operation::rename;
		PendingProfile    = selected_profile();
		SubmittedLabel    = Name->text();
		if (Mode == edit_mode::create) {
			Dispatch(application::create_bpr_profile_intent{ SubmittedLabel.toStdString() });
		} else {
			Dispatch(application::rename_bpr_profile_intent{ PendingProfile, SubmittedLabel.toStdString() });
		}
	}

	void profile_management_window::cancel_edit()
	{
		// RFC-BPR-0033: Escape cancels and keeps the name. Nothing is dispatched, so
		// the profile keeps the name it already had.
		PreserveCandidate = false;
		PendingEdit.reset();
		end_edit();
		Notice->hide();
		update_actions();
	}

	void profile_management_window::end_edit()
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
		if (Profiles->isVisible()) {
			Profiles->setFocus(Qt::OtherFocusReason);
		}
	}

	void profile_management_window::open_selected()
	{
		// Return and double-click open the selected profile, and the existing activate
		// intent focuses it when it is already open.
		if (Dispatch && Mode == edit_mode::none && Ready && !Closing && selected_profile().value() != 0U) {
			Dispatch(application::open_bpr_profile_intent{ selected_profile() });
		}
	}

	void profile_management_window::activate_footer()
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

	void profile_management_window::update_actions()
	{
		const auto editing = Mode != edit_mode::none;
		const auto idle    = Ready && !Busy && !Closing;
		Create->setEnabled(idle && !editing);
		Rename->setVisible(!editing);
		Open->setVisible(!editing);
		Save->setVisible(editing);
		Cancel->setVisible(editing);
		Rename->setEnabled(idle && selected_profile().value() != 0U);
		Open->setText(selected_is_open() ? QStringLiteral("New window") : QStringLiteral("Open"));
		Open->setAccessibleName(selected_is_open()
									? QStringLiteral("Open a new window in the selected profile")
									: QStringLiteral("Open the selected profile"));
		Open->setEnabled(Ready && !Closing && selected_profile().value() != 0U
						 && Profiles->currentItem() != nullptr
						 && Profiles->currentItem()->data(profile_row_role::runtime).toInt()
							 != static_cast<int>(application::profile_management_runtime::closing));
		// A blank name is the host's to refuse, so Save stays reachable and the
		// refusal explains itself in the notice.
		Save->setEnabled(!Busy && !Closing);
		Cancel->setEnabled(true);
		if (editing) {
			setTabOrder(Name, Cancel);
			setTabOrder(Cancel, Save);
		}
	}

	void profile_management_window::place_editor()
	{
		if (Mode == edit_mode::none || Profiles->currentItem() == nullptr) {
			return;
		}
		const auto row    = Profiles->visualItemRect(Profiles->currentItem());
		const auto extent = home_tile_extent(Profiles->font());
		const auto left   = row.left() + row_inset + extent + row_gap;
		const auto height = std::min(row.height() - 8, Name->sizeHint().height());
		Name->setGeometry(QRect{ left, row.center().y() - height / 2, std::max(0, row.right() - row_inset - left), height });
	}

	bool profile_management_window::eventFilter(QObject* aObject, QEvent* aEvent)
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
		return QWidget::eventFilter(aObject, aEvent);
	}

	void profile_management_window::keyPressEvent(QKeyEvent* aEvent)
	{
		if (aEvent->key() == Qt::Key_Escape) {
			if (Mode != edit_mode::none) {
				cancel_edit();
				aEvent->accept();
				return;
			}
			close();
			aEvent->accept();
			return;
		}
		QWidget::keyPressEvent(aEvent);
	}

	auto profile_management_window::profiles() const noexcept -> QListWidget* { return Profiles; }
	auto profile_management_window::name_editor() const noexcept -> QLineEdit* { return Name; }
	auto profile_management_window::create_button() const noexcept -> QPushButton* { return Create; }
	auto profile_management_window::rename_button() const noexcept -> QPushButton* { return Rename; }
	auto profile_management_window::open_button() const noexcept -> QPushButton* { return Open; }
	auto profile_management_window::save_button() const noexcept -> QPushButton* { return Save; }
	auto profile_management_window::cancel_button() const noexcept -> QPushButton* { return Cancel; }
	auto profile_management_window::try_again_button() const noexcept -> QPushButton* { return TryAgain; }
	auto profile_management_window::status_text() const -> QString { return Notice->isVisible() ? NoticeText->text() : QString{ }; }
	auto profile_management_window::editing() const noexcept -> bool { return Mode != edit_mode::none; }
}
