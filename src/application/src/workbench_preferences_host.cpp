#include "bpr/application/workbench_preferences_host.hpp"
#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>

namespace bpr::application {
	struct workbench_preferences_host::implementation final : std::enable_shared_from_this<implementation>
	{
		struct subscription
		{
			std::uint64_t Id;
			consumer      Publish;
			bool          Active = true;
		};
		implementation(workbench_settings_port& aSettings, effect_executor& aEffects,
					   decision_executor& aDecision, const core::bpr_profile_id aProfile)
			: Settings{ aSettings }
			, Effects{ aEffects }
			, Decision{ aDecision }
			, Profile{ aProfile }
		{ }
		void publish()
		{
			if (Closing || !Accepted) {
				return;
			}
			const auto accepted  = *Accepted;
			const auto consumers = Consumers;
			for (const auto& subscriber : consumers) {
				if (Closing || !Accepted || *Accepted != accepted) {
					return;
				}
				if (!subscriber->Active) {
					continue;
				}
				try {
					subscriber->Publish(accepted);
				} catch (...) {
					DeliveryFailed = true;
				}
			}
		}
		auto submit(const std::optional<workbench_profile_preferences>& aCandidate) -> bool
		{
			if (Closing || Pending || Generation == std::numeric_limits<std::uint64_t>::max()) {
				return false;
			}
			Pending          = true;
			PendingCandidate = aCandidate;
			Stop             = std::stop_source{ };
			const auto token = ++Generation;
			const auto stop  = Stop.get_token();
			const auto self  = shared_from_this();
			++Outstanding;
			try {
				Effects.post([self, token, stop, aCandidate] {
					auto result = workbench_settings_result<workbench_profile_preferences>{ std::unexpected(workbench_settings_error{ }) };
					try {
						if (stop.stop_requested()) {
							result = std::unexpected(workbench_settings_error{ workbench_settings_failure::cancelled });
						} else if (aCandidate) {
							const auto stored = self->Settings.store_preferences(*aCandidate, stop);
							result            = stored ? workbench_settings_result<workbench_profile_preferences>{ *aCandidate }
													   : std::unexpected(stored.error());
						} else {
							result = self->Settings.load_preferences(stop);
						}
					} catch (...) { }
					try {
						self->Decision.post([self, token, stop, terminal = std::move(result)] {
							--self->Outstanding;
							if (self->Closing || !self->Pending || self->Generation != token || stop.stop_requested()) {
								return;
							}
							self->Pending = false;
							self->PendingCandidate.reset();
							if (terminal && valid(*terminal)) {
								self->Accepted = *terminal;
							}
							const auto queued = std::exchange(self->QueuedCandidate, std::nullopt);
							if (queued) {
								static_cast<void>(self->submit(queued));
							}
							self->publish();
						});
					} catch (...) {
						--self->Outstanding;
						self->DeliveryFailed = true;
					}
				});
			} catch (...) {
				--Outstanding;
				Pending        = false;
				DeliveryFailed = true;
				return false;
			}
			return true;
		}
		void cancel()
		{
			Stop.request_stop();
			if (Pending) {
				Accepted.reset();
			}
			Pending = false;
			PendingCandidate.reset();
			QueuedCandidate.reset();
			if (Generation < std::numeric_limits<std::uint64_t>::max()) {
				++Generation;
			}
		}
		workbench_settings_port&                     Settings;
		effect_executor&                             Effects;
		decision_executor&                           Decision;
		core::bpr_profile_id                         Profile;
		std::optional<workbench_profile_preferences> Accepted, PendingCandidate, QueuedCandidate;
		std::vector<std::shared_ptr<subscription>>   Consumers;
		std::uint64_t                                NextSubscription = 1, Generation = 0;
		std::stop_source                             Stop;
		std::atomic_size_t                           Outstanding{ 0 };
		std::atomic_bool                             DeliveryFailed{ false };
		bool                                         Pending = false, Closing = false;
	};
	workbench_preferences_host::workbench_preferences_host(workbench_settings_port& aSettings,
														   effect_executor& aEffects, decision_executor& aDecision, const core::bpr_profile_id aProfile)
		: Implementation{ std::make_shared<implementation>(aSettings, aEffects, aDecision, aProfile) }
	{ }
	workbench_preferences_host::~workbench_preferences_host() { shutdown(); }
	auto workbench_preferences_host::subscribe(const core::bpr_profile_id aProfile, consumer aConsumer) -> std::uint64_t
	{
		auto& host = *Implementation;
		std::erase_if(host.Consumers, [](const auto& aConsumerState) { return !aConsumerState->Active; });
		if (host.Closing || aProfile != host.Profile || aProfile.value() == 0 || !aConsumer || host.Consumers.size() >= maximum_workbench_window_layouts
			|| host.NextSubscription == std::numeric_limits<std::uint64_t>::max()) {
			return 0;
		}
		const auto id         = host.NextSubscription++;
		auto       subscriber = std::make_shared<implementation::subscription>(implementation::subscription{ id, std::move(aConsumer) });
		host.Consumers.push_back(subscriber);
		if (host.Accepted) {
			try {
				const auto accepted = *host.Accepted;
				subscriber->Publish(accepted);
			} catch (...) {
				host.DeliveryFailed = true;
			}
		}
		return id;
	}
	void workbench_preferences_host::unsubscribe(const std::uint64_t aSubscription)
	{
		for (auto& subscriber : Implementation->Consumers) {
			if (subscriber->Id == aSubscription) {
				subscriber->Active = false;
			}
		}
	}
	void workbench_preferences_host::restore()
	{
		if (!Implementation->Accepted) {
			static_cast<void>(Implementation->submit(std::nullopt));
		}
	}
	void workbench_preferences_host::adopt_loaded(const workbench_profile_preferences& aPreferences)
	{
		auto& host = *Implementation;
		if (!host.Closing && !host.Accepted && !host.Pending && valid(aPreferences)) {
			host.Accepted = aPreferences;
			host.publish();
		}
	}
	auto workbench_preferences_host::apply_behaviour(const review_behaviour_settings aBehaviour) -> bool
	{
		if (!Implementation->Accepted || !valid(aBehaviour)) {
			return false;
		}
		auto candidate = Implementation->QueuedCandidate.value_or(Implementation->PendingCandidate.value_or(*Implementation->Accepted));
		// [[BPR-1088]]. An edit writes preferences only: what a load found
		// wrong with the stored label columns is not carried into the write.
		candidate.Behaviour = edited(aBehaviour);
		if (Implementation->Pending) {
			Implementation->QueuedCandidate = candidate;
			return true;
		}
		return Implementation->submit(candidate);
	}
	auto workbench_preferences_host::apply_defaults(const workbench_window_layout aDefaults) -> bool
	{
		if (!Implementation->Accepted || !valid(aDefaults)) {
			return false;
		}
		auto candidate              = Implementation->QueuedCandidate.value_or(Implementation->PendingCandidate.value_or(*Implementation->Accepted));
		candidate.NewWindowDefaults = aDefaults;
		if (Implementation->Pending) {
			Implementation->QueuedCandidate = candidate;
			return true;
		}
		return Implementation->submit(candidate);
	}
	auto workbench_preferences_host::apply_writing_hook(core::writing_hook aHook, std::string aInstructions) -> bool
	{
		auto&      host  = *Implementation;
		const auto index = static_cast<std::size_t>(aHook);
		if (host.Closing || !host.Accepted || index >= core::writing_hook_count) {
			return false;
		}
		auto candidate                          = host.QueuedCandidate.value_or(host.PendingCandidate.value_or(*host.Accepted));
		candidate.WritingHooks.Overrides[index] = std::move(aInstructions);
		++candidate.WritingHooks.Revision;
		if (!core::valid(candidate.WritingHooks)) {
			return false;
		}
		if (host.Pending) {
			host.QueuedCandidate = std::move(candidate);
			return true;
		}
		return host.submit(candidate);
	}
	void workbench_preferences_host::cancel() { Implementation->cancel(); }
	void workbench_preferences_host::shutdown()
	{
		Implementation->Closing = true;
		Implementation->cancel();
		for (auto& subscriber : Implementation->Consumers) {
			subscriber->Active = false;
		}
	}
	auto workbench_preferences_host::accepted() const noexcept -> const std::optional<workbench_profile_preferences>& { return Implementation->Accepted; }
	auto workbench_preferences_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0; }
	auto workbench_preferences_host::is_busy() const noexcept -> bool { return Implementation->Pending; }
	auto workbench_preferences_host::delivery_failed() const noexcept -> bool { return Implementation->DeliveryFailed; }
}
