#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMMIT_FILTER_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMMIT_FILTER_VIEW_HPP

#include "bpr/application/commit_filter_projection.hpp"

#include <QAbstractListModel>
#include <QListView>
#include <QStyledItemDelegate>
#include <QWidget>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QMenu;
class QShowEvent;
class QToolButton;

/// \file
/// \brief RFC-BPR-0089 M1. The Status drawer's Commits tab and the filter chip.
///
/// Presentation only: rows, states and reasons are the host's projection of
/// the core's decisions, and every gesture is dispatched as a
/// `commit_filter_gesture` naming full commit IDs. Nothing here runs Git,
/// decides a filter or builds an address.
namespace bpr::qt {
	/// \brief Why the pull request has no filter, in a sentence.
	[[nodiscard]] auto commit_filter_unavailable_text(core::commit_filter_unavailable aReason) -> QString;
	/// \brief Why the filter is paused under this comparison, in a sentence.
	[[nodiscard]] auto commit_filter_suspension_text(core::commit_filter_suspension aReason) -> QString;
	/// \brief Where the chain's trees are, or empty while ready or not needed.
	[[nodiscard]] auto commit_filter_trees_text(application::commit_filter_trees aTrees) -> QString;
	/// \brief Why a gesture was refused, in a sentence.
	[[nodiscard]] auto commit_filter_refusal_text(core::commit_filter_refusal aRefusal) -> QString;
	/// \brief "2 of 3 commits shown", or "3 commits" with no filter.
	[[nodiscard]] auto commit_filter_summary_text(const application::commit_filter_projection& aProjection) -> QString;
	/// \brief "12 of 340 files, filtered by commits", or empty while the file
	/// set is not filtered.
	[[nodiscard]] auto commit_filter_files_text(const application::commit_filter_projection& aProjection) -> QString;
	/// \brief The hidden commits a force push removed, named.
	[[nodiscard]] auto commit_filter_gone_text(const application::commit_filter_projection& aProjection) -> QString;

	/// \brief The listed commits, in GitHub's order, reconciled by commit ID.
	///
	/// \details A changed projection inserts, removes, moves and updates rows;
	/// the model never resets, so selection and the current row survive a
	/// refresh while their commits are still listed.
	class commit_filter_list_model final : public QAbstractListModel
	{
	public:
		enum role : int {
			commit_role = Qt::UserRole + 1,
			short_commit_role,
			author_role,
			/// -1 unknown, else the file count.
			files_role,
			/// `core::filter_commit_role`.
			commit_kind_role,
			/// `core::filter_commit_state`.
			state_role,
			merge_role,
		};

		explicit commit_filter_list_model(QObject* aParent = nullptr);

		void               apply(const std::vector<application::commit_filter_row>& aRows);
		[[nodiscard]] auto row_at(int aRow) const -> const application::commit_filter_row*;
		[[nodiscard]] auto row_of(const std::string& aCommit) const -> int;
		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRole) const -> QVariant override;
		/// \brief Structural signals since construction, for tests.
		[[nodiscard]] auto resets() const noexcept -> int;

	private:
		std::vector<application::commit_filter_row> Rows;
		int                                         Resets = 0;
	};

	/// \brief One commit as two lines: the headline, wrapped and never elided,
	/// over the short ID in the code font, the author and the file count; a
	/// tag at the right says merge, hidden or not in this comparison.
	class commit_filter_delegate final : public QStyledItemDelegate
	{
	public:
		explicit commit_filter_delegate(QObject* aParent = nullptr);

		void               paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override;
		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override;
		/// \brief The tag a row shows at its right, or empty.
		[[nodiscard]] static auto tag(const QModelIndex& aIndex) -> QString;
	};

	/// \brief The Status drawer's Commits tab.
	class commit_filter_tab final : public QWidget
	{
	public:
		using gesture_dispatch = std::function<void(const application::commit_filter_gesture&)>;
		using commit_dispatch  = std::function<void(const std::string&)>;

		explicit commit_filter_tab(QWidget* aParent = nullptr);

		void present(const application::commit_filter_projection& aProjection);
		/// \brief Context-menu and Clear all filters gestures.
		void set_gesture_dispatch(gesture_dispatch aDispatch);
		/// \brief The tab first became visible: the host may read the trees.
		void set_shown_dispatch(std::function<void()> aDispatch);
		/// \brief Open on GitHub for one commit.
		void set_open_dispatch(commit_dispatch aDispatch);

		/// \brief The context menu for the current selection; the caller owns it.
		[[nodiscard]] auto make_menu(QWidget* aParent) -> QMenu*;
		/// \brief The full IDs of the selected rows, in list order.
		[[nodiscard]] auto selected_commits() const -> std::vector<std::string>;

		[[nodiscard]] auto list() const noexcept -> QListView*;
		[[nodiscard]] auto model() const noexcept -> commit_filter_list_model*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		[[nodiscard]] auto clear_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto projection() const noexcept -> const std::optional<application::commit_filter_projection>&;

	protected:
		void showEvent(QShowEvent* aEvent) override;

	private:
		void dispatch(core::commit_filter_operation_kind aKind, std::vector<std::string> aCommits);

		commit_filter_list_model*                            Model  = nullptr;
		QListView*                                           List   = nullptr;
		QLabel*                                              Status = nullptr;
		QLabel*                                              Hint   = nullptr;
		QToolButton*                                         Clear  = nullptr;
		gesture_dispatch                                     Gestures;
		std::function<void()>                                Shown;
		commit_dispatch                                      Open;
		std::optional<application::commit_filter_projection> Projection;
		bool                                                 Announced = false;
	};

	/// \brief The filter chip: what the filter shows and Clear all filters,
	/// above the Overview's files and in the Files drawer.
	///
	/// \details Hidden while no filter applies, is paused, or names a gone
	/// commit (`commit_filter_chip_shown`). In Review it also offers Commits,
	/// which opens the Commits tab, so a filter is never active out of sight.
	class commit_filter_chip final : public QWidget
	{
	public:
		commit_filter_chip(bool aOffersCommits, QWidget* aParent = nullptr);

		void present(const application::commit_filter_projection& aProjection);
		void set_clear_dispatch(std::function<void()> aDispatch);
		void set_commits_dispatch(std::function<void()> aDispatch);

		[[nodiscard]] auto summary() const noexcept -> QLabel*;
		[[nodiscard]] auto detail() const noexcept -> QLabel*;
		[[nodiscard]] auto clear_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto commits_button() const noexcept -> QToolButton*;

	private:
		QLabel*               Summary = nullptr;
		QLabel*               Detail  = nullptr;
		QToolButton*          Clear   = nullptr;
		QToolButton*          Commits = nullptr;
		std::function<void()> ClearDispatch;
		std::function<void()> CommitsDispatch;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMMIT_FILTER_VIEW_HPP
