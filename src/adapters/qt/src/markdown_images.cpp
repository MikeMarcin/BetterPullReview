#include "markdown_images.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>
#include <QTextOption>
#include <QTextTable>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

namespace bpr::qt::detail {
	namespace {
		constexpr auto text_object_interface = std::string_view{ "org.qt-project.Qt.QTextObjectInterface" };
		/// GitHub rounds an uploaded attachment's corners by 6 px.
		constexpr qreal attachment_radius = 6.0;
		/// Browsers show a frame of no stated delay for 100 ms.
		constexpr std::int64_t default_frame_ms = 100;

		struct candidate
		{
			int                 Position = 0;
			int                 Length   = 0;
			markdown_image_slot Slot;
		};

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			const auto blend = [aAmount](const int aA, const int aB) { return static_cast<int>(std::lround(aA + (aB - aA) * aAmount)); };
			return QColor{ blend(aFrom.red(), aTo.red()), blend(aFrom.green(), aTo.green()), blend(aFrom.blue(), aTo.blue()) };
		}

		[[nodiscard]] auto code_block(const QTextBlock& aBlock) -> bool
		{
			const auto format = aBlock.blockFormat();
			return format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage);
		}

		[[nodiscard]] auto attachment(const QString& aSource) -> bool
		{
			return aSource.contains(QStringLiteral("/user-attachments/assets/"));
		}

		[[nodiscard]] auto text_of(const std::string& aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		/// Markdown images: Qt's importer gives each one an image format.
		void collect_markdown_images(const QTextBlock& aBlock, std::vector<candidate>& aFound)
		{
			for (auto it = aBlock.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				const auto format   = fragment.charFormat();
				if (!format.isImageFormat()) {
					continue;
				}
				const auto image      = format.toImageFormat();
				auto       found      = candidate{ .Position = fragment.position(), .Length = fragment.length(), .Slot = { } };
				found.Slot.Source     = image.name();
				found.Slot.Alt        = image.stringProperty(QTextFormat::ImageAltText);
				found.Slot.Link       = format.isAnchor() ? format.anchorHref() : QString{ };
				found.Slot.Attachment = attachment(found.Slot.Source);
				aFound.push_back(std::move(found));
			}
		}

		/// GitHub's `<img>` markup in prose. Code and other objects are masked so
		/// a tag can neither start nor end inside them.
		void collect_html_images(const QTextBlock& aBlock, const bool aDark, std::vector<candidate>& aFound)
		{
			if (code_block(aBlock)) {
				return;
			}
			QString text;
			for (auto it = aBlock.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				const auto masked   = fragment.charFormat().fontFixedPitch() || fragment.charFormat().isImageFormat();
				text += masked ? QString(fragment.length(), QChar{ 0x1 }) : fragment.text();
			}
			if (!text.contains(QLatin1Char{ '<' })) {
				return;
			}
			// The scanner reads UTF-8; map each UTF-8 byte back to its UTF-16 index.
			std::string            bytes;
			std::vector<qsizetype> index;
			for (qsizetype position = 0; position < text.size(); ++position) {
				const auto pair  = text[position].isHighSurrogate() && position + 1 < text.size() && text[position + 1].isLowSurrogate();
				const auto piece = text.mid(position, pair ? 2 : 1).toUtf8();
				for (qsizetype byte = 0; byte < piece.size(); ++byte) {
					bytes += piece[byte];
					index.push_back(position);
				}
				position += pair ? 1 : 0;
			}
			index.push_back(text.size());
			for (auto& span : core::scan_html_images(bytes)) {
				const auto begin      = index[span.Begin];
				const auto end        = index[span.End];
				auto       found      = candidate{ .Position = aBlock.position() + static_cast<int>(begin), .Length = static_cast<int>(end - begin), .Slot = { } };
				found.Slot.Fallback   = text_of(span.Source);
				found.Slot.Light      = span.LightSource ? text_of(*span.LightSource) : QString{ };
				found.Slot.Dark       = span.DarkSource ? text_of(*span.DarkSource) : QString{ };
				found.Slot.Source     = aDark && !found.Slot.Dark.isEmpty() ? found.Slot.Dark : !aDark && !found.Slot.Light.isEmpty() ? found.Slot.Light
																																	  : found.Slot.Fallback;
				found.Slot.Alt        = text_of(span.Alt);
				found.Slot.Link       = span.Link ? text_of(*span.Link) : QString{ };
				found.Slot.Hints      = span.Hints;
				found.Slot.Attachment = attachment(found.Slot.Source);
				aFound.push_back(std::move(found));
			}
		}

		[[nodiscard]] auto views_of(const application::markdown_decoded_image& aImage) -> std::vector<QImage>
		{
			std::vector<QImage> views;
			views.reserve(aImage.Frames.size());
			if (aImage.Scale == 0U || aImage.Scale > 2U) {
				return { };
			}
			const auto width  = static_cast<int>(aImage.Width * aImage.Scale);
			const auto height = static_cast<int>(aImage.Height * aImage.Scale);
			for (const auto& frame : aImage.Frames) {
				if (frame.Pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U) {
					return { };
				}
				// A view of the shared pixels; the slot keeps them alive.
				views.emplace_back(frame.Pixels.data(), width, height, width * 4, QImage::Format_ARGB32_Premultiplied);
			}
			return views;
		}

		[[nodiscard]] auto frame_ms(const markdown_image_slot& aSlot) -> std::int64_t
		{
			const auto delay = aSlot.Image->Frames[aSlot.Frame].DelayMs;
			return delay == 0U ? default_frame_ms : static_cast<std::int64_t>(delay);
		}

		[[nodiscard]] auto animated(const markdown_image_slot& aSlot) -> bool
		{
			return aSlot.State == core::markdown_image_state::loaded && aSlot.Image != nullptr && aSlot.Image->Frames.size() > 1U;
		}
	} // namespace

	markdown_image_store::markdown_image_store(QWidget& aOwner)
		: QObject{ &aOwner }
		, Owner{ &aOwner }
	{
	}

	auto markdown_image_store::qt_metacast(const char* aName) -> void*
	{
		// QTextDocumentLayout finds the handler interface by its interface
		// identifier; answering it here stands in for Q_INTERFACES.
		if (aName != nullptr && text_object_interface == aName) {
			return static_cast<QTextObjectInterface*>(this);
		}
		return QObject::qt_metacast(aName);
	}

	void markdown_image_store::place(QTextDocument& aDocument, const bool aDark, std::vector<core::markdown_linked_image> aLinked)
	{
		Slots.clear();
		std::vector<candidate> found;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			const auto before = found.size();
			collect_markdown_images(block, found);
			// Each Markdown image takes the next unused link written around its source.
			for (auto index = before; index < found.size(); ++index) {
				auto&      slot   = found[index].Slot;
				const auto source = slot.Source.toStdString();
				const auto link   = std::ranges::find(aLinked, source, &core::markdown_linked_image::Source);
				if (slot.Link.isEmpty() && link != aLinked.end()) {
					slot.Link = text_of(link->Link);
					aLinked.erase(link);
				}
			}
			collect_html_images(block, aDark, found);
		}
		std::ranges::sort(found, { }, &candidate::Position);
		for (std::size_t index = 0; index < found.size(); ++index) {
			found[index].Slot.Ordinal = static_cast<std::uint32_t>(index);
		}
		// Back to front, so earlier positions stay valid while later ones change.
		for (auto it = found.rbegin(); it != found.rend(); ++it) {
			auto cursor = QTextCursor{ &aDocument };
			cursor.setPosition(it->Position);
			cursor.setPosition(it->Position + it->Length, QTextCursor::KeepAnchor);
			auto format = QTextCharFormat{ };
			format.setObjectType(markdown_image_object);
			format.setProperty(markdown_image_ordinal, static_cast<int>(it->Slot.Ordinal));
			format.setAnchor(true);
			format.setAnchorHref(it->Slot.Link.isEmpty() ? it->Slot.Source : it->Slot.Link);
			cursor.insertText(QString{ QChar::ObjectReplacementCharacter }, format);
		}
		Slots.reserve(found.size());
		for (auto& item : found) {
			Slots.push_back(std::move(item.Slot));
		}
	}

	void markdown_image_store::clear()
	{
		Slots.clear();
	}

	auto markdown_image_store::images() const noexcept -> const std::vector<markdown_image_slot>&
	{
		return Slots;
	}

	auto markdown_image_store::waiting() const -> std::vector<core::markdown_image_reference>
	{
		std::vector<core::markdown_image_reference> references;
		for (const auto& slot : Slots) {
			if (slot.State == core::markdown_image_state::loading) {
				references.push_back({ .Ordinal = slot.Ordinal, .Source = slot.Source.toStdString() });
			}
		}
		return references;
	}

	auto markdown_image_store::find(const std::uint32_t aOrdinal) -> markdown_image_slot*
	{
		const auto found = std::ranges::find(Slots, aOrdinal, &markdown_image_slot::Ordinal);
		return found == Slots.end() ? nullptr : &*found;
	}

	auto markdown_image_store::apply(QTextDocument& aDocument, const application::markdown_image_projection& aImage) -> bool
	{
		auto* slot = find(aImage.Ordinal);
		if (slot == nullptr || slot->Source != text_of(aImage.Source)) {
			return false;
		}
		if (slot->State != core::markdown_image_state::loading || aImage.State == core::markdown_image_state::loading) {
			return true;
		}
		if (aImage.State == core::markdown_image_state::loaded && aImage.Image != nullptr && !aImage.Image->Frames.empty()) {
			auto views = views_of(*aImage.Image);
			if (!views.empty()) {
				slot->State        = core::markdown_image_state::loaded;
				slot->Image        = aImage.Image;
				slot->Views        = std::move(views);
				slot->Frame        = 0U;
				slot->FrameElapsed = 0;
				slot->Scaled       = { };
				if (const auto position = markdown_image_position(aDocument, slot->Ordinal)) {
					aDocument.markContentsDirty(*position, 1);
				}
				return true;
			}
		}
		slot->State = aImage.State == core::markdown_image_state::loaded ? core::markdown_image_state::failed : aImage.State;
		fall_back(aDocument, *slot);
		return true;
	}

	auto markdown_image_store::placeholder_of(const markdown_image_slot& aSlot) -> QString
	{
		const auto alt    = aSlot.Alt.isEmpty() ? QStringLiteral("no alternative text") : aSlot.Alt;
		const auto reason = aSlot.State == core::markdown_image_state::too_large ? QStringLiteral(" (too large to show)")
			: aSlot.State == core::markdown_image_state::unsupported             ? QStringLiteral(" (format not shown)")
			: aSlot.State == core::markdown_image_state::over_limit              ? QStringLiteral(" (not loaded: too many images)")
																				 : QString{ };
		return QStringLiteral("[Image: %1%2 — open image link]").arg(alt, reason);
	}

	void markdown_image_store::fall_back(QTextDocument& aDocument, const markdown_image_slot& aSlot)
	{
		const auto position = markdown_image_position(aDocument, aSlot.Ordinal);
		if (!position) {
			return;
		}
		auto cursor = QTextCursor{ &aDocument };
		cursor.setPosition(*position);
		cursor.setPosition(*position + 1, QTextCursor::KeepAnchor);
		auto format = QTextCharFormat{ };
		format.setAnchor(true);
		format.setAnchorHref(aSlot.Link.isEmpty() ? aSlot.Source : aSlot.Link);
		if (Owner) {
			format.setForeground(Owner->palette().color(QPalette::Active, QPalette::Link));
			format.setFontUnderline(true);
		}
		cursor.insertText(placeholder_of(aSlot), format);
	}

	auto markdown_image_store::available_width(QTextDocument* aDocument, const int aPosition) const -> std::uint32_t
	{
		auto width = aDocument->textWidth();
		if (width <= 0.0) {
			width = Owner ? static_cast<qreal>(Owner->width()) : 640.0;
		}
		width -= 2.0 * aDocument->documentMargin();
		const auto block  = aDocument->findBlock(aPosition);
		const auto format = block.blockFormat();
		width -= format.leftMargin() + format.rightMargin() + format.indent() * aDocument->indentWidth();
		if (const auto* list = block.textList()) {
			width -= list->format().indent() * aDocument->indentWidth();
		}
		if (const auto* table = QTextCursor{ block }.currentTable()) {
			width = width / std::max(1, table->columns()) - 2.0 * table->format().cellPadding();
		}
		return static_cast<std::uint32_t>(std::max(1.0, std::floor(width)));
	}

	auto markdown_image_store::intrinsicSize(QTextDocument* aDocument, const int aPosition, const QTextFormat& aFormat) -> QSizeF
	{
		const auto* slot = find(static_cast<std::uint32_t>(aFormat.intProperty(markdown_image_ordinal)));
		if (slot == nullptr || aDocument == nullptr) {
			return { 1.0, 1.0 };
		}
		const auto available = available_width(aDocument, aPosition);
		if (slot->State == core::markdown_image_state::loaded && slot->Image != nullptr) {
			const auto size = core::markdown_image_display_size({ slot->Image->Width, slot->Image->Height }, slot->Hints, available);
			return { static_cast<qreal>(size.Width), static_cast<qreal>(size.Height) };
		}
		if (const auto reserved = core::markdown_image_reserved_size(slot->Hints, available)) {
			return { static_cast<qreal>(reserved->Width), static_cast<qreal>(reserved->Height) };
		}
		// Unknown size: one line, as wide as its alternative text.
		const auto metrics = QFontMetricsF{ Owner ? Owner->font() : QFont{ } };
		const auto label   = slot->Alt.isEmpty() ? QStringLiteral("Image") : slot->Alt;
		const auto width   = std::clamp(metrics.horizontalAdvance(label) + 24.0, 64.0, std::max(64.0, static_cast<qreal>(available)));
		return { std::min(width, static_cast<qreal>(available)), std::ceil(metrics.height()) + 8.0 };
	}

	void markdown_image_store::drawObject(QPainter* aPainter, const QRectF& aRect, QTextDocument* aDocument, const int, const QTextFormat& aFormat)
	{
		auto* slot = find(static_cast<std::uint32_t>(aFormat.intProperty(markdown_image_ordinal)));
		if (slot == nullptr || aPainter == nullptr || aDocument == nullptr) {
			return;
		}
		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing);
		aPainter->setRenderHint(QPainter::SmoothPixmapTransform);
		if (slot->State == core::markdown_image_state::loaded && !slot->Views.empty()) {
			const auto ratio  = aPainter->device() != nullptr ? aPainter->device()->devicePixelRatioF() : 1.0;
			const auto device = QSize{ std::max(1, static_cast<int>(std::lround(aRect.width() * ratio))), std::max(1, static_cast<int>(std::lround(aRect.height() * ratio))) };
			if (slot->Scaled.isNull() || slot->ScaledFor != device || slot->ScaledFrame != slot->Frame) {
				const auto& view = slot->Views[slot->Frame];
				slot->Scaled     = QPixmap::fromImage(view.size() == device ? view : view.scaled(device, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
				slot->Scaled.setDevicePixelRatio(ratio);
				slot->ScaledFor   = device;
				slot->ScaledFrame = slot->Frame;
			}
			if (slot->Attachment) {
				auto clip = QPainterPath{ };
				clip.addRoundedRect(aRect, attachment_radius, attachment_radius);
				aPainter->setClipPath(clip, Qt::IntersectClip);
			}
			aPainter->drawPixmap(aRect.topLeft(), slot->Scaled);
		} else {
			const auto palette = Owner ? Owner->palette() : QPalette{ };
			const auto base    = palette.color(QPalette::Active, QPalette::Base);
			const auto ink     = palette.color(QPalette::Active, QPalette::Text);
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(mixed(base, ink, 0.07));
			aPainter->drawRoundedRect(aRect.adjusted(0.5, 0.5, -0.5, -0.5), attachment_radius, attachment_radius);
			auto font = Owner ? Owner->font() : QFont{ };
			aPainter->setFont(font);
			aPainter->setPen(mixed(base, ink, 0.55));
			const auto label   = slot->Alt.isEmpty() ? QStringLiteral("Image") : slot->Alt;
			const auto metrics = QFontMetricsF{ font };
			const auto inner   = aRect.adjusted(8.0, 0.0, -8.0, 0.0);
			const auto shown   = metrics.elidedText(label, Qt::ElideRight, std::max(0.0, inner.width()));
			const auto tall    = aRect.height() > metrics.height() * 2.5;
			const auto flags   = tall ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignVCenter);
			auto       option  = QTextOption{ flags };
			option.setWrapMode(QTextOption::NoWrap);
			aPainter->drawText(inner, shown, option);
		}
		aPainter->restore();
	}

	auto markdown_image_store::advance(const std::int64_t aElapsedMs) -> bool
	{
		auto changed = false;
		for (auto& slot : Slots) {
			if (!animated(slot)) {
				continue;
			}
			slot.FrameElapsed += aElapsedMs;
			// Bounded catch-up: a long stall moves on at most one loop of frames.
			for (std::size_t step = 0; step < slot.Image->Frames.size() && slot.FrameElapsed >= frame_ms(slot); ++step) {
				slot.FrameElapsed -= frame_ms(slot);
				slot.Frame = (slot.Frame + 1U) % slot.Image->Frames.size();
				changed    = true;
			}
			slot.FrameElapsed = std::min(slot.FrameElapsed, frame_ms(slot));
		}
		return changed;
	}

	auto markdown_image_store::next_frame_ms() const -> std::optional<std::int64_t>
	{
		std::optional<std::int64_t> next;
		for (const auto& slot : Slots) {
			if (animated(slot)) {
				const auto due = std::max<std::int64_t>(10, frame_ms(slot) - slot.FrameElapsed);
				next           = next ? std::min(*next, due) : due;
			}
		}
		return next;
	}

	auto markdown_image_store::choose_scheme(const bool aDark) -> bool
	{
		auto changed = false;
		for (auto& slot : Slots) {
			if (slot.Light.isEmpty() && slot.Dark.isEmpty()) {
				continue;
			}
			const auto chosen = aDark && !slot.Dark.isEmpty() ? slot.Dark : !aDark && !slot.Light.isEmpty() ? slot.Light
																											: slot.Fallback;
			if (chosen == slot.Source || (slot.State != core::markdown_image_state::loading && slot.State != core::markdown_image_state::loaded)) {
				continue;
			}
			slot.Source = chosen;
			slot.State  = core::markdown_image_state::loading;
			slot.Image.reset();
			slot.Views.clear();
			slot.Scaled = { };
			changed     = true;
		}
		return changed;
	}

	auto markdown_image_store::slot_at(const QTextDocument& aDocument, const int aPosition) const -> const markdown_image_slot*
	{
		const auto block = aDocument.findBlock(aPosition);
		for (auto it = block.begin(); !it.atEnd(); ++it) {
			const auto fragment = it.fragment();
			if (aPosition >= fragment.position() && aPosition < fragment.position() + fragment.length()
				&& fragment.charFormat().objectType() == markdown_image_object) {
				const auto ordinal = static_cast<std::uint32_t>(fragment.charFormat().intProperty(markdown_image_ordinal));
				const auto found   = std::ranges::find(Slots, ordinal, &markdown_image_slot::Ordinal);
				return found == Slots.end() ? nullptr : &*found;
			}
		}
		return nullptr;
	}

	auto markdown_image_store::markdown_of(const markdown_image_slot& aSlot) -> QString
	{
		const auto image = QStringLiteral("![%1](%2)").arg(aSlot.Alt, aSlot.Source);
		return aSlot.Link.isEmpty() ? image : QStringLiteral("[%1](%2)").arg(image, aSlot.Link);
	}

	auto markdown_image_position(const QTextDocument& aDocument, const std::uint32_t aOrdinal) -> std::optional<int>
	{
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				const auto format   = fragment.charFormat();
				if (format.objectType() == markdown_image_object && static_cast<std::uint32_t>(format.intProperty(markdown_image_ordinal)) == aOrdinal) {
					return fragment.position();
				}
			}
		}
		return std::nullopt;
	}
} // namespace bpr::qt::detail
