#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_ACCOUNT_SETUP_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_ACCOUNT_SETUP_HOST_HPP

#include "bpr/application/account_setup_projection.hpp"
#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/account_setup.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>

namespace bpr::application {
	/// \brief The effects the setup asks for, performed by the composition.
	///
	/// \details Each call starts work and returns; none may block the
	/// decision lane. `inspect_github_cli` answers later through
	/// `account_setup_host::complete_github_cli`, naming the request it was
	/// given. The device and session outcomes are not answered here: the host
	/// observes them as the window's device and GitHub presentations.
	class account_setup_effect_port
	{
	public:
		virtual ~account_setup_effect_port() = default;

		virtual void inspect_github_cli(std::uint64_t aRequest)         = 0;
		virtual void begin_browser_sign_in()                            = 0;
		virtual void cancel_browser_sign_in()                           = 0;
		virtual void connect_github_cli(core::host_profile_id aProfile) = 0;
		/// Opens the device code's address. Not a setup transition: it decides
		/// nothing about the grant.
		virtual void open_verification_address() = 0;
	};

	/// \brief RFC-BPR-0032 amendment r1. Owns one window's account setup:
	/// applies the core transition, publishes the projection and asks the
	/// effect port for what the transition requested.
	///
	/// \details Lives on the decision lane. Events raised while an event is
	/// being applied (an effect whose port publishes synchronously) are queued
	/// and applied in order once it finishes. After `shutdown` nothing is
	/// published, no effect is asked for, and a completion still on its way
	/// is dropped.
	class account_setup_host final
	{
	public:
		account_setup_host(account_setup_effect_port& aEffects, decision_executor& aLane, account_setup_sink aSink, bool aGithubCliOffered);
		~account_setup_host();
		account_setup_host(const account_setup_host&)                    = delete;
		auto operator=(const account_setup_host&) -> account_setup_host& = delete;

		/// \brief A reviewer's gesture, or an observation the caller decoded.
		void dispatch(core::account_setup_event aEvent);
		/// \brief The device host's presentation, on every publication.
		void observe_device(const device_authorization_presentation& aPresentation);
		/// \brief A GitHub session outcome for this window.
		void observe_session(core::account_setup_session_observed aSession);
		/// \brief Opens the code's address in the browser, through the port.
		void open_verification_address();
		/// \brief gh's decoded answer. Callable from any thread: it is posted
		/// to the decision lane and applied there as a typed event.
		void complete_github_cli(core::github_cli_inspected aAnswer);
		void shutdown();

		[[nodiscard]] auto projection() const noexcept -> const account_setup_projection& { return Projection; }
		/// \brief The setup is in the window: begun and not finished.
		[[nodiscard]] auto active() const noexcept -> bool { return Projection.State.Phase != core::account_setup_phase::idle; }

	private:
		void apply_pending();
		void publish();
		void perform(const core::account_setup_effect& aEffect);

		account_setup_effect_port&            Effects;
		decision_executor&                    Lane;
		account_setup_sink                    Sink;
		account_setup_projection              Projection;
		std::deque<core::account_setup_event> Pending;
		std::shared_ptr<account_setup_host*>  Self;
		bool                                  Applying = false;
		bool                                  Stopped  = false;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_ACCOUNT_SETUP_HOST_HPP
