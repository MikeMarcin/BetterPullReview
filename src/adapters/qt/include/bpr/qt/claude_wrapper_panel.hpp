#ifndef BPR_QT_CLAUDE_WRAPPER_PANEL_HPP
#define BPR_QT_CLAUDE_WRAPPER_PANEL_HPP

#include "bpr/application/claude_process_wrapper.hpp"

#include <QWidget>

#include <functional>
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
	class claude_wrapper_panel final : public QWidget
	{
	public:
		explicit claude_wrapper_panel(QWidget* aParent = nullptr);

		void set_dispatch(std::function<void(std::string)> aSave);
		/// The profile's saved wrapper, empty for none.
		void present(std::string_view aSaved);
		void show_answer(const application::claude_wrapper_answer& aAnswer);

	private:
		void save(const QString& aText);
		void show_status(const QString& aText);

		std::function<void(std::string)> Save;
		std::string                      Saved;
		bool                             Presented  = false;
		QLineEdit*                       Path       = nullptr;
		QPushButton*                     Choose     = nullptr;
		QPushButton*                     SaveButton = nullptr;
		QLabel*                          Status     = nullptr;
	};
}

#endif
