#include "bpr/qt/github_conversation_delegate.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextLayout>
#include <QTextOption>
#include <QWidget>
#include <QtCore/qhashfunctions.h>

#include <algorithm>
#include <cstddef>
#include <unordered_map>

namespace bpr::qt {
	namespace {
		struct conversation_presentation
		{
			QString Heading;
			QString Body;
			QString Facts;
			/// [[BPR-1083]]. The post's label, or -1; and its decoration.
			int                    Label    = -1;
			feedback_mark_emphasis Emphasis = feedback_mark_emphasis::undecorated;
		};

		/// \brief The room a mark takes at the start of the heading line.
		constexpr qreal mark_lead = 16.0 + 5.0;

		struct delegate_metrics
		{
			QFont HeadingFont;
			QFont BodyFont;
			QFont FactFont;
			int   HorizontalPadding = 0;
			int   VerticalPadding   = 0;
			int   SectionSpacing    = 0;
		};

		[[nodiscard]] auto author_text(const QModelIndex& aIndex) -> QString
		{
			const auto author = aIndex.data(github_conversation_model::author_login_role).toString();
			return author.isEmpty() ? QStringLiteral("Unknown author")
									: QStringLiteral("@%1").arg(author);
		}

		[[nodiscard]] auto review_state_text(const int aState) -> QString
		{
			using review_state = application::github_review_state_projection;
			switch (static_cast<review_state>(aState)) {
			case review_state::pending:
				return QStringLiteral("Pending");
			case review_state::commented:
				return QStringLiteral("Commented");
			case review_state::approved:
				return QStringLiteral("Approved");
			case review_state::changes_requested:
				return QStringLiteral("Changes requested");
			case review_state::dismissed:
				return QStringLiteral("Dismissed");
			case review_state::none:
				return { };
			}
			return { };
		}

		[[nodiscard]] auto heading_text(const QModelIndex& aIndex) -> QString
		{
			using conversation_kind = application::github_conversation_kind;
			const auto kind         = static_cast<conversation_kind>(
				aIndex.data(github_conversation_model::kind_role).toInt());
			const auto author = author_text(aIndex);

			switch (kind) {
			case conversation_kind::review:
				return QStringLiteral("Review by %1").arg(author);
			case conversation_kind::review_thread:
				return QStringLiteral("Review thread");
			case conversation_kind::review_comment:
				return aIndex.parent().isValid()
					? QStringLiteral("Reply by %1").arg(author)
					: QStringLiteral("Review comment by %1").arg(author);
			case conversation_kind::issue_comment:
				return QStringLiteral("Pull request comment by %1").arg(author);
			}
			return QStringLiteral("Conversation");
		}

		[[nodiscard]] auto location_text(const QModelIndex& aIndex) -> QString
		{
			auto       path = aIndex.data(github_conversation_model::path_role).toString();
			const auto line = aIndex.data(github_conversation_model::line_role);
			if (!line.isValid()) {
				return path;
			}

			if (path.isEmpty()) {
				return QStringLiteral("Line %1").arg(line.toULongLong());
			}
			path.append(QLatin1Char(':'));
			path.append(QString::number(line.toULongLong()));
			return path;
		}

		[[nodiscard]] auto presentation_for(const QModelIndex& aIndex, const bool aCompact)
			-> conversation_presentation
		{
			auto result = conversation_presentation{
				.Heading = heading_text(aIndex),
				.Body    = aIndex.data(github_conversation_model::body_role).toString(),
				.Facts   = { },
			};
			// [[BPR-1083]]. The mark stands in for a recognized prefix, so the
			// body starts at the subject; Copy source keeps the prefix.
			if (const auto label = aIndex.data(github_conversation_model::feedback_label_role); label.isValid()) {
				result.Label    = label.toInt();
				result.Emphasis = static_cast<feedback_mark_emphasis>(aIndex.data(github_conversation_model::feedback_emphasis_role).toInt());
				result.Body     = result.Body.mid(aIndex.data(github_conversation_model::feedback_subject_role).toLongLong());
			}

			if (aCompact) {
				result.Body = result.Body.simplified();
				if (result.Body.size() > 120) {
					result.Body = result.Body.left(120) + QChar{ 0x2026 };
				}
			}
			QStringList facts;
			if (const auto location = location_text(aIndex); !location.isEmpty()) {
				facts.push_back(location);
			}
			if (const auto reviewState = review_state_text(
					aIndex.data(github_conversation_model::review_state_role).toInt());
				!reviewState.isEmpty()) {
				facts.push_back(reviewState);
			}

			using conversation_kind = application::github_conversation_kind;
			const auto kind         = static_cast<conversation_kind>(
				aIndex.data(github_conversation_model::kind_role).toInt());
			const auto resolved = aIndex.data(github_conversation_model::resolved_role).toBool();
			if (kind == conversation_kind::review_thread) {
				facts.push_back(
					resolved ? QStringLiteral("Resolved") : QStringLiteral("Open"));
			} else if (resolved) {
				facts.push_back(QStringLiteral("Resolved"));
			}
			if (aIndex.data(github_conversation_model::outdated_role).toBool()) {
				facts.push_back(QStringLiteral("Outdated"));
			}
			if (aIndex.data(github_conversation_model::deleted_role).toBool()) {
				facts.push_back(QStringLiteral("Deleted"));
			}
			const auto description = aIndex.data(Qt::AccessibleDescriptionRole).toString();
			const auto anchor      = description.indexOf(QStringLiteral("Anchor unavailable:"));
			if (anchor >= 0) {
				facts.push_back(description.mid(anchor));
			}

			result.Facts = facts.join(QStringLiteral("  ·  "));
			return result;
		}

		[[nodiscard]] auto metrics_for(
			const QStyleOptionViewItem& aOption,
			const QStyle* const         aStyle) -> delegate_metrics
		{
			auto headingFont = aOption.font;
			headingFont.setWeight(QFont::DemiBold);

			auto factFont = aOption.font;
			if (factFont.pointSizeF() > 0.0) {
				factFont.setPointSizeF(
					std::max(1.0, factFont.pointSizeF() * 0.9));
			} else if (factFont.pixelSize() > 0) {
				factFont.setPixelSize(std::max(1, factFont.pixelSize() * 9 / 10));
			}

			const QFontMetrics bodyMetrics{ aOption.font };
			const auto         horizontalMargin = aStyle->pixelMetric(QStyle::PM_FocusFrameHMargin, &aOption, aOption.widget);
			const auto         verticalMargin   = aStyle->pixelMetric(QStyle::PM_FocusFrameVMargin, &aOption, aOption.widget);
			return {
				.HeadingFont       = headingFont,
				.BodyFont          = aOption.font,
				.FactFont          = factFont,
				.HorizontalPadding = std::max(2, horizontalMargin + 2),
				.VerticalPadding   = std::max(2, verticalMargin + 2),
				.SectionSpacing    = std::max(2, bodyMetrics.lineSpacing() / 4),
			};
		}

		[[nodiscard]] auto wrapped_text_height(
			const QString&            aText,
			const QFont&              aFont,
			const qreal               aWidth,
			const QPaintDevice* const aDevice,
			QPainter* const           aPainter = nullptr,
			const QPointF             aOrigin  = { }) -> qreal
		{
			if (aText.isEmpty()) {
				return 0.0;
			}

			auto paragraphs = aText;
			paragraphs.replace(QLatin1Char('\n'), QChar::LineSeparator);
			QTextLayout layout{ paragraphs, aFont, aDevice };
			QTextOption textOption;
			textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			layout.setTextOption(textOption);

			qreal height = 0.0;
			layout.beginLayout();
			while (true) {
				auto line = layout.createLine();
				if (!line.isValid()) {
					break;
				}
				line.setLineWidth(std::max(1.0, aWidth));
				line.setPosition(QPointF{ 0.0, height });
				height += line.height();
			}
			layout.endLayout();

			if (aPainter != nullptr) {
				layout.draw(aPainter, aOrigin);
			}
			return height;
		}

		[[nodiscard]] auto content_height(
			const conversation_presentation& aPresentation,
			const delegate_metrics&          aMetrics,
			const qreal                      aWidth,
			const QPaintDevice* const        aDevice) -> qreal
		{
			auto height = wrapped_text_height(
				aPresentation.Heading, aMetrics.HeadingFont, aPresentation.Label >= 0 ? std::max(1.0, aWidth - mark_lead) : aWidth, aDevice);
			if (aPresentation.Label >= 0) {
				height = std::max(height, 16.0);
			}
			if (!aPresentation.Body.isEmpty()) {
				height += static_cast<qreal>(aMetrics.SectionSpacing);
				height += wrapped_text_height(
					aPresentation.Body, aMetrics.BodyFont, aWidth, aDevice);
			}
			if (!aPresentation.Facts.isEmpty()) {
				height += static_cast<qreal>(aMetrics.SectionSpacing);
				height += wrapped_text_height(
					aPresentation.Facts, aMetrics.FactFont, aWidth, aDevice);
			}
			return height;
		}

		[[nodiscard]] auto palette_group(const QStyleOptionViewItem& aOption)
			-> QPalette::ColorGroup
		{
			if (!aOption.state.testFlag(QStyle::State_Enabled)) {
				return QPalette::Disabled;
			}
			return aOption.state.testFlag(QStyle::State_Active) ? QPalette::Active
																: QPalette::Inactive;
		}

		void set_text_pen(
			QPainter* const             aPainter,
			const QStyleOptionViewItem& aOption,
			const QPalette::ColorRole   aRole)
		{
			aPainter->setPen(QPen{ aOption.palette.brush(palette_group(aOption), aRole), 0.0 });
		}

		/// One step, for any reply, at any depth.
		///
		/// \details The view no longer indents by depth - BPR-0456, on the
		/// owner's direction that a staircase spends width the comment text
		/// needs. What remains is the grouping: a reply sits one step in from
		/// the root it belongs to, and a reply to a reply sits in the same
		/// place. Depth is still in the model, and [[BPR-0418]]'s thread map is
		/// where it is now read.
		constexpr auto ReplyIndent = 16;

		[[nodiscard]] auto text_rect(
			const QStyleOptionViewItem& aOption,
			const QStyle* const         aStyle,
			const QModelIndex&          aIndex) -> QRect
		{
			auto result = aStyle->subElementRect(QStyle::SE_ItemViewItemText, &aOption, aOption.widget);
			if (!result.isValid()) {
				result = aOption.rect;
			}
			if (aIndex.parent().isValid()) {
				result.setLeft(result.left() + ReplyIndent);
			}
			return result;
		}
	}

	/// \brief Row sizes the delegate has already measured.
	///
	/// \details [[BPR-1043]]. `QTreeView` asks for every row's size again on
	/// each `doItemsLayout`, because a view that scrolls by pixel sums every
	/// row's height for its scroll range, and the conversation panel lays out its
	/// whole list whenever the anchored file changes. Each ask shaped the
	/// heading, body and facts afresh and asked the style for the row's
	/// geometry. For the large review's 2,000 posts that held the GUI thread for
	/// about 100 ms at every epoch refresh.
	///
	/// A size is decided by what the row shows (the presentation and the
	/// display text the style lays out), the font, the width it wraps at,
	/// whether it is a reply, the style option's features and decoration, the
	/// style, and the device it is shaped for. All of them are the key, so a
	/// change to any of them measures afresh. Painting still lays out what it
	/// draws.
	struct github_conversation_delegate::measurement_cache
	{
		struct key
		{
			QString       Heading;
			QString       Body;
			QString       Facts;
			QString       DisplayText;
			int           Mark = -1;
			QFont         Font;
			int           Width    = 0;
			bool          Reply    = false;
			int           Features = 0;
			QSize         DecorationSize;
			const QStyle* Style            = nullptr;
			qreal         DevicePixelRatio = 1.0;
			int           LogicalDpi       = 0;

			friend auto operator==(const key&, const key&) -> bool = default;
		};

		struct key_hash
		{
			auto operator()(const key& aKey) const noexcept -> std::size_t
			{
				return qHashMulti(0U, aKey.Heading, aKey.Body, aKey.Facts, aKey.DisplayText, aKey.Mark, aKey.Font, aKey.Width, aKey.Reply,
								  aKey.Features, aKey.DecorationSize.width(), aKey.DecorationSize.height(),
								  reinterpret_cast<quintptr>(aKey.Style), aKey.DevicePixelRatio, aKey.LogicalDpi);
			}
		};

		/// Enough for every post of the largest measured review at a few
		/// widths. Past it the cache starts again rather than growing without
		/// bound when a reviewer resizes the drawer through many widths.
		static constexpr std::size_t Capacity = 16'384U;

		std::unordered_map<key, QSize, key_hash> Sizes;
	};

	github_conversation_delegate::github_conversation_delegate(QObject* const aParent, const bool aCompact)
		: QStyledItemDelegate{ aParent }
		, Compact{ aCompact }
		, Measured{ std::make_unique<measurement_cache>() }
	{
	}

	github_conversation_delegate::~github_conversation_delegate() = default;

	void github_conversation_delegate::set_label_marks(const feedback_mark_theme& aTheme)
	{
		Theme = aTheme;
	}

	void github_conversation_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		if (aPainter == nullptr || !aIndex.isValid()) {
			return;
		}

		QStyleOptionViewItem nativeOption{ aOption };
		initStyleOption(&nativeOption, aIndex);
		const auto* const style          = nativeOption.widget != nullptr ? nativeOption.widget->style()
																		  : QApplication::style();
		const auto        nativeTextRect = text_rect(nativeOption, style, aIndex);
		const auto        metrics        = metrics_for(nativeOption, style);
		const auto        presentation   = presentation_for(aIndex, Compact);

		nativeOption.text.clear();
		style->drawControl(
			QStyle::CE_ItemViewItem,
			&nativeOption,
			aPainter,
			nativeOption.widget);

		const QRectF contentRect = nativeTextRect.adjusted(
			metrics.HorizontalPadding,
			metrics.VerticalPadding,
			-metrics.HorizontalPadding,
			-metrics.VerticalPadding);
		const auto width       = std::max(1.0, contentRect.width());
		auto       y           = contentRect.top();
		const auto selected    = nativeOption.state.testFlag(QStyle::State_Selected);
		const auto primaryRole = selected ? QPalette::HighlightedText : QPalette::Text;

		aPainter->save();
		aPainter->setClipRect(aOption.rect);
		// [[BPR-1083]]. A rail down the post's left edge in its label's tint,
		// grey for a plain post, 2 px, 4 px when blocking, dashed or dotted
		// when non-blocking or if-minor, in the padding
		// the row already had; and the 16 px mark leading the heading.
		const auto marked = presentation.Label >= 0 && static_cast<std::size_t>(presentation.Label) < core::feedback_label_count;
		const auto label  = static_cast<core::feedback_label>(static_cast<std::uint8_t>(std::max(0, presentation.Label)));
		const auto rail   = marked ? Theme.Tints[static_cast<std::size_t>(presentation.Label)] : nativeOption.palette.color(QPalette::Mid);
		const auto post   = static_cast<application::github_conversation_kind>(aIndex.data(github_conversation_model::kind_role).toInt())
			!= application::github_conversation_kind::review_thread;
		if (post) {
			paint_feedback_rail(*aPainter, contentRect.left() - 2.0, static_cast<qreal>(aOption.rect.top()) + 2.0, static_cast<qreal>(aOption.rect.bottom()) - 1.0, rail, marked ? presentation.Emphasis : feedback_mark_emphasis::undecorated);
		}
		set_text_pen(aPainter, nativeOption, primaryRole);
		if (marked) {
			paint_feedback_mark(*aPainter, QRectF{ contentRect.left(), y, 16.0, 16.0 }, label, presentation.Emphasis,
								feedback_mark_colours{ .Tint = Theme.Tints[static_cast<std::size_t>(presentation.Label)], .Panel = nativeOption.palette.color(QPalette::Base), .Ink = nativeOption.palette.color(QPalette::Text), .Style = Theme.Style });
			set_text_pen(aPainter, nativeOption, primaryRole);
		}
		const auto headingLead = marked ? mark_lead : 0.0;
		y += std::max(marked ? 16.0 : 0.0, wrapped_text_height(presentation.Heading, metrics.HeadingFont, std::max(1.0, width - headingLead), nativeOption.widget, aPainter, QPointF{ contentRect.left() + headingLead, y }));

		if (!presentation.Body.isEmpty()) {
			y += static_cast<qreal>(metrics.SectionSpacing);
			y += wrapped_text_height(
				presentation.Body,
				metrics.BodyFont,
				width,
				nativeOption.widget,
				aPainter,
				QPointF{ contentRect.left(), y });
		}
		if (!presentation.Facts.isEmpty()) {
			y += static_cast<qreal>(metrics.SectionSpacing);
			static_cast<void>(wrapped_text_height(
				presentation.Facts,
				metrics.FactFont,
				width,
				nativeOption.widget,
				aPainter,
				QPointF{ contentRect.left(), y }));
		}
		aPainter->restore();
	}

	auto github_conversation_delegate::sizeHint(
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const -> QSize
	{
		if (!aIndex.isValid()) {
			return QStyledItemDelegate::sizeHint(aOption, aIndex);
		}

		QStyleOptionViewItem nativeOption{ aOption };
		initStyleOption(&nativeOption, aIndex);
		// QTreeView asks this single-column delegate with an unspecified width.
		// Measure at the width it will paint, before forming the cache key;
		// a guessed width can truncate the final reply beyond the scroll range.
		if (nativeOption.rect.width() <= 0) {
			if (const auto* view = qobject_cast<const QAbstractItemView*>(nativeOption.widget); view != nullptr) {
				nativeOption.rect.setWidth(view->viewport()->width());
			}
		}
		const auto* const style        = nativeOption.widget != nullptr ? nativeOption.widget->style()
																		: QApplication::style();
		auto              presentation = presentation_for(aIndex, Compact);
		auto              cacheKey     = measurement_cache::key{
			.Heading          = presentation.Heading,
			.Body             = presentation.Body,
			.Facts            = presentation.Facts,
			.DisplayText      = nativeOption.text,
			.Mark             = (presentation.Label * 4) + static_cast<int>(presentation.Emphasis),
			.Font             = nativeOption.font,
			.Width            = nativeOption.rect.width(),
			.Reply            = aIndex.parent().isValid(),
			.Features         = static_cast<int>(nativeOption.features),
			.DecorationSize   = nativeOption.decorationSize,
			.Style            = style,
			.DevicePixelRatio = nativeOption.widget != nullptr ? nativeOption.widget->devicePixelRatio() : 1.0,
			.LogicalDpi       = nativeOption.widget != nullptr ? nativeOption.widget->logicalDpiY() : 0,
		};
		if (const auto found = Measured->Sizes.find(cacheKey); found != Measured->Sizes.end()) {
			return found->second;
		}
		const auto         metrics = metrics_for(nativeOption, style);
		const QFontMetrics fontMetrics{ nativeOption.font };

		auto width = nativeOption.rect.width();
		if (width <= 0) {
			width = std::max(
				fontMetrics.horizontalAdvance(QStringLiteral("Conversations")),
				fontMetrics.averageCharWidth() * 48);
		}

		auto measuringOption      = nativeOption;
		measuringOption.rect      = QRect{ 0, 0, width, std::max(1, fontMetrics.height()) };
		const auto availableWidth = std::max(
			1,
			text_rect(measuringOption, style, aIndex).width()
				- (metrics.HorizontalPadding * 2));
		const auto height = content_height(
			presentation,
			metrics,
			static_cast<qreal>(availableWidth),
			nativeOption.widget);
		const auto paddedHeight = height + static_cast<qreal>(metrics.VerticalPadding * 2);
		const auto nativeSize   = style->sizeFromContents(
			QStyle::CT_ItemViewItem,
			&nativeOption,
			QSize{
				availableWidth + (metrics.HorizontalPadding * 2),
				std::max(
					fontMetrics.height(),
					static_cast<int>(paddedHeight + 0.999)) },
			nativeOption.widget);
		const auto size = QSize{
			width,
			std::max(
				nativeSize.height(),
				static_cast<int>(paddedHeight + 0.999))
		};
		if (Measured->Sizes.size() >= measurement_cache::Capacity) {
			Measured->Sizes.clear();
		}
		Measured->Sizes.emplace(std::move(cacheKey), size);
		return size;
	}
}
