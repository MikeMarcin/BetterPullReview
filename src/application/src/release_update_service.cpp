#include "bpr/application/release_update_service.hpp"

#include <utility>

namespace bpr::application {
	struct release_update_service::implementation final : release_update_projection_port,
														  std::enable_shared_from_this<implementation>
	{
		std::string                                       Installed;
		core::release_update_capability                   Capability;
		release_update_clock_port&                        Clock;
		decision_executor&                                Decision;
		effect_executor&                                  Effects;
		std::shared_ptr<release_update_settings_port>     Store;
		std::shared_ptr<release_update_verification_port> Verifier;
		std::shared_ptr<release_update_install_port>      Installer;
		release_update_projection_port*                   Sink;
		std::unique_ptr<release_update_application_host>  Host;
		release_update_settings                           Settings;
		bool                                              Started = false;
		bool                                              Pending = false;
		bool                                              Dirty   = false;
		bool                                              Durable = false;
		bool                                              Closing = false;

		implementation(std::string aInstalled, core::release_update_capability aCapability,
					   release_update_clock_port& aClock, decision_executor& aDecision, effect_executor& aEffects,
					   std::shared_ptr<release_update_settings_port>     aStore,
					   std::shared_ptr<release_update_verification_port> aVerifier, release_update_projection_port& aSink,
					   std::shared_ptr<release_update_install_port> aInstaller)
			: Installed{ std::move(aInstalled) }
			, Capability{ aCapability }
			, Clock{ aClock }
			, Decision{ aDecision }
			, Effects{ aEffects }
			, Store{ std::move(aStore) }
			, Verifier{ std::move(aVerifier) }
			, Installer{ std::move(aInstaller) }
			, Sink{ &aSink }
		{ }

		void publish(const release_update_projection& aProjection, bool aChanged) override
		{
			if (aChanged) {
				Settings.Preferences = aProjection.Preferences;
				Dirty                = true;
				save();
			}
			notify(aProjection);
		}
		void notify(const release_update_projection& aProjection)
		{
			if (Sink) {
				try {
					Sink->publish(aProjection, false);
				} catch (...) { }
			}
		}
		void start()
		{
			if (Started || Closing) {
				return;
			}
			Started = true;
			Pending = true;
			try {
				Effects.post([self = shared_from_this()] {
					auto settings = std::expected<release_update_settings, core::release_update_reason>{
						std::unexpected(core::release_update_reason::unavailable)
					};
					try {
						if (self->Store) {
							settings = self->Store->load();
						}
					} catch (...) { }
					self->Decision.post([self, loaded = std::move(settings)] {
						self->Pending = false;
						if (self->Closing) {
							return;
						}
						self->Durable = loaded.has_value();
						if (loaded) {
							self->Settings = *loaded;
						}
						self->Host                 = std::make_unique<release_update_application_host>(core::application_session_id{ 1U },
																									   self->Installed, self->Capability, self->Settings.Preferences, self->Clock, self->Decision,
																									   self->Effects, self->Verifier, *self, self->Installer);
						self->Settings.Preferences = self->Host->projection().Preferences;
						self->notify(self->Host->projection());
					});
				});
			} catch (...) {
				Pending = false;
			}
		}
		void save()
		{
			if (Pending || !Dirty || !Store) {
				return;
			}
			Dirty   = false;
			Pending = true;
			try {
				Effects.post([self = shared_from_this(), snapshot = Settings] {
					bool saved = false;
					try {
						saved = self->Store->save(snapshot);
					} catch (...) { }
					self->Decision.post([self, saved] {
						self->Pending = false;
						self->Durable = saved;
						self->save();
						if (self->Host) {
							self->notify(self->Host->projection());
						}
					});
				});
			} catch (...) {
				Pending = false;
				Durable = false;
			}
		}
	};

	release_update_service::release_update_service(std::string aInstalled, core::release_update_capability aCapability,
												   release_update_clock_port& aClock, decision_executor& aDecision, effect_executor& aEffects,
												   std::shared_ptr<release_update_settings_port> aSettings, std::shared_ptr<release_update_verification_port> aVerifier,
												   release_update_projection_port&              aProjection,
												   std::shared_ptr<release_update_install_port> aInstaller)
		: Impl{ std::make_shared<implementation>(std::move(aInstalled), aCapability, aClock, aDecision, aEffects,
												 std::move(aSettings), std::move(aVerifier), aProjection, std::move(aInstaller)) }
	{ }
	release_update_service::~release_update_service()
	{
		Impl->Sink = nullptr;
		shutdown();
	}
	void release_update_service::start() { Impl->start(); }
	void release_update_service::select_access(std::string aBinding, std::shared_ptr<release_update_access_port> aAccess)
	{
		if (!Impl->Host || Impl->Closing || aBinding.size() > 128U) {
			return;
		}
		if (Impl->Settings.AccountBinding != aBinding) {
			Impl->Settings.AccountBinding = std::move(aBinding);
			Impl->Dirty                   = true;
			Impl->save();
		}
		Impl->Host->select_access(std::move(aAccess));
	}
	void release_update_service::revoke_access()
	{
		if (Impl->Host) {
			Impl->Host->select_access({ });
		}
	}
	void release_update_service::dispatch(core::release_update_intent aIntent)
	{
		if (Impl->Host && !Impl->Closing) {
			static_cast<void>(Impl->Host->dispatch(std::move(aIntent)));
		}
	}
	void release_update_service::set_automatic_checks(bool aEnabled)
	{
		if (Impl->Host && !Impl->Closing) {
			static_cast<void>(Impl->Host->set_automatic_checks(aEnabled));
		}
	}
	void release_update_service::tick()
	{
		if (Impl->Host && !Impl->Closing) {
			static_cast<void>(Impl->Host->tick());
		}
	}
	auto release_update_service::observe_installation_result(bool aInstalled) -> bool
	{
		if (!Impl->Host || Impl->Closing) {
			return false;
		}
		Impl->Host->observe_installation_result(aInstalled);
		return true;
	}
	void release_update_service::shutdown()
	{
		Impl->Closing = true;
		if (Impl->Host) {
			Impl->Host->shutdown();
		}
		Impl->save();
	}
	auto release_update_service::ready() const -> bool { return Impl->Host != nullptr && !Impl->Closing; }
	auto release_update_service::durable() const -> bool { return Impl->Durable; }
	auto release_update_service::quiescent() const -> bool { return !Impl->Pending && (!Impl->Host || Impl->Host->quiescent()); }
	auto release_update_service::account_binding() const -> std::string { return Impl->Settings.AccountBinding; }
	auto release_update_service::projection() const -> release_update_projection { return Impl->Host ? Impl->Host->projection() : release_update_projection{ }; }
}
