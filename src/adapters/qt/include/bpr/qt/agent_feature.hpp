#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FEATURE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FEATURE_HPP

#include <QColor>
#include <QPalette>

#include <cstdint>

class QWidget;

namespace bpr::qt {
	/// \brief [[BPR-1881]]. Whether the agent's setup has finished, as the
	/// Agent tab knows it from the core's suggestions verdict: the one place
	/// that already decides whether the setup still stands in for the
	/// conversation.
	enum class agent_setup_readiness : std::uint8_t {
		/// The setup is still finding agents and checking the link, so it
		/// cannot yet say what is missing.
		checking,
		/// No agent, no repository, a failed or running check, or the setup
		/// on screen.
		unfinished,
		/// Set up: the agent's features can be used.
		ready,
	};

	/// \brief [[BPR-1881]], owner 2026-11-06. The one colour every agent
	/// feature wears, before and after setup, so the agentic stands apart
	/// from the ordinary at a glance.
	///
	/// \details A deep magenta in a light window and a soft one in a dark
	/// window, each at least 4.5:1 against the window and base colours of
	/// either kind (macOS's own and the profiles'). The hue is one no state
	/// in the application uses: blue is unread and focus, green accepted,
	/// red a defect, violet fixed, teal verified and amber the tour's route.
	[[nodiscard]] auto agent_accent(const QPalette& aPalette) -> QColor;

	/// \brief [[BPR-1881]]. The accent as a disabled control's words: the
	/// accent faded toward the window, so a disabled agent control still
	/// reads as an agent's.
	[[nodiscard]] auto agent_accent_disabled(const QPalette& aPalette) -> QColor;

	/// \brief [[BPR-1881]]. Gives `aWidget` (and the children that inherit
	/// its palette) the accent as its words, from `aGround`'s window. Only
	/// the text roles are set, so every other role still follows the
	/// application's palette; call it again when that palette changes.
	void apply_agent_accent(QWidget& aWidget, const QPalette& aGround);
} // namespace bpr::qt

#endif
