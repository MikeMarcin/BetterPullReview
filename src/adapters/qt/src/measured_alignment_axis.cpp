#include "bpr/qt/measured_alignment_axis.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QFontMetrics>
#include <QMetaType>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextLayout>
#include <QTextOption>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// Qt lays text out in its private `QFixed` (26.6 fixed point, 1/64 px); this mirrors it in a plain `std::int32_t` rather than depend on the private header.
		constexpr auto layout_units_per_pixel = qreal{ 64.0 };

		/// \brief The value as a whole number of layout units, if it is exactly
		/// one that fits in 32 bits.
		///
		/// \details Every height and natural width a text layout produces is a
		/// `QFixed` read back as `qreal`, so it fits and comes back exactly, at
		/// any device pixel ratio. A value off that grid, beyond its range or
		/// not finite has no layout units.
		[[nodiscard]] auto to_layout_units(const qreal aPixels) noexcept -> std::optional<std::int32_t>
		{
			const auto units = aPixels * layout_units_per_pixel;
			if (!(units >= static_cast<qreal>(std::numeric_limits<std::int32_t>::min())
				  && units <= static_cast<qreal>(std::numeric_limits<std::int32_t>::max()))
				|| units != std::trunc(units)) {
				return std::nullopt;
			}
			return static_cast<std::int32_t>(units);
		}

		[[nodiscard]] constexpr auto from_layout_units(const std::int32_t aUnits) noexcept -> qreal
		{
			return static_cast<qreal>(aUnits) / layout_units_per_pixel;
		}

		/// The format key of a block that is never reused. No format list hashes
		/// to it, so it matches nothing, itself included.
		constexpr auto not_reusable = std::uint64_t{ 0U };

		/// Folds one value into a running key through splitmix64's finalizer.
		[[nodiscard]] constexpr auto mixed(const std::uint64_t aKey, const std::uint64_t aValue) noexcept -> std::uint64_t
		{
			auto value = aKey ^ (aValue + 0x9E37'79B9'7F4A'7C15ULL + (aKey << 6U) + (aKey >> 2U));
			value      = (value ^ (value >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
			value      = (value ^ (value >> 27U)) * 0x94D0'49BB'1331'11EBULL;
			return value ^ (value >> 31U);
		}

		/// \brief The character format properties a block's layout reads.
		///
		/// \details `QTextEngine` builds each run's font from the format's font
		/// properties, scales and shifts the run by its vertical alignment and
		/// baseline properties, and sizes an inline object by its type and
		/// index. Colours, outlines, underline colours, tool tips and anchors are
		/// only painted, so they are not keyed: recolouring a block reuses it.
		/// Underline, overline and strike-out are painted too, but Qt reads them
		/// into the font, so they are keyed with it.
		constexpr auto layout_properties = std::array{
			QTextFormat::ObjectIndex,
			QTextFormat::FontCapitalization,
			QTextFormat::FontLetterSpacing,
			QTextFormat::FontWordSpacing,
			QTextFormat::FontStyleHint,
			QTextFormat::FontStyleStrategy,
			QTextFormat::FontKerning,
			QTextFormat::FontHintingPreference,
			QTextFormat::FontFamilies,
			QTextFormat::FontStyleName,
			QTextFormat::FontLetterSpacingType,
			QTextFormat::FontStretch,
			QTextFormat::FontFamily,
			QTextFormat::FontPointSize,
			QTextFormat::FontSizeAdjustment,
			QTextFormat::FontWeight,
			QTextFormat::FontItalic,
			QTextFormat::FontUnderline,
			QTextFormat::FontOverline,
			QTextFormat::FontStrikeOut,
			QTextFormat::FontFixedPitch,
			QTextFormat::FontPixelSize,
			QTextFormat::FontFeatures,
			QTextFormat::FontVariableAxes,
			QTextFormat::TextVerticalAlignment,
			QTextFormat::TextUnderlineStyle,
			QTextFormat::TextSuperScriptBaseline,
			QTextFormat::TextSubScriptBaseline,
			QTextFormat::TextBaselineOffset,
			QTextFormat::OldFontLetterSpacingType,
			QTextFormat::OldFontStretch,
			QTextFormat::ObjectType,
		};
		// A Qt that adds a font property must add it to the list above.
		static_assert(QTextFormat::FirstFontProperty == QTextFormat::FontCapitalization
						  && QTextFormat::LastFontProperty == QTextFormat::FontVariableAxes,
					  "Qt's font properties changed: review layout_properties");

		[[nodiscard]] auto text_key(std::uint64_t aKey, const QString& aText) noexcept -> std::uint64_t
		{
			aKey = mixed(aKey, static_cast<std::uint64_t>(aText.size()));
			for (const auto unit : aText) {
				aKey = mixed(aKey, unit.unicode());
			}
			return aKey;
		}

		/// A property value's key, or none for a type the key cannot read.
		[[nodiscard]] auto value_key(const QVariant& aValue) -> std::optional<std::uint64_t>
		{
			const auto type = aValue.typeId();
			auto       key  = mixed(0U, static_cast<std::uint64_t>(type));
			switch (type) {
			case QMetaType::Bool:
			case QMetaType::Int:
			case QMetaType::UInt:
			case QMetaType::LongLong:
				return mixed(key, static_cast<std::uint64_t>(aValue.toLongLong()));
			case QMetaType::ULongLong:
				return mixed(key, aValue.toULongLong());
			case QMetaType::Float:
			case QMetaType::Double:
				return mixed(key, std::bit_cast<std::uint64_t>(aValue.toDouble()));
			case QMetaType::QString:
				return text_key(key, aValue.toString());
			case QMetaType::QStringList: {
				const auto texts = aValue.toStringList();
				key              = mixed(key, static_cast<std::uint64_t>(texts.size()));
				for (const auto& text : texts) {
					key = text_key(key, text);
				}
				return key;
			}
			default:
				return std::nullopt;
			}
		}

		/// One range's position, length and layout properties folded into the
		/// key, or none if a property's value cannot be keyed.
		[[nodiscard]] auto range_key(std::uint64_t aKey, const QTextLayout::FormatRange& aRange) -> std::optional<std::uint64_t>
		{
			aKey = mixed(mixed(aKey, static_cast<std::uint32_t>(aRange.start)), static_cast<std::uint32_t>(aRange.length));
			// Syntax commonly carries only a foreground. Visit the properties
			// actually present, in Qt's stable property order, instead of asking
			// Qt for every possible font property on every coloured range.
			const auto properties = aRange.format.properties();
			for (auto property = properties.cbegin(); property != properties.cend(); ++property) {
				if (std::ranges::find(layout_properties, property.key()) == layout_properties.end()) {
					continue;
				}
				const auto valueKey = value_key(property.value());
				if (!valueKey) {
					return std::nullopt;
				}
				aKey = mixed(mixed(aKey, static_cast<std::uint32_t>(property.key())), *valueKey);
			}
			return aKey;
		}

		/// \brief A 64-bit key over a block's additional formats.
		///
		/// \details [[BPR-1142]]. It folds in each range's position and length,
		/// which decide where the layout splits its runs, and the properties its
		/// layout reads (`layout_properties`), in order, because later ranges
		/// override earlier ones. Two lists that differ there almost surely
		/// differ in key: two differing lists share one with probability 2^-64,
		/// and a block that did would keep its old height and width, which
		/// misplaces the comparison axis below it until the pane is measured
		/// afresh. A list with a value the key cannot read is `not_reusable`.
		[[nodiscard]] auto format_key(const QList<QTextLayout::FormatRange>& aFormats) -> std::uint64_t
		{
			auto key = std::optional{ mixed(0U, static_cast<std::uint64_t>(aFormats.size())) };
			for (auto range = aFormats.cbegin(); key && range != aFormats.cend(); ++range) {
				key = range_key(*key, *range);
			}
			if (!key) {
				return not_reusable;
			}
			return *key != not_reusable ? *key : not_reusable + 1U;
		}

		/// A block's extent as a transient layout measured it, without the
		/// document margin.
		struct block_extent
		{
			qreal Bottom       = 0.0;
			qreal NaturalWidth = 0.0;
		};

		/// \brief One unwrapped block as a transient layout measured it.
		///
		/// \details [[BPR-1123]], compacted by [[BPR-1142]]. The key stands for
		/// the block's additional formats. Its height and width are in layout
		/// units and leave out the document margin, which is added back where
		/// they are read, so every value a layout produces is held exactly. A
		/// block measured natively (hidden, or not unwrapped), or with a value
		/// or format the record cannot hold, has the `not_reusable` key.
		struct recorded_block
		{
			std::uint64_t FormatKey = not_reusable;
			std::int32_t  Height    = 0;
			std::int32_t  Width     = 0;
		};
		static_assert(sizeof(recorded_block) == 16U, "a recorded block is 16 bytes a line");

		/// The block as the record keeps it: `not_reusable` if a value would not
		/// come back exactly.
		[[nodiscard]] auto compact(const std::uint64_t aKey, const block_extent& aExtent) noexcept -> recorded_block
		{
			const auto height = to_layout_units(aExtent.Bottom);
			const auto width  = to_layout_units(aExtent.NaturalWidth);
			if (!height || !width) {
				return recorded_block{ };
			}
			return recorded_block{ .FormatKey = aKey, .Height = *height, .Width = *width };
		}

		/// \brief Measures pane blocks without keeping a shaped layout in each one.
		///
		/// \details [[BPR-1032]]. Since Qt 6.11 (qtbase 8209078e0e) a
		/// `QTextEngine` keeps its HarfBuzz buffer until it is destroyed, and a
		/// `QPlainTextEdit` keeps one layout per block for the block's
		/// lifetime. Asking the pane's own layout for every block's height
		/// therefore left about 1.8 KiB in every measured line: 70 MiB for the
		/// large review's 40,000.
		///
		/// For a `NoWrap` pane this lays the block out in a `QTextLayout` of
		/// its own, bound to the same block so it reads the same document
		/// formats and paint device, with the pane's additional formats. It
		/// repeats the steps `QPlainTextDocumentLayout::layoutBlock` and
		/// `blockBoundingRect` take, then destroys the layout. The height is
		/// the pane's own to the last bit; `measured_alignment_axis_tests`
		/// compares every line against the pane's native answer.
		///
		/// The pane still has to know its widest line, because its
		/// horizontal range comes from the lines it has laid out itself. So
		/// a block that is wider than any before it is laid out natively,
		/// which leaves the pane's range where measuring every block
		/// natively left it. Wrapping panes and hidden blocks are measured
		/// natively as before: there, laying a block out also sets its line
		/// count, which the pane's own scrolling reads.
		///
		/// [[BPR-1123]]. Nothing keeps a transient answer, so a pane's next
		/// measurement would shape every block again. Given the pane's
		/// recorded blocks, this reuses a block's recorded height and width
		/// while its additional formats have the key it was measured with
		/// ([[BPR-1142]]), and records every block it shapes. A reused width
		/// still goes through the widest-line rule, so a relayout that emptied
		/// the pane's own layouts gets its horizontal range back exactly as a
		/// fresh measurement would.
		class pane_block_measurer final
		{
		public:
			pane_block_measurer() = default;
			pane_block_measurer(const QAbstractTextDocumentLayout& aLayout, const bool aNoWrap)
				: Native{ &aLayout }
				, Plain{ aNoWrap ? qobject_cast<const QPlainTextDocumentLayout*>(&aLayout) : nullptr }
			{
			}

			/// Reads and records blocks in `aRecorded`, which outlives this.
			void record_into(std::vector<recorded_block>& aRecorded) noexcept
			{
				Recorded = &aRecorded;
			}

			void use_peer(const QTextDocument* aDocument, const std::vector<recorded_block>* aRecorded) noexcept
			{
				PeerDocument = aDocument;
				PeerRecorded = aRecorded;
			}

			[[nodiscard]] auto reused() const noexcept -> std::uint32_t
			{
				return Reused;
			}

			/// \param aIndex The block's number, which is also its line.
			[[nodiscard]] auto height(const QTextBlock& aBlock, const std::size_t aIndex) -> qreal
			{
				if (Plain == nullptr) {
					return Native->blockBoundingRect(aBlock).height();
				}
				if (!aBlock.isVisible()
					|| aBlock.document()->defaultTextOption().wrapMode() != QTextOption::NoWrap) {
					record(aIndex, recorded_block{ });
					return Native->blockBoundingRect(aBlock).height();
				}
				// A block's layout exists whenever a highlighter has styled it,
				// and asking for it shapes nothing.
				const auto formats = aBlock.layout()->formats();
				const auto key     = format_key(formats);
				auto       extent  = block_extent{ };
				if (const auto* const known = recorded(aIndex, key); known != nullptr) {
					extent = { .Bottom = from_layout_units(known->Height), .NaturalWidth = from_layout_units(known->Width) };
					++Reused;
				} else if (const auto peer = peer_extent(aBlock, aIndex, key, formats)) {
					extent = *peer;
					record(aIndex, compact(key, extent));
					++Reused;
				} else {
					extent = transient_extent(aBlock, formats);
					record(aIndex, compact(key, extent));
				}
				// The margins are added as `QPlainTextDocumentLayout` adds them.
				const auto margin = aBlock.document()->documentMargin();
				const auto width  = std::max(qreal{ 0.0 }, extent.NaturalWidth + 2.0 * margin);
				if (width > Widest) {
					Widest = width;
					return Native->blockBoundingRect(aBlock).height();
				}
				return aBlock.next().isValid() ? extent.Bottom : extent.Bottom + margin;
			}

		private:
			/// The recorded block, if it was measured with formats of this key.
			[[nodiscard]] auto recorded(const std::size_t aIndex, const std::uint64_t aKey) const noexcept -> const recorded_block*
			{
				if (Recorded == nullptr || aIndex >= Recorded->size() || aKey == not_reusable) {
					return nullptr;
				}
				const auto& block = (*Recorded)[aIndex];
				return block.FormatKey == aKey ? &block : nullptr;
			}

			void record(const std::size_t aIndex, const recorded_block& aBlock) noexcept
			{
				if (Recorded != nullptr && aIndex < Recorded->size()) {
					(*Recorded)[aIndex] = aBlock;
				}
			}

			/// The complete plain-row case; all other formatting shapes normally.
			[[nodiscard]] static auto plain_text(const QTextBlock& aBlock, const QList<QTextLayout::FormatRange>& aFormats) -> std::optional<QString>
			{
				const auto text   = aBlock.text();
				const auto option = aBlock.document()->defaultTextOption();
				if (option.wrapMode() != QTextOption::NoWrap || !aFormats.empty() || text.isEmpty() || text.endsWith(QLatin1Char{ ' ' })
					|| !aBlock.charFormat().isEmpty() || !aBlock.blockFormat().isEmpty()
					|| option.flags() != QTextOption::Flags{ } || option.useDesignMetrics()
					|| option.alignment() != Qt::AlignLeft || option.textDirection() == Qt::RightToLeft
					|| !std::ranges::all_of(text, [](const QChar aCharacter) { return aCharacter.unicode() >= 0x20U && aCharacter.unicode() <= 0x7EU; })) {
					return std::nullopt;
				}
				auto fragment = aBlock.begin();
				if (fragment.atEnd() || !fragment.fragment().charFormat().isEmpty()) {
					return std::nullopt;
				}
				++fragment;
				if (!fragment.atEnd()) {
					return std::nullopt;
				}
				return text;
			}

			/// Reuses the other pane's exact recorded extent only for identical
			/// entirely plain rows. Its owner checks all pane-level facts first.
			[[nodiscard]] auto peer_extent(const QTextBlock& aBlock, const std::size_t aIndex, const std::uint64_t aKey, const QList<QTextLayout::FormatRange>& aFormats) const -> std::optional<block_extent>
			{
				if (!PeerDocument || PeerRecorded == nullptr || aIndex >= PeerRecorded->size()) {
					return std::nullopt;
				}
				const auto& known = (*PeerRecorded)[aIndex];
				if (aKey == not_reusable || known.FormatKey != aKey) {
					return std::nullopt;
				}
				const auto peer = PeerDocument->findBlockByNumber(static_cast<int>(aIndex));
				const auto text = plain_text(aBlock, aFormats);
				if (!peer.isValid() || !text || plain_text(peer, peer.layout()->formats()) != text) {
					return std::nullopt;
				}
				return block_extent{ .Bottom = from_layout_units(known.Height), .NaturalWidth = from_layout_units(known.Width) };
			}

			/// [[BPR-1525]]. One Qt engine still shapes each distinct plain row.
			[[nodiscard]] auto reusable_plain_layout(const QTextBlock& aBlock, const QList<QTextLayout::FormatRange>& aFormats) -> QTextLayout*
			{
				const auto text = plain_text(aBlock, aFormats);
				if (!text) {
					return nullptr;
				}
				const auto  font   = aBlock.document()->defaultFont();
				const auto* device = Native->paintDevice();
				if (!PlainLayout || PlainFont != font || PlainDevice != device) {
					PlainLayout = std::make_unique<QTextLayout>(QString{ }, font, device);
					PlainFont   = font;
					PlainDevice = device;
				}
				PlainLayout->setText(*text);
				return PlainLayout.get();
			}

			[[nodiscard]] auto transient_extent(const QTextBlock& aBlock, const QList<QTextLayout::FormatRange>& aFormats) -> block_extent
			{
				const auto* const document = aBlock.document();
				const auto        margin   = document->documentMargin();
				const auto        option   = document->defaultTextOption();
				auto              local    = std::optional<QTextLayout>{ };
				auto*             layout   = reusable_plain_layout(aBlock, aFormats);
				if (layout == nullptr) {
					layout = &local.emplace(aBlock);
				}
				layout->setTextOption(option);
				// Qt's setFormats also marks its bound document dirty. A fresh
				// plain layout already has no formats, so leave it alone.
				if (!aFormats.empty()) {
					layout->setFormats(aFormats);
				}
				auto extraMargin = 0;
				if (option.flags().testFlag(QTextOption::AddSpaceForLineAndParagraphSeparators)) {
					extraMargin += QFontMetrics{ aBlock.charFormat().font() }.horizontalAdvance(QChar{ 0x21B5 });
				}
				// The pane gives each line its viewport's width, but an unwrapped
				// line is one line whatever width it is given, and the width
				// reaches neither its height nor its natural width.
				const auto available = static_cast<qreal>(INT_MAX) - (2.0 * margin + extraMargin);
				auto       top       = qreal{ 0.0 };
				auto       extent    = block_extent{ };
				layout->beginLayout();
				for (auto line = layout->createLine(); line.isValid(); line = layout->createLine()) {
					line.setLeadingIncluded(true);
					line.setLineWidth(available);
					line.setPosition(QPointF{ margin, top });
					top += line.height();
					if (line.leading() < 0.0) {
						top += std::ceil(line.leading());
					}
					extent.NaturalWidth = std::max(extent.NaturalWidth, line.naturalTextWidth());
				}
				layout->endLayout();
				extent.Bottom = layout->boundingRect().bottom();
				return extent;
			}

			QPointer<const QTextDocument>      PeerDocument;
			const std::vector<recorded_block>* PeerRecorded = nullptr;

			std::unique_ptr<QTextLayout> PlainLayout;
			QFont                        PlainFont;
			const QPaintDevice*          PlainDevice = nullptr;

			const QAbstractTextDocumentLayout* Native = nullptr;
			const QPlainTextDocumentLayout*    Plain  = nullptr;
			// Reconstruct the maximum from this measurement's actual extents.
			// Qt can retain the previous font's larger maximum until its old
			// widest block is laid out again; it must not suppress that visit.
			qreal                        Widest   = 0.0;
			std::vector<recorded_block>* Recorded = nullptr;
			std::uint32_t                Reused   = 0U;
		};

		[[nodiscard]] auto finite_nonnegative(const qreal aValue) noexcept -> bool
		{
			return std::isfinite(aValue) && aValue >= 0.0;
		}

		[[nodiscard]] auto valid_measurements(
			const pane_line_measurements& aMeasurements,
			const std::uint32_t           aLineCount) noexcept -> bool
		{
			if (aMeasurements.Boundaries.size()
					!= static_cast<std::size_t>(aLineCount) + 1U
				|| !finite_nonnegative(aMeasurements.DocumentExtent)
				|| !finite_nonnegative(aMeasurements.ViewportExtent)
				|| !std::isfinite(aMeasurements.DevicePixelRatio)
				|| aMeasurements.DevicePixelRatio <= 0.0) {
				return false;
			}

			if (!std::ranges::all_of(
					aMeasurements.Boundaries,
					[](const qreal aBoundary) {
						return finite_nonnegative(aBoundary);
					})) {
				return false;
			}

			return std::ranges::is_sorted(aMeasurements.Boundaries)
				&& aMeasurements.Boundaries.back()
				<= aMeasurements.DocumentExtent;
		}

		[[nodiscard]] auto project_within(
			const measured_alignment_segment& aSegment,
			const qreal                       aPosition) noexcept
			-> measured_alignment_projection
		{
			const auto extent = aSegment.ComparisonEnd
				- aSegment.ComparisonBegin;
			const auto ratio = extent > 0.0
				? std::clamp(
					  (aPosition - aSegment.ComparisonBegin) / extent,
					  0.0,
					  1.0)
				: 0.0;
			return {
				.Segment            = aSegment.Id,
				.ComparisonPosition = aPosition,
				.LeftDocumentY      = std::lerp(
					aSegment.LeftBegin, aSegment.LeftEnd, ratio),
				.RightDocumentY = std::lerp(
					aSegment.RightBegin, aSegment.RightEnd, ratio),
			};
		}
	} // namespace

	/// [[BPR-1123]]. The recorded blocks, one for each of the document's blocks,
	/// and every fact their heights depend on.
	struct pane_block_heights::state
	{
		QPointer<const QTextDocument>               Document;
		QPointer<const QAbstractTextDocumentLayout> Layout;
		const QPaintDevice*                         Device   = nullptr;
		int                                         Revision = 0;
		QFont                                       Font;
		QFont                                       DocumentFont;
		qreal                                       DevicePixelRatio = 0.0;
		qreal                                       Margin           = 0.0;
		qreal                                       TabDistance      = 0.0;
		std::vector<recorded_block>                 Blocks;

		/// Keeps the recorded blocks only if the pane's facts are the ones they
		/// were measured under; otherwise takes these facts and starts one
		/// `not_reusable` block for each of the document's blocks.
		[[nodiscard]] auto matches(const QPlainTextEdit& aPane) const -> bool
		{
			const auto* const document = aPane.document();
			const auto        count    = static_cast<std::size_t>(document->blockCount());
			return Document == document && Revision == document->revision() && Blocks.size() == count
				&& Font == aPane.font() && DocumentFont == document->defaultFont()
				&& DevicePixelRatio == aPane.devicePixelRatioF() && Margin == document->documentMargin()
				&& TabDistance == aPane.tabStopDistance() && Layout == document->documentLayout() && Device == document->documentLayout()->paintDevice();
		}

		void bind(const QPlainTextEdit& aPane)
		{
			const auto* const document = aPane.document();
			const auto        count    = static_cast<std::size_t>(document->blockCount());
			if (matches(aPane)) {
				return;
			}
			Blocks.assign(count, recorded_block{ });
			Document         = document;
			Layout           = document->documentLayout();
			Device           = Layout->paintDevice();
			Revision         = document->revision();
			Font             = aPane.font();
			DocumentFont     = document->defaultFont();
			DevicePixelRatio = aPane.devicePixelRatioF();
			Margin           = document->documentMargin();
			TabDistance      = aPane.tabStopDistance();
		}
	};

	pane_block_heights::pane_block_heights()
		: State{ std::make_unique<state>() }
	{
	}
	pane_block_heights::~pane_block_heights()                                                = default;
	pane_block_heights::pane_block_heights(pane_block_heights&&) noexcept                    = default;
	auto pane_block_heights::operator=(pane_block_heights&&) noexcept -> pane_block_heights& = default;

	void pane_block_heights::clear() noexcept
	{
		if (State) {
			// `std::vector::clear` would keep a large file's worth of capacity
			// after its comparison has gone ([[BPR-1142]]). A document always has
			// a block, so the next measurement binds afresh.
			State->Blocks = std::vector<recorded_block>{ };
		}
	}

	struct plain_text_pane_measurement::state
	{
		QPointer<const QPlainTextEdit>              Pane;
		QPointer<const QTextDocument>               Document;
		QPointer<const QAbstractTextDocumentLayout> Layout;
		QTextBlock                                  Block;
		QFont                                       Font;
		QFont                                       DocumentFont;
		QSize                                       ViewportSize;
		int                                         Revision     = 0;
		int                                         WrapMode     = 0;
		int                                         WordWrapMode = 0;
		qreal                                       Margin       = 0.0;
		qreal                                       TextWidth    = 0.0;
		qreal                                       TabDistance  = 0.0;
		std::uint32_t                               LineCount    = 0U;
		std::uint32_t                               NextLine     = 0U;
		pane_measurement_progress                   Progress     = pane_measurement_progress::invalidated;
		pane_line_measurements                      Measurements;
		pane_block_measurer                         Measurer;
		QPointer<const QPlainTextEdit>              PeerPane;
		const pane_block_heights::state*            PeerHeights = nullptr;

		void admit_peer()
		{
			const auto usable = PeerPane && PeerHeights && PeerHeights->matches(*PeerPane)
				&& WrapMode == QPlainTextEdit::NoWrap && PeerPane->lineWrapMode() == QPlainTextEdit::NoWrap
				&& Font == PeerHeights->Font && DocumentFont == PeerHeights->DocumentFont
				&& Measurements.DevicePixelRatio == PeerHeights->DevicePixelRatio
				&& Margin == PeerHeights->Margin && TabDistance == PeerHeights->TabDistance
				&& Layout->paintDevice() == PeerHeights->Device
				&& Pane->layoutDirection() == PeerPane->layoutDirection();
			Measurer.use_peer(usable ? PeerHeights->Document.data() : nullptr, usable ? &PeerHeights->Blocks : nullptr);
		}

		[[nodiscard]] auto current() const -> bool
		{
			return Pane && Document && Layout && Pane->document() == Document
				&& Document->documentLayout() == Layout && Document->revision() == Revision
				&& Pane->viewport() && Pane->viewport()->size() == ViewportSize
				&& Pane->devicePixelRatioF() == Measurements.DevicePixelRatio
				&& Pane->font() == Font && Document->defaultFont() == DocumentFont
				&& Document->documentMargin() == Margin && Document->textWidth() == TextWidth
				&& Pane->tabStopDistance() == TabDistance
				&& static_cast<int>(Pane->lineWrapMode()) == WrapMode
				&& static_cast<int>(Pane->wordWrapMode()) == WordWrapMode;
		}
	};

	plain_text_pane_measurement::plain_text_pane_measurement(
		const QPlainTextEdit& aPane, const std::uint32_t aLogicalLineCount)
		: State{ std::make_unique<state>() }
	{
		const auto* document = aPane.document();
		const auto* viewport = aPane.viewport();
		const auto* layout   = document ? document->documentLayout() : nullptr;
		if (!document || !viewport || !layout
			|| aLogicalLineCount > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
			return;
		}
		const auto count = static_cast<std::uint32_t>(document->blockCount());
		if (count < aLogicalLineCount || count > aLogicalLineCount + 1U
			|| (count == aLogicalLineCount + 1U && !document->lastBlock().text().isEmpty())) {
			return;
		}
		auto& value        = *State;
		value.Pane         = &aPane;
		value.Document     = document;
		value.Layout       = layout;
		value.Block        = document->begin();
		value.Font         = aPane.font();
		value.DocumentFont = document->defaultFont();
		value.ViewportSize = viewport->size();
		value.Revision     = document->revision();
		value.WrapMode     = static_cast<int>(aPane.lineWrapMode());
		value.WordWrapMode = static_cast<int>(aPane.wordWrapMode());
		value.Margin       = document->documentMargin();
		value.TextWidth    = document->textWidth();
		value.TabDistance  = aPane.tabStopDistance();
		value.LineCount    = aLogicalLineCount;
		value.Measurer     = pane_block_measurer{ *layout, aPane.lineWrapMode() == QPlainTextEdit::NoWrap };
		value.Measurements = { .DocumentExtent      = layout->documentSize().height(),
							   .ViewportExtent      = static_cast<qreal>(viewport->height()),
							   .DevicePixelRatio    = aPane.devicePixelRatioF(),
							   .TrailingVisualBlock = count == aLogicalLineCount + 1U };
		value.Measurements.Boundaries.reserve(static_cast<std::size_t>(aLogicalLineCount) + 1U);
		value.Measurements.Boundaries.push_back(value.Margin);
		value.Progress = pane_measurement_progress::pending;
	}

	plain_text_pane_measurement::plain_text_pane_measurement(
		const QPlainTextEdit& aPane, const std::uint32_t aLogicalLineCount, pane_block_heights& aHeights)
		: plain_text_pane_measurement{ aPane, aLogicalLineCount }
	{
		if (State->Progress != pane_measurement_progress::pending || !aHeights.State) {
			return;
		}
		aHeights.State->bind(aPane);
		State->Measurer.record_into(aHeights.State->Blocks);
	}

	plain_text_pane_measurement::plain_text_pane_measurement(const QPlainTextEdit& aPane, const std::uint32_t aLogicalLineCount, pane_block_heights& aHeights, const QPlainTextEdit& aPeer, const pane_block_heights& aPeerHeights)
		: plain_text_pane_measurement{ aPane, aLogicalLineCount, aHeights }
	{
		State->PeerPane    = &aPeer;
		State->PeerHeights = aPeerHeights.State.get();
	}

	plain_text_pane_measurement::~plain_text_pane_measurement()                                                         = default;
	plain_text_pane_measurement::plain_text_pane_measurement(plain_text_pane_measurement&&) noexcept                    = default;
	auto plain_text_pane_measurement::operator=(plain_text_pane_measurement&&) noexcept -> plain_text_pane_measurement& = default;

	auto plain_text_pane_measurement::advance(const std::uint32_t aMaximumLines) -> pane_measurement_progress
	{
		if (!State || State->Progress == pane_measurement_progress::invalidated) {
			return pane_measurement_progress::invalidated;
		}
		auto& value = *State;
		if (!value.current()) {
			value.Progress = pane_measurement_progress::invalidated;
			return value.Progress;
		}
		if (value.Progress == pane_measurement_progress::complete) {
			return value.Progress;
		}
		value.admit_peer();
		const auto count = std::min(aMaximumLines, value.LineCount - value.NextLine);
		for (std::uint32_t index = 0U; index < count; ++index) {
			if (!value.Block.isValid()) {
				value.Progress = pane_measurement_progress::invalidated;
				return value.Progress;
			}
			value.Measurements.Boundaries.push_back(value.Measurements.Boundaries.back()
													+ value.Measurer.height(value.Block, value.NextLine));
			value.Block = value.Block.next();
			++value.NextLine;
		}
		// Native layout can synchronously change scrollbars and viewport size.
		// Reject that batch instead of mixing measurements from two layouts.
		if (!value.current()) {
			value.Progress = pane_measurement_progress::invalidated;
		} else if (value.NextLine == value.LineCount) {
			value.Measurements.DocumentExtent = std::max(value.Measurements.DocumentExtent,
														 value.Measurements.Boundaries.back());
			value.Progress                    = valid_measurements(value.Measurements, value.LineCount)
				? pane_measurement_progress::complete
				: pane_measurement_progress::invalidated;
		}
		return value.Progress;
	}

	auto plain_text_pane_measurement::measured_lines() const noexcept -> std::uint32_t
	{
		return State ? State->NextLine : 0U;
	}

	auto plain_text_pane_measurement::reused_lines() const noexcept -> std::uint32_t
	{
		return State ? State->Measurer.reused() : 0U;
	}

	auto plain_text_pane_measurement::result() const -> const pane_line_measurements*
	{
		return State && State->Progress == pane_measurement_progress::complete && State->current()
			? &State->Measurements
			: nullptr;
	}

	auto measure_plain_text_pane(
		const QPlainTextEdit& aPane,
		const std::uint32_t   aLogicalLineCount)
		-> std::optional<pane_line_measurements>
	{
		const auto* const document = aPane.document();
		const auto* const viewport = aPane.viewport();
		const auto* const layout   = document != nullptr
			? document->documentLayout()
			: nullptr;
		if (document == nullptr || viewport == nullptr
			|| layout == nullptr
			|| aLogicalLineCount
				> static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
			return std::nullopt;
		}
		const auto blockCount = static_cast<std::uint32_t>(document->blockCount());
		if (blockCount < aLogicalLineCount
			|| blockCount > aLogicalLineCount + 1U
			|| (blockCount == aLogicalLineCount + 1U
				&& !document->lastBlock().text().isEmpty())) {
			return std::nullopt;
		}

		auto measurements = pane_line_measurements{
			.DocumentExtent      = layout->documentSize().height(),
			.ViewportExtent      = static_cast<qreal>(viewport->height()),
			.DevicePixelRatio    = aPane.devicePixelRatioF(),
			.TrailingVisualBlock = blockCount == aLogicalLineCount + 1U,
		};
		measurements.Boundaries.reserve(
			static_cast<std::size_t>(aLogicalLineCount) + 1U);
		auto documentY = document->documentMargin();
		measurements.Boundaries.push_back(documentY);
		auto measurer = pane_block_measurer{ *layout, aPane.lineWrapMode() == QPlainTextEdit::NoWrap };
		auto block    = document->begin();
		for (std::uint32_t line = 0U;
			 line < aLogicalLineCount;
			 ++line) {
			if (!block.isValid()) {
				return std::nullopt;
			}
			documentY += measurer.height(block, line);
			measurements.Boundaries.push_back(documentY);
			block = block.next();
		}
		measurements.DocumentExtent = std::max(
			measurements.DocumentExtent, measurements.Boundaries.back());

		return valid_measurements(measurements, aLogicalLineCount)
			? std::optional<pane_line_measurements>{ std::move(measurements) }
			: std::nullopt;
	}

	auto measured_alignment_axis::left_line_origin(
		const std::size_t aLine) const noexcept -> std::optional<qreal>
	{
		return aLine < LeftBoundaries.size()
			? std::optional<qreal>{ LeftBoundaries[aLine] }
			: std::nullopt;
	}

	auto measured_alignment_axis::right_line_origin(
		const std::size_t aLine) const noexcept -> std::optional<qreal>
	{
		return aLine < RightBoundaries.size()
			? std::optional<qreal>{ RightBoundaries[aLine] }
			: std::nullopt;
	}

	auto measured_alignment_axis::last_line_origin(
		const core::diff_side aSide) const noexcept -> qreal
	{
		const auto& boundaries = aSide == core::diff_side::left ? LeftBoundaries : RightBoundaries;
		const auto  last       = aSide == core::diff_side::left ? LeftLastLine : RightLastLine;
		return last == 0U || last >= boundaries.size() ? 0.0 : boundaries[last];
	}

	auto measured_alignment_axis::top_line_for_origin(
		const core::diff_side aSide, const qreal aOrigin) const noexcept -> std::size_t
	{
		const auto& boundaries = aSide == core::diff_side::left ? LeftBoundaries : RightBoundaries;
		const auto  last       = std::min(
			aSide == core::diff_side::left ? LeftLastLine : RightLastLine,
			boundaries.empty() ? std::size_t{ 0U } : boundaries.size() - 1U);
		if (last == 0U || !std::isfinite(aOrigin)) {
			return 0U;
		}
		const auto origin = [&boundaries](const std::size_t aLine) {
			return aLine == 0U ? 0.0 : boundaries[aLine];
		};
		// The last line whose origin is at or above the target, then whichever
		// of it and the line after is nearer.
		const auto lines = std::span{ boundaries }.subspan(1U, last);
		const auto above = static_cast<std::size_t>(
			std::ranges::upper_bound(lines, aOrigin) - lines.begin());
		if (above >= last) {
			return last;
		}
		const auto after = above + 1U;
		return aOrigin - origin(above) <= origin(after) - aOrigin ? above : after;
	}

	auto measured_alignment_segment::positive_extent() const noexcept -> bool
	{
		return ComparisonEnd > ComparisonBegin;
	}

	auto measured_alignment_axis::comparison() const noexcept
		-> core::file_comparison_digest
	{
		return Comparison;
	}

	auto measured_alignment_axis::segments() const noexcept
		-> std::span<const measured_alignment_segment>
	{
		return Segments;
	}

	auto measured_alignment_axis::positive_segment_count() const noexcept
		-> std::size_t
	{
		return PositiveSegments.size();
	}

	auto measured_alignment_axis::comparison_extent() const noexcept -> qreal
	{
		return ComparisonExtent;
	}

	auto measured_alignment_axis::left_document_extent() const noexcept -> qreal
	{
		return LeftDocumentExtent;
	}

	auto measured_alignment_axis::right_document_extent() const noexcept -> qreal
	{
		return RightDocumentExtent;
	}

	auto measured_alignment_axis::left_viewport_extent() const noexcept -> qreal
	{
		return LeftViewportExtent;
	}

	auto measured_alignment_axis::right_viewport_extent() const noexcept -> qreal
	{
		return RightViewportExtent;
	}

	auto measured_alignment_axis::clamp(const qreal aPosition) const noexcept -> qreal
	{
		if (!std::isfinite(aPosition)) {
			return 0.0;
		}
		return std::clamp(aPosition, 0.0, ComparisonExtent);
	}

	auto measured_alignment_axis::project(const qreal aPosition) const noexcept
		-> std::optional<measured_alignment_projection>
	{
		if (!std::isfinite(aPosition)) {
			return std::nullopt;
		}

		const auto position = clamp(aPosition);
		if (PositiveSegments.empty()) {
			return measured_alignment_projection{
				.ComparisonPosition = position,
				.LeftDocumentY      = LeftDocumentBegin,
				.RightDocumentY     = RightDocumentBegin,
			};
		}

		const auto found = position >= ComparisonExtent
			? std::prev(PositiveSegments.end())
			: std::ranges::upper_bound(
				  PositiveSegments,
				  position,
				  std::ranges::less{ },
				  [this](const std::size_t aIndex) {
					  return Segments[aIndex].ComparisonEnd;
				  });
		return project_within(Segments[*found], position);
	}

	auto build_measured_alignment_axis(
		const core::file_alignment&   aAlignment,
		const pane_line_measurements& aLeft,
		const pane_line_measurements& aRight)
		-> measured_alignment_build_result
	{
		if (!core::validate_alignment(aAlignment).valid()
			|| aAlignment.Content != core::diff_content_state::text) {
			return { .Error = measured_alignment_error::invalid_alignment };
		}
		if (aLeft.Boundaries.size()
				!= static_cast<std::size_t>(aAlignment.LeftLineCount) + 1U
			|| aRight.Boundaries.size()
				!= static_cast<std::size_t>(aAlignment.RightLineCount) + 1U) {
			return { .Error = measured_alignment_error::line_count_mismatch };
		}
		if (!valid_measurements(aLeft, aAlignment.LeftLineCount)
			|| !valid_measurements(aRight, aAlignment.RightLineCount)) {
			return { .Error = measured_alignment_error::invalid_pane_geometry };
		}

		auto axis                = measured_alignment_axis{ };
		axis.Comparison          = aAlignment.Digest;
		axis.LeftBoundaries      = aLeft.Boundaries;
		axis.RightBoundaries     = aRight.Boundaries;
		axis.LeftDocumentBegin   = aLeft.Boundaries.front();
		axis.RightDocumentBegin  = aRight.Boundaries.front();
		axis.LeftDocumentExtent  = aLeft.DocumentExtent;
		axis.RightDocumentExtent = aRight.DocumentExtent;
		axis.LeftViewportExtent  = aLeft.ViewportExtent;
		axis.RightViewportExtent = aRight.ViewportExtent;
		const auto lastLine      = [](const pane_line_measurements& aPane, const std::uint32_t aLines) -> std::size_t {
			return aPane.TrailingVisualBlock || aLines == 0U ? aLines : aLines - 1U;
		};
		axis.LeftLastLine  = lastLine(aLeft, aAlignment.LeftLineCount);
		axis.RightLastLine = lastLine(aRight, aAlignment.RightLineCount);
		axis.Segments.reserve(aAlignment.Segments.size());
		axis.PositiveSegments.reserve(aAlignment.Segments.size());

		qreal comparison = 0.0;
		for (const auto& semantic : aAlignment.Segments) {
			const auto leftBegin  = aLeft.Boundaries[semantic.Left.Begin];
			const auto leftEnd    = aLeft.Boundaries[semantic.Left.End];
			const auto rightBegin = aRight.Boundaries[semantic.Right.Begin];
			const auto rightEnd   = aRight.Boundaries[semantic.Right.End];
			const auto extent     = std::max(
				leftEnd - leftBegin, rightEnd - rightBegin);
			const auto comparisonEnd = comparison + extent;
			if (!std::isfinite(comparisonEnd)) {
				return {
					.Error = measured_alignment_error::comparison_extent_overflow,
				};
			}

			axis.Segments.push_back(measured_alignment_segment{
				.Id              = semantic.Id,
				.Kind            = semantic.Kind,
				.Group           = semantic.Group,
				.LeftBegin       = leftBegin,
				.LeftEnd         = leftEnd,
				.RightBegin      = rightBegin,
				.RightEnd        = rightEnd,
				.ComparisonBegin = comparison,
				.ComparisonEnd   = comparisonEnd,
			});
			if (extent > 0.0) {
				axis.PositiveSegments.push_back(axis.Segments.size() - 1U);
			}
			comparison = comparisonEnd;
		}
		axis.ComparisonExtent = comparison;

		return {
			.Axis = std::move(axis),
		};
	}

	void measured_alignment_axis_cache::invalidate(
		const measured_geometry_change aChange) noexcept
	{
		if (aChange == measured_geometry_change::none) {
			return;
		}
		PendingChanges = PendingChanges | aChange;
		Axis.reset();
	}

	auto measured_alignment_axis_cache::rebuild(
		const core::file_alignment&   aAlignment,
		const pane_line_measurements& aLeft,
		const pane_line_measurements& aRight)
		-> measured_alignment_error
	{
		auto result = build_measured_alignment_axis(aAlignment, aLeft, aRight);
		if (!result.valid()) {
			Axis.reset();
			PendingChanges = PendingChanges
				| measured_geometry_change::document_layout;
			return result.Error;
		}

		Axis           = std::move(result.Axis);
		PendingChanges = measured_geometry_change::none;
		++GeometryGeneration;
		++RebuildCount;
		return measured_alignment_error::none;
	}

	auto measured_alignment_axis_cache::axis() const noexcept
		-> const measured_alignment_axis*
	{
		return dirty() ? nullptr : &*Axis;
	}

	auto measured_alignment_axis_cache::dirty() const noexcept -> bool
	{
		return PendingChanges != measured_geometry_change::none
			|| !Axis.has_value();
	}

	auto measured_alignment_axis_cache::pending_changes() const noexcept
		-> measured_geometry_change
	{
		return PendingChanges;
	}

	auto measured_alignment_axis_cache::geometry_generation() const noexcept
		-> std::uint64_t
	{
		return GeometryGeneration;
	}

	auto measured_alignment_axis_cache::rebuild_count() const noexcept
		-> std::uint64_t
	{
		return RebuildCount;
	}
} // namespace bpr::qt
