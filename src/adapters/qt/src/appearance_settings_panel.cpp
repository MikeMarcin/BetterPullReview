#include "bpr/qt/appearance_settings_panel.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstdint>
#include <string>
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

		[[nodiscard]] auto theme_reason_text(const core::theme_import_reason aReason)
			-> QString
		{
			switch (aReason) {
			case core::theme_import_reason::document_too_large:
				return QStringLiteral("the document is larger than the safe limit");
			case core::theme_import_reason::malformed_document:
				return QStringLiteral("the document structure is malformed");
			case core::theme_import_reason::unsupported_version:
				return QStringLiteral("the theme version is unsupported");
			case core::theme_import_reason::unknown_field:
				return QStringLiteral("the document contains an unknown field");
			case core::theme_import_reason::duplicate_field:
				return QStringLiteral("the document repeats a field");
			case core::theme_import_reason::missing_field:
				return QStringLiteral("the document omits a required field");
			case core::theme_import_reason::external_reference:
				return QStringLiteral("external references are not allowed");
			case core::theme_import_reason::include_directive:
				return QStringLiteral("include directives are not allowed");
			case core::theme_import_reason::style_payload:
				return QStringLiteral("style payloads are not allowed");
			case core::theme_import_reason::executable_payload:
				return QStringLiteral("executable payloads are not allowed");
			case core::theme_import_reason::malformed_color:
				return QStringLiteral("a semantic colour is malformed");
			}
			return QStringLiteral("the document was refused");
		}

		[[nodiscard]] auto variant_text(const core::appearance_variant aVariant)
			-> QString
		{
			switch (aVariant) {
			case core::appearance_variant::light:
				return QStringLiteral("Light");
			case core::appearance_variant::dark:
				return QStringLiteral("Dark");
			case core::appearance_variant::high_contrast_light:
				return QStringLiteral("High contrast light");
			case core::appearance_variant::high_contrast_dark:
				return QStringLiteral("High contrast dark");
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

		RoleSelector = new QComboBox{ this };
		RoleSelector->setAccessibleName(QStringLiteral("Semantic colour role"));
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			const auto name = core::stable_name(
				static_cast<core::appearance_role>(index));
			RoleSelector->addItem(
				QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())),
				static_cast<int>(index));
		}
		ColorEditor = new QLineEdit{ this };
		ColorEditor->setAccessibleName(QStringLiteral("Semantic colour value"));
		ColorEditor->setPlaceholderText(QStringLiteral("#rrggbb or #aarrggbb"));
		PreviewButton = new QPushButton{ QStringLiteral("Preview"), this };
		CommitButton  = new QPushButton{ QStringLiteral("Commit"), this };
		CancelButton  = new QPushButton{ QStringLiteral("Cancel preview"), this };
		ImportButton  = new QPushButton{ QStringLiteral("Import theme…"), this };
		CommitButton->setEnabled(false);
		CancelButton->setEnabled(false);

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

		auto* const colorGroup = new QGroupBox{ QStringLiteral("Semantic colours"), this };
		auto* const colorForm  = new QFormLayout{ colorGroup };
		colorForm->addRow(QStringLiteral("Role"), RoleSelector);
		colorForm->addRow(QStringLiteral("Colour"), ColorEditor);
		auto* const colorActions = new QHBoxLayout{ };
		colorActions->addWidget(PreviewButton);
		colorActions->addWidget(CommitButton);
		colorActions->addWidget(CancelButton);
		auto* const exchangeActions = new QHBoxLayout{ };
		exchangeActions->addWidget(ImportButton);
		auto* const exportButton = new QPushButton{ QStringLiteral("Export saved theme…"), this };
		exportButton->setObjectName(QStringLiteral("appearanceExportTheme"));
		exchangeActions->addWidget(exportButton);
		colorForm->addRow(exchangeActions);
		QObject::connect(exportButton, &QPushButton::clicked, this, [this] {
			const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Export saved theme"), QStringLiteral("theme.bprtheme"), QStringLiteral("Theme files (*.bprtheme)"));
			request_theme_export(path);
		});
		colorForm->addRow(colorActions);

		auto* const layout = new QVBoxLayout{ this };
		// [[BPR-0652]]. The accepted page opens with the theme as four
		// previews drawn from the palettes themselves; choosing one is the
		// same gesture as the mode selector beneath, which stays for the
		// modes the previews do not name.
		auto* const subtitle = new QLabel{
			QStringLiteral("Changes apply as you make them and show in the review window behind this one."),
			this
		};
		subtitle->setObjectName(QStringLiteral("appearanceSubtitle"));
		subtitle->setWordWrap(true);
		layout->addWidget(subtitle);
		ThemePreviews = new theme_preview_strip{ this };
		ThemePreviews->set_dispatch([this](const core::appearance_mode aMode) {
			const auto index = static_cast<int>(aMode);
			if (ModeSelector->currentIndex() == index) {
				return;
			}
			ModeSelector->setCurrentIndex(index);
		});
		layout->addWidget(ThemePreviews);
		auto* const modeForm = new QFormLayout{ };
		modeForm->addRow(QStringLiteral("Appearance"), ModeSelector);
		const auto addReset = [this](QFormLayout& aForm, const QString& aLabel, const application::appearance_reset_scope aScope) {
			auto* const button = new QPushButton{ aLabel, this };
			button->setAccessibleName(aLabel);
			aForm.addRow(button);
			QObject::connect(button, &QPushButton::clicked, this, [this, aScope] {
				PendingPreview.reset();
				if (Dispatch) {
					Dispatch(application::reset_appearance_intent{ aScope });
				}
			});
		};
		addReset(*modeForm, QStringLiteral("Reset appearance mode"), application::appearance_reset_scope::mode);
		addReset(*uiForm, QStringLiteral("Reset interface font"), application::appearance_reset_scope::interface_font);
		addReset(*codeForm, QStringLiteral("Reset code font"), application::appearance_reset_scope::code_font);
		addReset(*colorForm, QStringLiteral("Reset semantic colours"), application::appearance_reset_scope::palette);
		layout->addLayout(modeForm);
		layout->addWidget(colorGroup);
		layout->addWidget(uiGroup);
		layout->addWidget(codeGroup);
		layout->addWidget(Status);

		QObject::connect(
			ModeSelector,
			&QComboBox::currentIndexChanged,
			this,
			[this](int) { dispatch_mode(); });
		QObject::connect(
			RoleSelector,
			&QComboBox::currentIndexChanged,
			this,
			[this](int) { show_selected_color(); });
		QObject::connect(
			PreviewButton, &QPushButton::clicked, this, [this] {
				dispatch_palette_preview();
			});
		QObject::connect(
			CommitButton, &QPushButton::clicked, this, [this] {
				dispatch_palette_commit();
			});
		QObject::connect(
			CancelButton, &QPushButton::clicked, this, [this] {
				dispatch_palette_cancel();
			});
		QObject::connect(
			ImportButton, &QPushButton::clicked, this, [this] {
				select_theme_file();
			});

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

		CommittedPalette = AdoptedSettings.Palette;
		show_adopted();
	}

	void appearance_settings_panel::set_dispatch(
		application::appearance_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void appearance_settings_panel::report_persistence_unavailable()
	{
		PersistenceAvailable = false;
		Status->setText(with_persistence_status(QString{ }));
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings)
	{
		adopt(aSettings, aSettings.Palette, std::nullopt);
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings,
		const core::appearance_palette&         aResolvedPalette)
	{
		adopt(aSettings, aResolvedPalette, std::nullopt);
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings,
		const core::appearance_palette&         aResolvedPalette,
		std::optional<core::appearance_palette> aActivePreview)
	{
		PendingPreview.reset();
		AdoptedSettings  = aSettings;
		CommittedPalette = aResolvedPalette;
		PreviewPalette   = std::move(aActivePreview);
		++Statistics.Adoptions;
		show_adopted();
		Status->setText(with_persistence_status(PreviewPalette.has_value()
													? QStringLiteral(
														  "Appearance settings are in effect; the palette preview remains unsaved.")
													: QStringLiteral("Appearance settings are in effect.")));
	}

	void appearance_settings_panel::preview_import(
		const core::appearance_palette& aPalette)
	{
		PendingPreview.reset();
		PreviewPalette = aPalette;
		show_selected_color();
		CommitButton->setEnabled(true);
		CancelButton->setEnabled(true);
		Status->setText(with_persistence_status(
			QStringLiteral("Imported theme is previewed only; commit to save it.")));
	}

	void appearance_settings_panel::report(
		const std::span<const core::theme_import_rejection> aRejections)
	{
		++Statistics.Rejections;
		auto text = QStringLiteral(
			"Theme import refused; the committed palette is unchanged:");
		for (const auto& rejection : aRejections) {
			text += QStringLiteral("\n· Line %1, %2: %3")
						.arg(static_cast<qulonglong>(rejection.Line))
						.arg(QString::fromStdString(rejection.Subject))
						.arg(theme_reason_text(rejection.Reason));
		}
		Status->setText(with_persistence_status(std::move(text)));
	}

	void appearance_settings_panel::report(
		const std::span<const application::appearance_contrast_rejection> aRejections)
	{
		++Statistics.Rejections;
		auto text = QStringLiteral(
			"Palette refused; the committed palette is unchanged:");
		for (const auto& rejection : aRejections) {
			const auto foreground = core::stable_name(rejection.Finding.Pair.Foreground);
			const auto background = core::stable_name(rejection.Finding.Pair.Background);
			text += QStringLiteral("\n· %1: %2 on %3 is %4:1; %5:1 is required")
						.arg(variant_text(rejection.Variant))
						.arg(QString::fromUtf8(
							foreground.data(), static_cast<qsizetype>(foreground.size())))
						.arg(QString::fromUtf8(
							background.data(), static_cast<qsizetype>(background.size())))
						.arg(static_cast<double>(rejection.Finding.RatioHundredths) / 100.0,
							 0,
							 'f',
							 2)
						.arg(static_cast<double>(rejection.Finding.MinimumHundredths) / 100.0,
							 0,
							 'f',
							 2);
		}
		Status->setText(with_persistence_status(std::move(text)));
	}

	void appearance_settings_panel::request_theme_export(const QString& aPath)
	{
		if (!aPath.isEmpty() && Dispatch) {
			Dispatch(application::export_appearance_theme_intent{ aPath.toStdString() });
		}
	}

	void appearance_settings_panel::report_theme_export(const bool aSucceeded)
	{
		Status->setText(with_persistence_status(aSucceeded
													? QStringLiteral("Saved theme exported. Unsaved previews are excluded.")
													: QStringLiteral("Theme export failed. Choose a writable regular file and try again.")));
	}

	void appearance_settings_panel::report_theme_document_unavailable()
	{
		++Statistics.Rejections;
		Status->setText(with_persistence_status(QStringLiteral(
			"Theme import failed because the selected local file could not be read; "
			"the committed palette is unchanged.")));
	}

	void appearance_settings_panel::request_theme_import(const QString& aPath)
	{
		if (aPath.isEmpty()) {
			return;
		}
		PendingPreview.reset();
		++Statistics.ThemeImports;
		if (Dispatch) {
			Dispatch(application::import_appearance_theme_intent{
				.Path = aPath.toStdString(),
			});
		}
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
			Status->setText(with_persistence_status(
				QStringLiteral("Appearance settings were refused.")));
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
		Status->setText(with_persistence_status(std::move(text)));
	}

	auto appearance_settings_panel::theme_previews() const noexcept -> theme_preview_strip*
	{
		return ThemePreviews;
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

	auto appearance_settings_panel::role_selector() const noexcept -> QComboBox*
	{
		return RoleSelector;
	}

	auto appearance_settings_panel::color_editor() const noexcept -> QLineEdit*
	{
		return ColorEditor;
	}

	auto appearance_settings_panel::preview_button() const noexcept -> QPushButton*
	{
		return PreviewButton;
	}

	auto appearance_settings_panel::commit_button() const noexcept -> QPushButton*
	{
		return CommitButton;
	}

	auto appearance_settings_panel::cancel_button() const noexcept -> QPushButton*
	{
		return CancelButton;
	}

	auto appearance_settings_panel::import_button() const noexcept -> QPushButton*
	{
		return ImportButton;
	}

	auto appearance_settings_panel::preview_palette() const noexcept
		-> const std::optional<core::appearance_palette>&
	{
		return PreviewPalette;
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

	void appearance_settings_panel::dispatch_palette_preview()
	{
		const auto index = RoleSelector->currentIndex();
		const auto color = QColor{ ColorEditor->text() };
		if (index < 0
			|| static_cast<std::size_t>(index) >= core::appearance_role_count
			|| !color.isValid()) {
			Status->setText(with_persistence_status(QStringLiteral(
				"Enter a valid #rrggbb or #aarrggbb semantic colour.")));
			return;
		}
		auto palette                             = PreviewPalette.value_or(CommittedPalette);
		palette[static_cast<std::size_t>(index)] = to_appearance_color(color);
		PreviewPalette                           = palette;
		PendingPreview                           = palette;
		CommitButton->setEnabled(true);
		CancelButton->setEnabled(true);
		Status->setText(with_persistence_status(
			QStringLiteral("Palette is previewed only; commit to save it.")));
		if (!PreviewDeliveryQueued) {
			PreviewDeliveryQueued = true;
			// One widget-bound delivery consumes the latest candidate in this event burst.
			QMetaObject::invokeMethod(this, [this] {
				PreviewDeliveryQueued = false;
				auto pending = std::exchange(PendingPreview, std::nullopt);
				if (pending && Dispatch) {
					++Statistics.PalettePreviews;
					Dispatch(application::preview_appearance_palette_intent{
						.Palette = std::move(*pending),
					});
				} }, Qt::QueuedConnection);
		}
	}

	void appearance_settings_panel::dispatch_palette_commit()
	{
		if (!PreviewPalette.has_value()) {
			return;
		}
		PendingPreview.reset();
		++Statistics.PaletteCommits;
		if (Dispatch) {
			Dispatch(application::commit_appearance_palette_intent{
				.Palette = *PreviewPalette,
			});
		}
	}

	void appearance_settings_panel::cancel_preview()
	{
		dispatch_palette_cancel();
	}

	void appearance_settings_panel::dispatch_palette_cancel()
	{
		PendingPreview.reset();
		PreviewPalette.reset();
		++Statistics.PaletteCancels;
		show_selected_color();
		CommitButton->setEnabled(PreviewPalette.has_value());
		CancelButton->setEnabled(PreviewPalette.has_value());
		Status->setText(with_persistence_status(
			QStringLiteral("Preview cancelled; the committed palette is restored.")));
		if (Dispatch) {
			Dispatch(application::cancel_appearance_palette_preview_intent{ });
		}
	}

	void appearance_settings_panel::select_theme_file()
	{
		request_theme_import(QFileDialog::getOpenFileName(
			this,
			QStringLiteral("Import appearance theme"),
			{ },
			QStringLiteral("BetterPullReview themes (*.bprtheme *.theme);;All files (*)")));
	}

	void appearance_settings_panel::show_selected_color()
	{
		const auto index = RoleSelector->currentIndex();
		if (index < 0
			|| static_cast<std::size_t>(index) >= core::appearance_role_count) {
			ColorEditor->clear();
			return;
		}
		const auto& palette = PreviewPalette.value_or(CommittedPalette);
		ColorEditor->setText(
			to_presentation_color(palette[static_cast<std::size_t>(index)])
				.name(QColor::HexArgb));
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
		ThemePreviews->set_current(record.Mode);
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
		show_selected_color();
		CommitButton->setEnabled(PreviewPalette.has_value());
		CancelButton->setEnabled(PreviewPalette.has_value());
		Echoing = false;
	}

	auto appearance_settings_panel::with_persistence_status(
		QString aStatus) const -> QString
	{
		if (PersistenceAvailable) {
			return aStatus;
		}
		if (!aStatus.isEmpty()) {
			aStatus += QLatin1Char('\n');
		}
		aStatus += QStringLiteral(
			"Settings storage is unavailable. Appearance changes apply only "
			"for this session; if storage remains unavailable, the next launch "
			"uses System defaults.");
		return aStatus;
	}
} // namespace bpr::qt
