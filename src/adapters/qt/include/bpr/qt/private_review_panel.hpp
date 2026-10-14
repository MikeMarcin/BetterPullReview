#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_REVIEW_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_REVIEW_PANEL_HPP
#include "bpr/application/private_review_projection.hpp"
#include <QAbstractListModel>
#include <QPointer>
#include <QWidget>
class QDialog;
class QLabel;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QComboBox;
namespace bpr::qt {
	class private_thread_model final : public QAbstractListModel
	{
	public:
		using QAbstractListModel::QAbstractListModel;
		auto rowCount(const QModelIndex& = { }) const -> int override;
		auto data(const QModelIndex&, int = Qt::DisplayRole) const -> QVariant override;
		void present(const std::vector<application::private_thread_row>&);
		auto rows() const -> const std::vector<application::private_thread_row>& { return Rows; }

	private:
		std::vector<application::private_thread_row> Rows;
	};
	class private_turn_model final : public QAbstractListModel
	{
	public:
		using QAbstractListModel::QAbstractListModel;
		auto rowCount(const QModelIndex& = { }) const -> int override;
		auto data(const QModelIndex&, int = Qt::DisplayRole) const -> QVariant override;
		void present(const std::vector<application::private_turn_row>&);
		auto rows() const -> const std::vector<application::private_turn_row>& { return Rows; }

	private:
		std::vector<application::private_turn_row> Rows;
	};
	/// One bounded page, stable IDs, explicit history selection, and semantic gestures only.
	class private_review_panel final : public QWidget
	{
	public:
		explicit private_review_panel(QWidget* = nullptr);
		void set_dispatch(application::private_conversation_dispatch);
		void present(const application::private_conversation_projection&);
		void present_body(const core::private_source_reference&, std::string_view, bool);
		void present_provenance(const core::agent_run_provenance&);
		void present_finding(std::string_view);
		void show_status(const QString&);
		/// Inspect retained text without changing its original authority, including earlier heads.
		void show_saved_comments();
		auto selected_sources() const -> const std::vector<core::private_source_selection>& { return Sources; }
		auto thread_model() -> private_thread_model* { return Threads; }
		auto turn_model() -> private_turn_model* { return Turns; }

	private:
		void                                         emit_action(application::private_conversation_action);
		void                                         select_turn();
		void                                         present_saved_comments();
		void                                         show_saved_comment();
		QPointer<QDialog>                            SavedComments;
		void                                         select_source();
		application::private_conversation_dispatch   Dispatch;
		application::private_conversation_projection Shown;
		std::vector<core::private_source_selection>  Sources;
		private_thread_model*                        Threads;
		private_turn_model*                          Turns;
		QListView*                                   ThreadList;
		QListView*                                   TurnList;
		QPlainTextEdit*                              Body;
		QPlainTextEdit*                              Composer;
		QPlainTextEdit*                              Provenance;
		QLabel*                                      Status;
		QLabel*                                      Selection;
		QCheckBox*                                   Historical;
		QPushButton*                                 MoreThreads;
		QPushButton*                                 MoreTurns;
		QPushButton*                                 MoreBody;
		QComboBox*                                   Findings;
		bool                                         Applying = false;
	};
}
#endif
