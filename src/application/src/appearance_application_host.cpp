#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/syntax_classification_application_host.hpp"
#include <stdexcept>
#include <utility>
namespace bpr::application {
	struct appearance_application_host::implementation
	{
		struct projection_bridge final : appearance_projection_port
		{
			explicit projection_bridge(appearance_projection_port& aPort)
				: Port{ aPort }
			{ }
			void publish(const appearance_projection& aProjection) override
			{
				if (Active) {
					Port.publish(aProjection);
				}
			}
			void fail(const appearance_failure_publication& aFailure) override
			{
				if (Active) {
					Port.fail(aFailure);
				}
			}
			void                          rejected(appearance_terminal_rejection aRejection) override { LastRejection = aRejection; }
			appearance_terminal_rejection LastRejection = appearance_terminal_rejection::none;
			appearance_projection_port&   Port;
			bool                          Active = true;
		};
		implementation(std::shared_ptr<appearance_preferences_application_host> aPreferences,
					   bool aOwnPreferences, syntax_classification_port& aClassification, effect_executor& aEffects,
					   decision_executor& aDecisionLane, appearance_projection_port& aProjections,
					   core::application_session_id aSession, appearance_host_limits aLimits)
			: Preferences{ std::move(aPreferences) }
			, OwnPreferences{ aOwnPreferences }
			, Bridge{ std::make_shared<projection_bridge>(aProjections) }
			, Classification{ aClassification, aEffects, aDecisionLane, *Bridge, aSession, aLimits }
		{
			if (!Preferences) {
				throw std::invalid_argument{ "missing appearance preferences owner" };
			}
			const auto attached = Preferences->attach(Bridge, !OwnPreferences);
			if (!attached) {
				throw std::invalid_argument{ "appearance consumer attachment refused" };
			}
			Consumer = *attached;
		}
		~implementation() { shutdown(); }
		void shutdown()
		{
			if (Closing) {
				return;
			}
			Closing        = true;
			Bridge->Active = false;
			Preferences->detach(Consumer);
			Classification.shutdown();
			if (OwnPreferences) {
				Preferences->shutdown();
			}
		}
		std::shared_ptr<appearance_preferences_application_host> Preferences;
		bool                                                     OwnPreferences;
		std::shared_ptr<projection_bridge>                       Bridge;
		syntax_classification_application_host                   Classification;
		appearance_consumer_id                                   Consumer;
		bool                                                     Closing = false;
	};
	void appearance_application_host::observe_work(std::shared_ptr<insights_observer> aObserver) { Implementation->Classification.observe_work(std::move(aObserver)); }

	appearance_application_host::appearance_application_host(appearance_settings aInitialSettings, const appearance_builtin_palettes& aBuiltins,
															 appearance_settings_port& aSettings, theme_document_port& aThemeDocuments, syntax_classification_port& aClassification,
															 effect_executor& aEffects, decision_executor& aDecisionLane, appearance_projection_port& aProjections,
															 core::application_session_id aSession, appearance_host_limits aLimits)
		: Implementation{ std::make_unique<implementation>(std::make_shared<appearance_preferences_application_host>(
															   std::move(aInitialSettings), aBuiltins, aSettings, aThemeDocuments, aEffects, aDecisionLane, aSession, aLimits),
														   true,
														   aClassification, aEffects, aDecisionLane, aProjections, aSession, aLimits) }
	{ }
	appearance_application_host::appearance_application_host(std::shared_ptr<appearance_preferences_application_host> aPreferences,
															 syntax_classification_port& aClassification, effect_executor& aEffects, decision_executor& aDecisionLane,
															 appearance_projection_port& aProjections, core::application_session_id aSession, appearance_host_limits aLimits)
		: Implementation{ std::make_unique<implementation>(std::move(aPreferences), false,
														   aClassification, aEffects, aDecisionLane, aProjections, aSession, aLimits) }
	{ }
	appearance_application_host::~appearance_application_host() = default;
	void appearance_application_host::apply(core::dif_preferences aPreferences)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->apply(Implementation->Consumer, aPreferences);
		}
	}
	void appearance_application_host::observe_motion(core::motion_observation aObservation)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->observe_motion(Implementation->Consumer, aObservation);
		}
	}
	auto appearance_application_host::dif_resolution() const noexcept -> core::dif_resolution
	{
		return Implementation->Preferences->dif_resolution(Implementation->Consumer);
	}
	void appearance_application_host::restore()
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->restore(Implementation->Consumer);
		}
	}
	void appearance_application_host::select(const core::appearance_mode aMode)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->select(Implementation->Consumer, aMode);
		}
	}
	void appearance_application_host::observe(const core::appearance_observation aObservation)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->observe(Implementation->Consumer, aObservation);
		}
	}
	void appearance_application_host::import_theme_file(std::string aPath)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->import_theme_file(Implementation->Consumer, std::move(aPath));
		}
	}
	void appearance_application_host::cancel_theme_import()
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->cancel_theme_import(Implementation->Consumer);
		}
	}
	void appearance_application_host::export_theme_file(std::string aPath, core::appearance_palette aPalette, const core::appearance_variant aVariant)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->export_theme_file(Implementation->Consumer, std::move(aPath), aPalette, aVariant);
		}
	}
	void appearance_application_host::reset(const appearance_reset_scope aScope, const core::appearance_variant aVariant)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->reset(Implementation->Consumer, aScope, aVariant);
		}
	}
	void appearance_application_host::apply(core::appearance_palette aPalette, const core::appearance_variant aVariant)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->apply(Implementation->Consumer, std::move(aPalette), aVariant);
		}
	}
	void appearance_application_host::apply(core::typography_settings aTypography)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->apply(Implementation->Consumer, std::move(aTypography));
		}
	}
	void appearance_application_host::step_code_zoom(const core::code_zoom_step aStep)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->step_code_zoom(Implementation->Consumer, aStep);
		}
	}
	void appearance_application_host::apply(core::known_namespaces aNamespaces)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->apply(Implementation->Consumer, std::move(aNamespaces));
		}
	}
	void appearance_application_host::apply(const core::identity_avatar_style aStyle)
	{
		if (!Implementation->Closing) {
			Implementation->Preferences->apply(Implementation->Consumer, aStyle);
		}
	}
	auto appearance_application_host::classification_key(token_span_cache_key aKey) const -> token_span_cache_key
	{
		return application::classification_key(std::move(aKey), Implementation->Preferences->settings().Namespaces);
	}
	void appearance_application_host::classify(token_span_cache_key aKey) { Implementation->Classification.classify(classification_key(std::move(aKey))); }
	void appearance_application_host::cancel_classifications() { Implementation->Classification.cancel(); }
	void appearance_application_host::cancel()
	{
		if (Implementation->OwnPreferences) {
			Implementation->Preferences->cancel();
		} else {
			Implementation->Preferences->cancel_theme_import(Implementation->Consumer);
		}
		Implementation->Classification.cancel();
	}
	void appearance_application_host::shutdown() { Implementation->shutdown(); }
	auto appearance_application_host::settings() const noexcept -> const appearance_settings& { return Implementation->Preferences->settings(); }
	auto appearance_application_host::resolution() const noexcept -> core::appearance_resolution { return Implementation->Preferences->resolution(Implementation->Consumer); }
	auto appearance_application_host::observation() const noexcept -> const core::appearance_observation& { return Implementation->Preferences->observation(Implementation->Consumer); }
	auto appearance_application_host::epoch() const noexcept -> appearance_epoch { return Implementation->Preferences->epoch(Implementation->Consumer); }
	auto appearance_application_host::last_rejections() const noexcept -> std::span<const core::typography_rejection> { return Implementation->Preferences->last_rejections(); }
	auto appearance_application_host::cached(const token_span_cache_key& aKey) const -> shared_token_spans { return Implementation->Classification.cached(classification_key(aKey)); }
	auto appearance_application_host::cached_classifications() const noexcept -> std::size_t { return Implementation->Classification.cached_classifications(); }
	auto appearance_application_host::cache_hits() const noexcept -> std::size_t { return Implementation->Classification.cache_hits(); }
	auto appearance_application_host::classification_submissions() const noexcept -> std::size_t { return Implementation->Classification.classification_submissions(); }
	auto appearance_application_host::coalesced_classifications() const noexcept -> std::size_t { return Implementation->Classification.coalesced_classifications(); }
	auto appearance_application_host::classification_failures() const noexcept -> std::size_t { return Implementation->Classification.classification_failures(); }
	auto appearance_application_host::classification_terminals() const noexcept -> std::size_t { return Implementation->Classification.classification_terminals(); }
	auto appearance_application_host::cache_evictions() const noexcept -> std::size_t { return Implementation->Classification.cache_evictions(); }
	auto appearance_application_host::persisted_settings() const noexcept -> std::size_t { return Implementation->Preferences->persisted_settings(); }
	auto appearance_application_host::outstanding_requests() const noexcept -> std::size_t { return Implementation->Classification.outstanding_requests() + Implementation->Preferences->outstanding_requests(); }
	auto appearance_application_host::rejected_terminals() const noexcept -> std::size_t { return Implementation->Classification.rejected_terminals() + Implementation->Preferences->rejected_terminals(); }
	auto appearance_application_host::last_rejection() const noexcept -> appearance_terminal_rejection
	{
		return Implementation->Bridge->LastRejection;
	}
	auto appearance_application_host::is_closing() const noexcept -> bool { return Implementation->Closing; }
	auto appearance_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Classification.is_quiescent()
			&& (!Implementation->OwnPreferences || Implementation->Preferences->is_quiescent());
	}
	auto appearance_application_host::projection_delivery_failed() const noexcept -> bool
	{
		return Implementation->Classification.projection_delivery_failed() || Implementation->Preferences->projection_delivery_failed();
	}
}
