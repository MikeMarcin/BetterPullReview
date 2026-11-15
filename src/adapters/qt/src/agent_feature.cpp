#include "bpr/qt/agent_feature.hpp"

#include <QWidget>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto dark_ground(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Active, QPalette::Window).lightness() < 128; }
	} // namespace

	namespace {
		/// The Agent accent of the appearance on screen, as the application
		/// last gave it. GUI thread only, as every widget is.
		[[nodiscard]] auto current_accent() -> std::optional<QColor>&
		{
			static auto accent = std::optional<QColor>{ };
			return accent;
		}
	} // namespace

	auto agent_accent(const QPalette& aPalette) -> QColor
	{
		if (const auto& accent = current_accent(); accent && accent->isValid()) {
			return *accent;
		}
		// The shipped values, which the built-in Light and Dark themes carry.
		// Light #4A3FD6: 7.1:1 on white, 5.7:1 on macOS's #E7E7E7 window.
		// Dark #A3A0FF: 8.1:1 on #0D1117, 5.5:1 on macOS's #323232 window.
		return dark_ground(aPalette) ? QColor{ 0xA3, 0xA0, 0xFF } : QColor{ 0x4A, 0x3F, 0xD6 };
	}

	auto set_agent_accent(std::optional<QColor> aAccent) -> bool
	{
		auto& accent = current_accent();
		if (accent == aAccent) {
			return false;
		}
		accent = std::move(aAccent);
		return true;
	}

	void wear_agent_accent(QWidget& aControl, const QPalette& aGround)
	{
		aControl.setProperty("bprAgentRun", true);
		apply_agent_accent(aControl, aGround);
	}

	auto agent_run_control(const QWidget& aControl) -> bool { return aControl.property("bprAgentRun").toBool(); }

	void wear_ordinary_roles(QWidget& aControl)
	{
		aControl.setProperty("bprAgentRun", false);
		aControl.setPalette(QPalette{ });
	}

	auto agent_accent_disabled(const QPalette& aPalette) -> QColor
	{
		const auto     ground = aPalette.color(QPalette::Active, QPalette::Window);
		const auto     accent = agent_accent(aPalette);
		constexpr auto keep   = 0.55;
		return QColor::fromRgbF(static_cast<float>((accent.redF() * keep) + (ground.redF() * (1.0 - keep))), static_cast<float>((accent.greenF() * keep) + (ground.greenF() * (1.0 - keep))),
								static_cast<float>((accent.blueF() * keep) + (ground.blueF() * (1.0 - keep))));
	}

	void apply_agent_accent(QWidget& aWidget, const QPalette& aGround)
	{
		const auto accent   = agent_accent(aGround);
		const auto disabled = agent_accent_disabled(aGround);
		auto       colours  = QPalette{ };
		for (const auto role : { QPalette::WindowText, QPalette::ButtonText, QPalette::Text }) {
			colours.setColor(QPalette::Active, role, accent);
			colours.setColor(QPalette::Inactive, role, accent);
			colours.setColor(QPalette::Disabled, role, disabled);
		}
		aWidget.setPalette(colours);
	}
} // namespace bpr::qt
