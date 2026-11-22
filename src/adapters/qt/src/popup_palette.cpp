#include "popup_palette.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace bpr::qt::detail {
	namespace {
		/// \brief The follower's object name, which makes it findable as the
		/// application's one.
		constexpr auto follower_name = "bprPopupPaletteFollower";

		/// \brief The dynamic property holding the palette the follower last
		/// gave a popup, which tells its own palette from an owner's.
		constexpr auto applied_property = "bprOpenerPalette";

		/// \brief `aPalette` with every colour of every group set, so a
		/// window given it holds it rather than resolving back to the
		/// application's.
		[[nodiscard]] auto every_role_of(const QPalette& aPalette) -> QPalette
		{
			auto result = aPalette;
			for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
				for (auto role = 0; role < static_cast<int>(QPalette::NColorRoles); ++role) {
					const auto colourRole = static_cast<QPalette::ColorRole>(role);
					if (colourRole != QPalette::NoRole) {
						result.setColor(group, colourRole, aPalette.color(group, colourRole));
					}
				}
			}
			return result;
		}

		class popup_palette_follower final : public QObject
		{
		public:
			explicit popup_palette_follower(QObject* const aParent)
				: QObject{ aParent }
			{
				setObjectName(QString::fromLatin1(follower_name));
			}

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				// The Show event reaches a window before it is mapped, so the
				// first frame is already drawn in the opener's palette.
				if (aEvent->type() == QEvent::Show && aWatched->isWidgetType()) {
					wear_opener_palette(*static_cast<QWidget*>(aWatched));
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			static void wear_opener_palette(QWidget& aPopup)
			{
				if (!aPopup.isWindow() || aPopup.windowType() != Qt::Popup) {
					return;
				}
				const auto* const opener = aPopup.parentWidget();
				if (opener == nullptr) {
					return;
				}
				// An owner's own palette stays: a combo box's list takes
				// the box's, and a popup drawn in a chosen ground sets one.
				// The follower's own is recognised by being unchanged since
				// it was given, so the next opening refreshes it.
				const auto applied = aPopup.property(applied_property);
				if (aPopup.testAttribute(Qt::WA_SetPalette) && (!applied.isValid() || applied.value<QPalette>() != aPopup.palette())) {
					return;
				}
				const auto palette = every_role_of(opener->palette());
				if (palette != aPopup.palette()) {
					aPopup.setPalette(palette);
				}
				aPopup.setProperty(applied_property, QVariant::fromValue(aPopup.palette()));
			}
		};
	} // namespace

	void follow_opener_palette_in_popups()
	{
		auto* const application = QCoreApplication::instance();
		if (application == nullptr
			|| application->findChild<QObject*>(QString::fromLatin1(follower_name), Qt::FindDirectChildrenOnly) != nullptr) {
			return;
		}
		application->installEventFilter(new popup_palette_follower{ application });
	}
} // namespace bpr::qt::detail
