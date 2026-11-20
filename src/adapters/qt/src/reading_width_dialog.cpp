#include "bpr/qt/reading_width_dialog.hpp"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr std::array surfaces{ core::reading_surface::threads, core::reading_surface::discuss, core::reading_surface::reports };
		constexpr std::array presets{ core::reading_width::narrow, core::reading_width::medium, core::reading_width::wide };

		[[nodiscard]] auto surface_name(const core::reading_surface aSurface) -> QString
		{
			switch (aSurface) {
			case core::reading_surface::threads: return QObject::tr("Threads");
			case core::reading_surface::discuss: return QObject::tr("Discuss");
			case core::reading_surface::reports: return QObject::tr("Reports");
			}
			return { };
		}

		[[nodiscard]] auto object_name(const core::reading_surface aSurface) -> QString
		{
			switch (aSurface) {
			case core::reading_surface::threads: return QStringLiteral("Threads");
			case core::reading_surface::discuss: return QStringLiteral("Discuss");
			case core::reading_surface::reports: return QStringLiteral("Reports");
			}
			return { };
		}

		[[nodiscard]] auto preset_name(const core::reading_width aWidth) -> QString
		{
			switch (aWidth) {
			case core::reading_width::narrow: return QObject::tr("Narrow");
			case core::reading_width::medium: return QObject::tr("Medium");
			case core::reading_width::wide:   return QObject::tr("Wide");
			case core::reading_width::full:   return QObject::tr("Full width");
			}
			return { };
		}

		[[nodiscard]] auto pixels(const std::uint32_t aWidth) -> QString
		{
			return QObject::tr("%1 px").arg(QLocale{ }.toString(aWidth));
		}

		/// "Wide, 1,100 px"; "Full width".
		[[nodiscard]] auto preset_words(const core::reading_width aWidth) -> QString
		{
			const auto limit = core::reading_width_limit(aWidth);
			return limit.has_value() ? QObject::tr("%1, %2").arg(preset_name(aWidth), pixels(*limit)) : preset_name(aWidth);
		}

		[[nodiscard]] auto choice_of(const core::reading_override& aOverride) -> reading_width_dialog::choice
		{
			switch (aOverride.Kind) {
			case core::reading_override_kind::reading_width: return reading_width_dialog::choice::reading_width;
			case core::reading_override_kind::full:          return reading_width_dialog::choice::full;
			case core::reading_override_kind::width:         break;
			}
			for (auto index = std::size_t{ 0U }; index < presets.size(); ++index) {
				if (core::reading_width_limit(presets[index]) == aOverride.Width) {
					return static_cast<reading_width_dialog::choice>(index + 1U);
				}
			}
			return reading_width_dialog::choice::custom;
		}
	} // namespace

	reading_width_dialog::reading_width_dialog(QWidget* const aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("readingWidthAdvanced"));
		setWindowTitle(tr("Reading width: Advanced"));
		auto* const layout = new QVBoxLayout{ this };
		layout->setSpacing(10);
		auto* const intro = new QLabel{ tr("Threads, Discuss and reports use the Reading width. Give any of them a width of its own."), this };
		intro->setObjectName(QStringLiteral("readingWidthAdvancedIntro"));
		intro->setWordWrap(true);
		layout->addWidget(intro);
		auto* const grid = new QGridLayout{ };
		grid->setHorizontalSpacing(10);
		grid->setVerticalSpacing(8);
		for (const auto surface : surfaces) {
			const auto  index = static_cast<std::size_t>(surface);
			auto* const label = new QLabel{ surface_name(surface), this };
			auto* const list  = new QComboBox{ this };
			list->setObjectName(QStringLiteral("readingWidth%1").arg(object_name(surface)));
			list->setAccessibleName(tr("%1 width").arg(surface_name(surface)));
			list->addItem(QString{ });
			for (const auto preset : presets) {
				list->addItem(preset_words(preset));
			}
			list->addItem(preset_name(core::reading_width::full));
			list->addItem(tr("Custom width"));
			label->setBuddy(list);
			auto* const custom = new QSpinBox{ this };
			custom->setObjectName(QStringLiteral("readingWidth%1Custom").arg(object_name(surface)));
			custom->setAccessibleName(tr("%1 custom width in pixels").arg(surface_name(surface)));
			custom->setRange(static_cast<int>(core::minimum_reading_override), static_cast<int>(core::maximum_reading_override));
			custom->setSingleStep(20);
			custom->setSuffix(tr(" px"));
			custom->setKeyboardTracking(false);
			custom->setToolTip(tr("At least %1, Narrow's width; at most %2.").arg(pixels(core::minimum_reading_override), pixels(core::maximum_reading_override)));
			Choices[index] = list;
			Custom[index]  = custom;
			const auto row = static_cast<int>(index);
			grid->addWidget(label, row, 0);
			grid->addWidget(list, row, 1);
			grid->addWidget(custom, row, 2);
			connect(list, &QComboBox::currentIndexChanged, this, [this, surface](const int) {
				settle(surface);
				dispatch();
			});
			connect(custom, &QSpinBox::valueChanged, this, [this](const int) { dispatch(); });
		}
		grid->setColumnStretch(1, 1);
		layout->addLayout(grid);
		Bounds = new QLabel{ tr("A custom width is at least %1, Narrow's width, and at most %2.").arg(pixels(core::minimum_reading_override), pixels(core::maximum_reading_override)), this };
		Bounds->setObjectName(QStringLiteral("readingWidthAdvancedBounds"));
		Bounds->setWordWrap(true);
		Bounds->setForegroundRole(QPalette::PlaceholderText);
		layout->addWidget(Bounds);
		auto* const buttons = new QHBoxLayout{ };
		Reset               = new QPushButton{ tr("Use Reading width for all"), this };
		Reset->setObjectName(QStringLiteral("readingWidthAdvancedReset"));
		Reset->setAutoDefault(false);
		connect(Reset, &QPushButton::clicked, this, [this] {
			set_state(Width, core::reading_overrides{ });
			dispatch();
		});
		auto* const done = new QPushButton{ tr("Done"), this };
		done->setObjectName(QStringLiteral("readingWidthAdvancedDone"));
		done->setDefault(true);
		connect(done, &QPushButton::clicked, this, &QDialog::accept);
		buttons->addWidget(Reset);
		buttons->addStretch(1);
		buttons->addWidget(done);
		layout->addLayout(buttons);
		set_state(Width, core::reading_overrides{ });
	}

	void reading_width_dialog::set_state(const core::reading_width aWidth, const core::reading_overrides& aOverrides)
	{
		const auto applying = std::exchange(Applying, true);
		Width               = aWidth;
		for (const auto surface : surfaces) {
			const auto  index = static_cast<std::size_t>(surface);
			const auto& one   = aOverrides[index];
			auto* const list  = Choices[index];
			const auto  block = QSignalBlocker{ list };
			list->setItemText(static_cast<int>(choice::reading_width), tr("Use Reading width (%1)").arg(preset_words(aWidth)));
			list->setCurrentIndex(static_cast<int>(choice_of(one)));
			if (one.Kind == core::reading_override_kind::width) {
				const auto spin = QSignalBlocker{ Custom[index] };
				Custom[index]->setValue(static_cast<int>(core::reading_override_of(one.Width).Width));
			}
			settle(surface);
		}
		Applying = applying;
	}

	void reading_width_dialog::set_dispatch(std::function<void(const core::reading_overrides&)> aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto reading_width_dialog::overrides() const -> core::reading_overrides
	{
		auto result = core::reading_overrides{ };
		for (const auto surface : surfaces) {
			const auto index  = static_cast<std::size_t>(surface);
			const auto chosen = static_cast<choice>(Choices[index]->currentIndex());
			switch (chosen) {
			case choice::reading_width: break;
			case choice::narrow:
			case choice::medium:
			case choice::wide:
				result[index] = core::reading_override_of(*core::reading_width_limit(presets[static_cast<std::size_t>(chosen) - 1U]));
				break;
			case choice::full:   result[index] = core::reading_override{ .Kind = core::reading_override_kind::full }; break;
			case choice::custom: result[index] = core::reading_override_of(static_cast<std::uint32_t>(Custom[index]->value())); break;
			}
		}
		return result;
	}

	auto reading_width_dialog::choices(const core::reading_surface aSurface) const -> QComboBox*
	{
		return Choices[static_cast<std::size_t>(aSurface)];
	}

	auto reading_width_dialog::custom_width(const core::reading_surface aSurface) const -> QSpinBox*
	{
		return Custom[static_cast<std::size_t>(aSurface)];
	}

	/// The width box is for Custom width; otherwise it shows, greyed, the
	/// width the choice gives.
	void reading_width_dialog::settle(const core::reading_surface aSurface)
	{
		const auto  index  = static_cast<std::size_t>(aSurface);
		auto* const custom = Custom[index];
		const auto  chosen = static_cast<choice>(Choices[index]->currentIndex());
		custom->setEnabled(chosen == choice::custom);
		if (chosen == choice::custom) {
			custom->setSpecialValueText(QString{ });
			return;
		}
		const auto shown = core::reading_surface_limit(Width, overrides(), aSurface);
		const auto block = QSignalBlocker{ custom };
		custom->setSpecialValueText(shown.has_value() ? QString{ } : preset_name(core::reading_width::full));
		custom->setValue(shown.has_value() ? static_cast<int>(*shown) : custom->minimum());
	}

	void reading_width_dialog::dispatch()
	{
		if (!Applying && Dispatch) {
			Dispatch(overrides());
		}
	}
} // namespace bpr::qt
