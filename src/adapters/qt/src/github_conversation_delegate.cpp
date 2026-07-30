#include "bpr/qt/github_conversation_delegate.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QString>
#include <QStringList>
#include <QTextLayout>
#include <QTextOption>
#include <QWidget>

#include <algorithm>

namespace bpr::qt
{
namespace
{
struct conversation_presentation
{
    QString Heading;
    QString Body;
    QString Facts;
};

struct delegate_metrics
{
    QFont HeadingFont;
    QFont BodyFont;
    QFont FactFont;
    int HorizontalPadding = 0;
    int VerticalPadding = 0;
    int SectionSpacing = 0;
};

[[nodiscard]] auto author_text(const QModelIndex& index) -> QString
{
    const auto author =
        index.data(github_conversation_model::author_login_role).toString();
    return author.isEmpty() ? QStringLiteral("Unknown author")
                            : QStringLiteral("@%1").arg(author);
}

[[nodiscard]] auto review_state_text(const int state) -> QString
{
    using review_state = application::github_review_state_projection;
    switch(static_cast<review_state>(state))
    {
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
        return {};
    }
    return {};
}

[[nodiscard]] auto heading_text(const QModelIndex& index) -> QString
{
    using conversation_kind = application::github_conversation_kind;
    const auto kind = static_cast<conversation_kind>(
        index.data(github_conversation_model::kind_role).toInt());
    const auto author = author_text(index);

    switch(kind)
    {
    case conversation_kind::review:
        return QStringLiteral("Review by %1").arg(author);
    case conversation_kind::review_thread:
        return QStringLiteral("Review thread");
    case conversation_kind::review_comment:
        return index.parent().isValid()
            ? QStringLiteral("Reply by %1").arg(author)
            : QStringLiteral("Review comment by %1").arg(author);
    case conversation_kind::issue_comment:
        return QStringLiteral("Pull request comment by %1").arg(author);
    }
    return QStringLiteral("Conversation");
}

[[nodiscard]] auto location_text(const QModelIndex& index) -> QString
{
    auto path = index.data(github_conversation_model::path_role).toString();
    const auto line = index.data(github_conversation_model::line_role);
    if(!line.isValid())
    {
        return path;
    }

    if(path.isEmpty())
    {
        return QStringLiteral("Line %1").arg(line.toULongLong());
    }
    path.append(QLatin1Char(':'));
    path.append(QString::number(line.toULongLong()));
    return path;
}

[[nodiscard]] auto presentation_for(const QModelIndex& index)
    -> conversation_presentation
{
    auto result = conversation_presentation{
        .Heading = heading_text(index),
        .Body =
            index.data(github_conversation_model::body_role).toString(),
        .Facts = {},
    };

    QStringList facts;
    if(const auto location = location_text(index); !location.isEmpty())
    {
        facts.push_back(location);
    }
    if(const auto review_state = review_state_text(
           index.data(github_conversation_model::review_state_role).toInt());
       !review_state.isEmpty())
    {
        facts.push_back(review_state);
    }

    using conversation_kind = application::github_conversation_kind;
    const auto kind = static_cast<conversation_kind>(
        index.data(github_conversation_model::kind_role).toInt());
    const auto resolved =
        index.data(github_conversation_model::resolved_role).toBool();
    if(kind == conversation_kind::review_thread)
    {
        facts.push_back(
            resolved ? QStringLiteral("Resolved") : QStringLiteral("Open"));
    }
    else if(resolved)
    {
        facts.push_back(QStringLiteral("Resolved"));
    }
    if(index.data(github_conversation_model::outdated_role).toBool())
    {
        facts.push_back(QStringLiteral("Outdated"));
    }
    if(index.data(github_conversation_model::deleted_role).toBool())
    {
        facts.push_back(QStringLiteral("Deleted"));
    }

    result.Facts = facts.join(QStringLiteral("  ·  "));
    return result;
}

[[nodiscard]] auto metrics_for(
    const QStyleOptionViewItem& option,
    const QStyle* const style) -> delegate_metrics
{
    auto heading_font = option.font;
    heading_font.setWeight(QFont::DemiBold);

    auto fact_font = option.font;
    if(fact_font.pointSizeF() > 0.0)
    {
        fact_font.setPointSizeF(
            std::max(1.0, fact_font.pointSizeF() * 0.9));
    }
    else if(fact_font.pixelSize() > 0)
    {
        fact_font.setPixelSize(std::max(1, fact_font.pixelSize() * 9 / 10));
    }

    const QFontMetrics body_metrics{option.font};
    const auto horizontal_margin =
        style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget);
    const auto vertical_margin =
        style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget);
    return {
        .HeadingFont = heading_font,
        .BodyFont = option.font,
        .FactFont = fact_font,
        .HorizontalPadding = std::max(2, horizontal_margin + 2),
        .VerticalPadding = std::max(2, vertical_margin + 2),
        .SectionSpacing = std::max(2, body_metrics.lineSpacing() / 4),
    };
}

[[nodiscard]] auto wrapped_text_height(
    const QString& text,
    const QFont& font,
    const qreal width,
    const QPaintDevice* const device,
    QPainter* const painter = nullptr,
    const QPointF origin = {}) -> qreal
{
    if(text.isEmpty())
    {
        return 0.0;
    }

    QTextLayout layout{text, font, device};
    QTextOption text_option;
    text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(text_option);

    qreal height = 0.0;
    layout.beginLayout();
    while(true)
    {
        auto line = layout.createLine();
        if(!line.isValid())
        {
            break;
        }
        line.setLineWidth(std::max(1.0, width));
        line.setPosition(QPointF{0.0, height});
        height += line.height();
    }
    layout.endLayout();

    if(painter != nullptr)
    {
        layout.draw(painter, origin);
    }
    return height;
}

[[nodiscard]] auto content_height(
    const conversation_presentation& presentation,
    const delegate_metrics& metrics,
    const qreal width,
    const QPaintDevice* const device) -> qreal
{
    auto height = wrapped_text_height(
        presentation.Heading, metrics.HeadingFont, width, device);
    if(!presentation.Body.isEmpty())
    {
        height += static_cast<qreal>(metrics.SectionSpacing);
        height += wrapped_text_height(
            presentation.Body, metrics.BodyFont, width, device);
    }
    if(!presentation.Facts.isEmpty())
    {
        height += static_cast<qreal>(metrics.SectionSpacing);
        height += wrapped_text_height(
            presentation.Facts, metrics.FactFont, width, device);
    }
    return height;
}

[[nodiscard]] auto palette_group(const QStyleOptionViewItem& option)
    -> QPalette::ColorGroup
{
    if(!option.state.testFlag(QStyle::State_Enabled))
    {
        return QPalette::Disabled;
    }
    return option.state.testFlag(QStyle::State_Active) ? QPalette::Active
                                                       : QPalette::Inactive;
}

void set_text_pen(
    QPainter* const painter,
    const QStyleOptionViewItem& option,
    const QPalette::ColorRole role)
{
    painter->setPen(QPen{option.palette.brush(palette_group(option), role), 0.0});
}

[[nodiscard]] auto text_rect(
    const QStyleOptionViewItem& option,
    const QStyle* const style) -> QRect
{
    auto result =
        style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    if(!result.isValid())
    {
        result = option.rect;
    }
    return result;
}
}

github_conversation_delegate::github_conversation_delegate(QObject* const parent)
    : QStyledItemDelegate{parent}
{
}

void github_conversation_delegate::paint(
    QPainter* const painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if(painter == nullptr || !index.isValid())
    {
        return;
    }

    QStyleOptionViewItem native_option{option};
    initStyleOption(&native_option, index);
    const auto* const style =
        native_option.widget != nullptr ? native_option.widget->style()
                                        : QApplication::style();
    const auto native_text_rect = text_rect(native_option, style);
    const auto metrics = metrics_for(native_option, style);
    const auto presentation = presentation_for(index);

    native_option.text.clear();
    style->drawControl(
        QStyle::CE_ItemViewItem,
        &native_option,
        painter,
        native_option.widget);

    const QRectF content_rect = native_text_rect.adjusted(
        metrics.HorizontalPadding,
        metrics.VerticalPadding,
        -metrics.HorizontalPadding,
        -metrics.VerticalPadding);
    const auto width = std::max(1.0, content_rect.width());
    auto y = content_rect.top();
    const auto selected =
        native_option.state.testFlag(QStyle::State_Selected);
    const auto primary_role =
        selected ? QPalette::HighlightedText : QPalette::Text;

    painter->save();
    painter->setClipRect(option.rect);
    set_text_pen(painter, native_option, primary_role);
    y += wrapped_text_height(
        presentation.Heading,
        metrics.HeadingFont,
        width,
        native_option.widget,
        painter,
        QPointF{content_rect.left(), y});

    if(!presentation.Body.isEmpty())
    {
        y += static_cast<qreal>(metrics.SectionSpacing);
        y += wrapped_text_height(
            presentation.Body,
            metrics.BodyFont,
            width,
            native_option.widget,
            painter,
            QPointF{content_rect.left(), y});
    }
    if(!presentation.Facts.isEmpty())
    {
        y += static_cast<qreal>(metrics.SectionSpacing);
        static_cast<void>(wrapped_text_height(
            presentation.Facts,
            metrics.FactFont,
            width,
            native_option.widget,
            painter,
            QPointF{content_rect.left(), y}));
    }
    painter->restore();
}

auto github_conversation_delegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const -> QSize
{
    if(!index.isValid())
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    QStyleOptionViewItem native_option{option};
    initStyleOption(&native_option, index);
    const auto* const style =
        native_option.widget != nullptr ? native_option.widget->style()
                                        : QApplication::style();
    const auto metrics = metrics_for(native_option, style);
    const QFontMetrics font_metrics{native_option.font};

    auto width = native_option.rect.width();
    if(width <= 0)
    {
        width = std::max(
            font_metrics.horizontalAdvance(QStringLiteral("Conversations")),
            font_metrics.averageCharWidth() * 48);
    }

    auto measuring_option = native_option;
    measuring_option.rect =
        QRect{0, 0, width, std::max(1, font_metrics.height())};
    const auto available_width = std::max(
        1,
        text_rect(measuring_option, style).width()
            - (metrics.HorizontalPadding * 2));
    const auto height = content_height(
        presentation_for(index),
        metrics,
        static_cast<qreal>(available_width),
        native_option.widget);
    const auto padded_height =
        height + static_cast<qreal>(metrics.VerticalPadding * 2);
    const auto native_size = style->sizeFromContents(
        QStyle::CT_ItemViewItem,
        &native_option,
        QSize{
            available_width + (metrics.HorizontalPadding * 2),
            std::max(
                font_metrics.height(),
                static_cast<int>(padded_height + 0.999))},
        native_option.widget);
    return QSize{
        width,
        std::max(
            native_size.height(),
            static_cast<int>(padded_height + 0.999))};
}
}
