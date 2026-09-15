#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_ROW_DELEGATE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_ROW_DELEGATE_HPP

#include "bpr/qt/review_landing_model.hpp"

#include <QColor>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QtTypes>

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
		};
	} // namespace home_row_role

	/// \brief Home's three state colours at text strength.
	struct home_state_colours
	{
		QColor NeedsMe;
		QColor WaitingOnOthers;
		QColor Following;

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

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			override;
		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			-> QSize override;

		/// \brief Where a row's action sits, in the option's coordinates; empty for none.
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
		bool               Marks = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_ROW_DELEGATE_HPP
