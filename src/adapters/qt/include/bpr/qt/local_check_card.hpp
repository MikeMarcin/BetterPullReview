#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_CHECK_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_CHECK_CARD_HPP

#include "bpr/core/local_draft_check.hpp"

#include <QFrame>
#include <QPalette>

#include <functional>
#include <string>

class QCheckBox;
class QLabel;
class QPushButton;

/// \file
/// \brief RFC-BPR-0120 ([[BPR-2189]]). A local draft's check, as its
/// whole-change conversation shows it: a plan card (the commands in order,
/// what each shows, the two grants, what it won't cover, Run and Discard) or
/// a report card (the outcome word, the candidate checked, Open report and,
/// on an older candidate, Run again). It presents what it is given and says
/// what was pressed; it decides nothing.
namespace bpr::qt {
	/// \brief What a card's controls ask for.
	struct local_check_card_dispatch
	{
		std::function<void()>           Run;
		std::function<void()>           Discard;
		std::function<void()>           Open;
		std::function<void()>           Again;
		std::function<void(bool, bool)> Grants;
	};

	/// \brief The draft's two grants for the chosen agent, as the toggles show them.
	struct local_check_toggles
	{
		bool Scratch = false;
		bool Builds  = false;
		/// The toggles can be changed now. A running check keeps the grants
		/// it started with.
		bool Enabled = true;
	};

	class local_check_card final : public QFrame
	{
	public:
		/// \param aCheck A plan or a report.
		/// \param aBlocked Why Run is unavailable; empty when it is available.
		/// \param aLatest The draft's latest candidate.
		local_check_card(const core::local_check& aCheck, const QString& aBlocked, std::uint64_t aLatest, const local_check_toggles& aToggles, local_check_card_dispatch aDispatch,
						 QWidget* aParent = nullptr);

		/// \brief The outcome word's colour for a report (done, open, partial).
		static auto outcome_colour(core::local_check_outcome aOutcome, const QPalette& aPalette) -> QColor;

	private:
		void                      build_plan(const core::local_check_plan& aPlan, const QString& aBlocked, const local_check_toggles& aToggles);
		void                      build_report(const core::local_check_report& aReport, std::uint64_t aLatest);
		local_check_card_dispatch Dispatch;
	};
}

#endif
