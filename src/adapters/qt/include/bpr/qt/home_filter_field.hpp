#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_FILTER_FIELD_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_FILTER_FIELD_HPP

#include <QKeySequence>
#include <QString>
#include <QWidget>

#include <functional>

class QEvent;
class QLabel;
class QLineEdit;
class QPaintEvent;

namespace bpr::qt {
	/// \brief [[BPR-1865]]. Home's filter: one quiet line under the Open bar.
	///
	/// \details The owner, 2026-11-04: "I want to be able to fuzzy filter the
	/// home screen to quickly find a PR." The Open bar names a pull request
	/// anywhere; this finds one among Home's own rows, so it is drawn as part
	/// of the lists rather than as a second bordered field: a magnifier, the
	/// field, and at its right either its key or how many rows it shows, over
	/// a hairline that darkens while it has the keyboard. It decides nothing.
	/// It reports the query as it is typed, and the keys a result list
	/// answers: Up and Down move the current result, Return opens it (with
	/// the command modifier, in a new window), and Escape clears the query
	/// and then leaves the field.
	class home_filter_field final : public QWidget
	{
	public:
		explicit home_filter_field(QWidget* aParent = nullptr);

		[[nodiscard]] auto line_edit() const noexcept -> QLineEdit*;
		[[nodiscard]] auto count_label() const noexcept -> QLabel*;
		[[nodiscard]] auto shortcut_label() const noexcept -> QLabel*;
		[[nodiscard]] auto query() const -> QString;

		/// \brief The key printed at the right while the field is empty.
		void set_shortcut(const QKeySequence& aKey);
		/// \brief How many rows the query shows; said at the right while it filters.
		void set_count(int aShown);
		/// \brief Takes the keyboard and selects what was typed, as ⌘F does.
		void focus_field();
		/// \brief Takes the keyboard from a list: a character typed there is
		/// added to the query; an empty text ("/") only moves the keyboard.
		void take_typing(const QString& aText);
		/// \brief Empties the query, which reports it.
		void clear_query();

		void set_query_observer(std::function<void(const QString&)> aObserver);
		void set_step_dispatch(std::function<bool(int aStep)> aDispatch);
		void set_open_dispatch(std::function<void(bool aNewWindow)> aDispatch);
		/// \brief Escape on an empty field: the keyboard goes back to the list.
		void set_leave_dispatch(std::function<void()> aDispatch);

		static constexpr int glyph_extent = 14;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void update_trailing();

		QLineEdit*                          Edit     = nullptr;
		QLabel*                             Count    = nullptr;
		QLabel*                             Shortcut = nullptr;
		int                                 Shown    = 0;
		std::function<void(const QString&)> QueryObserver;
		std::function<bool(int)>            StepDispatch;
		std::function<void(bool)>           OpenDispatch;
		std::function<void()>               LeaveDispatch;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_FILTER_FIELD_HPP
