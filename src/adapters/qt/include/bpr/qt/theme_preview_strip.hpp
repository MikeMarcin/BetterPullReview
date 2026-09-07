#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THEME_PREVIEW_STRIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THEME_PREVIEW_STRIP_HPP

#include "bpr/core/appearance_model.hpp"

#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace bpr::qt {
	/// \brief One of the four theme choices the strip draws.
	struct theme_preview_card
	{
		core::appearance_mode Mode = core::appearance_mode::system;
		QString               Name;
		/// The small note beneath the name: "dark now", "follows desktop".
		QString Note;
		QRect   Rect;
	};

	using theme_preview_dispatch = std::function<void(core::appearance_mode)>;

	inline constexpr std::size_t theme_preview_count = 4U;

	/// \brief RFC-BPR-0028's theme previews: four cards drawn from the real palettes.
	///
	/// \details [[BPR-0652]]. System is half light and half dark, Light and
	/// Dark are themselves, and High contrast follows the desktop. Each
	/// card is a few lines of code painted in the palette's own roles (the
	/// code background, a keyword, an identifier, an added line), so the
	/// preview is the theme rather than a picture of it. Choosing a card
	/// reports the mode; the panel turns it into the same appearance intent
	/// the mode selector sends. The current mode is drawn with the accent.
	class theme_preview_strip final : public QWidget
	{
	public:
		explicit theme_preview_strip(QWidget* aParent = nullptr);

		void               set_current(core::appearance_mode aMode);
		[[nodiscard]] auto current() const noexcept -> core::appearance_mode;
		/// \brief What the desktop is now, for System's note.
		void set_system_variant(core::appearance_variant aVariant);
		void set_dispatch(theme_preview_dispatch aDispatch);

		[[nodiscard]] auto cards() const noexcept
			-> const std::array<theme_preview_card, theme_preview_count>&;
		[[nodiscard]] auto hit_test(QPoint aPoint) const -> std::optional<std::size_t>;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void relayout();
		void choose(std::size_t aCard);

		std::array<theme_preview_card, theme_preview_count> Cards{ };
		core::appearance_mode                               Current = core::appearance_mode::system;
		core::appearance_variant                            System  = core::appearance_variant::light;
		theme_preview_dispatch                              Dispatch;
	};
} // namespace bpr::qt

#endif
