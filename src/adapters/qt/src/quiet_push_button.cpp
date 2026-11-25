#include "bpr/qt/quiet_push_button.hpp"

#include "button_surface.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>

#include <algorithm>

namespace bpr::qt {
	quiet_push_button::quiet_push_button(QWidget* const aParent)
		: QPushButton{ aParent }
	{
		setAttribute(Qt::WA_Hover, true);
	}

	quiet_push_button::quiet_push_button(const QString& aText, QWidget* const aParent)
		: quiet_push_button{ aParent }
	{
		setText(aText);
	}

	auto quiet_push_button::uses_chip() const -> bool
	{
		return !text().isEmpty() && !isDefault() && menu() == nullptr;
	}

	auto quiet_push_button::sizeHint() const -> QSize
	{
		const auto native = QPushButton::sizeHint();
		if (!uses_chip()) {
			return native;
		}
		// [[BPR-1855]]. The words in the button's own font, rounded up, inside the
		// chip's padding: the same geometry paint_button_label draws in.
		const auto content = chip_content_size(*this, iconSize(), !icon().isNull());
		return QSize{ (2 * chip_padding) + content.width(), std::max(native.height(), content.height() + 6) };
	}

	auto quiet_push_button::minimumSizeHint() const -> QSize
	{
		const auto hint = sizeHint();
		if (wraps()) {
			// Its longest word, whole.
			return QSize{ std::min(hint.width(), (2 * chip_padding) + chip_longest_word_width(*this)), hint.height() };
		}
		if (shortens()) {
			return QSize{ std::min(hint.width(), (2 * chip_padding) + chip_words_width(*this, ShortText)), hint.height() };
		}
		return hint;
	}

	void quiet_push_button::set_word_wrap(const bool aWrap)
	{
		if (WordWrap == aWrap) {
			return;
		}
		WordWrap = aWrap;
		apply_size_policy();
	}

	void quiet_push_button::set_short_text(const QString& aShort)
	{
		if (ShortText == aShort) {
			return;
		}
		ShortText = aShort;
		apply_size_policy();
	}

	void quiet_push_button::apply_size_policy()
	{
		auto policy = sizePolicy();
		// It may be narrower than its one line: then taller than it when it
		// wraps, or showing its shorter words.
		policy.setHorizontalPolicy(WordWrap || !ShortText.isEmpty() ? QSizePolicy::Preferred : QSizePolicy::Minimum);
		policy.setVerticalPolicy(WordWrap ? QSizePolicy::Minimum : QSizePolicy::Fixed);
		policy.setHeightForWidth(WordWrap);
		setSizePolicy(policy);
		updateGeometry();
		update();
	}

	auto quiet_push_button::wraps() const -> bool
	{
		return WordWrap && uses_chip() && icon().isNull();
	}

	auto quiet_push_button::shortens() const -> bool
	{
		return !ShortText.isEmpty() && !WordWrap && uses_chip() && icon().isNull();
	}

	auto quiet_push_button::shortened() const -> bool
	{
		return shortens() && width() < sizeHint().width();
	}

	auto quiet_push_button::hasHeightForWidth() const -> bool
	{
		return wraps();
	}

	auto quiet_push_button::heightForWidth(const int aWidth) const -> int
	{
		const auto hint = sizeHint();
		if (!wraps() || aWidth >= hint.width()) {
			return hint.height();
		}
		// The same room above and below the words as on one line.
		const auto line = chip_content_size(*this, iconSize(), false).height();
		return hint.height() - line + chip_wrapped_words_height(*this, aWidth - (2 * chip_padding));
	}

	void quiet_push_button::focusInEvent(QFocusEvent* const aEvent)
	{
		update_button_keyboard_focus(KeyboardFocus, *aEvent);
		QPushButton::focusInEvent(aEvent);
	}

	void quiet_push_button::changeEvent(QEvent* const aEvent)
	{
		QPushButton::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::PaletteChange) {
			updateGeometry();
			update();
		}
	}

	void quiet_push_button::paintEvent(QPaintEvent* const aEvent)
	{
		if (!uses_chip()) {
			QPushButton::paintEvent(aEvent);
			return;
		}
		auto painter = QPainter{ this };
		paint_button_surface(painter, *this, isChecked() || (!isCheckable() && !isFlat()), KeyboardFocus);
		if (wraps() && width() < sizeHint().width()) {
			paint_wrapped_button_label(painter, *this);
			return;
		}
		if (shortened()) {
			paint_short_button_label(painter, *this, ShortText);
			return;
		}
		paint_button_label(painter, *this, iconSize(), !icon().isNull());
	}
} // namespace bpr::qt
