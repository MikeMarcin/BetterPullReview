#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <functional>
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
		// No customization, for any variant: a composed base is what a fresh
		// installation starts from, and that is the absence of a decision.
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

	/// \brief Returns built-in palettes that assign every role, as a composition's do.
	[[nodiscard]] auto builtin_palettes() -> application::appearance_builtin_palettes
	{
		auto palettes = application::appearance_builtin_palettes{ };
		palettes.fill(contrast_safe_palette());
		return palettes;
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
			if (OnPublish) {
				OnPublish(aProjection);
			}
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

		std::function<void(const application::appearance_projection&)> OnPublish;
		std::vector<application::appearance_projection>                Published;
		std::vector<application::appearance_failure_publication>       Failed;
	};

	struct profile_harness
	{
		fake_settings                                                         Settings;
		fake_theme_documents                                                  Documents;
		application::manual_executor                                          Effects;
		application::manual_executor                                          Lane;
		std::shared_ptr<application::appearance_preferences_application_host> Preferences
			= std::make_shared<application::appearance_preferences_application_host>(base_settings(), builtin_palettes(), Settings, Documents, Effects, Lane, core::application_session_id{ 20U });
		void settle()
		{
			while (Effects.pending() || Lane.pending()) {
				Effects.drain();
				Lane.drain();
			}
		}
		~profile_harness()
		{
			Preferences->shutdown();
			settle();
		}
	};
	void test_shared_preferences_independent_windows()
	{
		profile_harness                          h;
		counting_classifier                      firstClassifier, secondClassifier;
		recording_projections                    firstProjection, secondProjection;
		application::appearance_application_host first{ h.Preferences, firstClassifier, h.Effects, h.Lane, firstProjection, core::application_session_id{ 21U } };
		application::appearance_application_host second{ h.Preferences, secondClassifier, h.Effects, h.Lane, secondProjection, core::application_session_id{ 22U } };
		first.observe({ .Scheme = core::os_color_scheme::dark });
		second.observe({ .Scheme = core::os_color_scheme::light });
		expect(first.resolution().Variant == core::appearance_variant::dark && second.resolution().Variant == core::appearance_variant::light, "native observations resolve independently");
		first.classify(key(1U));
		second.classify(key(1U));
		h.settle();
		const auto firstSpans  = first.cached(key(1U));
		const auto secondSpans = second.cached(key(1U));
		expect(firstSpans && secondSpans && firstSpans != secondSpans, "same blob has distinct per-window cache ownership");
		const auto firstCount  = firstProjection.presentations();
		const auto secondCount = secondProjection.presentations();
		auto       typography  = first.settings().Typography;
		typography.Ui.Font.PointSizeCentipoints += 100U;
		first.apply(typography);
		// Ordinary window cancellation must not retire the shared durable settings effect.
		first.cancel();
		h.settle();
		expect(h.Settings.Stores == 1U && !h.Settings.StoreStopObserved.back(), "one accepted gesture writes once and window cancel cannot cancel it");
		expect(first.settings() == second.settings() && firstProjection.presentations() == firstCount + 1U
				   && secondProjection.presentations() == secondCount + 1U,
			   "both consumers receive one accepted preferences change");
		first.classify(key(1U));
		second.classify(key(1U));
		h.settle();
		expect(first.cached(key(1U)) == firstSpans && second.cached(key(1U)) == secondSpans
				   && firstClassifier.Requests.size() == 1U && secondClassifier.Requests.size() == 1U,
			   "appearance never reparses either unchanged blob");
		first.classify(key(2U));
		second.classify(key(2U));
		h.Effects.drain();
		first.cancel();
		h.Lane.drain();
		expect(!first.cached(key(2U)) && second.cached(key(2U)), "late cancelled window classification cannot suppress sibling result");
		const auto firstPublications = firstProjection.Published.size();
		first.classify(key(3U));
		h.Effects.drain();
		first.shutdown();
		h.Lane.drain();
		second.select(core::appearance_mode::dark);
		h.settle();
		expect(first.is_quiescent() && h.Preferences->attached_consumers() == 1U
				   && firstProjection.Published.size() == firstPublications,
			   "detached window receives neither late syntax nor later preferences");
		expect(second.cached(key(2U)) != nullptr && !h.Preferences->is_closing(), "sibling cache and profile lifetime survive window close");
		second.shutdown();
		h.settle();
	}
	void test_transient_results_target_only_live_origin()
	{
		profile_harness                          h;
		counting_classifier                      ca, cb;
		recording_projections                    pa, pb;
		application::appearance_application_host first{ h.Preferences, ca, h.Effects, h.Lane, pa, core::application_session_id{ 31U } };
		application::appearance_application_host second{ h.Preferences, cb, h.Effects, h.Lane, pb, core::application_session_id{ 32U } };
		h.Documents.Document = core::export_theme(contrast_safe_palette());
		const auto countB    = pb.Published.size();
		first.import_theme_file("first.json");
		h.settle();
		expect(pa.Published.back().ThemePreview.has_value() && pb.Published.size() == countB, "import preview reaches its origin only");
		first.export_theme_file("first-out.json", contrast_safe_palette(), core::appearance_variant::dark);
		h.settle();
		expect(pa.Published.back().ThemeExported && pb.Published.size() == countB, "export completion reaches its origin only");
		first.import_theme_file("retired.json");
		second.import_theme_file("live.json");
		h.Effects.drain();
		const auto countA = pa.Published.size();
		first.shutdown();
		h.Lane.drain();
		expect(pa.Published.size() == countA && pb.Published.back().ThemePreview.has_value(), "detaching preview origin fences late result without cancelling sibling import");
		second.shutdown();
		h.settle();
	}
	void test_bounded_reentrant_detachment()
	{
		profile_harness h;
		auto            first  = std::make_shared<recording_projections>();
		auto            second = std::make_shared<recording_projections>();
		const auto      a      = h.Preferences->attach(first, false);
		const auto      b      = h.Preferences->attach(second, false);
		expect(a && b, "two consumers attach");
		first->OnPublish = [&](const auto&) { h.Preferences->detach(*b); };
		h.Preferences->select(*a, core::appearance_mode::dark);
		h.settle();
		expect(first->Published.size() == 1U && second->Published.empty(), "reentrant detach fences sibling already in fan-out snapshot");
		first->OnPublish = { };
		// A detached identity cannot mutate shared durable state or be reused by a new consumer.
		h.Preferences->select(*b, core::appearance_mode::light);
		h.settle();
		expect(h.Settings.Stores == 1U, "retired attachment cannot mutate profile preferences");
		std::vector<application::appearance_consumer_id> identities{ *a };
		while (identities.size() < core::maximum_bpr_windows_per_profile) {
			const auto added = h.Preferences->attach(std::make_shared<recording_projections>(), false);
			expect(added.has_value(), "attachment limit includes exact supported window capacity");
			identities.push_back(*added);
		}
		const auto overflow = h.Preferences->attach(std::make_shared<recording_projections>(), false);
		expect(!overflow && overflow.error() == application::appearance_attachment_failure::capacity, "capacity plus one fails before registering consumer");
		h.Preferences->detach(identities.back());
		const auto replacement = h.Preferences->attach(std::make_shared<recording_projections>(), false);
		expect(replacement && *replacement != identities.back() && *replacement != *b, "attachment identities never revive detached consumers");
		h.Preferences->shutdown();
		h.settle();
		expect(h.Preferences->is_quiescent(), "shared preferences owner explicitly quiesces");
		const auto closing = h.Preferences->attach(first, false);
		expect(!closing && closing.error() == application::appearance_attachment_failure::closing, "closed owner rejects new attachment");
	}
	void test_dif_profile_preferences_and_failure_fanout()
	{
		profile_harness                          h, other;
		counting_classifier                      ca, cb, cc;
		recording_projections                    pa, pb, pc;
		application::appearance_application_host first{ h.Preferences, ca, h.Effects, h.Lane, pa, core::application_session_id{ 41U } };
		application::appearance_application_host second{ h.Preferences, cb, h.Effects, h.Lane, pb, core::application_session_id{ 42U } };
		application::appearance_application_host third{ other.Preferences, cc, other.Effects, other.Lane, pc, core::application_session_id{ 43U } };
		first.observe_motion(core::motion_observation::full);
		second.observe_motion(core::motion_observation::unsupported);
		expect(first.dif_resolution().EffectiveMotion == core::motion_mode::full && second.dif_resolution().EffectiveMotion == core::motion_mode::reduced, "System resolution belongs to the attachment");
		expect(h.Settings.Stores == 0U && h.Effects.pending() == 0U, "observations never write preferences");
		first.classify(key(7U));
		h.settle();
		const auto spans    = first.cached(key(7U));
		const auto original = first.settings();
		const auto full     = core::dif_preferences{ .Visibility = core::dif_visibility::hidden, .Motion = core::motion_mode::full };
		first.apply(full);
		h.settle();
		expect(first.settings().Dif == full && second.settings().Dif == full && third.settings().Dif == core::dif_preferences{ }, "one profile fans out without changing another profile");
		expect(second.dif_resolution().EffectiveMotion == core::motion_mode::full && h.Settings.Stored->Dif == full, "Full overrides unsupported and persists");
		const auto& change = *pa.Published.back().Presentation;
		expect(change.Dif && !change.Palettes && !change.Typography && !change.Resolution && !pa.Published.back().Spans, "Dif preference change carries only the Dif domain");
		first.classify(key(7U));
		h.settle();
		expect(first.cached(key(7U)) == spans && ca.Requests.size() == 1U, "Dif change preserves classification cache identity");
		first.reset(application::appearance_reset_scope::dif, core::appearance_variant::light);
		h.settle();
		expect(first.settings() == original && h.Settings.Stored == original, "Dif reset preserves all other appearance domains");
		first.apply(core::dif_preferences{ .Motion = static_cast<core::motion_mode>(99) });
		expect(first.settings() == original && pa.Failed.back().Failure == application::appearance_host_failure::invalid_dif_preferences, "invalid intent is refused before state or effect");
		first.restore();
		h.Effects.drain();
		first.apply(full);
		h.Lane.drain();
		h.settle();
		expect(first.settings().Dif == full && first.rejected_terminals() > 0U, "late startup restoration cannot overwrite accepted Dif gesture");
		h.Settings.Available = false;
		first.apply(core::dif_preferences{ .Motion = core::motion_mode::reduced });
		h.settle();
		expect(first.settings() == second.settings() && first.settings().Dif.Motion == core::motion_mode::reduced, "write failure retains the shared accepted session state");
		expect(pa.Failed.back().Failure == application::appearance_host_failure::settings_unavailable && pb.Failed.back().Failure == application::appearance_host_failure::settings_unavailable && pc.Failed.empty(), "persistence failure reaches every same-profile window only");
		auto       late     = std::make_shared<recording_projections>();
		const auto attached = h.Preferences->attach(late);
		expect(attached && late->Failed.size() == 1U && late->Published.back().Presentation->Dif->Preferences == first.settings().Dif, "late attachment receives session-only truth and accepted preferences");
		h.Preferences->detach(*attached);
		const auto count = late->Published.size();
		h.Preferences->observe_motion(*attached, core::motion_observation::full);
		h.Preferences->apply(*attached, full);
		h.settle();
		expect(late->Published.size() == count && first.settings().Dif.Motion == core::motion_mode::reduced, "detached observation and preference callbacks cannot act");
		first.shutdown();
		second.shutdown();
		third.shutdown();
		h.settle();
		other.settle();
		expect(first.is_quiescent() && second.is_quiescent() && third.is_quiescent(), "all preference consumers quiesce");
	}

	/// [[BPR-1714]]. The known namespaces are a profile preference and the
	/// classification's one reviewer input: a changed list keys C++ afresh,
	/// leaves every other language's spans alone, and is refused whole when any
	/// entry is not a namespace name.
	void test_known_namespaces_key_cpp_classifications()
	{
		profile_harness                          h;
		counting_classifier                      ca, cb;
		recording_projections                    pa, pb;
		application::appearance_application_host first{ h.Preferences, ca, h.Effects, h.Lane, pa, core::application_session_id{ 51U } };
		application::appearance_application_host second{ h.Preferences, cb, h.Effects, h.Lane, pb, core::application_session_id{ 52U } };
		first.classify(key(7U));
		first.classify(key(8U, "Python"));
		h.settle();
		const auto cpp    = first.cached(key(7U));
		const auto python = first.cached(key(8U, "Python"));
		expect(cpp && python && ca.Requests.size() == 2U && ca.Requests.front().Key.Namespaces == core::known_namespaces{ },
			   "the default list keys C++ under std alone");

		const auto listed    = core::known_namespaces{ .Names = { "std", "Qt", "boost::asio" } };
		const auto canonical = core::known_namespaces{ .Names = { "Qt", "boost::asio", "std" } };
		first.apply(listed);
		h.settle();
		expect(first.settings().Namespaces == listed && second.settings().Namespaces == listed && h.Settings.Stored->Namespaces == listed,
			   "an accepted list fans out to the profile's windows and persists as written");
		const auto& change = *pa.Published.back().Presentation;
		expect(change.Namespaces == listed && !change.Palettes && !change.Typography && !change.Resolution && !change.Dif,
			   "a list change carries only the namespace domain");
		expect(pb.Published.back().Presentation->Namespaces == listed, "every window of the profile is told");
		expect(first.classification_key(key(7U)).Namespaces == canonical && first.classification_key(key(8U, "Python")) == key(8U, "Python"),
			   "C++ is keyed under the canonical list and no other language reads it");
		expect(!first.cached(key(7U)), "the old list's C++ spans are never served under the new list");
		expect(first.cached(key(8U, "Python")) == python, "no other language is keyed away");
		first.classify(key(7U));
		first.classify(key(8U, "Python"));
		h.settle();
		expect(ca.Requests.size() == 3U && ca.Requests.back().Key.Namespaces == canonical,
			   "C++ is classified again under the new list and Python answers from the cache");
		expect(cb.Requests.empty(), "another window classifies nothing it was not asked for");

		const auto hits = first.cache_hits();
		first.apply(core::known_namespaces{ .Names = { "boost::asio", "std", "Qt" } });
		h.settle();
		first.classify(key(7U));
		h.settle();
		expect(ca.Requests.size() == 3U && first.cache_hits() == hits + 1U,
			   "a reordered list shares the canonical identity and reparses nothing");
		first.apply(core::known_namespaces{ });
		h.settle();
		expect(first.cached(key(7U)) == cpp, "an earlier list answers from the spans cached under it");

		const auto stores = h.Settings.Stores;
		first.apply(core::known_namespaces{ .Names = { "std", "3d", "std" } });
		h.settle();
		expect(first.settings().Namespaces == core::known_namespaces{ } && h.Settings.Stores == stores,
			   "a list with a refused entry changes and stores nothing");
		expect(pa.Failed.back().Failure == application::appearance_host_failure::invalid_known_namespaces
				   && pa.Failed.back().NamespaceRejections
					   == std::vector<core::known_namespace_rejection>{ { .Index = 1U, .Problem = core::known_namespace_problem::leading_digit },
																		{ .Index = 2U, .Problem = core::known_namespace_problem::duplicate } },
			   "every refused entry is named by its position");

		first.apply(listed);
		h.settle();
		first.reset(application::appearance_reset_scope::known_namespaces, core::appearance_variant::light);
		h.settle();
		expect(first.settings().Namespaces == core::known_namespaces{ } && h.Settings.Stored->Namespaces == core::known_namespaces{ }
				   && first.settings().Typography == base_settings().Typography,
			   "Reset restores std alone, persists it and keeps every other domain");

		h.Settings.Stored->Namespaces = core::known_namespaces{ .Names = { "Qt", "9" } };
		first.restore();
		h.settle();
		expect(first.settings().Namespaces == core::known_namespaces{ } && pa.Failed.back().Failure == application::appearance_host_failure::invalid_known_namespaces,
			   "a durable list the core refuses is refused whole");
		h.Settings.Stored->Namespaces = core::known_namespaces{ .Names = { "Qt" } };
		first.restore();
		h.settle();
		expect(first.settings().Namespaces == core::known_namespaces{ .Names = { "Qt" } } && pa.Published.back().Presentation->Namespaces == core::known_namespaces{ .Names = { "Qt" } },
			   "a durable legal list is adopted and published");
		first.shutdown();
		second.shutdown();
		h.settle();
		expect(first.is_quiescent() && second.is_quiescent(), "both windows quiesce");
	}
}
int main()
{
	test_known_namespaces_key_cpp_classifications();
	test_dif_profile_preferences_and_failure_fanout();
	test_shared_preferences_independent_windows();
	test_transient_results_target_only_live_origin();
	test_bounded_reentrant_detachment();
	return EXIT_SUCCESS;
}
