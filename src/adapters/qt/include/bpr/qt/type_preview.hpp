#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TYPE_PREVIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TYPE_PREVIEW_HPP

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QString>
#include <QWidget>

#include <cstdint>

class QLabel;
class QPlainTextEdit;

namespace bpr::qt {
	/// \brief Says one font size in points, never in hundredths.
	///
	/// \details The core stores centipoints so that every legal size is exactly
	/// representable without floating point. A settings surface must not show that
	/// storage: RFC-BPR-0034 replaces the page whose spin boxes read "1300" for
	/// thirteen points. Two decimals are shown only when they carry a value, so a
	/// whole size reads as a whole number.
	[[nodiscard]] auto point_size_text(std::uint32_t aCentipoints) -> QString;

	/// \brief Records directly observable preview painting.
	struct type_preview_statistics
	{
		/// Every adoption, cumulative.
		std::uint64_t Adoptions = 0U;
		/// Rows the last adoption wrote syntax formats to. Zero is the honest
		/// answer for an adoption that changed no colour and no emphasis: the
		/// formats were compared and found equal, so no block was relaid out.
		std::uint64_t FormattedRows = 0U;
		/// Diff, selection and current-line bands the last adoption emitted.
		std::uint64_t BandsPainted = 0U;
		/// [[BPR-1107]]. White space marks the code surface drew on its last
		/// repaint.
		std::uint64_t WhiteSpacePainted = 0U;
		/// Line numbers the gutter drew on its last repaint.
		std::uint64_t GutterNumbersPainted = 0U;

		auto operator<=>(const type_preview_statistics&) const = default;
	};

	/// \brief RFC-BPR-0034's labelled Type preview: the review window in miniature.
	///
	/// \details [[BPR-0790]]. An interface sample at the accepted interface font,
	/// size, weight and line height, a six-line code sample, and a caption that
	/// states the values in effect.
	///
	/// The code sample is painted by the review window's own painter rather than by
	/// a second renderer, which RFC-BPR-0034 prohibits: the sample is expressed as
	/// document text plus a classification, and the formats, the diff bands, the
	/// selection wash and the current-line band all come from
	/// `code_sample_painter`, which is the same translation unit the comparison
	/// panes paint through. The font and the tab stop arrive through
	/// `appearance_profile::apply_code_font_to`, which is the path every code
	/// surface takes.
	///
	/// It is the first surface to consume the two syntax emphasis switches and the
	/// `code_current_line_background` role. Both are accepted appearance values
	/// that no painter read before; showing them here is what the criterion asks
	/// for and changes nothing about what the review panes paint.
	class type_preview final : public QWidget
	{
	public:
		explicit type_preview(QWidget* aParent = nullptr);

		/// \brief Adopts the typography and the theme on screen, and repaints.
		///
		/// \details The profile is not const because adopting applies it: the
		/// palette, the interface font, and the code font with its tab stop all
		/// reach this surface through the profile's own `apply_*` members, which
		/// count the surfaces they touched. Re-deriving those values here instead
		/// would be the second renderer this class exists to avoid.
		void adopt_appearance(appearance_profile& aProfile);
		/// \brief Names the theme the caption says the colours come from.
		///
		/// \details Supplied by the composition, which resolves the variant. Until
		/// it is supplied the caption says "the theme on screen", which is true
		/// whatever that theme turns out to be.
		void set_theme_name(QString aName);

		[[nodiscard]] auto heading() const noexcept -> QLabel*;
		[[nodiscard]] auto interface_sample() const noexcept -> QWidget*;
		[[nodiscard]] auto code_surface() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto caption() const noexcept -> QLabel*;
		[[nodiscard]] auto caption_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept -> type_preview_statistics;

	private:
		class code_pane;

		void refresh_code();
		void refresh_interface();
		void refresh_caption();

		QLabel*    Heading         = nullptr;
		QWidget*   InterfaceSample = nullptr;
		QLabel*    InterfaceRow    = nullptr;
		QLabel*    InterfaceCount  = nullptr;
		QLabel*    InterfacePath   = nullptr;
		code_pane* Code            = nullptr;
		QLabel*    Caption         = nullptr;

		code_role_colors          Colors{ };
		code_syntax_emphasis      Emphasis;
		core::typography_settings Typography;
		QString                   ThemeName;
		type_preview_statistics   Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TYPE_PREVIEW_HPP
