#include "bpr/qt/profile_management_window.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		auto failure_text(const application::profile_management_failure aFailure) -> QString
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
			case failure::cancelled:            return QStringLiteral("The profile operation was cancelled. Your text has been kept.");
			case failure::conflict:             return QStringLiteral("Profiles changed during this edit. Your text has been kept; review the list and try again.");
			case failure::registry_unavailable:
			case failure::invalid_registry:     return QStringLiteral("Profiles could not be loaded or saved. Your text has been kept.");
			}
			return QStringLiteral("The profile operation failed. Your text has been kept.");
		}

		auto item_profile(const QListWidgetItem* aItem) -> core::bpr_profile_id
		{
			return core::bpr_profile_id{ aItem == nullptr ? 0U : aItem->data(Qt::UserRole).toULongLong() };
		}
	}

	profile_management_window::profile_management_window(QWidget* aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("profileManagementWindow"));
		setWindowTitle(QStringLiteral("Manage Profiles"));
		setAccessibleName(QStringLiteral("Manage Profiles"));
		setMinimumSize(400, 340);
		resize(480, 420);
		Profiles = new QListWidget{ this };
		Profiles->setObjectName(QStringLiteral("managedProfiles"));
		Profiles->setAccessibleName(QStringLiteral("Profiles"));
		Profiles->setSelectionMode(QAbstractItemView::SingleSelection);
		Profiles->installEventFilter(this);
		Name = new QLineEdit{ this };
		Name->setObjectName(QStringLiteral("profileName"));
		Name->setAccessibleName(QStringLiteral("Profile name"));
		Name->setMaxLength(static_cast<int>(core::maximum_bpr_profile_label_bytes));
		Create      = new QPushButton{ QStringLiteral("Create"), this };
		Rename      = new QPushButton{ QStringLiteral("Rename"), this };
		Open        = new QPushButton{ QStringLiteral("Open"), this };
		auto* close = new QPushButton{ QStringLiteral("Close"), this };
		Create->setAccessibleName(QStringLiteral("Create profile"));
		Rename->setAccessibleName(QStringLiteral("Rename selected profile"));
		Open->setAccessibleName(QStringLiteral("Open selected profile"));
		close->setAccessibleName(QStringLiteral("Close profile management"));
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("profileManagementStatus"));
		Status->setAccessibleName(QStringLiteral("Profile management status"));
		Status->setWordWrap(true);
		Status->setTextFormat(Qt::PlainText);
		auto* form = new QFormLayout;
		form->addRow(QStringLiteral("Profile name"), Name);
		auto* actions = new QHBoxLayout;
		actions->addWidget(Create);
		actions->addWidget(Rename);
		actions->addWidget(Open);
		actions->addStretch();
		actions->addWidget(close);
		auto* layout = new QVBoxLayout{ this };
		layout->addWidget(Profiles, 1);
		layout->addLayout(form);
		layout->addWidget(Status);
		layout->addLayout(actions);
		setTabOrder(Profiles, Name);
		setTabOrder(Name, Create);
		setTabOrder(Create, Rename);
		setTabOrder(Rename, Open);
		setTabOrder(Open, close);
		connect(Profiles, &QListWidget::currentRowChanged, this, [this] { select_profile(); });
		connect(Profiles, &QListWidget::itemActivated, this, [this] { open_selected(); });
		connect(Create, &QPushButton::clicked, this, [this] { submit_edit(true); });
		connect(Rename, &QPushButton::clicked, this, [this] { submit_edit(false); });
		connect(Open, &QPushButton::clicked, this, [this] { open_selected(); });
		connect(close, &QPushButton::clicked, this, &QWidget::close);
		update_actions();
	}

	void profile_management_window::set_dispatch(application::profile_management_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void profile_management_window::adopt(const application::profile_management_projection& aProjection)
	{
		if (aProjection.Profiles.size() > core::maximum_bpr_profiles) {
			Status->setText(QStringLiteral("Profile list exceeds the supported limit."));
			Ready = false;
			update_actions();
			return;
		}
		const auto selected = selected_profile();
		const auto blocker  = QSignalBlocker{ Profiles };
		for (auto row = Profiles->count() - 1; row >= 0; --row) {
			if (std::ranges::none_of(aProjection.Profiles, [this, row](const auto& aProfile) {
					return aProfile.Profile == item_profile(Profiles->item(row));
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
				item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(profile.Profile.value()));
				Profiles->insertItem(static_cast<int>(index), item);
			} else if (Profiles->row(item) != static_cast<int>(index)) {
				Profiles->takeItem(Profiles->row(item));
				Profiles->insertItem(static_cast<int>(index), item);
			}
			item->setData(Qt::UserRole + 1, static_cast<int>(profile.Runtime));
			const auto runtime = profile.Runtime == application::profile_management_runtime::open ? QStringLiteral("Open")
				: profile.Runtime == application::profile_management_runtime::opening             ? QStringLiteral("Opening")
				: profile.Runtime == application::profile_management_runtime::closing             ? QStringLiteral("Closing")
																								  : QStringLiteral("Closed");
			item->setToolTip(runtime);
			item->setData(Qt::AccessibleDescriptionRole, runtime);
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
		if (Profiles->currentItem() == nullptr && Profiles->count() != 0) {
			Profiles->setCurrentRow(0);
			if (Name->text().isEmpty()) {
				Name->setText(Profiles->currentItem()->text());
			}
		}
		if (!aProjection.Failure && !PendingEdit && !PreserveCandidate && !Name->isModified() && Profiles->currentItem() != nullptr) {
			Name->setText(Profiles->currentItem()->text());
		}
		if (aProjection.Failure) {
			Status->setText(failure_text(aProjection.Failure->Reason));
			PendingEdit.reset();
		} else if (aProjection.AcceptedEdit && PendingEdit
				   && aProjection.AcceptedEdit->Operation == *PendingEdit
				   && (*PendingEdit == application::profile_management_operation::create
					   || aProjection.AcceptedEdit->Profile == PendingProfile)) {
			if (Name->text() == SubmittedLabel) {
				PreserveCandidate = false;
				for (auto row = 0; row < Profiles->count(); ++row) {
					if (item_profile(Profiles->item(row)) == aProjection.AcceptedEdit->Profile) {
						Profiles->setCurrentRow(row);
						Name->setText(Profiles->item(row)->text());
						break;
					}
				}
			}
			PendingEdit.reset();
			Status->setText(QStringLiteral("Profile saved."));
		} else {
			Status->setText(Closing     ? QStringLiteral("Profiles are closing.")
								: Busy  ? QStringLiteral("Saving profile changes…")
								: Ready ? QStringLiteral("Open a profile, or create or rename one.")
										: QStringLiteral("Loading profiles…"));
		}
		update_actions();
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

	void profile_management_window::select_profile()
	{
		PreserveCandidate = false;
		if (Profiles->currentItem() != nullptr) {
			Name->setText(Profiles->currentItem()->text());
		}
		update_actions();
	}

	void profile_management_window::submit_edit(const bool aCreate)
	{
		if (!Dispatch || !(aCreate ? Create : Rename)->isEnabled()) {
			return;
		}
		PreserveCandidate = true;
		PendingEdit       = aCreate ? application::profile_management_operation::create : application::profile_management_operation::rename;
		PendingProfile    = selected_profile();
		SubmittedLabel    = Name->text();
		if (aCreate) {
			Dispatch(application::create_bpr_profile_intent{ SubmittedLabel.toStdString() });
		} else {
			Dispatch(application::rename_bpr_profile_intent{ PendingProfile, SubmittedLabel.toStdString() });
		}
	}

	void profile_management_window::open_selected()
	{
		if (Dispatch && Open->isEnabled()) {
			Dispatch(application::open_bpr_profile_intent{ selected_profile() });
		}
	}

	void profile_management_window::update_actions()
	{
		Create->setEnabled(Ready && !Busy && !Closing);
		Rename->setEnabled(Ready && !Busy && !Closing && selected_profile().value() != 0U);
		Open->setEnabled(Ready && !Closing && selected_profile().value() != 0U
						 && Profiles->currentItem()->data(Qt::UserRole + 1).toInt() != static_cast<int>(application::profile_management_runtime::closing));
	}

	bool profile_management_window::eventFilter(QObject* aObject, QEvent* aEvent)
	{
		if (aObject == Profiles && aEvent->type() == QEvent::KeyPress) {
			const auto* key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				open_selected();
				return true;
			}
		}
		return QWidget::eventFilter(aObject, aEvent);
	}

	void profile_management_window::keyPressEvent(QKeyEvent* aEvent)
	{
		if (aEvent->key() == Qt::Key_Escape) {
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
	auto profile_management_window::status_text() const -> QString { return Status->text(); }
}
