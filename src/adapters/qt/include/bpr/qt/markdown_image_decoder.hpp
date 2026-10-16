#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MARKDOWN_IMAGE_DECODER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MARKDOWN_IMAGE_DECODER_HPP

#include "bpr/application/markdown_image_ports.hpp"

#include <expected>
#include <string_view>

namespace bpr::qt {
	/// \brief RFC-BPR-0084. Decodes body images with Qt's own image readers.
	///
	/// \details PNG, JPEG, GIF and WebP through Qt's readers, and SVG through
	/// QtSvg's renderer, chosen by the bytes' signature with Qt's format
	/// detection off, so a mislabeled or other payload never reaches another
	/// reader. An SVG must pass `core::admit_markdown_svg` (self-contained,
	/// bounded) and is rasterized once, statically, at twice its natural size. Sizes are checked before pixels are allocated. EXIF
	/// orientation is applied and colours are converted to sRGB, as a browser
	/// shows them. Stateless and safe to call from any thread.
	class markdown_image_decoder final : public application::markdown_image_decoder_port
	{
	public:
		[[nodiscard]] auto decode(std::string_view aBytes) const
			-> std::expected<application::markdown_decoded_image, core::markdown_image_state> override;
	};
} // namespace bpr::qt

#endif
