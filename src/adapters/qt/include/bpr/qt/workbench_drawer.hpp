#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_DRAWER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_DRAWER_HPP

#include "bpr/core/workbench_interaction.hpp"

#include <QAbstractListModel>
#include <QString>
#include <QWidget>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QLabel;
class QListView;
class QResizeEvent;

namespace bpr::qt {
	/// \brief Describes one drawer row by its stable presentation identity.
	///
	/// \details `Identity` is the only member the model reconciles against. It is
	/// supplied by whatever bounded observation fills the drawer and must stay
	/// stable for the lifetime of the thing it names, because every insert, move,
	/// removal, and data change is decided by comparing it.
	struct workbench_drawer_row
	{
		QString Identity;
		QString Title;
		QString Detail;

		// QString orders only through a locale-aware comparison, so this record
		// offers equality alone; reconciliation never needs an order.
		auto operator==(const workbench_drawer_row&) const -> bool = default;
	};

	/// \brief Records directly observable drawer model activity.
	///
	/// \details `ModelResets` exists to be observed as zero. A drawer whose
	/// bounded content changed must reach the view through incremental signals.
	struct workbench_drawer_model_statistics
	{
		std::uint64_t RowsInserted = 0U;
		std::uint64_t RowsRemoved  = 0U;
		std::uint64_t RowsMoved    = 0U;
		std::uint64_t RowsChanged  = 0U;
		std::uint64_t ModelResets  = 0U;

		auto operator<=>(const workbench_drawer_model_statistics&) const = default;
	};

	/// \brief Presents drawer contents as a stable-identity incremental model.
	///
	/// \details apply() reconciles the retained rows with the supplied order by
	/// identity and emits only insert, remove, move, and data-change signals. It
	/// never resets the model, so a view keeps its selection, expansion, and
	/// persistent indexes across a bounded content change.
	class workbench_drawer_model final : public QAbstractListModel
	{
	public:
		enum role : int {
			identity_role = Qt::UserRole + 1,
			detail_role,
		};

		explicit workbench_drawer_model(QObject* aParent = nullptr);

		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const
			-> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRequestedRole) const
			-> QVariant override;
		[[nodiscard]] auto flags(const QModelIndex& aIndex) const
			-> Qt::ItemFlags override;
		[[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

		/// \brief Reconciles the retained rows with the supplied presentation order.
		void apply(const std::vector<workbench_drawer_row>& aRows);

		[[nodiscard]] auto index_for_identity(const QString& aIdentity) const
			-> QModelIndex;
		[[nodiscard]] auto identity_at(const QModelIndex& aIndex) const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> workbench_drawer_model_statistics;

	private:
		[[nodiscard]] auto row_for_identity(const QString& aIdentity) const
			-> std::optional<std::size_t>;
		void retire_absent(const std::vector<workbench_drawer_row>& aRows);
		void insert_or_move(std::size_t aRequestedIndex, workbench_drawer_row aRow);
		void remove(const QString& aIdentity);

		std::vector<workbench_drawer_row> Rows;
		workbench_drawer_model_statistics Statistics;
	};

	/// \brief Reports one observed drawer width out of the presentation layer.
	///
	/// \details Width is host-owned durable layout with no semantic meaning, so it
	/// leaves through this sink and reaches the host's layout persistence path.
	/// Collapsed and expanded never travel here; they are core state.
	using workbench_drawer_width_dispatch = std::function<void(core::workbench_drawer, std::uint32_t)>;

	/// \brief Presents one collapsible, resizable review drawer.
	///
	/// \details The panel is presentation only. It never decides whether it may
	/// collapse: adopt() applies a state the core already decided, and the width
	/// it observes is reported outward rather than retained as authority. A
	/// collapsed panel is hidden, which is how its width returns to the centre.
	class workbench_drawer_panel final : public QWidget
	{
	public:
		explicit workbench_drawer_panel(
			core::workbench_drawer aDrawer,
			QWidget*               aParent = nullptr);

		workbench_drawer_panel(const workbench_drawer_panel&) = delete;
		auto operator=(const workbench_drawer_panel&)
			-> workbench_drawer_panel&                                      = delete;
		workbench_drawer_panel(workbench_drawer_panel&&)                    = delete;
		auto operator=(workbench_drawer_panel&&) -> workbench_drawer_panel& = delete;

		[[nodiscard]] auto drawer() const noexcept -> core::workbench_drawer;
		[[nodiscard]] auto state() const noexcept -> core::workbench_drawer_state;
		[[nodiscard]] auto contents() const noexcept -> workbench_drawer_model*;
		[[nodiscard]] auto contents_view() const noexcept -> QListView*;

		void set_width_dispatch(workbench_drawer_width_dispatch aDispatch);

		/// \brief Adopts the drawer state the core decided.
		void adopt(core::workbench_drawer_state aState);

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		core::workbench_drawer          Drawer;
		workbench_drawer_width_dispatch WidthDispatch;
		QLabel*                         Heading  = nullptr;
		QListView*                      View     = nullptr;
		workbench_drawer_model*         Contents = nullptr;
		core::workbench_drawer_state    State    = core::workbench_drawer_state::expanded;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_DRAWER_HPP
