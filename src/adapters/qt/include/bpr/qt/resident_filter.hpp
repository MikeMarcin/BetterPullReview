#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RESIDENT_FILTER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RESIDENT_FILTER_HPP

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringView>
#include <QWidget>

namespace bpr::qt {
	/// \brief Whether resident text matches a drawer filter ([[BPR-0981]]).
	///
	/// \details RFC-BPR-0025's fast fuzzy filter over projections a drawer
	/// already holds. The needle is split on spaces and every term must appear
	/// in the text as an ordered, case-folded subsequence of its characters:
	/// "adisp" finds "adaptDispatch.cpp", and "src b" finds "src/b.cpp" but
	/// not "src/a.cpp". An empty needle matches everything. It ranks nothing
	/// and performs no effect; the trees keep their own order and the core
	/// decides nothing from it.
	[[nodiscard]] auto resident_filter_matches(QStringView aText, QStringView aNeedle) -> bool;

	/// \brief The honest empty state of a filtered drawer, with Clear filter.
	///
	/// \details Shown by the window in the tree's column when a filter leaves
	/// nothing to list. Clearing is the same gesture as emptying the filter
	/// field; it changes no selection and no open view.
	class filter_empty_notice final : public QWidget
	{
	public:
		/// \param aSubject What the drawer lists, plural: "files", "pull requests".
		/// \param aObjectPrefix The object-name prefix of the notice and its button.
		explicit filter_empty_notice(
			const QString& aSubject,
			const QString& aObjectPrefix,
			QWidget*       aParent = nullptr);

		/// \brief Names the filter that found nothing.
		void set_filter(const QString& aFilter);
		[[nodiscard]] auto message() const -> QString;
		[[nodiscard]] auto clear_button() const noexcept -> QPushButton*;

	private:
		QString      Subject;
		QLabel*      Message = nullptr;
		QPushButton* Clear   = nullptr;
	};
} // namespace bpr::qt

#endif
