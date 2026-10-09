#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_ROW_DELEGATE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_ROW_DELEGATE_HPP

#include "bpr/qt/review_landing_model.hpp"

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QtTypes>

#include <cstdint>

class QPainter;

namespace bpr::qt {
	class appearance_profile;

	/// \brief [[BPR-0776]]. The roles every Home row answers, whichever model it
	/// comes from.
	///
	/// \details The queue model and the recently viewed items both answer these,
	/// so one delegate draws both. An empty string is a fact that was not
	/// projected, and nothing is painted for it.
	namespace home_row_role {
		enum : int {
			/// "owner/name#123", literal.
			identity = Qt::UserRole + 64,
			/// The literal remote title; may be empty.
			title,
			/// The sentence that says why, without the reason word.
			why,
			/// One word in the state colour; empty for a row without a state.
			reason_word,
			/// `int(home_group)`, or -1 for a row without a state.
			state,
			/// When the row was last viewed; empty where nothing was projected.
			time,
			/// My reading progress; empty where nothing was projected.
			progress,
			/// The account chip ([[BPR-0783]]); empty until then.
			account_chip,
			/// The row action's label ("Resume"), or empty for none.
			row_action,
			/// [[BPR-1338]]. The name of the row's icon action ("Open on
			/// GitHub"), or empty for none. Drawn as an icon-only button
			/// where a row has no labelled action.
			icon_action,
		};
	} // namespace home_row_role

	/// \brief Home's three state colours at text strength.
	struct home_state_colours
	{
		QColor NeedsMe;
		QColor WaitingOnOthers;
		QColor Following;
		/// [[BPR-1357]]. The ink a row says it could not be opened in: the
		/// theme's defect colour, which every variant holds at text strength.
		QColor Failure;

		/// \brief The colour of one of Home's groups.
		[[nodiscard]] auto for_group(home_group aGroup) const -> QColor;
	};

	/// \brief Reads Home's state colours for the variant a profile resolved.
	[[nodiscard]] auto home_state_colours_for(const appearance_profile& aProfile)
		-> home_state_colours;

	/// \brief The strengths Home mixes a state colour at, over the palette's ground.
	namespace home_strength {
		/// A panel header's tint.
		inline constexpr qreal header = 0.18;
		/// The rule under an expanded header.
		inline constexpr qreal header_rule = 0.35;
		/// The rail down a state panel, and a band's mark.
		inline constexpr qreal rail = 0.6;
		/// A neutral header: Mid over Base.
		inline constexpr qreal neutral_header = 0.35;
	} // namespace home_strength

	/// \brief Mixes one colour over a ground at the given strength, channel by channel.
	[[nodiscard]] auto mix_colour(const QColor& aTone, const QColor& aGround, qreal aStrength)
		-> QColor;

	/// \brief [[BPR-0783]]. The ink an account chip or an avatar tile is drawn in.
	///
	/// \details Neutral palette roles only. On Home colour means state, so a chip
	/// drawn in a state's colour would claim a state its account does not have.
	struct home_chip_ink
	{
		QColor Label;
		QColor Initial;
		QColor Tile;
		QColor Border;
	};

	/// \brief The chip and tile ink for one palette group; a selected row keeps
	/// the style's own selected text colour.
	[[nodiscard]] auto home_chip_ink_for(
		const QPalette& aPalette, QPalette::ColorGroup aGroup, bool aSelected) -> home_chip_ink;

	/// \brief The one character an avatar tile shows for a login or an owner.
	[[nodiscard]] auto home_tile_initial(const QString& aName) -> QString;

	/// \brief [[BPR-0783]]. An account chip's text: the login, and " on host" off
	/// github.com, which is the one place a host is repeated.
	[[nodiscard]] auto home_account_chip_text(const QString& aLogin, const QString& aHost) -> QString;

	/// \brief What one chip occupies: its tile, the gap and the whole text.
	///
	/// \details Remote literal text is never elided here, so the chip asks for
	/// the width its login needs and the row's text yields instead.
	[[nodiscard]] auto home_chip_size(const QFont& aFont, const QString& aText) -> QSize;

	/// \brief Draws one account chip: a round initial tile, then its text, in a
	/// bordered pill.
	void draw_home_chip(
		QPainter& aPainter, const QRect& aRect, const QString& aText, const QFont& aFont, const home_chip_ink& aInk);

	/// \brief An avatar tile's extent for a font.
	[[nodiscard]] auto home_tile_extent(const QFont& aFont) -> int;

	/// \brief Draws one initial tile: round for an account, a small radius for an
	/// owner that is not one ([[BPR-0783]]; no avatar image is cached).
	void draw_home_tile(
		QPainter&            aPainter,
		const QRect&         aRect,
		const QString&       aInitial,
		bool                 aRound,
		const QFont&         aFont,
		const home_chip_ink& aInk);

	/// \brief [[BPR-1357]]. Where the pull request a Home row opens stands.
	enum class home_row_open_phase : std::uint8_t {
		none,
		/// Asked for; a turning mark in the line's ink and "Opening #42…".
		opening,
		/// Still unanswered after the core's wait: the step it is on, and Cancel.
		waiting,
		/// It could not be opened: the reason in the failure ink, and Retry.
		failed,
	};

	/// \brief [[BPR-1357]]. The one Home row a pull request is being opened from.
	///
	/// \details Presentation of the core's opening, by the row's identity: the
	/// row's second line gives way to `Line` behind a mark, and the action
	/// place holds `Action` ("Cancel" or "Retry") for as long as it is shown.
	/// `Frame` turns the opening mark; it is animation, and decides nothing.
	struct home_row_open
	{
		QString             Identity;
		home_row_open_phase Phase = home_row_open_phase::none;
		QString             Line;
		QString             Action;
		int                 Frame = 0;

		auto operator==(const home_row_open&) const -> bool = default;
	};

	/// \brief How many steps the opening mark turns through in one revolution.
	inline constexpr int home_open_frames = 12;

	/// \brief [[BPR-1357]]. Draws the opening mark: a faint ring and a turning arc.
	void draw_home_open_mark(QPainter& aPainter, const QRect& aRect, int aFrame, const QColor& aArc, const QColor& aRing);
	/// \brief [[BPR-1357]]. Draws the failure mark: a ring with an exclamation in it.
	void draw_home_failure_mark(QPainter& aPainter, const QRect& aRect, const QFont& aFont, const QColor& aInk);

	/// \brief [[BPR-0776]]. Draws one Home row: identity and title, the why
	/// sentence ending in the reason word, and the time and progress at the right.
	///
	/// \details One anatomy for the queue and for Recently viewed. The row keeps
	/// the style's own selection, hover and focus painting; its text wraps and is
	/// never elided; its colours and fonts come from the style option, except the
	/// reason word, which is the one place a state colour is drawn.
	class home_row_delegate final : public QStyledItemDelegate
	{
	public:
		explicit home_row_delegate(QObject* aParent = nullptr);

		void               set_state_colours(const home_state_colours& aColours);
		[[nodiscard]] auto state_colours() const noexcept -> const home_state_colours&;
		/// \brief Draws a 3-point mark at each row's left ([[BPR-0783]], State headers off).
		void set_marks(bool aMarks);
		/// \brief [[BPR-1357]]. The row a pull request is being opened from.
		void               set_open(home_row_open aOpen);
		[[nodiscard]] auto open() const noexcept -> const home_row_open&;
		/// \brief Whether a row is the one being opened from.
		[[nodiscard]] auto opens(const QModelIndex& aIndex) const -> bool;
		/// \brief Where a row's open action ("Cancel" or "Retry") sits, in the
		/// option's coordinates; empty for a row that has none.
		[[nodiscard]] auto open_action_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QRect;

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			override;
		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			-> QSize override;

		/// \brief Where a row's action sits, in the option's coordinates; empty for none.
		/// \details The labelled action's place, or the icon action's where the
		/// row has no labelled one.
		[[nodiscard]] static auto action_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex)
			-> QRect;
		/// \brief Where the reason word is painted, in the option's coordinates.
		[[nodiscard]] auto reason_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			-> QRect;
		/// \brief The laid-out lines of the row's text, first line group then second.
		[[nodiscard]] auto line_texts(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			-> QStringList;

		static constexpr int minimum_row_height = 44;
		static constexpr int hairline           = 1;
		static constexpr int inset              = 14;
		static constexpr int mark_width         = 3;
		static constexpr int vertical_padding   = 5;
		static constexpr int line_gap           = 1;
		static constexpr int column_gap         = 12;

	private:
		[[nodiscard]] auto state_colour(const QModelIndex& aIndex) const -> QColor;

		home_state_colours Colours;
		home_row_open      Open;
		bool               Marks = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_ROW_DELEGATE_HPP
