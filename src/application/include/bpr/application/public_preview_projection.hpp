#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLIC_PREVIEW_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLIC_PREVIEW_PROJECTION_HPP

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

/// The signed-out reading mode, carried outward for presentation alone. This
/// is a third narrow contract beside the GitHub and device-authorization
/// projections rather than an exception to either: it names a mode, a shared
/// hourly budget, the fixed list of what the mode cannot do, a typed refusal,
/// and the inline review conversation a signed-out read can observe. Nothing
/// here can represent a provider, a credential, a vault, a transport, an
/// address, or a request, and no type here carries a clock: an instant is a
/// value a caller measured and handed in.
namespace bpr::application {
	/// Which mode the reader is in. Signed out is a mode of its own and is
	/// never inferred from an absent account.
	enum class public_preview_mode : std::uint8_t {
		signed_in,
		signed_out,
	};

	/// Why a signed-out read or gesture was refused. Each value is a distinct
	/// sentence a reviewer can act on, and none of them is a missing value
	/// dressed as a refusal.
	enum class public_preview_refusal_reason : std::uint8_t {
		/// The shared hourly budget is spent. Carries the limit and the reset.
		budget_exhausted,
		/// The repository is private, or it does not exist. Signed out these
		/// two are one answer and the wording says so rather than guessing.
		private_or_missing,
		/// An enterprise host publishes nothing signed out.
		enterprise_host_unavailable,
		/// The review threads and their resolution need a reader.
		review_threads_unavailable,
		/// There is no viewer to identify.
		viewer_identity_unavailable,
		/// Triage, queue role, viewed state, resolution, and publishing all
		/// need a reader, so all of them are refused before a request exists.
		triage_or_publishing_refused,
		/// This build composed no signed-out read authority at all, so the
		/// mode is unavailable rather than degraded.
		mode_unavailable,
	};

	/// Whether an observation was made at all. An unobservable conversation
	/// and an observed empty one are different answers.
	enum class public_preview_observation : std::uint8_t {
		unavailable,
		observed,
	};

	/// \brief Describes the shared hourly budget a signed-out reader spends.
	///
	/// \details `ResetUnixSeconds` is an opaque instant the host measured.
	/// Presentation subtracts a now it was given from it; it never reads a
	/// clock of its own, so the same projection renders the same words on
	/// every platform and in every test.
	struct public_preview_budget
	{
		std::uint64_t Limit            = 0;
		std::uint64_t Remaining        = 0;
		std::uint64_t ResetUnixSeconds = 0;

		auto operator<=>(const public_preview_budget&) const = default;
	};

	/// \brief Describes a refused signed-out read or gesture.
	///
	/// \details Sign-in is offered from every refusal, so it is a property of
	/// the type rather than a flag a caller sets: no value of this type can
	/// represent a refusal that leaves the reviewer nowhere to go.
	struct public_preview_refusal_presentation
	{
		static constexpr bool SignInOffered = true;

		public_preview_refusal_reason Reason
			= public_preview_refusal_reason::mode_unavailable;
		// Present exactly when the reason is a spent budget, so a refusal can
		// name the limit and the reset it is talking about.
		std::optional<public_preview_budget> Budget;

		auto operator<=>(const public_preview_refusal_presentation&) const
			= default;
	};

	/// \brief Describes one inline review comment a signed-out read observed.
	struct public_preview_review_comment
	{
		std::string                  Id;
		std::string                  AuthorLogin;
		std::string                  Body;
		std::string                  Path;
		std::optional<std::uint32_t> Line;

		auto operator<=>(const public_preview_review_comment&) const = default;
	};

	/// \brief Describes one observed inline conversation and its replies.
	///
	/// \details A group, not a thread. Signed out there is no thread identity
	/// and no resolution to report, and calling this a thread would invite a
	/// reader to render a resolution state nothing observed.
	struct public_preview_review_comment_group
	{
		public_preview_review_comment              Root;
		std::vector<public_preview_review_comment> Replies;

		auto operator<=>(const public_preview_review_comment_group&) const
			= default;
	};

	inline constexpr std::size_t public_preview_limitation_count = 3U;

	/// \brief Yields what the preview cannot do, as three fixed sentences.
	///
	/// \details Fixed in arity as well as in wording: the presentation carries
	/// an array of exactly this size, so a caller can neither add a fourth
	/// limitation nor quietly drop one.
	[[nodiscard]] inline auto public_preview_limitations()
		-> std::array<std::string, public_preview_limitation_count>
	{
		return {
			std::string{ "GitHub.com only." },
			std::string{ "No review threads." },
			std::string{ "No triage or publishing." },
		};
	}

	/// \brief Describes the public-preview presentation record.
	///
	/// \details One record answers every question presentation may ask about
	/// the mode: which mode it is, how much of the shared hourly budget is
	/// spent and when it returns, what the mode cannot do, what refused the
	/// last read, and whether the inline review conversation was observed.
	///
	/// `NowUnixSeconds` is the host's own measured instant, or zero when the
	/// host has no clock bound; presentation then falls back to a now its
	/// composition gave it. Either way the instant arrives from outside, which
	/// is what keeps the rendered reset text deterministic.
	struct public_preview_presentation
	{
		public_preview_mode Mode = public_preview_mode::signed_in;
		// Absent until a read observes the shared budget. Absent is not zero:
		// a ring with no budget draws no consumed fraction at all.
		std::optional<public_preview_budget>               Budget;
		std::uint64_t                                      NowUnixSeconds = 0;
		std::optional<public_preview_refusal_presentation> Refusal;
		// Leads with `unavailable`, so a presentation that was never told
		// cannot read as one that was told and found nothing.
		public_preview_observation ReviewComments
			= public_preview_observation::unavailable;
		std::vector<public_preview_review_comment_group>         ReviewCommentGroups;
		std::array<std::string, public_preview_limitation_count> Limitations
			= public_preview_limitations();

		auto operator<=>(const public_preview_presentation&) const = default;
	};

	/// \brief Describes the switch-to-sign-in gesture record.
	///
	/// \details Deliberately empty. The profile a code is asked for is the one
	/// the host picker names, and it is resolved where that picker lives; a
	/// gesture that carried a profile would be a second place for the two to
	/// disagree.
	struct public_preview_sign_in_gesture
	{
		auto operator<=>(const public_preview_sign_in_gesture&) const = default;
	};

	// One gesture today, expressed as a variant so a second one cannot be
	// added by widening a callback's meaning.
	using public_preview_gesture = std::variant<public_preview_sign_in_gesture>;
	using public_preview_gesture_dispatch
		= std::function<void(public_preview_gesture)>;

	/// \brief Provides the public preview projection port contract.
	class public_preview_projection_port
	{
	public:
		virtual ~public_preview_projection_port() = default;

		// Delivery is synchronous and observational, on activation, on every
		// budget observation, on every refusal, and on invalidation. The host
		// contains projection exceptions; presentation cannot veto a refusal
		// the core policy decided.
		virtual void publish(
			const public_preview_presentation& aPresentation) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLIC_PREVIEW_PROJECTION_HPP
