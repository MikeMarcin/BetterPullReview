#include "bpr/qt/appearance_settings_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstdint>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Returns the reviewer-facing name of one appearance mode.
		///
		/// \details Presentation text, derived from the core's own enumeration
		/// rather than from a parallel list, so a mode the core declares cannot
		/// be one this surface silently omits. The switch has no default: a
		/// seventh mode is a compile error here rather than a control that
		/// quietly offers six.
		[[nodiscard]] auto mode_text(const core::appearance_mode aMode) -> QString
		{
			switch (aMode) {
			case core::appearance_mode::system:
				return QStringLiteral("Follow the system");
			case core::appearance_mode::light:
				return QStringLiteral("Light");
			case core::appearance_mode::dark:
				return QStringLiteral("Dark");
			case core::appearance_mode::high_contrast_system:
				return QStringLiteral("High contrast, follow the system");
			case core::appearance_mode::high_contrast_light:
				return QStringLiteral("High contrast light");
			case core::appearance_mode::high_contrast_dark:
				return QStringLiteral("High contrast dark");
			}
			return { };
		}

		[[nodiscard]] auto target_text(const core::typography_target aTarget)
			-> QString
		{
			switch (aTarget) {
			case core::typography_target::ui_font:
				return QStringLiteral("Interface font");
			case core::typography_target::code_font:
				return QStringLiteral("Code font");
			}
			return { };
		}

		[[nodiscard]] auto field_text(const core::typography_field aField)
			-> QString
		{
			switch (aField) {
			case core::typography_field::family:
				return QStringLiteral("family");
			case core::typography_field::point_size:
				return QStringLiteral("size");
			case core::typography_field::weight:
				return QStringLiteral("weight");
			case core::typography_field::line_height:
				return QStringLiteral("line height");
			case core::typography_field::tab_width:
				return QStringLiteral("tab width");
			}
			return { };
		}

		[[nodiscard]] auto reason_text(const core::typography_reason aReason)
			-> QString
		{
			switch (aReason) {
			case core::typography_reason::none:
				return QStringLiteral("is not in effect");
			case core::typography_reason::below_minimum:
				return QStringLiteral("is below the smallest allowed value");
			case core::typography_reason::above_maximum:
				return QStringLiteral("is above the largest allowed value");
			case core::typography_reason::malformed_value:
				return QStringLiteral("is not a usable value");
			}
			return { };
		}

		/// \brief Applies the core's own bounds to one numeric control.
		///
		/// \details Taken from `core::bounds_of` rather than restated, so the
		/// control cannot offer a value the validator would refuse. That is the
		/// reason nearly every rejection path here is unreachable by pointing at
		/// a control - and why the one that is reachable, the family, is the one
		/// the tests drive.
		void bind_bounds(QSpinBox& aBox, const core::typography_field aField)
		{
			const auto bounds = core::bounds_of(aField);
			aBox.setRange(
				static_cast<int>(bounds.Minimum), static_cast<int>(bounds.Maximum));
		}
	} // namespace

	appearance_settings_panel::appearance_settings_panel(
		application::appearance_intent_dispatch aDispatch,
		QWidget* const                          aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setAccessibleName(QStringLiteral("Appearance settings"));

		ModeSelector = new QComboBox{ this };
		ModeSelector->setAccessibleName(QStringLiteral("Appearance mode"));
		// Built from the core's published count, so a mode added there appears
		// here without this file being edited. A restated list is exactly the
		// drift this avoids.
		for (auto index = std::size_t{ 0U }; index < core::appearance_mode_count;
			 ++index) {
			const auto mode = static_cast<core::appearance_mode>(index);
			ModeSelector->addItem(mode_text(mode), static_cast<int>(index));
		}

		UiFamily = new QLineEdit{ this };
		UiFamily->setAccessibleName(QStringLiteral("Interface font family"));
		UiSize       = new QSpinBox{ this };
		UiWeight     = new QSpinBox{ this };
		UiLineHeight = new QSpinBox{ this };
		UiSize->setAccessibleName(QStringLiteral("Interface font size, hundredths of a point"));
		UiWeight->setAccessibleName(QStringLiteral("Interface font weight"));
		UiLineHeight->setAccessibleName(QStringLiteral("Interface line height, percent"));
		bind_bounds(*UiSize, core::typography_field::point_size);
		bind_bounds(*UiWeight, core::typography_field::weight);
		bind_bounds(*UiLineHeight, core::typography_field::line_height);

		CodeFamily = new QLineEdit{ this };
		CodeFamily->setAccessibleName(QStringLiteral("Code font family"));
		CodeSize   = new QSpinBox{ this };
		CodeWeight = new QSpinBox{ this };
		TabWidth   = new QSpinBox{ this };
		CodeSize->setAccessibleName(QStringLiteral("Code font size, hundredths of a point"));
		CodeWeight->setAccessibleName(QStringLiteral("Code font weight"));
		TabWidth->setAccessibleName(QStringLiteral("Tab width, in spaces"));
		bind_bounds(*CodeSize, core::typography_field::point_size);
		bind_bounds(*CodeWeight, core::typography_field::weight);
		bind_bounds(*TabWidth, core::typography_field::tab_width);

		Ligatures    = new QCheckBox{ QStringLiteral("Ligatures"), this };
		SyntaxBold   = new QCheckBox{ QStringLiteral("Bold syntax emphasis"), this };
		SyntaxItalic = new QCheckBox{ QStringLiteral("Italic syntax emphasis"), this };

		Status = new QLabel{ this };
		Status->setWordWrap(true);
		Status->setAccessibleName(QStringLiteral("Appearance settings status"));

		// There is deliberately no code line height. BPR-0118 retired it on the
		// product owner's decision, and `code_font_settings` has no field one
		// could occupy, so this omission is structural rather than an oversight
		// a later reader should correct.
		auto* const uiGroup = new QGroupBox{ QStringLiteral("Interface font"), this };
		auto* const uiForm  = new QFormLayout{ uiGroup };
		uiForm->addRow(QStringLiteral("Family"), UiFamily);
		uiForm->addRow(QStringLiteral("Size"), UiSize);
		uiForm->addRow(QStringLiteral("Weight"), UiWeight);
		uiForm->addRow(QStringLiteral("Line height"), UiLineHeight);

		auto* const codeGroup = new QGroupBox{ QStringLiteral("Code font"), this };
		auto* const codeForm  = new QFormLayout{ codeGroup };
		codeForm->addRow(QStringLiteral("Family"), CodeFamily);
		codeForm->addRow(QStringLiteral("Size"), CodeSize);
		codeForm->addRow(QStringLiteral("Weight"), CodeWeight);
		codeForm->addRow(QStringLiteral("Tab width"), TabWidth);
		codeForm->addRow(Ligatures);
		codeForm->addRow(SyntaxBold);
		codeForm->addRow(SyntaxItalic);

		auto* const layout   = new QVBoxLayout{ this };
		auto* const modeForm = new QFormLayout{ };
		modeForm->addRow(QStringLiteral("Appearance"), ModeSelector);
		layout->addLayout(modeForm);
		layout->addWidget(uiGroup);
		layout->addWidget(codeGroup);
		layout->addWidget(Status);

		QObject::connect(
			ModeSelector,
			&QComboBox::currentIndexChanged,
			this,
			[this](int) { dispatch_mode(); });

		const auto onTypography = [this] { dispatch_typography(); };
		QObject::connect(UiFamily, &QLineEdit::editingFinished, this, onTypography);
		QObject::connect(CodeFamily, &QLineEdit::editingFinished, this, onTypography);
		for (auto* const box : { UiSize, UiWeight, UiLineHeight, CodeSize, CodeWeight, TabWidth }) {
			QObject::connect(
				box, &QSpinBox::valueChanged, this, [onTypography](int) {
					onTypography();
				});
		}
		for (auto* const box : { Ligatures, SyntaxBold, SyntaxItalic }) {
			QObject::connect(
				box, &QCheckBox::toggled, this, [onTypography](bool) {
					onTypography();
				});
		}

		show_adopted();
	}

	void appearance_settings_panel::set_dispatch(
		application::appearance_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings)
	{
		AdoptedSettings = aSettings;
		++Statistics.Adoptions;
		show_adopted();
		Status->setText(QStringLiteral("Appearance settings are in effect."));
	}

	void appearance_settings_panel::report(
		const std::span<const core::typography_rejection> aRejections)
	{
		++Statistics.Rejections;
		// Restored first. A reviewer who is told their value was refused must
		// be looking at the value that is actually in effect, not at the one
		// that was not adopted.
		show_adopted();
		if (aRejections.empty()) {
			Status->setText(QStringLiteral("Appearance settings were refused."));
			return;
		}
		auto text = QStringLiteral("Refused, and nothing changed:");
		for (const auto& rejection : aRejections) {
			text += QStringLiteral("\n· %1 %2 %3")
						.arg(
							target_text(rejection.Target),
							field_text(rejection.Field),
							reason_text(rejection.Reason));
		}
		Status->setText(text);
	}

	auto appearance_settings_panel::mode_selector() const noexcept -> QComboBox*
	{
		return ModeSelector;
	}

	auto appearance_settings_panel::ui_family_editor() const noexcept -> QLineEdit*
	{
		return UiFamily;
	}

	auto appearance_settings_panel::ui_size_editor() const noexcept -> QSpinBox*
	{
		return UiSize;
	}

	auto appearance_settings_panel::ui_weight_editor() const noexcept -> QSpinBox*
	{
		return UiWeight;
	}

	auto appearance_settings_panel::ui_line_height_editor() const noexcept
		-> QSpinBox*
	{
		return UiLineHeight;
	}

	auto appearance_settings_panel::code_family_editor() const noexcept
		-> QLineEdit*
	{
		return CodeFamily;
	}

	auto appearance_settings_panel::code_size_editor() const noexcept -> QSpinBox*
	{
		return CodeSize;
	}

	auto appearance_settings_panel::code_weight_editor() const noexcept
		-> QSpinBox*
	{
		return CodeWeight;
	}

	auto appearance_settings_panel::tab_width_editor() const noexcept -> QSpinBox*
	{
		return TabWidth;
	}

	auto appearance_settings_panel::ligatures_switch() const noexcept -> QCheckBox*
	{
		return Ligatures;
	}

	auto appearance_settings_panel::syntax_bold_switch() const noexcept
		-> QCheckBox*
	{
		return SyntaxBold;
	}

	auto appearance_settings_panel::syntax_italic_switch() const noexcept
		-> QCheckBox*
	{
		return SyntaxItalic;
	}

	auto appearance_settings_panel::status_text() const -> QString
	{
		return Status->text();
	}

	auto appearance_settings_panel::adopted() const noexcept
		-> const application::appearance_settings&
	{
		return AdoptedSettings;
	}

	auto appearance_settings_panel::statistics() const noexcept
		-> appearance_settings_panel_statistics
	{
		return Statistics;
	}

	void appearance_settings_panel::dispatch_mode()
	{
		if (Echoing) {
			++Statistics.SuppressedEchoes;
			return;
		}
		const auto index = ModeSelector->currentIndex();
		if (index < 0 || static_cast<std::size_t>(index) >= core::appearance_mode_count) {
			return;
		}
		++Statistics.ModeGestures;
		if (Dispatch) {
			Dispatch(application::select_appearance_mode_intent{
				.Mode = static_cast<core::appearance_mode>(index),
			});
		}
	}

	void appearance_settings_panel::dispatch_typography()
	{
		if (Echoing) {
			++Statistics.SuppressedEchoes;
			return;
		}
		++Statistics.FontGestures;
		if (Dispatch) {
			Dispatch(application::apply_typography_intent{
				.Typography = composed_typography(),
			});
		}
	}

	auto appearance_settings_panel::composed_typography() const
		-> core::typography_settings
	{
		return core::typography_settings{
			.Ui = {
				.Font = {
					.Family               = UiFamily->text().toStdString(),
					.PointSizeCentipoints = static_cast<std::uint32_t>(UiSize->value()),
					.Weight               = static_cast<std::uint32_t>(UiWeight->value()),
				},
				.LineHeightPercent = static_cast<std::uint32_t>(UiLineHeight->value()),
			},
			.Code = {
				.Font = {
					.Family               = CodeFamily->text().toStdString(),
					.PointSizeCentipoints = static_cast<std::uint32_t>(CodeSize->value()),
					.Weight               = static_cast<std::uint32_t>(CodeWeight->value()),
				},
				.TabWidth     = static_cast<std::uint32_t>(TabWidth->value()),
				.Ligatures    = Ligatures->isChecked(),
				.SyntaxBold   = SyntaxBold->isChecked(),
				.SyntaxItalic = SyntaxItalic->isChecked(),
			},
		};
	}

	void appearance_settings_panel::show_adopted()
	{
		Echoing            = true;
		const auto& record = AdoptedSettings;
		ModeSelector->setCurrentIndex(static_cast<int>(record.Mode));
		UiFamily->setText(QString::fromStdString(record.Typography.Ui.Font.Family));
		UiSize->setValue(
			static_cast<int>(record.Typography.Ui.Font.PointSizeCentipoints));
		UiWeight->setValue(static_cast<int>(record.Typography.Ui.Font.Weight));
		UiLineHeight->setValue(
			static_cast<int>(record.Typography.Ui.LineHeightPercent));
		CodeFamily->setText(
			QString::fromStdString(record.Typography.Code.Font.Family));
		CodeSize->setValue(
			static_cast<int>(record.Typography.Code.Font.PointSizeCentipoints));
		CodeWeight->setValue(static_cast<int>(record.Typography.Code.Font.Weight));
		TabWidth->setValue(static_cast<int>(record.Typography.Code.TabWidth));
		Ligatures->setChecked(record.Typography.Code.Ligatures);
		SyntaxBold->setChecked(record.Typography.Code.SyntaxBold);
		SyntaxItalic->setChecked(record.Typography.Code.SyntaxItalic);
		Echoing = false;
	}
} // namespace bpr::qt
