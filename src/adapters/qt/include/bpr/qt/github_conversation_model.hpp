#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_MODEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_MODEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/diff_model.hpp"

#include <QAbstractItemModel>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::qt {
	/// \brief Converts only the two valid projected GitHub diff-side values.
	[[nodiscard]] auto github_conversation_source_side(int aProjectedSide) noexcept
		-> std::optional<core::diff_side>;

	/// \brief Describes the github conversation model statistics record.
	struct github_conversation_model_statistics
	{
		std::size_t RowsInserted = 0;
		std::size_t RowsRemoved  = 0;
		std::size_t RowsMoved    = 0;
		std::size_t RowsChanged  = 0;
		std::size_t ModelResets  = 0;
	};

	/// \brief Provides the github conversation model contract.
	class github_conversation_model final : public QAbstractItemModel
	{
	public:
		enum role : int {
			identity_role = Qt::UserRole + 1,
			parent_identity_role,
			kind_role,
			author_login_role,
			body_role,
			path_role,
			line_role,
			original_line_role,
			diff_side_role,
			anchor_availability_role,
			review_state_role,
			resolved_role,
			outdated_role,
			deleted_role,
			/// [[BPR-1083]]. The body's recognized label as `core::feedback_label`,
			/// or no value for a plain, unknown, malformed or limited body.
			feedback_label_role,
			/// Whether that label is explicitly `(blocking)`.
			feedback_blocking_role,
			/// The mark's name, as in "issue (non-blocking, if-minor)"; empty
			/// with no mark.
			feedback_name_role,
			/// The written decorations as `feedback_mark_emphasis_code`.
			feedback_emphasis_role,
		};

		explicit github_conversation_model(QObject* aParent = nullptr);
		~github_conversation_model() override;

		github_conversation_model(const github_conversation_model&) = delete;
		auto operator=(const github_conversation_model&)
			-> github_conversation_model&                      = delete;
		github_conversation_model(github_conversation_model&&) = delete;
		auto operator=(github_conversation_model&&)
			-> github_conversation_model& = delete;

		[[nodiscard]] auto index(
			int                aRow,
			int                aColumn,
			const QModelIndex& aParent = { }) const -> QModelIndex override;
		[[nodiscard]] auto parent(const QModelIndex& aChild) const
			-> QModelIndex override;
		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const
			-> int override;
		[[nodiscard]] auto columnCount(const QModelIndex& aParent = { }) const
			-> int override;
		[[nodiscard]] auto data(const QModelIndex& aModelIndex, int aRequestedRole) const
			-> QVariant override;
		[[nodiscard]] auto headerData(
			int             aSection,
			Qt::Orientation aOrientation,
			int             aRequestedRole) const -> QVariant override;
		[[nodiscard]] auto flags(const QModelIndex& aModelIndex) const
			-> Qt::ItemFlags override;
		[[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

		void apply(
			const std::vector<application::github_conversation_projection>&
				aConversations);
		/// \brief Applies a Qt-local placement refusal to one visible node.
		void set_anchor_availability(
			const QModelIndex&                                   aIndex,
			application::github_conversation_anchor_availability aAvailability);
		void clear();

		[[nodiscard]] auto index_for_id(std::string_view aIdentity) const
			-> QModelIndex;
		[[nodiscard]] auto id_for_index(const QModelIndex& aModelIndex) const
			-> std::string;
		/// \brief Returns the host-validated exact comparison anchor, when one exists.
		[[nodiscard]] auto semantic_anchor(const QModelIndex& aIndex) const -> std::optional<core::source_line_ref>;
		[[nodiscard]] auto statistics() const noexcept
			-> github_conversation_model_statistics;

	private:
		struct node;

		[[nodiscard]] auto        index_for_node(const node* aValue) const -> QModelIndex;
		[[nodiscard]] static auto find_node(
			const std::vector<std::unique_ptr<node>>& aValues,
			std::string_view                          aIdentity) -> const node*;
		static void update_rows(
			std::vector<std::unique_ptr<node>>& aValues,
			std::size_t                         aFirst) noexcept;

		std::vector<std::unique_ptr<node>>   Roots;
		github_conversation_model_statistics Statistics;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_MODEL_HPP
