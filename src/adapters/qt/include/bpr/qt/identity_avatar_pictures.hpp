#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_IDENTITY_AVATAR_PICTURES_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_IDENTITY_AVATAR_PICTURES_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/identity_avatar_picture.hpp"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class QPaintDevice;

namespace bpr::qt {
	/// \brief [[BPR-1807]]. One window's people's pictures, as the host
	/// published them, for `paint_identity_avatar` to draw.
	///
	/// \details A direct child of the window it serves; the painter finds it
	/// from the widget it paints, walking out through owning windows, so every
	/// surface that draws a person gets pictures without being told. It holds
	/// decoded pixels and states only, never an address: a face it has no
	/// picture for is asked for as a login and a device size through the
	/// dispatch the composition installs, once, at the end of the event-loop
	/// turn, and the widget that drew it repaints when the answer arrives.
	/// With the Generated style it asks nothing and the painter draws initials.
	class identity_avatar_pictures final : public QObject
	{
	public:
		using dispatch = std::function<void(std::vector<core::identity_avatar_intent>)>;

		/// \brief At most this many pictures are held; one more forgets them all,
		/// and the faces on screen ask again.
		static constexpr std::size_t maximum_pictures = 256U;

		explicit identity_avatar_pictures(QWidget* aWindow);

		/// \brief The pictures serving the window `aDevice` belongs to, if any.
		[[nodiscard]] static auto of(const QPaintDevice* aDevice) -> identity_avatar_pictures*;

		void               set_dispatch(dispatch aDispatch);
		void               set_style(core::identity_avatar_style aStyle);
		[[nodiscard]] auto style() const noexcept -> core::identity_avatar_style;

		/// \brief Takes one published answer and repaints the faces that asked.
		void apply(const application::identity_avatar_projection& aAvatar);
		/// \brief Forgets every picture and answer, as a change of account or
		/// host requires, and repaints the window so its faces ask again.
		void reset();

		/// \brief The picture for this login at this device size, masked to a
		/// disc, or null. A face with none yet is asked for on `aRequester`'s
		/// behalf, unless it was already asked or already refused.
		[[nodiscard]] auto picture(const QString& aLogin, int aDevicePixels, QWidget* aRequester) -> const QImage*;

		/// \brief Pictures held and requests sent, for tests and diagnostics.
		[[nodiscard]] auto held() const noexcept -> std::size_t;
		[[nodiscard]] auto asked() const noexcept -> std::size_t;

	private:
		enum class standing : std::uint8_t {
			waiting,
			shown,
			refused,
		};

		struct slot
		{
			standing                       Standing = standing::waiting;
			QImage                         Source;
			std::map<int, QImage>          Masked;
			std::vector<QPointer<QWidget>> Requesters;
		};

		void flush();
		void repaint_window();

		QPointer<QWidget>                                     Window;
		dispatch                                              Dispatch;
		core::identity_avatar_style                           Style = core::identity_avatar_style::github;
		std::map<std::pair<std::string, std::uint32_t>, slot> Slots;
		std::vector<core::identity_avatar_intent>             Queue;
		std::size_t                                           Asked       = 0U;
		bool                                                  FlushPosted = false;
	};
} // namespace bpr::qt

#endif
