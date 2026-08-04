#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_QUEUE_MODEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_QUEUE_MODEL_HPP

#include "bpr/core/review_ledger.hpp"

#include <QAbstractListModel>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace bpr::qt {
	/// \brief Records directly observable queue-model activity.
	struct review_queue_model_statistics
	{
		std::size_t RowsInserted = 0U;
		std::size_t RowsRemoved  = 0U;
		std::size_t RowsChanged  = 0U;
		std::size_t ModelResets  = 0U;
	};

	/// \brief Presents the core's ranked actionable queue.
	///
	/// \details The model holds what the core ranked and renders it. It decides
	/// no order and composes no reason of its own: the reason is a core value,
	/// and an adapter that wrote its own explanation text would be deciding
	/// policy while looking like presentation.
	class review_queue_model final : public QAbstractListModel
	{
	public:
		enum role : int {
			file_role = Qt::UserRole + 1,
			reason_role,
			marker_role,
		};

		explicit review_queue_model(QObject* aParent = nullptr);

		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const
			-> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRequestedRole) const
			-> QVariant override;
		[[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

		/// \brief Adopts one ranked queue, signalling only what moved.
		///
		/// \details A queue that changed one row emits one change rather than a
		/// reset, because a reset discards selection and scroll position and
		/// tells assistive technology that everything it knew is gone.
		void apply(std::span<const core::actionable_item> aRanked);

		void clear();

		[[nodiscard]] auto item_at(const QModelIndex& aIndex) const
			-> std::optional<core::actionable_item>;
		[[nodiscard]] auto statistics() const noexcept
			-> review_queue_model_statistics;

	private:
		std::vector<core::actionable_item> Items;
		review_queue_model_statistics      Statistics;
	};

	/// \brief Returns the human explanation for one actionability reason.
	///
	/// \details The mapping is total over the closed vocabulary, so a reason
	/// added to the core cannot reach the screen as an empty string.
	[[nodiscard]] auto explanation_for(core::actionability_reason aReason) -> QString;

	/// \brief Returns the non-colour marker for one actionability reason.
	///
	/// \details Colour is never the only signal for a review state. This is the
	/// redundant one: a short textual mark that survives a greyscale display, a
	/// screen reader, and a colour-vision deficiency.
	[[nodiscard]] auto marker_for(core::actionability_reason aReason) -> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_QUEUE_MODEL_HPP
