#include "bpr/qt/identity_avatar.hpp"

#include "bpr/core/identity_avatar.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/identity_avatar_pictures.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>

namespace bpr::qt {
	namespace {
		constexpr auto one_letter_scale = 0.53;
		constexpr auto two_letter_scale = 0.44;
		constexpr auto least_pixels     = 6;
		constexpr auto group_corner     = 0.22;
		constexpr auto ring_offset      = 2.0;
		/// [[BPR-1807]]. A picture's hairline edge: 15% ink, GitHub's own.
		constexpr auto picture_edge_alpha = 38;
	} // namespace

	auto identity_avatar_grounds_of(const QPalette& aPalette) -> identity_avatar_grounds
	{
		return { .Window = aPalette.color(QPalette::Active, QPalette::Window), .Base = aPalette.color(QPalette::Active, QPalette::Base) };
	}

	auto identity_avatar_colours_for(const QString& aIdentity, const identity_avatar_grounds& aGrounds) -> identity_avatar_colours
	{
		const auto grounds = std::array{ to_appearance_color(aGrounds.Window), to_appearance_color(aGrounds.Base) };
		const auto tone    = core::identity_tone_for(aIdentity.toStdString(), grounds);
		return { .Fill = to_presentation_color(tone.Fill), .Ink = to_presentation_color(tone.Ink) };
	}

	auto identity_avatar_initials(const QString& aName) -> QString
	{
		return QString::fromStdString(core::identity_initials(aName.toStdString()));
	}

	void paint_identity_avatar(
		QPainter&                      aPainter,
		const QRectF&                  aBox,
		const identity_avatar_face&    aFace,
		const identity_avatar_grounds& aGrounds,
		const QFont&                   aFont)
	{
		if (aBox.isEmpty()) {
			return;
		}
		const auto colours  = identity_avatar_colours_for(aFace.Identity, aGrounds);
		const auto initials = identity_avatar_initials(aFace.InitialsFrom.isEmpty() ? aFace.Identity : aFace.InitialsFrom);
		const auto group    = aFace.Shape == identity_avatar_shape::group;
		const auto corner   = aBox.height() * group_corner;
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		if (aFace.Ring.isValid()) {
			aPainter.setPen(QPen{ aFace.Ring, 1.0 });
			aPainter.setBrush(Qt::NoBrush);
			const auto ring = aBox.adjusted(-ring_offset, -ring_offset, ring_offset, ring_offset);
			if (group) {
				aPainter.drawRoundedRect(ring, corner + ring_offset, corner + ring_offset);
			} else {
				aPainter.drawEllipse(ring);
			}
		}
		// [[BPR-1807]]. A person's own picture, when this window has one for them.
		if (!group && !aFace.Generated && !aFace.Identity.isEmpty()) {
			auto* const device = aPainter.device();
			if (auto* const pictures = identity_avatar_pictures::of(device); pictures != nullptr) {
				const auto  ratio     = device != nullptr ? device->devicePixelRatioF() : 1.0;
				const auto  pixels    = static_cast<int>(std::ceil(aBox.height() * ratio));
				auto* const requester = device != nullptr && device->devType() == QInternal::Widget ? static_cast<QWidget*>(device) : nullptr;
				if (const auto* const picture = pictures->picture(aFace.Identity, pixels, requester); picture != nullptr) {
					aPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
					aPainter.drawImage(aBox, *picture);
					// A hairline edge, as GitHub draws one, so a picture with a white
					// or black background keeps its disc on a ground of that colour.
					const auto light = core::identity_ground_of(to_appearance_color(aGrounds.Window)) == core::identity_ground::light;
					aPainter.setPen(QPen{ light ? QColor{ 0, 0, 0, picture_edge_alpha } : QColor{ 255, 255, 255, picture_edge_alpha }, 1.0 });
					aPainter.setBrush(Qt::NoBrush);
					aPainter.drawEllipse(aBox.adjusted(0.5, 0.5, -0.5, -0.5));
					aPainter.restore();
					return;
				}
			}
		}
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(colours.Fill);
		if (group) {
			aPainter.drawRoundedRect(aBox, corner, corner);
		} else {
			aPainter.drawEllipse(aBox);
		}
		auto font = aFont;
		font.setWeight(QFont::Bold);
		const auto scale = initials.size() > 1 && !initials.front().isHighSurrogate() ? two_letter_scale : one_letter_scale;
		font.setPixelSize(std::max(least_pixels, static_cast<int>(std::lround(aBox.height() * scale))));
		aPainter.setFont(font);
		aPainter.setPen(colours.Ink);
		aPainter.drawText(aBox, Qt::AlignCenter, initials);
		aPainter.restore();
	}

	identity_avatar_view::identity_avatar_view(const int aSide, QWidget* const aParent)
		: QWidget{ aParent }
		, Side{ aSide }
	{
		setFixedSize(aSide, aSide);
		setAttribute(Qt::WA_TranslucentBackground, true);
	}

	void identity_avatar_view::set_identity(const QString& aIdentity)
	{
		if (Identity == aIdentity) {
			return;
		}
		Identity = aIdentity;
		update();
	}

	void identity_avatar_view::set_generated(const bool aGenerated)
	{
		if (Generated == aGenerated) {
			return;
		}
		Generated = aGenerated;
		update();
	}

	auto identity_avatar_view::identity() const -> const QString&
	{
		return Identity;
	}

	auto identity_avatar_view::sizeHint() const -> QSize
	{
		return { Side, Side };
	}

	void identity_avatar_view::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		paint_identity_avatar(painter, QRectF{ rect() }, { .Identity = Identity, .Generated = Generated }, identity_avatar_grounds_of(palette()), font());
	}

	namespace {
		/// The picture at exactly `aPixels` device pixels, masked to a disc with
		/// an antialiased edge, so a face is drawn without scaling or clipping.
		[[nodiscard]] auto masked_picture(const QImage& aSource, const int aPixels) -> QImage
		{
			auto masked = QImage{ aPixels, aPixels, QImage::Format_ARGB32_Premultiplied };
			masked.fill(Qt::transparent);
			auto painter = QPainter{ &masked };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
			auto disc = QPainterPath{ };
			disc.addEllipse(QRectF{ 0.0, 0.0, static_cast<qreal>(aPixels), static_cast<qreal>(aPixels) });
			painter.setClipPath(disc);
			// A picture that is not square is cropped to its centre, as GitHub does.
			const auto side = std::min(aSource.width(), aSource.height());
			const auto crop = QRect{ (aSource.width() - side) / 2, (aSource.height() - side) / 2, side, side };
			painter.drawImage(QRectF{ 0.0, 0.0, static_cast<qreal>(aPixels), static_cast<qreal>(aPixels) }, aSource, QRectF{ crop });
			painter.end();
			return masked;
		}

		/// The first frame of a decoded picture as an image that owns its pixels.
		[[nodiscard]] auto first_frame(const application::markdown_decoded_image& aImage) -> QImage
		{
			if (aImage.Frames.empty() || aImage.Scale == 0U || aImage.Scale > 2U) {
				return { };
			}
			const auto  width  = static_cast<int>(aImage.Width * aImage.Scale);
			const auto  height = static_cast<int>(aImage.Height * aImage.Scale);
			const auto& frame  = aImage.Frames.front();
			if (width <= 0 || height <= 0 || frame.Pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U) {
				return { };
			}
			return QImage{ frame.Pixels.data(), width, height, width * 4, QImage::Format_ARGB32_Premultiplied }.copy();
		}
	} // namespace

	identity_avatar_pictures::identity_avatar_pictures(QWidget* const aWindow)
		: QObject{ aWindow }
		, Window{ aWindow }
	{
		setObjectName(QStringLiteral("identityAvatarPictures"));
	}

	auto identity_avatar_pictures::of(const QPaintDevice* const aDevice) -> identity_avatar_pictures*
	{
		if (aDevice == nullptr || aDevice->devType() != QInternal::Widget) {
			return nullptr;
		}
		// A popup or a dialog is its own window; its owner's pictures serve it.
		for (const auto* window = static_cast<const QWidget*>(aDevice)->window(); window != nullptr;
			 window             = window->parentWidget() != nullptr ? window->parentWidget()->window() : nullptr) {
			for (auto* const child : window->children()) {
				if (auto* const found = dynamic_cast<identity_avatar_pictures*>(child); found != nullptr) {
					return found;
				}
			}
		}
		return nullptr;
	}

	void identity_avatar_pictures::set_dispatch(dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void identity_avatar_pictures::set_style(const core::identity_avatar_style aStyle)
	{
		if (Style == aStyle || !core::valid(aStyle)) {
			return;
		}
		Style = aStyle;
		Queue.clear();
		repaint_window();
	}

	auto identity_avatar_pictures::style() const noexcept -> core::identity_avatar_style
	{
		return Style;
	}

	void identity_avatar_pictures::apply(const application::identity_avatar_projection& aAvatar)
	{
		auto& entry = Slots[{ aAvatar.Login, aAvatar.Pixels }];
		if (aAvatar.State == core::markdown_image_state::loading) {
			return;
		}
		auto source = aAvatar.State == core::markdown_image_state::loaded && aAvatar.Image != nullptr ? first_frame(*aAvatar.Image) : QImage{ };
		entry.Masked.clear();
		entry.Standing  = source.isNull() ? standing::refused : standing::shown;
		entry.Source    = std::move(source);
		auto requesters = std::move(entry.Requesters);
		entry.Requesters.clear();
		if (entry.Standing != standing::shown) {
			return;
		}
		for (const auto& requester : requesters) {
			if (requester != nullptr) {
				requester->update();
			}
		}
	}

	void identity_avatar_pictures::reset()
	{
		if (Slots.empty() && Queue.empty()) {
			return;
		}
		Slots.clear();
		Queue.clear();
		repaint_window();
	}

	auto identity_avatar_pictures::picture(const QString& aLogin, const int aDevicePixels, QWidget* const aRequester) -> const QImage*
	{
		if (Style != core::identity_avatar_style::github || aDevicePixels <= 0) {
			return nullptr;
		}
		const auto login = aLogin.toStdString();
		if (!core::identity_avatar_login(login)) {
			return nullptr;
		}
		const auto key   = std::pair{ core::identity_avatar_folded_login(login), core::identity_avatar_picture_pixels(static_cast<std::uint32_t>(aDevicePixels)) };
		auto       found = Slots.find(key);
		if (found == Slots.end()) {
			if (Slots.size() >= maximum_pictures) {
				Slots.clear();
			}
			found = Slots.emplace(key, slot{ }).first;
			Queue.push_back({ .Login = key.first, .DevicePixels = static_cast<std::uint32_t>(aDevicePixels) });
			if (!FlushPosted) {
				FlushPosted = true;
				QTimer::singleShot(0, this, [this] { flush(); });
			}
		}
		auto& entry = found->second;
		if (entry.Standing == standing::shown) {
			auto masked = entry.Masked.find(aDevicePixels);
			if (masked == entry.Masked.end()) {
				masked = entry.Masked.emplace(aDevicePixels, masked_picture(entry.Source, aDevicePixels)).first;
				masked->second.setDevicePixelRatio(1.0);
			}
			return &masked->second;
		}
		if (entry.Standing == standing::waiting && aRequester != nullptr
			&& std::ranges::none_of(entry.Requesters, [aRequester](const QPointer<QWidget>& aKnown) { return aKnown.data() == aRequester; })) {
			entry.Requesters.emplace_back(aRequester);
		}
		return nullptr;
	}

	auto identity_avatar_pictures::held() const noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(Slots, [](const auto& aSlot) { return aSlot.second.Standing == standing::shown; }));
	}

	auto identity_avatar_pictures::asked() const noexcept -> std::size_t
	{
		return Asked;
	}

	void identity_avatar_pictures::flush()
	{
		FlushPosted = false;
		if (Queue.empty()) {
			return;
		}
		auto queue = std::exchange(Queue, { });
		Asked += queue.size();
		if (Dispatch) {
			Dispatch(std::move(queue));
		}
	}

	void identity_avatar_pictures::repaint_window()
	{
		if (Window == nullptr) {
			return;
		}
		Window->update();
		for (auto* const child : Window->findChildren<QWidget*>()) {
			child->update();
		}
	}
} // namespace bpr::qt
