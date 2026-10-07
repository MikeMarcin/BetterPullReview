#ifndef BPR_QT_CLAUDE_WRAPPER_PANEL_HPP
#define BPR_QT_CLAUDE_WRAPPER_PANEL_HPP

#include "bpr/application/claude_process_wrapper.hpp"

#include <QWidget>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

class QLabel;
class QLineEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief [[BPR-1301]]. Settings > Agents & context: the profile's Claude
	/// process wrapper, and the line saying sign-in is not BetterPullReview's.
	///
	/// \details Presentation only. Save hands the typed text to the host,
	/// which inspects the file on its effect lane before it is written;
	/// Choose... picks a file with the platform's chooser and saves it the
	/// same way. The field shows the saved path once the profile publishes it.
	///
	/// With no wrapper saved, a read-only line says which `claude` a launch
	/// would run and where it was found, or that none was found (owner,
	/// 2026-10-07). The host looks it up whenever the row is shown or the
	/// wrapper is cleared; a launch looks again for itself.
	class claude_wrapper_panel final : public QWidget
	{
	public:
		explicit claude_wrapper_panel(QWidget* aParent = nullptr);

		/// `aLocate` asks the host which `claude` would run without a wrapper;
		/// the answer comes back through `show_found`.
		void set_dispatch(std::function<void(std::string)> aSave, std::function<void()> aLocate = { });
		/// The profile's saved wrapper, empty for none.
		void present(std::string_view aSaved);
		void show_answer(const application::claude_wrapper_answer& aAnswer);
		void show_found(const std::optional<core::claude_search_candidate>& aFound);

	protected:
		void showEvent(QShowEvent* aEvent) override;

	private:
		void save(const QString& aText);
		void show_status(const QString& aText);
		void locate();

		std::function<void(std::string)> Save;
		std::function<void()>            Locate;
		std::string                      Saved;
		bool                             Presented  = false;
		QLineEdit*                       Path       = nullptr;
		QPushButton*                     Choose     = nullptr;
		QPushButton*                     SaveButton = nullptr;
		QLabel*                          Status     = nullptr;
		QLabel*                          Found      = nullptr;
	};
}

#endif
