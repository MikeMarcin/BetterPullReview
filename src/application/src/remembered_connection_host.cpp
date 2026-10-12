#include "bpr/application/remembered_connection_host.hpp"
#include <deque>
#include <utility>
namespace bpr::application {
	class remembered_connection_host::implementation final : public std::enable_shared_from_this<implementation>
	{
	public:
		struct operation
		{
			connection_metadata_action  Action;
			core::remembered_connection Record;
			std::string                 Hostname;
		};
		remembered_connection_store_port& Store;
		effect_executor&                  Effects;
		decision_executor&                Decisions;
		observer                          Observer;
		core::remembered_connection_state Session;
		core::remembered_connection_state Durable;
		std::deque<operation>             Pending;
		bool                              Available = false;
		bool                              Closing   = false;
		bool                              Busy      = false;
		implementation(remembered_connection_store_port& aStore, remembered_connection_store_result<core::remembered_connection_state> aLoaded,
					   core::bpr_profile_id aProfile, effect_executor& aEffects, decision_executor& aDecisions, observer aObserver)
			: Store{ aStore }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Observer{ std::move(aObserver) }
		{
			Session.BprProfile = aProfile;
			Available          = aLoaded && aLoaded->BprProfile == aProfile && core::valid(*aLoaded);
			if (Available) {
				Session = std::move(*aLoaded);
			}
			Durable = Session;
		}
		static auto apply(const core::remembered_connection_state& aState, const operation& aOperation) -> core::remembered_connection_result
		{
			switch (aOperation.Action) {
			case connection_metadata_action::remember: return core::remember_connection(aState, aState.Revision, aOperation.Record);
			case connection_metadata_action::disable:  return core::disable_connection_reuse(aState, aState.Revision, aOperation.Hostname);
			case connection_metadata_action::forget:   return core::forget_connection(aState, aState.Revision, aOperation.Hostname);
			}
			return std::unexpected(core::remembered_connection_failure::invalid);
		}
		void notify(connection_metadata_action aAction, bool aSaved)
		{
			if (Observer) {
				try {
					Observer({ aAction, aSaved });
				} catch (...) { }
			}
		}
		auto admit(operation aOperation) -> bool
		{
			if (Closing) {
				return false;
			}
			auto next = apply(Session, aOperation);
			if (!next || !Available || Pending.size() >= 128U) {
				notify(aOperation.Action, false);
				return false;
			}
			Pending.push_back(std::move(aOperation));
			Session = std::move(*next);
			start();
			return true;
		}
		void start()
		{
			if (Busy || Pending.empty()) {
				return;
			}
			Busy                = true;
			const auto action   = Pending.front().Action;
			auto       prepared = apply(Durable, Pending.front());
			if (!prepared) {
				complete(action, std::nullopt);
				return;
			}
			try {
				Effects.post([self = shared_from_this(), action, candidate = std::move(*prepared), expected = Durable.Revision]() mutable {
					auto saved = false;
					try {
						saved = self->Store.save(candidate, expected, { }).has_value();
					} catch (...) { }
					self->Decisions.post([self, action, saved, completed = std::move(candidate)]() mutable {
						self->complete(action, saved ? std::optional{ std::move(completed) } : std::nullopt);
					});
				});
			} catch (...) {
				complete(action, std::nullopt);
			}
		}
		void complete(connection_metadata_action aAction, std::optional<core::remembered_connection_state> aSaved)
		{
			if (aSaved) {
				Durable = std::move(*aSaved);
			}
			Pending.pop_front();
			Busy = false;
			// A newer operation owns the visible result, but every admitted write drains.
			if (Pending.empty()) {
				notify(aAction, aSaved.has_value());
			}
			start();
		}
	};
	remembered_connection_host::remembered_connection_host(remembered_connection_store_port& aStore, remembered_connection_store_result<core::remembered_connection_state> aLoaded,
														   core::bpr_profile_id aProfile, effect_executor& aEffects, decision_executor& aDecisions, observer aObserver)
		: Implementation{ std::make_shared<implementation>(aStore, std::move(aLoaded), aProfile, aEffects, aDecisions, std::move(aObserver)) }
	{ }
	remembered_connection_host::~remembered_connection_host() = default;
	auto remembered_connection_host::state() const noexcept -> const core::remembered_connection_state& { return Implementation->Session; }
	auto remembered_connection_host::available() const noexcept -> bool { return Implementation->Available; }
	auto remembered_connection_host::remember(core::remembered_connection aRecord) -> bool { return Implementation->admit({ connection_metadata_action::remember, std::move(aRecord), { } }); }
	auto remembered_connection_host::disable(std::string aHostname) -> bool { return Implementation->admit({ connection_metadata_action::disable, { }, std::move(aHostname) }); }
	auto remembered_connection_host::forget(std::string aHostname) -> bool { return Implementation->admit({ connection_metadata_action::forget, { }, std::move(aHostname) }); }
	void remembered_connection_host::shutdown() { Implementation->Closing = true; }
	auto remembered_connection_host::quiescent() const noexcept -> bool { return !Implementation->Busy && Implementation->Pending.empty(); }
}
