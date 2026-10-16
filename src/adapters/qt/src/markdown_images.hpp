#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_IMAGES_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_IMAGES_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/markdown_image.hpp"

#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QSizeF>
#include <QString>
#include <QTextObjectInterface>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class QTextDocument;
class QWidget;

/// \brief RFC-BPR-0084. Images drawn inline in a rendered Markdown document.
///
/// \details Each image a body shows is one object character whose format
/// names its ordinal; this store holds what the reader knows of it and draws
/// it: a quiet reserved box while it loads, the decoded frame when it has
/// arrived. Every other state is replaced by the accessible text placeholder.
/// Nothing here fetches, decodes or resolves an address; the store receives
/// pixels from the application's projection.
namespace bpr::qt::detail {
	inline constexpr int markdown_image_object  = QTextFormat::UserObject + 84;
	inline constexpr int markdown_image_ordinal = QTextFormat::UserProperty + 84;

	/// \brief One image of the current document.
	struct markdown_image_slot
	{
		std::uint32_t Ordinal = 0U;
		/// The source as written: the reference the reader sent the host.
		QString                    Source;
		QString                    Alt;
		QString                    Link;
		core::markdown_image_hints Hints;
		/// A `<picture>`'s colour-scheme sources, kept for a theme change.
		QString                                                    Light;
		QString                                                    Dark;
		QString                                                    Fallback;
		bool                                                       Attachment = false;
		core::markdown_image_state                                 State      = core::markdown_image_state::loading;
		std::shared_ptr<const application::markdown_decoded_image> Image;
		std::size_t                                                Frame        = 0U;
		std::int64_t                                               FrameElapsed = 0;
		std::vector<QImage>                                        Views;
		QPixmap                                                    Scaled;
		QSize                                                      ScaledFor;
		std::size_t                                                ScaledFrame = 0U;
	};

	/// \brief Draws and sizes a document's image objects.
	/// \details A text-object handler for `markdown_image_object`; Qt finds the
	/// interface through `qt_metacast`, so no meta-object compiler is needed.
	class markdown_image_store final : public QObject, public QTextObjectInterface
	{
	public:
		explicit markdown_image_store(QWidget& aOwner);

		[[nodiscard]] auto qt_metacast(const char* aName) -> void* override;
		[[nodiscard]] auto intrinsicSize(QTextDocument* aDocument, int aPosition, const QTextFormat& aFormat) -> QSizeF override;
		void               drawObject(QPainter* aPainter, const QRectF& aRect, QTextDocument* aDocument, int aPosition, const QTextFormat& aFormat) override;

		/// \brief Replaces every image in `aDocument` with an object, in document order.
		/// \details Markdown images and GitHub's `<img>`, linked or chosen from
		/// a `<picture>` by `aDark`, in prose only: code keeps its text.
		/// `aLinked` pairs Markdown images with the links Qt's import drops.
		void place(QTextDocument& aDocument, bool aDark, std::vector<core::markdown_linked_image> aLinked = { });
		/// \brief Forgets every image.
		void               clear();
		[[nodiscard]] auto images() const noexcept -> const std::vector<markdown_image_slot>&;
		/// \brief The references still waiting for an answer, as the host takes them.
		[[nodiscard]] auto waiting() const -> std::vector<core::markdown_image_reference>;
		/// \brief Applies one projection; false when it is not for this document.
		/// \details A loaded image relayouts its object; any other terminal
		/// state replaces it with the accessible placeholder.
		auto apply(QTextDocument& aDocument, const application::markdown_image_projection& aImage) -> bool;
		/// \brief Moves animated images on by `aElapsedMs`; true when a frame changed.
		auto advance(std::int64_t aElapsedMs) -> bool;
		/// \brief The time until the next animated frame is due, if any image animates.
		[[nodiscard]] auto next_frame_ms() const -> std::optional<std::int64_t>;
		/// \brief Selects every `<picture>` source again for a theme; true when one changed.
		auto choose_scheme(bool aDark) -> bool;
		/// \brief The image an object character at `aPosition` shows, if any.
		[[nodiscard]] auto slot_at(const QTextDocument& aDocument, int aPosition) const -> const markdown_image_slot*;
		/// \brief The Markdown a selection copy writes for one image.
		[[nodiscard]] static auto markdown_of(const markdown_image_slot& aSlot) -> QString;
		/// \brief The accessible and fallback text of one image.
		[[nodiscard]] static auto placeholder_of(const markdown_image_slot& aSlot) -> QString;

	private:
		[[nodiscard]] auto available_width(QTextDocument* aDocument, int aPosition) const -> std::uint32_t;
		[[nodiscard]] auto find(std::uint32_t aOrdinal) -> markdown_image_slot*;
		void               fall_back(QTextDocument& aDocument, const markdown_image_slot& aSlot);

		QPointer<QWidget>                Owner;
		std::vector<markdown_image_slot> Slots;
	};

	/// \brief Where the object for `aOrdinal` sits in `aDocument`, if it still does.
	[[nodiscard]] auto markdown_image_position(const QTextDocument& aDocument, std::uint32_t aOrdinal) -> std::optional<int>;
} // namespace bpr::qt::detail

#endif
