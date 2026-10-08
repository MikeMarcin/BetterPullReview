#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_PREFERENCES_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_PREFERENCES_APPLICATION_HOST_HPP
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/profile_window.hpp"
#include <span>
namespace bpr::application {
	struct appearance_consumer_id_tag;
	using appearance_consumer_id = core::strong_id<appearance_consumer_id_tag>;
	enum class appearance_attachment_failure { invalid_consumer,
											   capacity,
											   closing,
											   identity_exhausted };
	/// \brief One profile's durable preferences and bounded window subscriptions.
	/// \details All public calls run on the decision lane. Attach owns the consumer port
	/// until detach; detach immediately fences callbacks, including reentrant fan-out.
	/// Referenced settings/document ports and executors outlive shutdown/is_quiescent.
	/// Observations and derived publications are per attachment, never durable settings.
	class appearance_preferences_application_host final
	{
	public:
		appearance_preferences_application_host(appearance_settings aInitialSettings, appearance_settings_port& aSettings,
												theme_document_port& aThemeDocuments, effect_executor& aEffects, decision_executor& aDecisionLane,
												core::application_session_id aSession, appearance_host_limits aLimits = { });
		~appearance_preferences_application_host();
		appearance_preferences_application_host(const appearance_preferences_application_host&)                                  = delete;
		auto               operator=(const appearance_preferences_application_host&) -> appearance_preferences_application_host& = delete;
		[[nodiscard]] auto attach(std::shared_ptr<appearance_projection_port> aPort, bool aPublishInitial = true) -> std::expected<appearance_consumer_id, appearance_attachment_failure>;
		void               detach(appearance_consumer_id aConsumer);
		/// \brief Applies profile preferences through the accepted settings path.
		void apply(appearance_consumer_id aConsumer, core::dif_preferences aPreferences);
		/// \brief Observes bounded native motion facts for this attachment only.
		void               observe_motion(appearance_consumer_id aConsumer, core::motion_observation aObservation);
		[[nodiscard]] auto dif_resolution(appearance_consumer_id aConsumer) const noexcept -> core::dif_resolution;
		void               restore(appearance_consumer_id aConsumer);
		void               select(appearance_consumer_id aConsumer, core::appearance_mode aMode);
		void               observe(appearance_consumer_id aConsumer, core::appearance_observation aObservation);
		/// \brief Commits one total palette as the named variant's customization.
		///
		/// \details The variant is supplied rather than derived. A palette is one
		/// total set of colours for exactly one theme, and the theme it belongs to
		/// is the one that was on screen when the reviewer chose the colours — a
		/// fact the composition holds and this host would otherwise have to guess
		/// from an observation that may already have moved. Every other variant's
		/// customization is left exactly as it was.
		void apply(appearance_consumer_id aConsumer, core::appearance_palette aPalette, core::appearance_variant aVariant);
		void apply(appearance_consumer_id aConsumer, core::typography_settings aTypography);
		/// \brief [[BPR-1336]]. Steps the comparison's zoom from the accepted record.
		///
		/// \details Stepped here, against the record this host holds, so two
		/// gestures in quick succession are two steps even while the first is
		/// still queued. The core picks the step; the result is validated and
		/// persisted as any typography record is.
		void step_code_zoom(appearance_consumer_id aConsumer, core::code_zoom_step aStep);
		void import_theme_file(appearance_consumer_id aConsumer, std::string aPath);
		void cancel_theme_import(appearance_consumer_id aConsumer);
		void export_theme_file(appearance_consumer_id aConsumer, std::string aPath, core::appearance_palette aPalette);
		/// \brief Resets one durable group; `palette` resets the named variant only.
		void               reset(appearance_consumer_id aConsumer, appearance_reset_scope aScope, core::appearance_variant aVariant);
		void               cancel();
		void               shutdown();
		[[nodiscard]] auto settings() const noexcept -> const appearance_settings&;
		[[nodiscard]] auto resolution(appearance_consumer_id aConsumer) const noexcept -> core::appearance_resolution;
		[[nodiscard]] auto observation(appearance_consumer_id aConsumer) const noexcept -> const core::appearance_observation&;
		[[nodiscard]] auto epoch(appearance_consumer_id aConsumer) const noexcept -> appearance_epoch;
		[[nodiscard]] auto last_rejections() const noexcept -> std::span<const core::typography_rejection>;
		[[nodiscard]] auto attached_consumers() const noexcept -> std::size_t;
		[[nodiscard]] auto persisted_settings() const noexcept -> std::size_t;
		[[nodiscard]] auto outstanding_requests() const noexcept -> std::size_t;
		[[nodiscard]] auto rejected_terminals() const noexcept -> std::size_t;
		[[nodiscard]] auto last_rejection() const noexcept -> appearance_terminal_rejection;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;
		/// \brief No appearance persistence refusal or outstanding effect remains.
		[[nodiscard]] auto durable() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
