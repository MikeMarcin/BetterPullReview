#include "conversation_reactions.hpp"

#include "bpr/qt/anchored_tooltip.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QScreen>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto tinted(QColor aColour, const qreal aAlpha) -> QColor
		{
			aColour.setAlphaF(static_cast<float>(aAlpha));
			return aColour;
		}

		[[nodiscard]] auto emoji(const core::reaction_content aContent) -> QString
		{
			const auto text = core::reaction_emoji(aContent);
			return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
		}

		[[nodiscard]] auto label(const core::reaction_content aContent) -> QString
		{
			const auto text = core::reaction_label(aContent);
			return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
		}

		[[nodiscard]] auto capitalised(QString aText) -> QString
		{
			if (!aText.isEmpty()) {
				aText[0] = aText[0].toUpper();
			}
			return aText;
		}

		/// The count, in the chip's own slightly smaller weight.
		[[nodiscard]] auto count_font(QFont aFont) -> QFont
		{
			aFont.setWeight(QFont::DemiBold);
			if (aFont.pointSizeF() > 0.0) {
				aFont.setPointSizeF(aFont.pointSizeF() * 0.92);
			}
			return aFont;
		}

		constexpr int chip_gap     = 6;
		constexpr int chip_padding = 8;

		/// [[BPR-PENDING-AR1]]. One reaction in the picker: its emoji in a
		/// square, washed in the accent when it is mine.
		class reaction_option final : public QAbstractButton
		{
		public:
			reaction_option(const core::reaction_content aContent, const bool aMine, const note_colours& aColours, QWidget* const aParent)
				: QAbstractButton{ aParent }
				, Content{ aContent }
				, Colours{ aColours }
			{
				setCheckable(true);
				setChecked(aMine);
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setAttribute(Qt::WA_Hover, true);
				setObjectName(QStringLiteral("reactionPickerOption"));
				setProperty("reactionContent", static_cast<int>(aContent));
				const auto words = capitalised(label(aContent));
				setAccessibleName(aMine ? QStringLiteral("%1, you reacted").arg(words) : words);
				setToolTip(aMine ? QStringLiteral("Remove %1").arg(label(aContent)) : capitalised(label(aContent)));
				anchor_tooltips(*this);
			}

			[[nodiscard]] auto content() const noexcept -> core::reaction_content
			{
				return Content;
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto side = std::max(30, QFontMetrics{ font() }.height() * 2);
				return { side, side };
			}

		protected:
			void keyPressEvent(QKeyEvent* const aEvent) override
			{
				// The picker moves between reactions and chooses on Return;
				// Space is the button's own press.
				switch (aEvent->key()) {
				case Qt::Key_Left:
				case Qt::Key_Right:
				case Qt::Key_Up:
				case Qt::Key_Down:
				case Qt::Key_Home:
				case Qt::Key_End:
				case Qt::Key_Return:
				case Qt::Key_Enter:
				case Qt::Key_Escape:
					aEvent->ignore();
					return;
				default:
					QAbstractButton::keyPressEvent(aEvent);
					return;
				}
			}

			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box = QRectF{ rect() }.adjusted(2.0, 2.0, -2.0, -2.0);
				if (isChecked()) {
					painter.setPen(QPen{ tinted(Colours.Signal, 0.7), 1.0 });
					painter.setBrush(tinted(Colours.Signal, Colours.Dark ? 0.26 : 0.14));
					painter.drawRoundedRect(box, 6.0, 6.0);
				} else if (underMouse()) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(Colours.Hover.isValid() ? Colours.Hover : tinted(Colours.Ink, 0.06));
					painter.drawRoundedRect(box, 6.0, 6.0);
				}
				if (hasFocus()) {
					painter.setPen(QPen{ Colours.Signal, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box.adjusted(-1.0, -1.0, 1.0, 1.0), 7.0, 7.0);
				}
				auto face = font();
				if (face.pointSizeF() > 0.0) {
					face.setPointSizeF(face.pointSizeF() * 1.3);
				}
				painter.setFont(face);
				painter.setPen(Colours.Ink);
				painter.drawText(rect(), Qt::AlignCenter, emoji(Content));
			}

		private:
			core::reaction_content Content;
			note_colours           Colours;
		};
	} // namespace

	auto reaction_description(const core::reaction_group& aGroup, const QString& aViewer) -> QString
	{
		auto names = QStringList{ };
		if (aGroup.ViewerReacted) {
			names << QStringLiteral("You");
		}
		for (const auto& login : aGroup.Reactors) {
			const auto name = QString::fromStdString(login);
			if (aGroup.ViewerReacted && !aViewer.isEmpty() && name.compare(aViewer, Qt::CaseInsensitive) == 0) {
				continue;
			}
			names << name;
		}
		const auto named  = static_cast<std::uint32_t>(names.size());
		const auto others = aGroup.Count > named ? aGroup.Count - named : 0U;
		auto       who    = QString{ };
		if (names.isEmpty()) {
			who = aGroup.Count == 1U ? QStringLiteral("1 person") : QStringLiteral("%1 people").arg(aGroup.Count);
		} else if (others > 0U) {
			who = names.join(QStringLiteral(", ")) + (others == 1U ? QStringLiteral(" and 1 other") : QStringLiteral(" and %1 others").arg(others));
		} else if (names.size() == 1) {
			who = names.front();
		} else {
			const auto last = names.takeLast();
			who             = names.join(QStringLiteral(", ")) + QStringLiteral(" and ") + last;
		}
		return QStringLiteral("%1 reacted with %2").arg(who, label(aGroup.Content));
	}

	reaction_chip::reaction_chip(const core::reaction_group& aGroup, const note_colours& aColours, const bool aCanReact, const QString& aViewer, QWidget* const aParent)
		: QAbstractButton{ aParent }
		, Group{ aGroup }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("reactionChip"));
		setProperty("reactionContent", static_cast<int>(aGroup.Content));
		setCheckable(false);
		setFocusPolicy(Qt::StrongFocus);
		setCursor(aCanReact ? Qt::PointingHandCursor : Qt::ArrowCursor);
		setAttribute(Qt::WA_Hover, true);
		setEnabled(aCanReact);
		const auto description = reaction_description(aGroup, aViewer);
		const auto pending     = aGroup.Pending ? QStringLiteral(", waiting for GitHub") : QString{ };
		setAccessibleName(QStringLiteral("%1 %2").arg(capitalised(label(aGroup.Content))).arg(aGroup.Count));
		setAccessibleDescription(description + pending + (aCanReact ? (aGroup.ViewerReacted ? QStringLiteral(". Press to remove yours") : QStringLiteral(". Press to add yours")) : QString{ }));
		setToolTip(description + pending);
		anchor_tooltips(*this);
	}

	auto reaction_chip::content() const noexcept -> core::reaction_content
	{
		return Group.Content;
	}

	auto reaction_chip::reaction() const noexcept -> const core::reaction_group&
	{
		return Group;
	}

	auto reaction_chip::sizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		const auto counts  = QFontMetrics{ count_font(font()) };
		const auto height  = std::max(22, metrics.height() + 8);
		const auto width   = chip_padding + metrics.horizontalAdvance(emoji(Group.Content)) + 4 + counts.horizontalAdvance(QString::number(Group.Count)) + chip_padding;
		return { std::max(width, height + 8), height };
	}

	void reaction_chip::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box    = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		const auto radius = box.height() / 2.0;
		auto       border = Group.ViewerReacted ? tinted(Colours.Signal, 0.75) : Colours.Rule2;
		auto       fill   = Group.ViewerReacted ? tinted(Colours.Signal, Colours.Dark ? 0.24 : 0.11) : QColor{ Qt::transparent };
		if (isEnabled() && underMouse()) {
			fill = Group.ViewerReacted ? tinted(Colours.Signal, Colours.Dark ? 0.32 : 0.17) : (Colours.Hover.isValid() ? Colours.Hover : tinted(Colours.Ink, 0.06));
		}
		auto pen = QPen{ border, 1.0 };
		if (Group.Pending) {
			pen.setStyle(Qt::DashLine);
		}
		painter.setPen(pen);
		painter.setBrush(fill);
		painter.drawRoundedRect(box, radius, radius);
		if (hasFocus()) {
			painter.setPen(QPen{ Colours.Signal, 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box.adjusted(-0.5, -0.5, 0.5, 0.5), radius, radius);
		}
		const auto metrics = QFontMetrics{ font() };
		auto       x       = static_cast<int>(box.left()) + chip_padding - 1;
		painter.setFont(font());
		painter.setPen(Colours.Ink);
		const auto face = emoji(Group.Content);
		painter.drawText(QRect{ x, 0, metrics.horizontalAdvance(face), height() }, Qt::AlignVCenter | Qt::AlignLeft, face);
		x += metrics.horizontalAdvance(face) + 4;
		painter.setFont(count_font(font()));
		painter.setPen(Group.ViewerReacted ? Colours.Signal : Colours.Ink2);
		painter.drawText(QRect{ x, 0, width() - x, height() }, Qt::AlignVCenter | Qt::AlignLeft, QString::number(Group.Count));
	}

	reaction_bar::reaction_bar(const note_colours& aColours, QWidget* const aParent)
		: QWidget{ aParent }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("reactionBar"));
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		hide();
	}

	void reaction_bar::set_reactions(const std::optional<core::reaction_summary>& aSummary, const QString& aViewer)
	{
		for (auto* const chip : Chips) {
			chip->hide();
			chip->deleteLater();
		}
		Chips.clear();
		auto names = QStringList{ };
		if (aSummary.has_value()) {
			for (const auto& group : aSummary->Groups) {
				if (group.Count == 0U) {
					continue;
				}
				auto* const chip = new reaction_chip{ group, Colours, aSummary->ViewerCanReact, aViewer, this };
				connect(chip, &QAbstractButton::clicked, this, [this, content = group.Content] {
					const auto toggled = Toggled;
					if (toggled) {
						toggled(content);
					}
				});
				chip->show();
				Chips.push_back(chip);
				names << QStringLiteral("%1 %2").arg(group.Count).arg(label(group.Content));
			}
		}
		setAccessibleName(names.isEmpty() ? QString{ } : QStringLiteral("Reactions: %1").arg(names.join(QStringLiteral(", "))));
		setVisible(!Chips.empty());
		place();
		updateGeometry();
	}

	auto reaction_bar::chips() const -> std::vector<reaction_chip*>
	{
		return Chips;
	}

	auto reaction_bar::arranged(const int aWidth) const -> std::vector<QRect>
	{
		auto places = std::vector<QRect>{ };
		places.reserve(Chips.size());
		auto x = 0;
		auto y = 0;
		auto h = 0;
		for (const auto* const chip : Chips) {
			const auto size = chip->sizeHint();
			if (x > 0 && x + size.width() > aWidth) {
				x = 0;
				y += h + chip_gap;
				h = 0;
			}
			places.emplace_back(x, y, size.width(), size.height());
			x += size.width() + chip_gap;
			h = std::max(h, size.height());
		}
		return places;
	}

	void reaction_bar::place()
	{
		const auto places = arranged(width());
		for (std::size_t index = 0; index < Chips.size(); ++index) {
			Chips[index]->setGeometry(places[index]);
		}
	}

	auto reaction_bar::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto reaction_bar::heightForWidth(const int aWidth) const -> int
	{
		auto bottom = 0;
		for (const auto& place : arranged(aWidth)) {
			bottom = std::max(bottom, place.bottom() + 1);
		}
		return bottom;
	}

	auto reaction_bar::sizeHint() const -> QSize
	{
		const auto width = std::max(120, this->width());
		return { width, heightForWidth(width) };
	}

	void reaction_bar::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		place();
	}

	reaction_picker::reaction_picker(const std::optional<core::reaction_summary>& aSummary, const note_colours& aColours, QWidget* const aParent)
		: QFrame{ aParent, Qt::Popup | Qt::FramelessWindowHint }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("reactionPicker"));
		setAttribute(Qt::WA_TranslucentBackground, true);
		setAttribute(Qt::WA_DeleteOnClose, true);
		setAccessibleName(QStringLiteral("Add reaction"));
		setAccessibleDescription(QStringLiteral("Arrow keys move between reactions; Space or Return chooses one; Esc closes"));
		auto x = 6;
		auto h = 0;
		for (const auto content : core::reaction_contents) {
			const auto* const group  = aSummary.has_value() ? core::find_reaction_group(*aSummary, content) : nullptr;
			auto* const       option = new reaction_option{ content, group != nullptr && group->ViewerReacted, Colours, this };
			const auto        size   = option->sizeHint();
			option->setGeometry(x, 6, size.width(), size.height());
			x += size.width() + 2;
			h = size.height();
			connect(option, &QAbstractButton::clicked, this, [this, content] { choose(content); });
			Buttons.push_back(option);
		}
		resize(x + 4, h + 12);
	}

	void reaction_picker::popup_at(const QPoint aGlobal)
	{
		auto position = aGlobal;
		if (const auto* const screen = QGuiApplication::screenAt(aGlobal); screen != nullptr) {
			const auto area = screen->availableGeometry();
			position.setX(std::clamp(position.x(), area.left(), std::max(area.left(), area.right() - width())));
			position.setY(std::clamp(position.y(), area.top(), std::max(area.top(), area.bottom() - height())));
		}
		move(position);
		show();
		const auto mine = std::ranges::find_if(Buttons, [](const QAbstractButton* aButton) { return aButton->isChecked(); });
		(mine != Buttons.end() ? *mine : Buttons.front())->setFocus(Qt::PopupFocusReason);
	}

	auto reaction_picker::buttons() const -> const std::vector<QAbstractButton*>&
	{
		return Buttons;
	}

	void reaction_picker::choose(const core::reaction_content aContent)
	{
		const auto chosen = Chosen;
		close();
		if (chosen) {
			chosen(aContent);
		}
	}

	void reaction_picker::keyPressEvent(QKeyEvent* const aEvent)
	{
		const auto focused = std::ranges::find(Buttons, focusWidget());
		const auto index   = focused == Buttons.end() ? std::size_t{ 0U } : static_cast<std::size_t>(std::distance(Buttons.begin(), focused));
		const auto last    = Buttons.size() - 1U;
		switch (aEvent->key()) {
		case Qt::Key_Left:
		case Qt::Key_Up:
			Buttons[index == 0U ? last : index - 1U]->setFocus(Qt::TabFocusReason);
			return;
		case Qt::Key_Right:
		case Qt::Key_Down:
			Buttons[index == last ? 0U : index + 1U]->setFocus(Qt::TabFocusReason);
			return;
		case Qt::Key_Home:
			Buttons.front()->setFocus(Qt::TabFocusReason);
			return;
		case Qt::Key_End:
			Buttons.back()->setFocus(Qt::TabFocusReason);
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			choose(static_cast<core::reaction_content>(Buttons[index]->property("reactionContent").toInt()));
			return;
		case Qt::Key_Escape:
			close();
			return;
		default:
			QFrame::keyPressEvent(aEvent);
			return;
		}
	}

	void reaction_picker::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.setPen(QPen{ Colours.Rule2, 1.0 });
		painter.setBrush(Colours.Panel);
		painter.drawRoundedRect(box, 9.0, 9.0);
	}
} // namespace bpr::qt
