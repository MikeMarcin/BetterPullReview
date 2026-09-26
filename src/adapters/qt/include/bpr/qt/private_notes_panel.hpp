#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_NOTES_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PRIVATE_NOTES_PANEL_HPP

#include "bpr/core/diff_semantics.hpp"
#include <QString>
#include <QWidget>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QAction;
class QLabel;
class QListWidget;
class QPlainTextEdit;

namespace bpr::qt {
	/// \brief Exact file comparison offered for a new private note.
	struct private_note_target
	{
		core::file_identity          File;
		core::file_comparison_digest Comparison;
		auto                         operator<=>(const private_note_target&) const = default;
	};
	/// \brief One accepted ledger record, with host-supplied presentation labels.
	struct private_note_item
	{
		std::uint64_t                      Id       = 0U;
		std::uint64_t                      Revision = 0U;
		std::optional<private_note_target> Attachment;
		std::string                        Label;
		std::string                        Text;
		auto                               operator<=>(const private_note_item&) const = default;
	};
	struct private_notes_projection
	{
		std::uint64_t                      Scope     = 0U;
		bool                               Available = false;
		std::optional<private_note_target> CurrentFile;
		std::string                        CurrentFileLabel;
		std::vector<private_note_item>     Notes;
	};
	enum class private_note_action : std::uint8_t { discard,
													save,
													erase,
													promote };
	/// \brief One gesture retains its original review scope and durable revision.
	struct private_note_intent
	{
		std::uint64_t                      Scope    = 0U;
		private_note_action                Action   = private_note_action::save;
		std::uint64_t                      Id       = 0U;
		std::uint64_t                      Revision = 0U;
		std::optional<private_note_target> Attachment;
		std::string                        Text;
	};
	using private_note_dispatch = std::function<void(private_note_intent)>;

	/// \brief Presents private ledger values and emits gestures; owns no effects.
	class private_notes_panel final : public QWidget
	{
	public:
		explicit private_notes_panel(QWidget* aParent = nullptr);
		void               set_dispatch(private_note_dispatch aDispatch);
		void               present(private_notes_projection aProjection);
		void               set_promotion_target(bool aAvailable, const QString& aReason);
		void               acknowledge(std::uint64_t aSavedId);
		void               refuse(const QString& aReason);
		[[nodiscard]] auto notes_list() const noexcept -> QListWidget*;
		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto new_review_action() const noexcept -> QAction*;
		[[nodiscard]] auto new_file_action() const noexcept -> QAction*;
		[[nodiscard]] auto save_action() const noexcept -> QAction*;
		[[nodiscard]] auto erase_action() const noexcept -> QAction*;
		[[nodiscard]] auto promote_action() const noexcept -> QAction*;
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;

	private:
		void                               start_note(bool aFile);
		void                               select_note();
		void                               load_editor();
		void                               refresh_actions();
		void                               emit_intent(private_note_action aAction);
		[[nodiscard]] auto                 can_promote() const -> bool;
		private_note_dispatch              Dispatch;
		private_notes_projection           Shown;
		std::optional<private_note_item>   Editing;
		std::optional<private_note_target> NewTarget;
		std::uint64_t                      Selected        = 0U;
		bool                               Creating        = false;
		bool                               Dirty           = false;
		bool                               Applying        = false;
		bool                               TargetAvailable = false;
		QString                            TargetReason;
		QListWidget*                       Notes      = nullptr;
		QPlainTextEdit*                    Body       = nullptr;
		QLabel*                            Attachment = nullptr;
		QLabel*                            Status     = nullptr;
		QAction*                           NewReview  = nullptr;
		QAction*                           NewFile    = nullptr;
		QAction*                           Save       = nullptr;
		QAction*                           Erase      = nullptr;
		QAction*                           Promote    = nullptr;
		QAction*                           Discard    = nullptr;
	};
}
#endif
