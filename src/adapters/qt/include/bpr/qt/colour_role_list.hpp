#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COLOUR_ROLE_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COLOUR_ROLE_LIST_HPP

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_role_display.hpp"

#include <QColor>
#include <QFont>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QAbstractButton;
class QLabel;
class QLineEdit;
class QResizeEvent;
class QScrollArea;
class QShowEvent;

namespace bpr::qt {
	/// \brief What one colour-picking session is given, and what it reports back.
	///
	/// \details `Changed` is the reason this is a request record rather than an
	/// initial colour and a return value: RFC-BPR-0034 asks for the change to
	/// preview in the review window *as the picker moves*, which a picker that only
	/// answered at the end could not do. `Title` is the role's name in words, so the
	/// picker says which colour is being changed.
	struct colour_pick_request
	{
		QString                            Title;
		QColor                             Initial;
		std::function<void(const QColor&)> Changed;
		QWidget*                           Parent = nullptr;
	};

	/// \brief Opens a colour picker and answers the colour that was accepted.
	///
	/// \details No value means the reviewer cancelled, and the list then reports the
	/// colour the role had before the session so that nothing is left previewed by
	/// a picker nobody accepted.
	///
	/// This is a settable seam for the same reason the panel's intent dispatch is
	/// one: the widget must perform no effect of its own beyond opening the
	/// platform picker, and a modal dialog no course could drive would make C5
	/// unverifiable. Production installs `platform_colour_picker`; a course installs
	/// a recording stub.
	using colour_picker
		= std::function<std::optional<QColor>(const colour_pick_request&)>;

	/// \brief Returns the platform colour picker, which is what a reviewer gets.
	///
	/// \details The operating system's own panel, opened with an alpha channel
	/// because a semantic colour carries one, and wired so every colour it moves
	/// through reaches `colour_pick_request::Changed`.
	[[nodiscard]] auto platform_colour_picker() -> colour_picker;

	/// \brief Reports that one role should now show one colour.
	///
	/// \details The list decides nothing: it reports the role and the colour, and
	/// whoever owns the palette folds it in and previews it. That is why a cancelled
	/// pick is reported too, as the colour the role started from.
	using colour_edit_dispatch
		= std::function<void(core::appearance_role, core::appearance_color)>;

	/// \brief Records directly observable list activity.
	struct colour_role_list_statistics
	{
		std::uint64_t PickersOpened = 0U;
		/// Colours reported while a picker was open, which is what makes the
		/// preview live.
		std::uint64_t LiveChanges    = 0U;
		std::uint64_t AcceptedPicks  = 0U;
		std::uint64_t CancelledPicks = 0U;
		/// Rows the last filter left listed; zero until the rows are built.
		std::size_t RowsListed = 0U;
		/// Whether the rows have been built at all.
		bool RowsBuilt = false;

		auto operator<=>(const colour_role_list_statistics&) const = default;
	};

	/// \brief RFC-BPR-0034's Colours list: 60 offered roles, grouped, filtered,
	/// picked.
	///
	/// \details [[BPR-0790]]. Every role of the core's closed vocabulary that the
	/// card offers, in the five groups the accepted design names, each in the
	/// core's own order, with sticky group headers carrying the count of the roles each one
	/// lists. A row is a swatch, the role's name in words, and its value in the
	/// code font; an edited row shows its new value in ink with the old one beside
	/// it. The rows scroll inside the list, so the page never has to widen or
	/// lengthen for them.
	///
	/// The words and the grouping are `appearance_role_display`'s, which derives
	/// them from the core enumeration. A `core::stable_name` is never shown: it is
	/// the durable exchange identifier, and the page this replaces put it in a combo
	/// box.
	///
	/// The rows are built when the list is first shown or first asked about, not
	/// when it is constructed. Sixty rows of four widgets each is the largest
	/// fixed structure in the application's chrome, and a reviewer who never opens
	/// Appearance should not be carrying it: the workbench's own bound on its
	/// retained widget tree is what makes that concrete. Everything supplied before
	/// the build — the palettes, the code font, the filter text — is kept and
	/// applied when the rows appear.
	///
	/// Presentation only. The list applies no colour, reads no store and holds no
	/// palette authority: it shows the two palettes it is given and reports the
	/// role and colour of every edit.
	class colour_role_list final : public QWidget
	{
	public:
		explicit colour_role_list(QWidget* aParent = nullptr);

		/// \brief Shows the colours of the theme on screen, as last adopted.
		void set_theme_colours(const core::appearance_palette& aColours);
		/// \brief Shows the colours currently previewed, or none.
		///
		/// \details A role whose previewed colour differs from the adopted one is an
		/// edited row, which is the only thing that makes a row read as edited.
		void set_previewed_colours(std::optional<core::appearance_palette> aColours);
		/// \brief Adopts the code font the accepted typography asks for.
		///
		/// \details The design sets each value in the code font, so a hex value
		/// lines up the way it does in a diff. Supplied rather than chosen: the
		/// composition owns the font.
		void set_value_font(QFont aFont);
		void set_picker(colour_picker aPicker);
		void set_dispatch(colour_edit_dispatch aDispatch);
		/// \brief Lists only the roles whose words contain the supplied text.
		void apply_filter(const QString& aText);

		[[nodiscard]] auto filter_field() const noexcept -> QLineEdit*;
		/// \brief The well the rows scroll inside.
		[[nodiscard]] auto well() const noexcept -> QScrollArea*;
		/// \brief The pinned header, shown when its group's own header scrolls off.
		[[nodiscard]] auto sticky_header() const noexcept -> QWidget*;
		[[nodiscard]] auto sticky_heading_text() const -> QString;
		/// Asking about a row builds the rows, which is why these are not const. A
		/// role the card does not offer (`appearance_role_offered`) has no row, and
		/// these answer null for it.
		[[nodiscard]] auto row_of(core::appearance_role aRole) -> QWidget*;
		[[nodiscard]] auto swatch_of(core::appearance_role aRole) -> QAbstractButton*;
		[[nodiscard]] auto name_label_of(core::appearance_role aRole) -> QLabel*;
		[[nodiscard]] auto value_label_of(core::appearance_role aRole) -> QLabel*;
		/// \brief The quiet "was #153622" beside an edited row's new value.
		[[nodiscard]] auto previous_value_label_of(core::appearance_role aRole)
			-> QLabel*;
		[[nodiscard]] auto group_header_of(appearance_role_group aGroup) -> QWidget*;
		[[nodiscard]] auto group_heading_text(appearance_role_group aGroup) -> QString;
		/// \brief The count the group's header carries: the roles it lists now.
		[[nodiscard]] auto group_count_text(appearance_role_group aGroup) -> QString;
		[[nodiscard]] auto listed_roles() -> std::vector<core::appearance_role>;
		[[nodiscard]] auto value_text(core::appearance_role aRole) -> QString;
		/// \brief Whether one role's previewed colour differs from the adopted one.
		///
		/// \details Answered from the two palettes rather than from the rows, so it
		/// is true of a list nobody has opened yet.
		[[nodiscard]] auto edited(core::appearance_role aRole) const noexcept -> bool;
		[[nodiscard]] auto edited_roles() const noexcept -> std::size_t;
		[[nodiscard]] auto statistics() const noexcept -> colour_role_list_statistics;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;

	private:
		/// \brief One role's row: the swatch, the words, the value, the old value.
		struct row
		{
			QWidget*         Frame    = nullptr;
			QAbstractButton* Swatch   = nullptr;
			QLabel*          Name     = nullptr;
			QLabel*          Value    = nullptr;
			QLabel*          Previous = nullptr;
		};

		/// \brief One group's header: its heading and the count of what it lists.
		struct group_header
		{
			QWidget* Frame   = nullptr;
			QLabel*  Heading = nullptr;
			QLabel*  Count   = nullptr;
		};

		/// \brief Builds the five group headers and the 60 offered rows, once.
		void ensure_rows();
		void refresh_row(core::appearance_role aRole);
		void refresh_rows();
		void refresh_counts();
		void refresh_sticky_header();
		void apply_value_font();
		void open_picker(core::appearance_role aRole);
		void report(core::appearance_role aRole, const QColor& aColour);
		/// \brief The colour a role shows now: its previewed one, else its adopted one.
		[[nodiscard]] auto colour_of(core::appearance_role aRole) const
			-> core::appearance_color;

		QLineEdit*   Filter = nullptr;
		QScrollArea* Well   = nullptr;
		QWidget*     Rows   = nullptr;

		std::array<row, core::appearance_role_count>          RoleRows{ };
		std::array<group_header, appearance_role_group_count> GroupHeaders{ };
		group_header                                          Sticky;
		core::appearance_palette                              ThemeColours{ };
		std::optional<core::appearance_palette>               PreviewedColours;
		QFont                                                 ValueFont;
		bool                                                  ValueFontSupplied = false;
		bool                                                  Built             = false;
		colour_picker                                         Picker;
		colour_edit_dispatch                                  Dispatch;
		colour_role_list_statistics                           Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COLOUR_ROLE_LIST_HPP
