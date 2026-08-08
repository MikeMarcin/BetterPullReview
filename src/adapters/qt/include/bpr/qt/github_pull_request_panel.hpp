#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_PULL_REQUEST_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_PULL_REQUEST_PANEL_HPP

#include "bpr/application/github_projection.hpp"

#include <QFrame>

class QLabel;
class QPlainTextEdit;
class QToolButton;

namespace bpr::qt {
	/// \brief Presents one pull request's identity, counts and description.
	///
	/// \details Extracted from `github_window` by BPR-0191 so the workbench
	/// shell can compose the pull-request content without inheriting the second
	/// window that owned it. It is the metadata region only: no epoch control,
	/// no comparison surface, no conversation drawer. Those are separately
	/// composed, and it was duplicating them that left two windows each
	/// believing they owned the review surface.
	///
	/// Every object name and every string is unchanged from the region this
	/// replaces, so the window tests that reach these widgets by `findChild`
	/// still reach them and still read the same words. That is the point: an
	/// extraction that renamed or reworded anything would make its own new tests
	/// the only evidence it preserved behaviour, which is the one thing they
	/// cannot be.
	///
	/// The description editor is built here because it is pull-request content,
	/// but it is deliberately reachable through `body_editor()`: the composing
	/// window places it in a splitter beside the conversation drawer, which is a
	/// layout decision belonging to whoever assembles the window.
	class github_pull_request_panel final : public QFrame
	{
	public:
		explicit github_pull_request_panel(QWidget* aParent = nullptr);

		github_pull_request_panel(const github_pull_request_panel&) = delete;
		auto operator=(const github_pull_request_panel&)
			-> github_pull_request_panel&                      = delete;
		github_pull_request_panel(github_pull_request_panel&&) = delete;
		auto operator=(github_pull_request_panel&&)
			-> github_pull_request_panel& = delete;

		void adopt(
			const application::github_pull_request_projection& aPullRequest);

		/// \brief Returns every field to the wording that names no pull request.
		void clear();

		/// \brief Re-derives the emphasis fonts from the inherited font.
		void refresh_native_appearance();

		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto technical_details_button() const noexcept
			-> QToolButton*;

	private:
		QLabel*         IdentityLabel    = nullptr;
		QLabel*         TitleLabel       = nullptr;
		QLabel*         StateLabel       = nullptr;
		QLabel*         AuthorLabel      = nullptr;
		QLabel*         BranchLabel      = nullptr;
		QLabel*         OidLabel         = nullptr;
		QLabel*         CountLabel       = nullptr;
		QPlainTextEdit* BodyEditor       = nullptr;
		QToolButton*    DetailsButton    = nullptr;
		QWidget*        TechnicalDetails = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_PULL_REQUEST_PANEL_HPP
