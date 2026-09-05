#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_PROJECTION_HPP

#include "bpr/core/device_authorization_transition.hpp"

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::application {
	/// \brief Reports what became of the last browser handoff attempt.
	///
	/// \details The handoff is a local convenience, not a step of the grant.
	/// None of these values is an authorization outcome: the flow is exactly as
	/// live after `refused`, `failed`, or `unavailable` as it is after `opened`,
	/// and the reviewer can still finish on another device by reading the code
	/// off the screen. Presentation exists so the window can say which of those
	/// happened instead of silently doing nothing.
	enum class device_authorization_handoff : std::uint8_t {
		// No open has been asked for in the current flow.
		not_attempted,
		// The host validated the address and the capability accepted it.
		opened,
		// The address did not match the compiled profile authority. No browser
		// was invoked; this is the only value that means the address itself is
		// not trustworthy.
		refused,
		// The capability was invoked for a valid address and could not launch.
		failed,
		// This build has no browser capability bound at all.
		unavailable,
	};

	/// \brief Describes the device authorization presentation record.
	///
	/// \details Six fields, and the list is closed to everything the flow
	/// keeps secret. The reviewer has to read a code off the screen and type it
	/// into a browser, so the user code and the address it goes to have to be
	/// presentable; everything else the flow touches — the device code, the
	/// tokens, the client identifier, the poll interval, the endpoint — has no
	/// presentation and is absent by construction rather than by omission.
	/// `Handoff` joined them because a browser that refuses to open has to be
	/// visible to the reviewer, and it reports only the fate of a local launch.
	///
	/// `Commands` is the core's own decision about which of begin and cancel is
	/// legal right now, and why the other is not, carried outward unchanged.
	/// It is here so that presentation can render command legality without
	/// deciding it: a widget that works out for itself whether a flow is
	/// running is a second copy of a rule the core already owns, and the two
	/// copies disagree the moment a stage is added. An absent command is a
	/// disabled command, so a projection that lost one fails closed.
	///
	/// `VerificationUri` arrives already checked against the server authority by
	/// the wire parser, and is checked a second time against the compiled
	/// profile authority before any open, so presentation never decides whether
	/// an address is safe to show or to follow. `RemainingValiditySeconds`
	/// counts to whichever terminal comes first, and both text fields are empty
	/// at every terminal, including the successful one: a user code that
	/// outlives its flow is a code the reviewer can still be tricked into
	/// typing.
	struct device_authorization_presentation
	{
		core::device_authorization_stage Stage
			= core::device_authorization_stage::idle;
		std::string                  UserCode;
		std::string                  VerificationUri;
		std::uint32_t                RemainingValiditySeconds = 0U;
		device_authorization_handoff Handoff
			= device_authorization_handoff::not_attempted;
		// The core's answer for a state nothing has happened in yet. A panel is
		// built before any transition publishes and still has to offer the one
		// command that is legal then, so the default asks the core instead of
		// restating it.
		std::vector<core::device_authorization_command_projection> Commands
			= core::device_authorization_commands(
				core::device_authorization_state{ });

		auto operator<=>(const device_authorization_presentation&) const = default;
	};

	/// \brief Describes the open verification address gesture record.
	///
	/// \details Deliberately empty. The address to open lives in the host for
	/// the length of the poll window and is never handed outward for a caller
	/// to pass back, because an address that can be supplied by the caller is
	/// an address the authority check can be argued out of.
	struct open_verification_address_gesture
	{
		auto operator<=>(const open_verification_address_gesture&) const
			= default;
	};

	// What presentation is allowed to ask for. Beginning and cancelling are
	// core intents because they move the flow; opening a browser is not,
	// because it does not.
	using device_authorization_gesture = std::variant<
		core::begin_device_authorization_intent,
		core::cancel_device_authorization_intent,
		open_verification_address_gesture>;
	using device_authorization_gesture_dispatch
		= std::function<void(device_authorization_gesture)>;

	enum class device_authorization_refresh_state : std::uint8_t {
		// No account has authorized material to renew.
		idle,
		// A renewal is armed for a moment before the access token expires.
		scheduled,
		// A renewal exchange is in flight.
		refreshing,
		// Renewal cannot continue. The reviewer has to authorize again, and the
		// host says so once instead of retrying forever.
		reenrollment_required,
	};

	enum class device_authorization_refresh_failure : std::uint8_t {
		none,
		// The vault could not be reached or refused the slot.
		vault_unavailable,
		// The vault holds no refresh material for this authority.
		refresh_absent,
		// The server refused the refresh material it was given.
		refresh_rejected,
		// The exchange did not complete.
		transport_failed,
		// New material arrived but could not be stored, so rotating would have
		// left nothing durable behind.
		storage_failed,
	};

	/// \brief Describes the device authorization refresh observation record.
	///
	/// \details The renewal lifecycle, in the three facts presentation is
	/// allowed to know: what the renewal is doing, why it stopped if it did,
	/// and how many times material has rotated. It carries no token, no
	/// authority, and no schedule, because a surface that can name the moment
	/// a renewal is due is a surface that can be asked to bring it forward.
	struct device_authorization_refresh_observation
	{
		device_authorization_refresh_state State
			= device_authorization_refresh_state::idle;
		device_authorization_refresh_failure Failure
			= device_authorization_refresh_failure::none;
		// How many times material has been rotated under this host.
		std::uint32_t Rotations = 0U;

		auto operator<=>(const device_authorization_refresh_observation&) const
			= default;
	};

	/// \brief Provides the device authorization projection port contract.
	class device_authorization_projection_port
	{
	public:
		virtual ~device_authorization_projection_port() = default;

		// Delivery is synchronous and observational, on every stage change. The
		// host contains projection exceptions; presentation cannot veto an
		// accepted core transition or an enrolled credential.
		virtual void publish(
			const device_authorization_presentation& aPresentation) = 0;

		// The renewal lifecycle, published whenever the observation changes.
		// It is a second method rather than six more fields on the
		// presentation: the presentation's list is closed to everything the
		// flow keeps secret, and renewal has its own vocabulary, its own
		// cadence, and readers that want one without the other. Pure, so a
		// projection that stopped forwarding renewal fails to compile rather
		// than silently leaving the reviewer with a token nothing renews.
		virtual void publish_refresh(
			const device_authorization_refresh_observation& aObservation) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_PROJECTION_HPP
