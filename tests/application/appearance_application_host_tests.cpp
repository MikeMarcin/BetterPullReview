#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto blob(const std::uint8_t aSeed) -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = aSeed;
		bytes[19]  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto key(const std::uint8_t     aSeed,
						   const std::string_view aDefinition = "C++",
						   const std::uint32_t    aVersion    = 1U)
		-> application::token_span_cache_key
	{
		return {
			.Blob       = blob(aSeed),
			.Definition = {
				.Name    = std::string{ aDefinition },
				.Version = aVersion,
			},
		};
	}

	/// \brief Returns the legal base appearance a composition would supply.
	[[nodiscard]] auto base_settings() -> application::appearance_settings
	{
		auto settings          = application::appearance_settings{ };
		settings.Typography.Ui = core::ui_font_settings{
			.Font = core::font_settings{
				.Family               = "Base UI",
				.PointSizeCentipoints = 1200U,
				.Weight               = 400U,
			},
			.LineHeightPercent = 120U,
		};
		settings.Typography.Code.Font = core::font_settings{
			.Family               = "Base Code",
			.PointSizeCentipoints = 1300U,
			.Weight               = 400U,
		};
		settings.Typography.Code.TabWidth = 4U;
		// No customization, for any variant. A composed base is what a fresh
		// installation starts from, and that is the absence of a decision rather
		// than a palette a reader would have to interpret.
		return settings;
	}

	[[nodiscard]] auto contrast_safe_palette() -> core::appearance_palette
	{
		auto palette = core::appearance_palette{ };
		palette.fill(core::appearance_color{ .Alpha = 255U });
		for (const auto& pair : core::required_contrast_pairs()) {
			palette[core::index_of(pair.Background)]
				= core::appearance_color{ .Alpha = 255U };
			if (pair.Wash.has_value()) {
				palette[core::index_of(*pair.Wash)]
					= core::appearance_color{ .Alpha = 255U };
			}
			palette[core::index_of(pair.Foreground)] = core::appearance_color{
				.Red = 255U, .Green = 255U, .Blue = 255U, .Alpha = 255U
			};
		}
		return palette;
	}

	/// \brief Returns the built-in palettes a composition would supply.
	///
	/// \details [[BPR-1479]]. Each is the contrast-safe palette, except that the
	/// two C++ identifier roles carry the shipped Light and Dark inks: Light's
	/// is legible only on a light canvas and Dark's only on a dark one, so an
	/// import filled from the wrong base is refused.
	[[nodiscard]] auto builtin_palettes() -> application::appearance_builtin_palettes
	{
		auto palettes = application::appearance_builtin_palettes{ };
		palettes.fill(contrast_safe_palette());
		auto& light                                                      = palettes[application::variant_index(core::appearance_variant::light)];
		auto& dark                                                       = palettes[application::variant_index(core::appearance_variant::dark)];
		light[core::index_of(core::appearance_role::syntax_namespace)]   = core::appearance_color{ .Red = 0x1BU, .Green = 0x5EU, .Blue = 0x70U, .Alpha = 255U };
		light[core::index_of(core::appearance_role::syntax_enum_member)] = core::appearance_color{ .Red = 0x2FU, .Green = 0x4FU, .Blue = 0x4FU, .Alpha = 255U };
		dark[core::index_of(core::appearance_role::syntax_namespace)]    = core::appearance_color{ .Red = 0xB8U, .Green = 0xD7U, .Blue = 0xA3U, .Alpha = 255U };
		dark[core::index_of(core::appearance_role::syntax_enum_member)]  = core::appearance_color{ .Red = 0xD6U, .Green = 0xA0U, .Blue = 0x5AU, .Alpha = 255U };
		return palettes;
	}

	/// \brief Returns a legal durable record that differs in every domain.
	[[nodiscard]] auto persisted_settings() -> application::appearance_settings
	{
		auto settings = base_settings();
		settings.Mode = core::appearance_mode::dark;
		// A persisted record, so the slot it carries is a stored one. BPR-0216: a
		// default palette and a chosen one are different facts, and only the second
		// is customisation. That is now said per variant, so this record customises
		// Dark and says nothing at all about the other three themes.
		auto dark = core::appearance_palette{ };
		dark.fill(core::appearance_color{
			.Red = 200U, .Green = 200U, .Blue = 200U, .Alpha = 255U });
		application::customisation_of(settings, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ .Palette = dark,
															 .Stored  = true };
		settings.Typography.Ui.Font.PointSizeCentipoints = 1100U;
		return settings;
	}

	/// \brief Returns typography with one illegal value in each font.
	[[nodiscard]] auto illegal_typography() -> core::typography_settings
	{
		auto typography                         = base_settings().Typography;
		typography.Ui.Font.PointSizeCentipoints = 100U;
		typography.Code.TabWidth                = 64U;
		return typography;
	}

	[[nodiscard]] auto expected_rejections()
		-> std::vector<core::typography_rejection>
	{
		return {
			core::typography_rejection{
				.Target = core::typography_target::ui_font,
				.Field  = core::typography_field::point_size,
				.Reason = core::typography_reason::below_minimum,
			},
			core::typography_rejection{
				.Target = core::typography_target::code_font,
				.Field  = core::typography_field::tab_width,
				.Reason = core::typography_reason::above_maximum,
			},
		};
	}

	auto expect_exact_rejections(
		const std::vector<core::typography_rejection>& aObserved,
		const std::string_view                         aMessage) -> void
	{
		const auto expected = expected_rejections();
		expect(aObserved.size() == expected.size(), aMessage);
		for (std::size_t index = 0U; index < expected.size(); ++index) {
			expect(aObserved[index] == expected[index], aMessage);
		}
	}

	/// \brief Stands in for the durable appearance path across host lifetimes.
	class fake_settings final : public application::appearance_settings_port
	{
	public:
		[[nodiscard]] auto load(const std::stop_token aStop)
			-> application::appearance_settings_result<
				std::optional<application::appearance_settings>> override
		{
			++Loads;
			LoadStopObserved.push_back(aStop.stop_requested());
			if (!Available) {
				return std::unexpected{ application::appearance_settings_error{
					.Failure = application::appearance_settings_failure::unavailable } };
			}
			return Stored;
		}

		[[nodiscard]] auto store(
			const application::appearance_settings& aSettings,
			const std::stop_token                   aStop)
			-> application::appearance_settings_result<void> override
		{
			++Stores;
			StoreStopObserved.push_back(aStop.stop_requested());
			if (!Available) {
				return std::unexpected{ application::appearance_settings_error{
					.Failure = application::appearance_settings_failure::unavailable } };
			}
			Stored = aSettings;
			return { };
		}

		std::optional<application::appearance_settings> Stored;
		bool                                            Available = true;
		std::size_t                                     Loads     = 0U;
		std::size_t                                     Stores    = 0U;
		std::vector<bool>                               LoadStopObserved;
		std::vector<bool>                               StoreStopObserved;
	};

	class fake_theme_documents final : public application::theme_document_port
	{
	public:
		[[nodiscard]] auto read(
			const std::string_view aPath,
			const std::size_t      aMaximumBytes,
			const std::stop_token  aStop)
			-> application::theme_document_result<std::string> override
		{
			++Reads;
			Path         = aPath;
			MaximumBytes = aMaximumBytes;
			if (!Available || aStop.stop_requested()) {
				return std::unexpected{ application::theme_document_error{
					.Failure = aStop.stop_requested()
						? application::theme_document_failure::cancelled
						: application::theme_document_failure::unavailable,
				} };
			}
			return Document;
		}

		[[nodiscard]] auto write(const std::string_view aPath, const std::string_view aDocument,
								 const std::stop_token aStop) -> application::theme_document_result<void> override
		{
			++Writes;
			if (!Available || aStop.stop_requested()) {
				return std::unexpected{ application::theme_document_error{ } };
			}
			Path     = aPath;
			Document = aDocument;
			return { };
		}

		std::size_t Writes = 0U;
		std::string Document;
		std::string Path;
		std::size_t MaximumBytes = 0U;
		std::size_t Reads        = 0U;
		bool        Available    = true;
	};

	/// \brief Counts every lexical classification the host could not avoid.
	///
	/// \details BPR-0110 owns the real highlighter. This fake stands in for it and
	/// exists to be counted: the parse count is the only direct evidence that an
	/// appearance change did not re-run lexical work.
	class counting_classifier final : public application::syntax_classification_port
	{
	public:
		[[nodiscard]] auto classify(
			const application::syntax_classification_request& aRequest,
			const std::stop_token                             aStop)
			-> application::syntax_classification override
		{
			Requests.push_back(aRequest);
			StopObserved.push_back(aStop.stop_requested());
			if (Throw) {
				throw std::runtime_error{ "classification failure" };
			}
			auto classification = application::syntax_classification{
				.Context = aRequest.Context,
				.Key     = aRequest.Key,
				.Status  = Status,
				.Spans   = spans_for(aRequest.Key),
			};
			if (ForgeKey) {
				classification.Key.Definition.Version += 1'000U;
			}
			return classification;
		}

		application::syntax_classification_status Status
			= application::syntax_classification_status::available;
		bool                                                    Throw     = false;
		bool                                                    ForgeKey  = false;
		std::size_t                                             SpanCount = 3U;
		std::vector<application::syntax_classification_request> Requests;
		std::vector<bool>                                       StopObserved;

	private:
		// The spans encode their key so a retained classification can be told
		// apart from a freshly produced one.
		[[nodiscard]] auto spans_for(
			const application::token_span_cache_key& aKey) const
			-> application::token_span_list
		{
			auto spans = application::token_span_list{ };
			for (std::size_t index = 0U; index < SpanCount; ++index) {
				spans.push_back(application::token_span{
					.Line   = static_cast<std::uint32_t>(index),
					.Column = aKey.Definition.Version,
					.Length = static_cast<std::uint32_t>(aKey.Definition.Name.size()),
					.Role   = core::appearance_role::syntax_keyword,
				});
			}
			return spans;
		}
	};

	class recording_projections final : public application::appearance_projection_port
	{
	public:
		void publish(const application::appearance_projection& aProjection) override
		{
			Published.push_back(aProjection);
		}

		void fail(const application::appearance_failure_publication& aFailure) override
		{
			Failed.push_back(aFailure);
		}

		[[nodiscard]] auto presentations() const noexcept -> std::size_t
		{
			auto count = std::size_t{ 0U };
			for (const auto& projection : Published) {
				if (projection.Presentation.has_value()) {
					++count;
				}
			}
			return count;
		}

		std::vector<application::appearance_projection>          Published;
		std::vector<application::appearance_failure_publication> Failed;
	};

	struct harness
	{
		explicit harness(const application::appearance_host_limits aLimits = { })
			: Host{ base_settings(),
					builtin_palettes(),
					Settings,
					ThemeDocuments,
					Classifier,
					Effects,
					Lane,
					Projections,
					core::application_session_id{ 11U },
					aLimits }
		{
		}

		void settle()
		{
			while (Effects.pending() != 0U || Lane.pending() != 0U) {
				Effects.drain();
				Lane.drain();
			}
		}

		fake_settings                            Settings;
		fake_theme_documents                     ThemeDocuments;
		counting_classifier                      Classifier;
		recording_projections                    Projections;
		application::manual_executor             Effects;
		application::manual_executor             Lane;
		application::appearance_application_host Host;
	};

	void test_invalid_limits_and_base_appearance_are_rejected()
	{
		auto settings       = fake_settings{ };
		auto themeDocuments = fake_theme_documents{ };
		auto classifier     = counting_classifier{ };
		auto projections    = recording_projections{ };
		auto effects        = application::manual_executor{ };
		auto lane           = application::manual_executor{ };

		auto threwOnLimits = false;
		try {
			const auto host = std::make_unique<application::appearance_application_host>(
				base_settings(),
				builtin_palettes(),
				settings,
				themeDocuments,
				classifier,
				effects,
				lane,
				projections,
				core::application_session_id{ 11U },
				application::appearance_host_limits{ .TokenSpanCacheEntries = 0U });
			expect(host == nullptr, "invalid appearance host limits must be rejected");
		} catch (const std::invalid_argument&) {
			threwOnLimits = true;
		}
		expect(threwOnLimits, "invalid appearance host limits are rejected");

		auto threwOnSettings = false;
		try {
			auto illegal       = base_settings();
			illegal.Typography = illegal_typography();
			const auto host    = std::make_unique<application::appearance_application_host>(
				std::move(illegal),
				builtin_palettes(),
				settings,
				themeDocuments,
				classifier,
				effects,
				lane,
				projections,
				core::application_session_id{ 11U });
			expect(host == nullptr, "an illegal base appearance must be rejected");
		} catch (const std::invalid_argument&) {
			threwOnSettings = true;
		}
		expect(threwOnSettings, "a base appearance the core rejects is refused");

		auto threwOnBuiltins = false;
		try {
			auto holed = builtin_palettes();
			holed[application::variant_index(core::appearance_variant::dark)]
				 [core::index_of(core::appearance_role::syntax_namespace)]
				= core::unassigned_appearance_color;
			const auto host = std::make_unique<application::appearance_application_host>(
				base_settings(),
				holed,
				settings,
				themeDocuments,
				classifier,
				effects,
				lane,
				projections,
				core::application_session_id{ 11U });
			expect(host == nullptr, "a built-in palette with an unassigned role must be rejected");
		} catch (const std::invalid_argument&) {
			threwOnBuiltins = true;
		}
		expect(threwOnBuiltins, "a built-in palette that cannot fill an older theme is refused");
	}

	void test_absent_durable_record_keeps_the_composed_appearance()
	{
		auto harnessed = harness{ };
		harnessed.Host.restore();
		harnessed.settle();

		expect(harnessed.Settings.Loads == 1U, "restore reads the durable record");
		expect(harnessed.Host.settings() == base_settings(),
			   "an absent record keeps the composed appearance");
		expect(harnessed.Host.settings().Mode == core::appearance_mode::system,
			   "a fresh installation resolves the system mode");
		expect(harnessed.Projections.Published.empty(),
			   "an absent record changes no presentation domain");
		expect(harnessed.Projections.Failed.empty(),
			   "an absent record is not a failure");
		expect(harnessed.Settings.Stores == 0U,
			   "restoring never writes the record back");
	}

	void test_persisted_appearance_is_restored()
	{
		auto harnessed            = harness{ };
		harnessed.Settings.Stored = persisted_settings();
		harnessed.Host.restore();
		harnessed.settle();

		expect(harnessed.Host.settings() == persisted_settings(),
			   "a legal durable record is adopted whole");
		expect(harnessed.Projections.Published.size() == 1U,
			   "an adopted record publishes once");

		const auto& projection = harnessed.Projections.Published.front();
		expect(projection.Context.Session == core::application_session_id{ 11U },
			   "the projection carries the host session");
		expect(projection.Presentation.has_value(),
			   "the projection carries a presentation invalidation");
		expect(!projection.Spans.has_value(),
			   "an appearance change publishes no token spans");
		expect(!projection.PaletteAdoption.has_value(),
			   "a durable restore is not reported as a palette commit acknowledgement");

		const auto& invalidation = *projection.Presentation;
		expect(invalidation.Epoch == application::appearance_epoch{ 1U },
			   "the first published change advances the epoch once");
		expect(invalidation.Resolution.has_value() && invalidation.Palettes.has_value()
				   && invalidation.Typography.has_value(),
			   "the first publication carries every changed domain");
		expect(invalidation.Resolution->Variant == core::appearance_variant::dark,
			   "the restored mode resolves its variant");
		expect(harnessed.Settings.Stores == 0U,
			   "restoring never writes the record back");
	}

	void test_a_new_settings_commit_supersedes_a_pending_startup_restore()
	{
		auto harnessed            = harness{ };
		harnessed.Settings.Stored = persisted_settings();
		auto palette              = contrast_safe_palette();
		palette[core::index_of(core::appearance_role::diff_group_boundary_rule)]
			.Alpha = 254U;

		harnessed.Host.restore();
		// Capture the old record on the effect lane, but deliberately hold its
		// terminal off the decision lane until after the newer gesture.
		harnessed.Effects.drain();
		harnessed.Host.apply(palette, core::appearance_variant::light);
		harnessed.settle();

		expect(harnessed.Settings.Loads == 1U
				   && harnessed.Settings.LoadStopObserved == std::vector{ false },
			   "the startup read captured the old record before the newer commit");
		expect(harnessed.Settings.Stores == 1U
				   && harnessed.Host.persisted_settings() == 1U,
			   "the newer palette is durably written exactly once");
		expect(harnessed.Settings.Stored.has_value()
				   && application::customisation_of(
						  *harnessed.Settings.Stored, core::appearance_variant::light)
						   .Palette
					   == palette
				   && harnessed.Host.settings() == *harnessed.Settings.Stored,
			   "the session and durable record both retain the newer palette");
		expect(harnessed.Projections.Published.size() == 1U
				   && harnessed.Projections.Published.front()
						  .PaletteAdoption.has_value()
				   && harnessed.Projections.Published.front()
						   .PaletteAdoption->Palette
					   == palette,
			   "only the newer commit reaches presentation with its exact acknowledgement");
		expect(harnessed.Host.rejected_terminals() == 1U
				   && harnessed.Host.last_rejection()
					   == application::appearance_terminal_rejection::superseded,
			   "the stale restore terminal is refused as superseded");
	}

	void test_rejected_persisted_appearance_is_not_adopted()
	{
		auto harnessed            = harness{ };
		auto stored               = persisted_settings();
		stored.Typography         = illegal_typography();
		harnessed.Settings.Stored = stored;
		harnessed.Host.restore();
		harnessed.settle();

		expect(harnessed.Host.settings() == base_settings(),
			   "a record the core rejects is refused whole");
		expect(harnessed.Host.settings().Typography.Ui.Font.PointSizeCentipoints == 1200U,
			   "the refused value is never clamped into range");
		expect(harnessed.Host.settings().Mode == core::appearance_mode::system,
			   "no domain of a refused record is adopted");
		expect(harnessed.Projections.Published.empty(),
			   "a refused record publishes no presentation change");
		expect(harnessed.Host.epoch() == application::appearance_epoch{ },
			   "a refused record advances no epoch");
		expect(harnessed.Settings.Stores == 0U,
			   "a refused record is not overwritten with a correction");
		expect(harnessed.Projections.Failed.size() == 1U,
			   "a refused record reports once");

		const auto& failure = harnessed.Projections.Failed.front();
		expect(failure.Failure == application::appearance_host_failure::settings_rejected,
			   "the refusal names the rejection");
		expect_exact_rejections(failure.Rejections,
								"the refusal carries the core's exact typed reasons");
		expect_exact_rejections(
			std::vector<core::typography_rejection>{
				harnessed.Host.last_rejections().begin(),
				harnessed.Host.last_rejections().end() },
			"the host retains the core's exact typed reasons");
	}

	void test_unknown_persisted_mode_is_refused()
	{
		auto harnessed            = harness{ };
		auto stored               = persisted_settings();
		stored.Mode               = static_cast<core::appearance_mode>(200U);
		harnessed.Settings.Stored = stored;
		harnessed.Host.restore();
		harnessed.settle();

		expect(harnessed.Host.settings() == base_settings(),
			   "a record outside the mode vocabulary is not adopted");
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.front().Failure
					   == application::appearance_host_failure::unknown_appearance_mode,
			   "an unknown persisted mode is named exactly");
		expect(harnessed.Projections.Published.empty(),
			   "an unknown persisted mode publishes nothing");
	}

	void test_settings_failures_are_reported()
	{
		auto harnessed               = harness{ };
		harnessed.Settings.Available = false;
		harnessed.Host.restore();
		harnessed.settle();

		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.front().Failure
					   == application::appearance_host_failure::settings_unavailable,
			   "an unavailable durable path is reported");
		expect(harnessed.Projections.Failed.front().Rejections.empty(),
			   "an unavailable path carries no typed rejection");
		expect(harnessed.Host.settings() == base_settings(),
			   "a failed restore changes no setting");

		harnessed.Host.select(core::appearance_mode::dark);
		harnessed.settle();
		expect(harnessed.Host.settings().Mode == core::appearance_mode::dark,
			   "an adopted change is local even when persistence fails");
		expect(harnessed.Projections.Failed.size() == 2U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::settings_unavailable,
			   "a failed write is reported");
		expect(harnessed.Host.persisted_settings() == 0U,
			   "a failed write is not counted as persisted");
	}

	void test_mode_selection_persists_and_publishes_bounded_changes()
	{
		auto harnessed = harness{ };
		harnessed.Host.select(core::appearance_mode::dark);
		harnessed.settle();

		expect(harnessed.Settings.Stores == 1U, "a mode change persists");
		expect(harnessed.Settings.Stored.has_value()
				   && harnessed.Settings.Stored->Mode == core::appearance_mode::dark,
			   "the durable record carries the selected mode");
		expect(harnessed.Host.persisted_settings() == 1U,
			   "the host counts the completed write");
		expect(harnessed.Projections.Published.size() == 1U,
			   "a mode change publishes once");

		harnessed.Host.select(core::appearance_mode::light);
		harnessed.settle();
		expect(harnessed.Projections.Published.size() == 2U,
			   "a second mode change publishes once");

		const auto& invalidation = *harnessed.Projections.Published.back().Presentation;
		expect(invalidation.Resolution.has_value() && !invalidation.Palettes.has_value()
				   && !invalidation.Typography.has_value(),
			   "a bounded mode change invalidates the resolved appearance only");
		expect(invalidation.Epoch == application::appearance_epoch{ 2U },
			   "each published change advances the epoch once");

		harnessed.Host.select(core::appearance_mode::light);
		harnessed.settle();
		expect(harnessed.Projections.Published.size() == 2U,
			   "an unchanged mode publishes nothing");
		expect(harnessed.Settings.Stores == 2U, "an unchanged mode persists nothing");

		harnessed.Host.select(static_cast<core::appearance_mode>(200U));
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.front().Failure
					   == application::appearance_host_failure::unknown_appearance_mode,
			   "a mode outside the vocabulary is refused");
		expect(harnessed.Host.settings().Mode == core::appearance_mode::light,
			   "a refused mode changes nothing");
	}

	void test_illegal_typography_is_refused_without_clamping()
	{
		auto harnessed = harness{ };
		harnessed.Host.apply(illegal_typography());
		harnessed.settle();

		expect(harnessed.Host.settings() == base_settings(),
			   "illegal typography is refused whole");
		expect(harnessed.Host.settings().Typography.Code.TabWidth == 4U,
			   "the refused tab width is never clamped into range");
		expect(harnessed.Settings.Stores == 0U, "refused typography does not persist");
		expect(harnessed.Projections.Published.empty(),
			   "refused typography publishes no presentation change");
		expect(harnessed.Projections.Failed.size() == 1U,
			   "refused typography reports once");
		expect(harnessed.Projections.Failed.front().Failure
				   == application::appearance_host_failure::settings_rejected,
			   "the refusal names the rejection");
		expect_exact_rejections(harnessed.Projections.Failed.front().Rejections,
								"the refusal carries the core's exact typed reasons");

		auto legal                         = base_settings().Typography;
		legal.Ui.Font.PointSizeCentipoints = 1400U;
		harnessed.Host.apply(legal);
		harnessed.settle();
		expect(harnessed.Host.settings().Typography == legal,
			   "legal typography is adopted");
		expect(harnessed.Host.last_rejections().empty(),
			   "an accepted value clears the retained rejections");
		expect(harnessed.Settings.Stores == 1U, "adopted typography persists");
	}

	/// \brief [[BPR-1336]]. A zoom gesture steps from the record the host holds,
	/// persists like any typography, leaves Code size alone, stops at the top,
	/// resets with the code font and survives a restart.
	void test_code_zoom_steps_from_the_accepted_record()
	{
		auto       h    = harness{ };
		const auto size = h.Host.settings().Typography.Code.Font.PointSizeCentipoints;
		h.Host.step_code_zoom(core::code_zoom_step::in);
		h.Host.step_code_zoom(core::code_zoom_step::in);
		h.settle();
		expect(h.Host.settings().Typography.Code.ZoomPercent == 125U,
			   "two gestures are two steps: 100% to 110% to 125%");
		expect(h.Host.settings().Typography.Code.Font.PointSizeCentipoints == size,
			   "zooming never rewrites Code size");
		expect(h.Settings.Stored.has_value() && h.Settings.Stored->Typography.Code.ZoomPercent == 125U,
			   "the zoom persists with the code font");
		const auto& invalidation = *h.Projections.Published.back().Presentation;
		expect(invalidation.Typography.has_value() && invalidation.Typography->Code.ZoomPercent == 125U,
			   "and is published as a typography change");
		h.Host.step_code_zoom(core::code_zoom_step::out);
		h.settle();
		expect(h.Host.settings().Typography.Code.ZoomPercent == 110U, "Zoom Out steps back down");
		h.Host.step_code_zoom(core::code_zoom_step::actual_size);
		h.settle();
		expect(h.Host.settings().Typography.Code.ZoomPercent == 100U, "Actual Size returns to 100%");
		for (auto step = 0; step < 20; ++step) {
			h.Host.step_code_zoom(core::code_zoom_step::in);
		}
		h.settle();
		const auto stores = h.Settings.Stores;
		h.Host.step_code_zoom(core::code_zoom_step::in);
		h.settle();
		expect(h.Host.settings().Typography.Code.ZoomPercent == 300U && h.Settings.Stores == stores,
			   "at the top a further step changes and stores nothing");
		auto restarted            = harness{ };
		restarted.Settings.Stored = h.Settings.Stored;
		restarted.Host.restore();
		restarted.settle();
		expect(restarted.Host.settings().Typography.Code.ZoomPercent == 300U, "the zoom survives a new host lifetime");
		h.Host.reset(application::appearance_reset_scope::code_font, core::appearance_variant::light);
		h.settle();
		expect(h.Host.settings().Typography.Code.ZoomPercent == 100U, "resetting the code font resets the zoom");
	}

	void test_palette_change_publishes_only_the_palette()
	{
		auto harnessed = harness{ };
		harnessed.Host.select(core::appearance_mode::dark);
		harnessed.settle();

		auto palette = contrast_safe_palette();
		palette[core::index_of(core::appearance_role::diff_group_boundary_rule)]
			.Alpha = 254U;
		harnessed.Host.apply(palette, core::appearance_variant::dark);
		harnessed.settle();

		expect(harnessed.Projections.Published.size() == 2U,
			   "a palette change publishes once");
		const auto& projection   = harnessed.Projections.Published.back();
		const auto& invalidation = *projection.Presentation;
		expect(invalidation.Palettes.has_value() && !invalidation.Resolution.has_value()
				   && !invalidation.Typography.has_value(),
			   "a bounded palette change invalidates the palette only");
		expect(application::customisation_of(
				   *invalidation.Palettes, core::appearance_variant::dark)
				   == application::appearance_palette_customisation{ .Palette = palette,
																	 .Stored  = true },
			   "the invalidation carries the adopted palette under its own variant");
		expect(projection.PaletteAdoption.has_value()
				   && projection.PaletteAdoption->Palette == palette
				   && projection.PaletteAdoption->Variant
					   == core::appearance_variant::dark,
			   "the same projection acknowledges the accepted preview commit and "
			   "names the theme it changed");
		expect(harnessed.Settings.Stored.has_value()
				   && application::customisation_of(
						  *harnessed.Settings.Stored, core::appearance_variant::dark)
						   .Palette
					   == palette,
			   "the palette persists");

		const auto storesBeforeEqual = harnessed.Settings.Stores;
		harnessed.Host.apply(palette, core::appearance_variant::dark);
		harnessed.settle();
		expect(harnessed.Settings.Stores == storesBeforeEqual,
			   "committing the already-stored palette does not rewrite it");
		expect(harnessed.Projections.Published.size() == 3U,
			   "an equal commit still publishes one terminal acknowledgement");
		const auto& equalCommit = harnessed.Projections.Published.back();
		expect(!equalCommit.Presentation.has_value()
				   && equalCommit.PaletteAdoption.has_value()
				   && equalCommit.PaletteAdoption->Palette == palette,
			   "an equal commit acknowledges without inventing a pixel invalidation");
	}

	void test_an_adopted_palette_records_the_variant_it_customises()
	{
		// A palette is one total set of colours for one variant, and the variant is
		// supplied by the composition rather than derived here from an observation
		// that may already have moved. Deriving it was what pinned a light palette
		// over the dark profile.
		auto       harnessed = harness{ };
		const auto chosen    = contrast_safe_palette();
		harnessed.Host.apply(chosen, core::appearance_variant::dark);
		harnessed.settle();

		const auto& stored = harnessed.Host.settings();
		expect(application::customisation_of(stored, core::appearance_variant::dark)
				   == application::appearance_palette_customisation{ .Palette = chosen,
																	 .Stored  = true },
			   "an adopted palette becomes exactly the named variant's customization");
		for (const auto untouched : { core::appearance_variant::light,
									  core::appearance_variant::high_contrast_light,
									  core::appearance_variant::high_contrast_dark }) {
			expect(application::customisation_of(stored, untouched)
					   == application::appearance_palette_customisation{ },
				   "and no other theme is given a customization it never had");
		}
		expect(harnessed.Settings.Stored.has_value()
				   && *harnessed.Settings.Stored == stored,
			   "the durable record carries the whole per-variant table");

		// The behaviour change RFC-BPR-0034 exists for: committing one theme must
		// leave the others exactly as they were. Under the single-palette record
		// this second commit replaced the first.
		auto  light = chosen;
		auto& stale = light[core::index_of(core::appearance_role::state_stale)];
		// A one-channel nudge rather than a fresh colour. The host validates every
		// committed palette against the required contrast pairs of all four
		// variants, and this fixture is about which theme the colours land under,
		// not about contrast: a colour chosen for contrast reasons would make the
		// commit refused and the assertion below vacuous.
		stale.Blue = static_cast<std::uint8_t>(stale.Blue == 0U ? 1U : stale.Blue - 1U);
		harnessed.Host.apply(light, core::appearance_variant::light);
		harnessed.settle();
		expect(application::customisation_of(
				   harnessed.Host.settings(), core::appearance_variant::dark)
					   .Palette
				   == chosen,
			   "committing Light leaves the Dark customization intact");
		expect(application::customisation_of(
				   harnessed.Host.settings(), core::appearance_variant::light)
					   .Palette
				   == light,
			   "and Light now carries its own colours");

		// The restart half of the claim: a fresh host lifetime reading the record
		// this one wrote finds both themes still customised, and differently.
		auto restarted            = harness{ };
		restarted.Settings.Stored = harnessed.Settings.Stored;
		restarted.Host.restore();
		restarted.settle();
		expect(restarted.Host.settings() == harnessed.Host.settings(),
			   "both variants' customizations survive a reopen, compared whole");
		expect(application::customisation_of(
				   restarted.Host.settings(), core::appearance_variant::dark)
					   .Palette
				   != application::customisation_of(
						  restarted.Host.settings(), core::appearance_variant::light)
						  .Palette,
			   "and they are still two different themes rather than one");
	}

	void test_a_palette_reset_names_one_variant_and_spares_the_others()
	{
		auto       h      = harness{ };
		const auto chosen = contrast_safe_palette();
		h.Host.apply(chosen, core::appearance_variant::dark);
		h.Host.apply(chosen, core::appearance_variant::light);
		h.settle();

		h.Host.reset(
			application::appearance_reset_scope::palette,
			core::appearance_variant::dark);
		h.settle();
		expect(application::customisation_of(
				   h.Host.settings(), core::appearance_variant::dark)
				   == application::appearance_palette_customisation{ },
			   "a palette reset clears exactly the theme it names");
		expect(application::customisation_of(
				   h.Host.settings(), core::appearance_variant::light)
					   .Palette
				   == chosen,
			   "and leaves every other theme's colours where they were");
		expect(h.Projections.Published.back().PaletteAdoption.has_value()
				   && h.Projections.Published.back().PaletteAdoption->Variant
					   == core::appearance_variant::dark,
			   "the reset acknowledgement names the theme it acted on");
		expect(h.Settings.Stored.has_value() && *h.Settings.Stored == h.Host.settings(),
			   "the reset persists the absence of one theme's overrides, not all");
	}

	void test_palette_commit_refuses_contrast_failures_without_writing()
	{
		auto harnessed = harness{ };
		auto palette   = core::appearance_palette{ };
		palette.fill(core::appearance_color{ .Alpha = 255U });
		harnessed.Host.apply(palette, core::appearance_variant::light);
		harnessed.settle();

		expect(harnessed.Host.settings() == base_settings(),
			   "a contrast-invalid palette changes no adopted setting");
		expect(harnessed.Settings.Stores == 0U,
			   "a contrast-invalid palette performs no store write");
		expect(harnessed.Projections.Published.empty(),
			   "a contrast-invalid palette publishes no presentation change");
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.front().Failure
					   == application::appearance_host_failure::palette_contrast_rejected,
			   "a contrast-invalid palette is refused with the typed failure");
		expect(!harnessed.Projections.Failed.front().ContrastRejections.empty(),
			   "the refusal preserves the core contrast findings and variants");
		for (const auto variant : {
				 core::appearance_variant::light,
				 core::appearance_variant::dark,
				 core::appearance_variant::high_contrast_light,
				 core::appearance_variant::high_contrast_dark,
			 }) {
			expect(
				std::ranges::any_of(
					harnessed.Projections.Failed.front().ContrastRejections,
					[variant](const auto& aRejection) {
						return aRejection.Variant == variant;
					}),
				"one invalid total palette is checked against every variant");
		}
	}

	/// \brief Returns a document without the two roles RFC-BPR-0072 appended.
	[[nodiscard]] auto without_identifier_roles(std::string aDocument) -> std::string
	{
		for (const auto role : { core::appearance_role::syntax_namespace, core::appearance_role::syntax_enum_member }) {
			const auto begin = aDocument.find(std::string{ core::stable_name(role) } + ' ');
			expect(begin != std::string::npos, "a theme export includes the identifier roles");
			aDocument.erase(begin, aDocument.find('\n', begin) + 1U - begin);
		}
		return aDocument;
	}

	void test_older_theme_takes_omitted_roles_from_its_base()
	{
		// [[BPR-1479]]. The reported case: a theme written before the namespace
		// and enum-member roles, on a black code canvas, imported while Light
		// is on screen. Filled from Light, its namespace ink measured 2.88:1
		// and Keep changes was refused.
		auto        h        = harness{ };
		const auto  builtins = builtin_palettes();
		const auto& dark     = builtins[application::variant_index(core::appearance_variant::dark)];
		auto        expected = contrast_safe_palette();
		for (const auto role : { core::appearance_role::syntax_namespace, core::appearance_role::syntax_enum_member }) {
			expected[core::index_of(role)] = dark[core::index_of(role)];
		}
		h.ThemeDocuments.Document = without_identifier_roles(core::export_theme(contrast_safe_palette()));
		h.Host.import_theme_file("older-dark.theme");
		h.settle();
		expect(h.Projections.Failed.empty() && h.Projections.Published.size() == 1U
				   && h.Projections.Published.back().ThemePreview && h.Projections.Published.back().ThemePreview->Palette == expected,
			   "a version-1 theme on a dark canvas previews the omitted roles in Dark's built-in colours");
		expect(h.Settings.Stores == 0U && h.Host.settings() == base_settings(), "the import is preview-only");

		h.Host.apply(h.Projections.Published.back().ThemePreview->Palette, core::appearance_variant::light);
		h.settle();
		expect(h.Projections.Failed.empty() && h.Settings.Stores == 1U
				   && application::customisation_of(h.Host.settings(), core::appearance_variant::light).Palette == expected,
			   "keeping it on Light stores the filled colours, which a later export writes as they are");

		h.Projections.Published.clear();
		h.ThemeDocuments.Document = without_identifier_roles(core::export_theme(contrast_safe_palette(), core::appearance_variant::light));
		h.Host.import_theme_file("declared-light.theme");
		h.settle();
		expect(h.Projections.Published.empty() && h.Projections.Failed.size() == 1U
				   && h.Projections.Failed.back().Failure == application::appearance_host_failure::palette_contrast_rejected
				   && std::ranges::any_of(h.Projections.Failed.back().ContrastRejections, [](const auto& aRejection) { return aRejection.Finding.Pair.Foreground == core::appearance_role::syntax_namespace; }),
			   "a declared base is honoured over the canvas, and a fill that cannot be read is refused at import rather than at Keep changes");

		auto palette                                                     = contrast_safe_palette();
		palette[core::index_of(core::appearance_role::syntax_namespace)] = core::appearance_color{ .Alpha = 255U };
		h.ThemeDocuments.Document                                        = core::export_theme(palette, core::appearance_variant::dark);
		h.Host.import_theme_file("explicit-unreadable-namespace.theme");
		h.settle();
		expect(h.Projections.Published.empty() && h.Projections.Failed.size() == 2U
				   && h.Projections.Failed.back().Failure == application::appearance_host_failure::palette_contrast_rejected
				   && std::ranges::any_of(h.Projections.Failed.back().ContrastRejections, [](const auto& aRejection) { return aRejection.Finding.Pair.Foreground == core::appearance_role::syntax_namespace; }),
			   "explicit unreadable identifier ink is rejected; the base fills only what the document omits");
		expect(h.Settings.Stores == 1U, "a refused import performs no durable write");
	}

	void test_theme_import_is_bounded_typed_and_preview_only()
	{
		auto       harnessed              = harness{ };
		const auto palette                = contrast_safe_palette();
		harnessed.ThemeDocuments.Document = core::export_theme(palette);
		harnessed.Host.import_theme_file("fixture-safe.theme");
		harnessed.settle();

		expect(harnessed.ThemeDocuments.Reads == 1U
				   && harnessed.ThemeDocuments.Path == "fixture-safe.theme",
			   "theme import reads the selected path through the effect port once");
		expect(harnessed.ThemeDocuments.MaximumBytes
				   == core::theme_document_byte_limit + 1U,
			   "theme import reads only the core limit plus its overflow byte");
		expect(harnessed.Projections.Published.size() == 1U
				   && harnessed.Projections.Published.front().ThemePreview.has_value()
				   && harnessed.Projections.Published.front().ThemePreview->Palette
					   == palette,
			   "a parsed and contrast-safe import publishes the total preview palette");
		expect(harnessed.Host.settings() == base_settings()
				   && harnessed.Settings.Stores == 0U,
			   "import preview changes and persists no adopted setting");

		harnessed.Projections.Published.clear();
		harnessed.ThemeDocuments.Document = "not a theme document";
		harnessed.Host.import_theme_file("fixture-malformed.theme");
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::theme_import_rejected
				   && !harnessed.Projections.Failed.back().ThemeRejections.empty(),
			   "a malformed import preserves the core parser's typed rejection");

		harnessed.ThemeDocuments.Document = core::export_theme(
			core::appearance_palette{ });
		harnessed.Host.import_theme_file("fixture-low-contrast.theme");
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 2U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::palette_contrast_rejected
				   && !harnessed.Projections.Failed.back().ContrastRejections.empty(),
			   "a structurally valid low-contrast import is refused with typed findings");
		expect(harnessed.Settings.Stores == 0U,
			   "every refused import performs zero durable writes");
	}

	void test_system_mode_follows_the_observation_and_explicit_mode_does_not()
	{
		auto harnessed = harness{ };
		harnessed.Host.observe(
			core::appearance_observation{ .Scheme = core::os_color_scheme::dark });
		harnessed.settle();

		expect(harnessed.Projections.Published.size() == 1U,
			   "the system mode responds to an observed scheme");
		expect(harnessed.Projections.Published.back().Presentation->Resolution->Variant
				   == core::appearance_variant::dark,
			   "the observed scheme selects the dark variant");
		expect(harnessed.Host.resolution().Scheme
				   == core::appearance_scheme_source::observed_scheme,
			   "the resolution names the observation as its source");
		expect(harnessed.Settings.Stores == 0U,
			   "an observation is not a setting and never persists");

		harnessed.Host.select(core::appearance_mode::light);
		harnessed.settle();
		const auto published = harnessed.Projections.Published.size();

		harnessed.Host.observe(
			core::appearance_observation{ .Scheme   = core::os_color_scheme::dark,
										  .Contrast = core::os_contrast_preference::standard });
		harnessed.Host.observe(
			core::appearance_observation{ .Scheme = core::os_color_scheme::light });
		harnessed.settle();
		expect(harnessed.Projections.Published.size() == published,
			   "an explicit mode ignores every observed scheme change");
		expect(harnessed.Host.resolution().Variant == core::appearance_variant::light,
			   "an explicit mode keeps its selected variant");
	}

	void test_classification_misses_once_then_hits()
	{
		auto       harnessed = harness{ };
		const auto blobKey   = key(1U);
		harnessed.Host.classify(blobKey);
		harnessed.settle();

		expect(harnessed.Classifier.Requests.size() == 1U,
			   "an uncached blob reaches the classifier once");
		expect(harnessed.Classifier.Requests.front().MaximumSpans
				   == application::appearance_host_limits{ }.TokenSpansPerBlob,
			   "the request carries the host bound");
		expect(harnessed.Host.classification_submissions() == 1U,
			   "the host counts the classification it could not avoid");
		expect(harnessed.Host.cached_classifications() == 1U,
			   "the classification is retained");
		expect(harnessed.Projections.Published.size() == 1U,
			   "the classification publishes once");

		const auto& first = harnessed.Projections.Published.front();
		expect(first.Spans.has_value() && !first.Presentation.has_value(),
			   "a classification publishes no presentation invalidation");
		expect(!first.Spans->CacheHit, "the first classification is not a cache hit");
		expect(first.Spans->Spans != nullptr && first.Spans->Spans->size() == 3U,
			   "the publication carries the classified spans");

		harnessed.Host.classify(blobKey);
		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 1U,
			   "a cached blob never reaches the classifier again");
		expect(harnessed.Host.cache_hits() == 1U, "the host counts the cache hit");
		expect(harnessed.Projections.Published.back().Spans->CacheHit,
			   "the second classification is published as a cache hit");
	}

	/// \brief Proves an appearance change costs no lexical work.
	///
	/// \details This is RFC-BPR-0006's "switching theme or colour does not re-run
	/// lexical parsing" at the host boundary. The cache key holds blob identity and
	/// definition version only, so every appearance domain can change without the
	/// classifier being asked anything.
	void test_appearance_changes_never_reparse()
	{
		auto       harnessed = harness{ };
		const auto blobKey   = key(1U);
		harnessed.Host.classify(blobKey);
		harnessed.settle();
		const auto retained = harnessed.Host.cached(blobKey);
		expect(retained != nullptr, "the classification is retained");

		auto palette = contrast_safe_palette();
		palette[core::index_of(core::appearance_role::diff_group_boundary_rule)]
			.Alpha                                = 254U;
		auto typography                           = base_settings().Typography;
		typography.Code.Font.PointSizeCentipoints = 1600U;

		harnessed.Host.select(core::appearance_mode::high_contrast_dark);
		harnessed.Host.apply(palette, core::appearance_variant::high_contrast_dark);
		harnessed.Host.apply(typography);
		harnessed.Host.observe(
			core::appearance_observation{ .Scheme = core::os_color_scheme::dark });
		harnessed.settle();

		expect(harnessed.Projections.presentations() >= 3U,
			   "every appearance domain really changed");
		expect(harnessed.Host.cached_classifications() == 1U,
			   "an appearance change evicts nothing");
		expect(harnessed.Host.cached(blobKey) == retained,
			   "an appearance change leaves the retained classification identical");

		harnessed.Host.classify(blobKey);
		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 1U,
			   "switching theme, palette, mode, and font re-runs no lexical parsing");
		expect(harnessed.Host.classification_submissions() == 1U,
			   "the host parse count does not move across an appearance change");
		expect(harnessed.Host.cache_hits() == 1U,
			   "the classification after an appearance change is a cache hit");
		expect(harnessed.Projections.Published.back().Spans->Spans == retained,
			   "the published spans are the same retained classification");
	}

	void test_cache_key_is_blob_and_definition_only()
	{
		auto harnessed = harness{ };
		harnessed.Host.classify(key(1U, "C++", 1U));
		harnessed.settle();
		harnessed.Host.classify(key(1U, "C++", 2U));
		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 2U,
			   "a definition version change re-classifies the same blob");

		harnessed.Host.classify(key(2U, "C++", 1U));
		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 3U,
			   "a different blob is classified separately");

		harnessed.Host.classify(key(1U, "CMake", 1U));
		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 4U,
			   "a different definition is classified separately");
		expect(harnessed.Host.cached_classifications() == 4U,
			   "each distinct key retains its own classification");
		expect(harnessed.Host.cache_hits() == 0U, "no distinct key answered from another");
	}

	void test_cache_evicts_the_least_recently_used_entry()
	{
		auto harnessed = harness{ application::appearance_host_limits{
			.TokenSpanCacheEntries = 2U } };
		harnessed.Host.classify(key(1U));
		harnessed.settle();
		harnessed.Host.classify(key(2U));
		harnessed.settle();
		harnessed.Host.classify(key(1U));
		harnessed.settle();
		harnessed.Host.classify(key(3U));
		harnessed.settle();

		expect(harnessed.Host.cache_evictions() == 1U, "the bound evicts exactly once");
		expect(harnessed.Host.cached_classifications() == 2U,
			   "the cache stays inside its bound");
		expect(harnessed.Host.cached(key(2U)) == nullptr,
			   "the least recently used entry is the victim");
		expect(harnessed.Host.cached(key(1U)) != nullptr,
			   "a reused entry survives eviction");

		harnessed.Host.classify(key(2U));
		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 4U,
			   "an evicted blob is classified again");
	}

	void test_classification_failures_are_reported()
	{
		auto harnessed = harness{ };
		harnessed.Host.classify(application::token_span_cache_key{ });
		harnessed.settle();
		expect(harnessed.Classifier.Requests.empty(),
			   "an invalid key never reaches the classifier");
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::invalid_classification_key,
			   "an invalid blob identity is refused");

		harnessed.Host.classify(key(1U, ""));
		harnessed.settle();
		expect(harnessed.Classifier.Requests.empty(),
			   "a key with no definition never reaches the classifier");
		expect(harnessed.Projections.Failed.size() == 2U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::invalid_classification_key,
			   "a key with no definition is refused");

		harnessed.Classifier.Status
			= application::syntax_classification_status::limit_exceeded;
		harnessed.Host.classify(key(1U));
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 3U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::classification_limit,
			   "a classification beyond the bound is refused, never truncated");
		expect(harnessed.Host.cached_classifications() == 0U,
			   "a refused classification is not retained");

		harnessed.Classifier.Status = application::syntax_classification_status::available;
		harnessed.Classifier.Throw  = true;
		harnessed.Host.classify(key(2U));
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 4U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::classification_unavailable,
			   "a throwing classifier is contained and reported");

		harnessed.Classifier.Throw    = false;
		harnessed.Classifier.ForgeKey = true;
		harnessed.Host.classify(key(3U));
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 5U
				   && harnessed.Projections.Failed.back().Failure
					   == application::appearance_host_failure::host_failure,
			   "a terminal naming another key cannot be adopted");
		expect(harnessed.Host.cached_classifications() == 0U,
			   "an inexact terminal retains nothing");
	}

	void test_duplicate_in_flight_classification_is_coalesced()
	{
		auto harnessed = harness{ };
		harnessed.Host.classify(key(1U));
		harnessed.Host.classify(key(1U));
		expect(harnessed.Host.outstanding_requests() == 1U,
			   "a duplicate request reserves nothing");
		expect(harnessed.Host.coalesced_classifications() == 1U,
			   "the duplicate is coalesced onto the in-flight request");

		harnessed.settle();
		expect(harnessed.Classifier.Requests.size() == 1U,
			   "an in-flight blob is classified once");
		expect(harnessed.Projections.Published.size() == 1U,
			   "the coalesced request is answered by one publication");
	}

	void test_cancellation_refuses_the_terminal_and_keeps_the_cache()
	{
		auto harnessed = harness{ };
		harnessed.Host.classify(key(1U));
		harnessed.settle();
		expect(harnessed.Host.cached_classifications() == 1U,
			   "the first classification is retained");

		harnessed.Host.classify(key(2U));
		harnessed.Effects.drain();
		harnessed.Host.cancel();
		expect(harnessed.Classifier.StopObserved.size() == 2U
				   && !harnessed.Classifier.StopObserved.back(),
			   "the classifier saw a live stop token before cancellation");
		harnessed.Lane.drain();

		expect(harnessed.Host.rejected_terminals() == 1U,
			   "the cancelled terminal is refused");
		expect(harnessed.Host.last_rejection()
				   == application::appearance_terminal_rejection::cancelled,
			   "the refusal names cancellation");
		expect(harnessed.Host.cached_classifications() == 1U,
			   "cancelling one classification retains the others");
		expect(harnessed.Host.cached(key(1U)) != nullptr,
			   "cancellation evicts nothing");
		expect(harnessed.Projections.Published.size() == 1U,
			   "a cancelled classification publishes nothing");
	}

	void test_export_round_trip_refusal_and_cancellation()
	{
		auto       h       = harness{ };
		const auto palette = contrast_safe_palette();
		h.Host.export_theme_file("chosen.json", palette, core::appearance_variant::high_contrast_dark);
		expect(h.ThemeDocuments.Writes == 0U, "export performs no synchronous file effect");
		h.settle();
		const auto exported = core::import_theme(h.ThemeDocuments.Document);
		expect(h.ThemeDocuments.Path == "chosen.json" && exported.Palette == palette
				   && exported.Base == core::appearance_variant::high_contrast_dark,
			   "exported complete palette round-trips through the core parser with the theme it customises as its base");
		expect(h.Projections.Published.back().ThemeExported, "success is acknowledged after write completion");
		expect(h.Settings.Stores == 0U && h.Host.settings() == base_settings(), "export changes no durable settings");
		h.ThemeDocuments.Available = false;
		h.Host.export_theme_file("refused.json", palette, core::appearance_variant::dark);
		h.settle();
		expect(h.Projections.Failed.back().Failure == application::appearance_host_failure::theme_export_unavailable,
			   "writer refusal produces an export-specific failure");
		const auto publications = h.Projections.Published.size();
		const auto failures     = h.Projections.Failed.size();
		h.Host.export_theme_file("cancelled.json", palette, core::appearance_variant::dark);
		h.Host.cancel();
		h.settle();
		expect(h.ThemeDocuments.Writes == 2U && h.Projections.Published.size() == publications
				   && h.Projections.Failed.size() == failures,
			   "cancel before effect prevents a write and suppresses its terminal");
	}

	void test_resets_are_scoped_durable_and_retire_import()
	{
		auto       h                = harness{ };
		const auto defaults         = base_settings();
		auto       typography       = defaults.Typography;
		typography.Ui.Font.Family   = "Chosen UI";
		typography.Code.Font.Family = "Chosen Code";
		h.Host.apply(typography);
		h.Host.select(core::appearance_mode::dark);
		h.Host.apply(contrast_safe_palette(), core::appearance_variant::dark);
		h.settle();
		const auto customised = [&h] {
			return application::customisation_of(
					   h.Host.settings(), core::appearance_variant::dark)
				.Stored;
		};
		h.Host.reset(
			application::appearance_reset_scope::interface_font,
			core::appearance_variant::dark);
		h.settle();
		expect(h.Host.settings().Typography.Ui == defaults.Typography.Ui
				   && h.Host.settings().Typography.Code == typography.Code
				   && h.Host.settings().Mode == core::appearance_mode::dark
				   && customised(),
			   "interface reset preserves code, mode and custom palette");
		h.Host.reset(
			application::appearance_reset_scope::code_font,
			core::appearance_variant::dark);
		h.Host.reset(
			application::appearance_reset_scope::mode, core::appearance_variant::dark);
		h.settle();
		expect(h.Host.settings().Typography == defaults.Typography && h.Host.settings().Mode == defaults.Mode
				   && customised(),
			   "code and mode resets preserve palette");
		h.ThemeDocuments.Document = core::export_theme(contrast_safe_palette());
		h.Host.import_theme_file("late.json");
		h.Effects.drain();
		h.Host.reset(
			application::appearance_reset_scope::palette,
			core::appearance_variant::dark);
		h.settle();
		expect(h.Host.settings() == defaults && h.Settings.Stored == defaults,
			   "palette reset persists the absence of overrides and exact composed defaults");
		expect(h.Host.last_rejection() == application::appearance_terminal_rejection::superseded,
			   "reset prevents late import from recreating a preview");
		expect(std::ranges::none_of(h.Projections.Published, [](const auto& aProjection) { return aProjection.ThemePreview.has_value(); }),
			   "the retired import never reaches presentation");
		auto restarted            = harness{ };
		restarted.Settings.Stored = h.Settings.Stored;
		restarted.Host.restore();
		restarted.settle();
		expect(restarted.Host.settings() == defaults, "reset survives a new host lifetime");
	}

	void test_preview_cancellation_retires_only_import()
	{
		auto h                    = harness{ };
		h.ThemeDocuments.Document = core::export_theme(contrast_safe_palette());
		h.Host.import_theme_file("late-preview.json");
		h.Effects.drain();
		h.Host.select(core::appearance_mode::dark);
		h.Host.classify(key(1U));
		h.Host.cancel_theme_import();
		h.settle();
		expect(h.Host.last_rejection() == application::appearance_terminal_rejection::cancelled,
			   "closing rejects an already staged import terminal");
		expect(std::ranges::none_of(h.Projections.Published, [](const auto& aProjection) { return aProjection.ThemePreview.has_value(); }),
			   "no late preview reaches the closed Settings surface");
		expect(h.Settings.Stored.has_value() && h.Settings.Stored->Mode == core::appearance_mode::dark
				   && h.Host.cached(key(1U)) != nullptr,
			   "accepted preference writes and unrelated syntax work survive closing Settings");
		h.Host.import_theme_file("new-preview.json");
		h.settle();
		expect(h.Projections.Published.back().ThemePreview.has_value(),
			   "a new import after reopening has a fresh accepted identity");
	}

	void test_shutdown_withholds_publication_and_quiesces()
	{
		auto harnessed = harness{ };
		harnessed.Host.classify(key(1U));
		harnessed.Effects.drain();
		harnessed.Host.shutdown();
		expect(harnessed.Host.is_closing(), "shutdown closes the host");

		harnessed.Lane.drain();
		expect(harnessed.Projections.Published.empty(),
			   "a terminal after shutdown publishes nothing");
		expect(harnessed.Projections.Failed.empty(),
			   "a terminal after shutdown reports no failure");
		expect(harnessed.Host.is_quiescent(), "the host reaches quiescence");
	}
} // namespace

auto main() -> int
{
	test_invalid_limits_and_base_appearance_are_rejected();
	test_absent_durable_record_keeps_the_composed_appearance();
	test_persisted_appearance_is_restored();
	test_a_new_settings_commit_supersedes_a_pending_startup_restore();
	test_rejected_persisted_appearance_is_not_adopted();
	test_unknown_persisted_mode_is_refused();
	test_settings_failures_are_reported();
	test_mode_selection_persists_and_publishes_bounded_changes();
	test_illegal_typography_is_refused_without_clamping();
	test_code_zoom_steps_from_the_accepted_record();
	test_palette_change_publishes_only_the_palette();
	test_an_adopted_palette_records_the_variant_it_customises();
	test_a_palette_reset_names_one_variant_and_spares_the_others();
	test_palette_commit_refuses_contrast_failures_without_writing();
	test_theme_import_is_bounded_typed_and_preview_only();
	test_older_theme_takes_omitted_roles_from_its_base();
	test_system_mode_follows_the_observation_and_explicit_mode_does_not();
	test_classification_misses_once_then_hits();
	test_appearance_changes_never_reparse();
	test_cache_key_is_blob_and_definition_only();
	test_cache_evicts_the_least_recently_used_entry();
	test_classification_failures_are_reported();
	test_duplicate_in_flight_classification_is_coalesced();
	test_cancellation_refuses_the_terminal_and_keeps_the_cache();
	test_export_round_trip_refusal_and_cancellation();
	test_resets_are_scoped_durable_and_retire_import();
	test_preview_cancellation_retires_only_import();
	test_shutdown_withholds_publication_and_quiesces();
	std::cout << "appearance application host tests passed\n";
	return EXIT_SUCCESS;
}
