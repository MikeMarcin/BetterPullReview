#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FEATURE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_FEATURE_HPP

#include <QColor>
#include <QPalette>

#include <cstdint>
#include <optional>

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
	/// \details An indigo, deep in a light window and soft in a dark one,
	/// each at least 4.5:1 against the window and base colours of either
	/// kind (macOS's own and the profiles'). Owner look v001 ("red looks like
	/// an error"): purple, blue or green. The hue (about 243°) sits about
	/// 30° from both the link, focus and unread blue (212°) and the violet
	/// that means fixed and merged (261°); green would read as added or
	/// accepted, and amber is the tour's route.
	///
	/// [[BPR-2000]], RFC-BPR-0101: the accent is the Agent accent role of the
	/// appearance on screen (Appearance › Colours), which the application
	/// gives `set_agent_accent`; until it does, the shipped Light or Dark
	/// value by `aPalette`'s window. No surface keeps a fixed value.
	[[nodiscard]] auto agent_accent(const QPalette& aPalette) -> QColor;

	/// \brief [[BPR-2000]]. The Agent accent of the appearance on screen;
	/// none returns to the shipped values. Answers whether it changed.
	auto set_agent_accent(std::optional<QColor> aAccent) -> bool;

	/// \brief [[BPR-2000]], RFC-BPR-0101. A control that starts or continues
	/// an agent run: it wears the accent (`apply_agent_accent`) and says so
	/// (`agent_run_control`), so an inventory can find every one. Any other
	/// control uses the ordinary button roles.
	void wear_agent_accent(QWidget& aControl, const QPalette& aGround);
	/// \brief The control was marked by `wear_agent_accent`.
	[[nodiscard]] auto agent_run_control(const QWidget& aControl) -> bool;
	/// \brief Takes the accent off a control that no longer starts a run
	/// (Stop, while Send is Stop), back to the ordinary roles.
	void wear_ordinary_roles(QWidget& aControl);

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
