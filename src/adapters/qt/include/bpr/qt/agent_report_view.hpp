#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REPORT_VIEW_HPP

#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QListWidget;
class QScrollArea;

namespace bpr::qt {
	class markdown_body_view;

	/// \brief RFC-BPR-0128 ([[BPR-2166]], owner Q8: "file tab is more
	/// flexible"). An agent's report as a file tab shows it: its outline at
	/// the left, the report beside it. A heading in the outline scrolls the
	/// report to it; a code reference in the report goes where the window's
	/// links go. It reads only what it is given.
	class agent_report_view final : public QWidget
	{
	public:
		explicit agent_report_view(QWidget* aParent = nullptr);

		/// \brief The report: its title and its Markdown, as the agent wrote it
		/// (references already links).
		void present(const QString& aTitle, const QString& aMarkdown);
		/// \brief Where a link goes (a code reference, a web address).
		void set_link_dispatch(std::function<void(const QString&)> aDispatch);
		/// \brief Lets the window dress the body as the Agent tab's answers are.
		void set_dresser(std::function<void(markdown_body_view&)> aDresser);
		/// \brief Scrolls to the outline's `aIndex`th heading.
		void go_to_heading(int aIndex);

		[[nodiscard]] auto outline() const noexcept -> QListWidget* { return Outline; }
		[[nodiscard]] auto body() const noexcept -> markdown_body_view* { return Body; }
		[[nodiscard]] auto title() const -> QString;

	private:
		QLabel*                                  Title   = nullptr;
		QListWidget*                             Outline = nullptr;
		QScrollArea*                             Scroll  = nullptr;
		markdown_body_view*                      Body    = nullptr;
		std::function<void(const QString&)>      LinkDispatch;
		std::function<void(markdown_body_view&)> Dresser;
	};
} // namespace bpr::qt

#endif
