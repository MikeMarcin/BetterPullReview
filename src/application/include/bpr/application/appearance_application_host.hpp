#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_APPLICATION_HOST_HPP

#include "bpr/application/appearance_preferences_application_host.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/port.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace bpr::application {
	/// \brief Provides the appearance application host contract.
	///
	/// \details Compatibility facade over one preferences owner and one window classifier.
	/// Existing composition creates a private preferences owner; a shared owner may be
	/// supplied to attach another window without creating another durable store.
	/// Appearance gestures never enter the classifier or invalidate its token cache.
	///
	/// Public operations and inspection are confined to the supplied decision
	/// lane. Effect work runs on the effect executor and posts immutable terminals
	/// back to that lane. Referenced ports and executors must stay live until
	/// is_quiescent().
	class appearance_application_host final
	{
	public:
		// The composition supplies the base appearance. The host invents no colour
		// and no font family, and rejects a base its own validator would refuse.
		appearance_application_host(
			appearance_settings                aInitialSettings,
			const appearance_builtin_palettes& aBuiltins,
			appearance_settings_port&          aSettings,
			theme_document_port&               aThemeDocuments,
			syntax_classification_port&        aClassification,
			effect_executor&                   aEffects,
			decision_executor&                 aDecisionLane,
			appearance_projection_port&        aProjections,
			core::application_session_id       aSession,
			appearance_host_limits             aLimits = { });
		/// \brief Attaches a window to an existing profile preference owner.
		appearance_application_host(std::shared_ptr<appearance_preferences_application_host> aPreferences,
									syntax_classification_port& aClassification, effect_executor& aEffects, decision_executor& aDecisionLane,
									appearance_projection_port& aProjections, core::application_session_id aSession, appearance_host_limits aLimits = { });

		~appearance_application_host();

		appearance_application_host(const appearance_application_host&) = delete;
		auto operator=(const appearance_application_host&)
			-> appearance_application_host&                        = delete;
		appearance_application_host(appearance_application_host&&) = delete;
		auto operator=(appearance_application_host&&)
			-> appearance_application_host& = delete;

		// Loads the durable record. An absent record keeps the composed settings;
		// a record the core rejects is refused whole and never written back.
		/// \brief Applies profile preferences through the accepted settings path.
		void apply(core::dif_preferences aPreferences);
		/// \brief Observes bounded native motion facts for this attachment only.
		void               observe_motion(core::motion_observation aObservation);
		[[nodiscard]] auto dif_resolution() const noexcept -> core::dif_resolution;
		void               restore();
		void               select(core::appearance_mode aMode);
		// Reports the observed operating-system scheme and contrast preference. An
		// observation is not a setting and is never persisted.
		void observe(core::appearance_observation aObservation);
		/// \brief Commits one total palette as the named variant's customization.
		///
		/// \details The composition supplies the variant: it is the theme on
		/// screen, which the core resolved, and it is never guessed here or in Qt.
		/// Every other variant's stored colours survive the commit.
		void apply(core::appearance_palette aPalette, core::appearance_variant aVariant);
		void apply(core::typography_settings aTypography);
		/// \brief [[BPR-1336]]. Steps the comparison's zoom; see the preferences host.
		void step_code_zoom(core::code_zoom_step aStep);
		/// \brief Imports one local document into a preview candidate.
		///
		/// \details The effect reads through the bounded document port, calls the
		/// core parser, and validates the result against every appearance variant.
		/// Success publishes a candidate and never changes or persists settings.
		void import_theme_file(std::string aPath);
		/// Retires only the pending preview import; accepted durable work continues.
		void cancel_theme_import();
		/// \brief Writes a theme document whose base is `aVariant`, the theme on screen.
		void export_theme_file(std::string aPath, core::appearance_palette aPalette, core::appearance_variant aVariant);
		/// \brief Resets one durable group; `palette` resets the named variant only.
		void reset(appearance_reset_scope aScope, core::appearance_variant aVariant);
		// Answers from the retained cache when the key is present and runs the
		// classification port only when it is not.
		void classify(token_span_cache_key aKey);
		void cancel();
		void shutdown();

		[[nodiscard]] auto settings() const noexcept -> const appearance_settings&;
		// Derived from the selected mode and the current observation every time.
		// The host retains no resolved variant, because a resolution is a decision
		// and never a setting.
		[[nodiscard]] auto resolution() const noexcept
			-> core::appearance_resolution;
		[[nodiscard]] auto observation() const noexcept
			-> const core::appearance_observation&;
		[[nodiscard]] auto epoch() const noexcept -> appearance_epoch;
		[[nodiscard]] auto last_rejections() const noexcept
			-> std::span<const core::typography_rejection>;
		// Null when the key is absent. The retained value is immutable and shared,
		// so a caller can compare identity across an appearance change.
		[[nodiscard]] auto cached(const token_span_cache_key& aKey) const
			-> shared_token_spans;
		[[nodiscard]] auto cached_classifications() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_hits() const noexcept -> std::size_t;
		// Classifications actually handed to the port. This is the host-side parse
		// count: it advances exactly when lexical work was unavoidable.
		[[nodiscard]] auto classification_submissions() const noexcept
			-> std::size_t;
		[[nodiscard]] auto coalesced_classifications() const noexcept
			-> std::size_t;
		// Classifications that ended in a failure publication instead of
		// spans: invalid key, mismatched claim, unavailable bytes or
		// definition, or the span limit. A pane that never heard back is
		// indistinguishable from one that was never asked without this count
		// (BPR-0368).
		[[nodiscard]] auto classification_failures() const noexcept
			-> std::size_t;
		// Classification terminals that reached the decision lane at all. The
		// difference from submissions is work that vanished in transit — an
		// executor that never ran the effect or a lane that never drained it.
		[[nodiscard]] auto classification_terminals() const noexcept
			-> std::size_t;
		[[nodiscard]] auto cache_evictions() const noexcept -> std::size_t;
		[[nodiscard]] auto persisted_settings() const noexcept -> std::size_t;
		[[nodiscard]] auto outstanding_requests() const noexcept -> std::size_t;
		[[nodiscard]] auto rejected_terminals() const noexcept -> std::size_t;
		[[nodiscard]] auto last_rejection() const noexcept
			-> appearance_terminal_rejection;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_APPLICATION_HOST_HPP
