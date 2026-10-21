#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMMIT_FILTER_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMMIT_FILTER_VIEW_HPP

#include "bpr/application/commit_filter_projection.hpp"

#include <QAbstractListModel>
#include <QListView>
#include <QStyledItemDelegate>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QHBoxLayout;
class QLabel;
class QMenu;
class QResizeEvent;
class QShowEvent;
class QToolButton;

/// \file
/// \brief RFC-BPR-0089 M1 and M2. The Status drawer's Commits tab and the filter chip.
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
	/// \brief "2 of 3 commits shown", "File filter on" with only a file
	/// filter, or "3 commits" with no filter.
	[[nodiscard]] auto commit_filter_summary_text(const application::commit_filter_projection& aProjection) -> QString;
	/// \brief "12 of 340 files, filtered by commits", with how many file
	/// filters hide and how many generated files are set aside; empty while
	/// the file set is the comparison's.
	[[nodiscard]] auto commit_filter_files_text(const application::commit_filter_projection& aProjection) -> QString;
	/// \brief The hidden commits a force push removed, named.
	[[nodiscard]] auto commit_filter_gone_text(const application::commit_filter_projection& aProjection) -> QString;
	/// \brief RFC-BPR-0089 M3. What a rematch offer says: which commit carries
	/// the gone one's change, or only its headline and author (a guess).
	[[nodiscard]] auto commit_filter_rematch_text(const application::commit_filter_rematch_row& aOffer) -> QString;
	/// \brief RFC-BPR-0089 M3. The offer's button: "Hide 9ab12cd".
	[[nodiscard]] auto commit_filter_rematch_action_text(const application::commit_filter_rematch_row& aOffer) -> QString;

	using rematch_dispatch = std::function<void(const application::commit_filter_rematch_gesture&)>;

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
			/// RFC-BPR-0089 M2. True when the commit is labelled bulk.
			bulk_role,
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
		using commits_dispatch = std::function<void(const std::vector<std::string>&)>;

		explicit commit_filter_tab(QWidget* aParent = nullptr);

		void present(const application::commit_filter_projection& aProjection);
		/// \brief Context-menu and Clear all filters gestures.
		void set_gesture_dispatch(gesture_dispatch aDispatch);
		/// \brief The tab first became visible: the host may read the trees.
		void set_shown_dispatch(std::function<void()> aDispatch);
		/// \brief Open on GitHub for one commit.
		void set_open_dispatch(commit_dispatch aDispatch);
		/// \brief RFC-BPR-0089 M2. "Hide files these commits changed" on the
		/// selected commits' full IDs.
		void set_file_filter_dispatch(commits_dispatch aDispatch);
		/// \brief RFC-BPR-0089 M3. A rematch offer the reviewer chose.
		void set_rematch_dispatch(rematch_dispatch aDispatch);
		/// \brief The offers' buttons, one per offer, in the projection's order.
		[[nodiscard]] auto rematch_buttons() const -> std::vector<QToolButton*>;

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
		commits_dispatch                                     FileFilter;
		rematch_dispatch                                     Rematch;
		QWidget*                                             Rematches = nullptr;
		std::vector<application::commit_filter_rematch_row>  ShownOffers;
		std::vector<QToolButton*>                            OfferButtons;
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
		/// \brief RFC-BPR-0089 M3. The chip offers the first rematch; the
		/// Commits tab lists every one.
		void set_rematch_dispatch(rematch_dispatch aDispatch);

		[[nodiscard]] auto summary() const noexcept -> QLabel*;
		[[nodiscard]] auto detail() const noexcept -> QLabel*;
		[[nodiscard]] auto clear_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto commits_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto rematch_button() const noexcept -> QToolButton*;

	private:
		QLabel*                                               Summary = nullptr;
		QLabel*                                               Detail  = nullptr;
		QToolButton*                                          Clear   = nullptr;
		QToolButton*                                          Commits = nullptr;
		QToolButton*                                          Rematch = nullptr;
		std::optional<application::commit_filter_rematch_row> Offer;
		std::function<void()>                                 ClearDispatch;
		std::function<void()>                                 CommitsDispatch;
		rematch_dispatch                                      RematchDispatch;
	};

	/// \brief RFC-BPR-0089 M3 (owner, Q2). The strip over a split file's
	/// diff: one button per shown commit's exact diff of the file, in chain
	/// order, each saying how far it is read, with the hidden commits that
	/// also changed the file named between them.
	///
	/// \details Hidden for any other diff. Choosing a button asks for that
	/// diff by its position; which one is on screen is the host's to say.
	class split_diff_strip final : public QWidget
	{
	public:
		explicit split_diff_strip(QWidget* aParent = nullptr);

		void present(const std::optional<application::split_diff_projection>& aProjection);
		void set_dispatch(std::function<void(std::size_t)> aDispatch);

		[[nodiscard]] auto lead() const noexcept -> QLabel*;
		[[nodiscard]] auto parts() const noexcept -> const std::vector<QToolButton*>&;
		[[nodiscard]] auto hidden() const noexcept -> const std::vector<QLabel*>&;
		[[nodiscard]] auto projection() const noexcept -> const std::optional<application::split_diff_projection>&;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief The fullest wording that fits the width: every headline,
		/// then elided headlines and the hidden commits by ID, then IDs only.
		void fit();

		QLabel*                                           Lead = nullptr;
		QHBoxLayout*                                      Row  = nullptr;
		std::vector<QToolButton*>                         Parts;
		std::vector<QLabel*>                              Hidden;
		std::optional<application::split_diff_projection> Projection;
		std::function<void(std::size_t)>                  Dispatch;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMMIT_FILTER_VIEW_HPP
