#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_VIEW_PICKER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_VIEW_PICKER_HPP

#include <QAbstractButton>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>

namespace bpr::qt {
	/// \brief How the review shows a file.
	enum class review_view : std::uint8_t {
		two_panes,
		one_pane,
		/// Author state only: the worktree's file, editable, no diff.
		edit,
	};

	/// \brief The review bar's view picker (owner, 2026-11-12): a segmented
	/// pick of two icons in a review and three in author state.
	/// \details The slot is three segments wide either way, so the third
	/// arrives in room already kept and nothing beside it moves. The picked
	/// segment is raised and neutral, never the accent: blue stays for where
	/// you are in the window. Each segment is a button with its own accessible
	/// name and tooltip; the picker decides nothing and reports a pick.
	class view_picker final : public QWidget
	{
	public:
		explicit view_picker(QWidget* aParent = nullptr);

		void               set_view(review_view aView);
		[[nodiscard]] auto view() const noexcept -> review_view;
		/// \brief Shows the third segment (author state).
		void               set_author(bool aAuthor);
		[[nodiscard]] auto author() const noexcept -> bool;
		/// \brief Dims Edit with the reason as its tooltip; empty enables it.
		void               set_edit_refusal(const QString& aReason);
		[[nodiscard]] auto segment(review_view aView) const -> QAbstractButton*;
		void               set_picked(std::function<void(review_view)> aPicked);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void place();
		void restyle();

		std::array<QAbstractButton*, 3>  Segments{ };
		review_view                      View   = review_view::two_panes;
		bool                             Author = false;
		std::function<void(review_view)> Picked;
	};
}

#endif
