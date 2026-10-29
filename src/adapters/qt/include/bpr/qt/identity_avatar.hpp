#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_IDENTITY_AVATAR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_IDENTITY_AVATAR_HPP

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>

#include <cstdint>

class QPainter;
class QPaintEvent;

namespace bpr::qt {
	/// \brief [[BPR-1583]]. The grounds an avatar is drawn over: the window and
	/// the base of the palette on screen.
	///
	/// \details Both are given so that a person is one colour on a drawer and on
	/// the panel inside it; the window ground decides the half of the tone.
	struct identity_avatar_grounds
	{
		QColor Window;
		QColor Base;
	};

	/// \brief The grounds of one palette's active group.
	[[nodiscard]] auto identity_avatar_grounds_of(const QPalette& aPalette) -> identity_avatar_grounds;

	/// \brief [[BPR-1583]]. One person's disc and initials over these grounds.
	struct identity_avatar_colours
	{
		QColor Fill;
		QColor Ink;
	};

	/// \brief The colours `core::identity_tone_for` gives this identity.
	[[nodiscard]] auto identity_avatar_colours_for(const QString& aIdentity, const identity_avatar_grounds& aGrounds) -> identity_avatar_colours;

	/// \brief The initials `core::identity_initials` gives this name.
	[[nodiscard]] auto identity_avatar_initials(const QString& aName) -> QString;

	/// \brief A person is a disc; a team or an owner that is not a person is a
	/// rounded square.
	enum class identity_avatar_shape : std::uint8_t {
		person,
		group,
	};

	/// \brief [[BPR-1583]]. What one avatar shows.
	struct identity_avatar_face
	{
		/// What the colour is keyed on: a login, `organization/slug` for a team.
		QString Identity;
		/// The name the initials come from when it is not the identity (a team's
		/// slug, a repository's owner); empty takes the identity's.
		QString               InitialsFrom;
		identity_avatar_shape Shape = identity_avatar_shape::person;
		/// A one-pixel ring two pixels outside the disc, for mine; no ring when invalid.
		QColor Ring;
		/// [[BPR-1807]]. Always the initials, never a picture: an account or an
		/// owner whose host is not the one this window's pictures come from.
		bool Generated = false;
	};

	/// \brief [[BPR-1583]]. Draws one avatar in `aBox`: the one painter every
	/// surface that shows a person uses.
	///
	/// \details The initials are bold, scaled with the box: 0.53 of its height
	/// for one letter, 0.44 for two, never under 6 px.
	///
	/// [[BPR-1807]]. A person drawn on a widget whose window has
	/// `identity_avatar_pictures` in the GitHub style shows their picture,
	/// masked to the disc, once the host has published it; until then, and
	/// whenever it cannot be had, the initials. The ring is drawn either way.
	void paint_identity_avatar(
		QPainter&                      aPainter,
		const QRectF&                  aBox,
		const identity_avatar_face&    aFace,
		const identity_avatar_grounds& aGrounds,
		const QFont&                   aFont);

	/// \brief [[BPR-1583]]. A widget that is one avatar, for a layout that places
	/// widgets rather than painting a row.
	class identity_avatar_view final : public QWidget
	{
	public:
		explicit identity_avatar_view(int aSide, QWidget* aParent = nullptr);

		/// \brief Shows this identity; an empty one is the neutral `?` face.
		void set_identity(const QString& aIdentity);
		/// \brief [[BPR-1807]]. Always the initials (an account's own avatar).
		void               set_generated(bool aGenerated);
		[[nodiscard]] auto identity() const -> const QString&;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QString Identity;
		int     Side      = 0;
		bool    Generated = false;
	};
} // namespace bpr::qt

#endif
