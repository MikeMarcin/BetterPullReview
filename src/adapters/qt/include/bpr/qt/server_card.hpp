#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SERVER_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SERVER_CARD_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/public_preview_projection.hpp"

#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QEvent;
class QFrame;
class QGridLayout;
class QLabel;
class QPaintEvent;
class QPushButton;
class QToolButton;

namespace bpr::qt {
	class usage_ring;

	/// \brief The connection state a server card names in its header, as one word.
	enum class server_card_state : std::uint8_t {
		signed_out,
		connecting,
		signing_in,
		connected,
		/// A short refusal: the session on this host failed.
		refused,
	};

	/// \brief The window's decision about one host, as its card presents it.
	///
	/// \details Everything here is decided outside the card, from the session
	/// presentation and the host last chosen. The card compares a new decision
	/// with the one it holds and changes only what differs.
	struct server_card_presentation
	{
		server_card_state State = server_card_state::signed_out;
		/// This is the host the session is on, or the chosen host while no
		/// session is. Its account row offers Sign in; the others offer Connect.
		bool Current = false;
		/// The chosen host profile, only when it is one of this host's. Without
		/// one the card speaks for the host's native profile.
		std::optional<core::host_profile_id> Chosen;
		/// The accepted session, only when it is this host's.
		std::optional<application::github_session_projection> Session;
		/// Reading public pull requests on this host without an account.
		bool ReadingPublic = false;
		/// Whether the core admits a Connect now, and what one would do or why not.
		bool    ConnectAdmitted = true;
		QString ConnectDescription;
		/// The last problem a failed session reported on this host; empty when none.
		QString                                    LastProblem;
		core::github_capabilities                  Capabilities;
		application::github_rate_limits_projection RateLimits;
		application::public_preview_presentation   PublicPreview;
		std::uint64_t                              PublicPreviewNow = 0;

		auto operator==(const server_card_presentation&) const -> bool = default;
	};

	/// \brief What a server card's gestures ask for. The card performs none of them.
	struct server_card_gestures
	{
		/// Choose this host profile, then connect it through the existing
		/// connect intent. Connect names the profile the card speaks for; the
		/// GitHub CLI offer names the host's GitHub CLI profile.
		std::function<void(core::host_profile_id)> Connect;
		/// Begin signing in with this host's native profile: its device flow
		/// where it has one, otherwise the way in Connect opens.
		std::function<void(core::host_profile_id)> SignIn;
		/// Sign the account out; the host forgets a remembered grant.
		std::function<void()> SignOut;
	};

	/// \brief Records directly observable server-card activity.
	///
	/// \details `Applications` counts every decision handed to the card and
	/// `Rebuilds` the ones that changed something, so a course can prove the
	/// card applies incrementally.
	struct server_card_statistics
	{
		std::size_t Applications = 0;
		std::size_t Rebuilds     = 0;
	};

	/// \brief One configured server on Accounts & hosts: a header, an account row and rows.
	///
	/// \details [[BPR-0771]], [[BPR-0772]], RFC-BPR-0032. One card per
	/// hostname: every configured host profile on that hostname is a way in
	/// to the same card, native HTTPS first. The header names the host once,
	/// the kind and transport of the profile the card speaks for, and the
	/// connection state in one word. The account row is who is signed in and
	/// on what terms, or the ways in: Sign in, Connect, and RFC-BPR-0015's
	/// GitHub CLI offer with its trust warning when the host has a GitHub CLI
	/// profile. The rows are Access, Rate limit and a Details disclosure
	/// holding the capabilities, the rate limits and the last problem with
	/// Copy details.
	///
	/// Presentation only. The card reaches no store, vault, provider or
	/// process; its gestures leave through `server_card_gestures`, and what it
	/// shows arrives as a `server_card_presentation`.
	class server_card final : public QWidget
	{
	public:
		server_card(
			application::github_profile_option aOption,
			server_card_gestures               aGestures,
			QWidget*                           aParent = nullptr);

		/// \brief Adds another way in to this server.
		///
		/// \returns false, changing nothing, unless the option names this
		/// card's hostname and a host profile the card does not hold yet.
		auto adopt_option(application::github_profile_option aOption) -> bool;
		/// \brief Adopts one decision, changing only what differs from the last.
		void apply(const server_card_presentation& aPresentation);
		/// \brief Adopts the sink the public-preview ring's sign-in gesture leaves through.
		void set_public_preview_dispatch(
			application::public_preview_gesture_dispatch aDispatch);

		[[nodiscard]] auto hostname() const noexcept -> const std::string&;
		/// \brief Whether a host profile is one of this server's ways in.
		[[nodiscard]] auto holds(core::host_profile_id aProfile) const noexcept -> bool;
		/// \brief This server's host profiles, native HTTPS first.
		[[nodiscard]] auto options() const noexcept
			-> const std::vector<application::github_profile_option>&;
		/// \brief The first native HTTPS profile; null when the host has none.
		[[nodiscard]] auto native_option() const noexcept
			-> const application::github_profile_option*;
		/// \brief The GitHub CLI profile; null when the host has none.
		[[nodiscard]] auto cli_option() const noexcept
			-> const application::github_profile_option*;
		/// \brief The host profile the card speaks for now.
		[[nodiscard]] auto profile() const noexcept -> core::host_profile_id;
		/// \brief The option the card speaks for now: the chosen one when it is
		/// this host's, otherwise the host's first profile.
		[[nodiscard]] auto option() const noexcept
			-> const application::github_profile_option&;
		[[nodiscard]] auto presentation() const noexcept
			-> const server_card_presentation&;
		[[nodiscard]] auto statistics() const noexcept -> server_card_statistics;
		/// \brief Whether this host has a device flow: GitHub.com over native HTTPS.
		[[nodiscard]] auto offers_device_sign_in() const noexcept -> bool;
		/// \brief Whether the account row currently shows a signed-in account.
		[[nodiscard]] auto signed_in() const noexcept -> bool;

		[[nodiscard]] auto host_label() const noexcept -> QLabel*;
		[[nodiscard]] auto kind_label() const noexcept -> QLabel*;
		[[nodiscard]] auto state_label() const noexcept -> QLabel*;
		[[nodiscard]] auto login_label() const noexcept -> QLabel*;
		[[nodiscard]] auto consequence_label() const noexcept -> QLabel*;
		/// \brief RFC-BPR-0015's GitHub CLI offer; null unless the host has a GitHub CLI profile.
		[[nodiscard]] auto cli_offer_button() const noexcept -> QPushButton*;
		/// \brief RFC-BPR-0015's configured-CLI trust warning; null unless the host has a GitHub CLI profile.
		[[nodiscard]] auto cli_trust_warning() const noexcept -> QLabel*;
		[[nodiscard]] auto sign_in_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto sign_out_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto connect_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto access_label() const noexcept -> QLabel*;
		[[nodiscard]] auto rate_limit_label() const noexcept -> QLabel*;
		/// \brief The public-preview hourly-limit ring, shown while reading without an account.
		[[nodiscard]] auto usage_indicator() const noexcept -> usage_ring*;
		[[nodiscard]] auto details_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto details() const noexcept -> QWidget*;
		[[nodiscard]] auto last_problem_label() const noexcept -> QLabel*;
		[[nodiscard]] auto copy_details_button() const noexcept -> QPushButton*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		[[nodiscard]] auto find_option(core::host_profile_id aProfile) const noexcept
			-> const application::github_profile_option*;
		void ensure_cli_offer();
		void apply_header();
		void apply_account_row();
		void apply_rows();
		void refresh_native_appearance();

		std::vector<application::github_profile_option> Options;
		server_card_gestures                            Gestures;
		server_card_presentation                        Presentation;
		server_card_statistics                          Statistics;
		bool                                            Applied = false;

		QLabel*                HostLabel        = nullptr;
		QLabel*                KindLabel        = nullptr;
		QLabel*                StateLabel       = nullptr;
		QWidget*               Account          = nullptr;
		QGridLayout*           AccountLayout    = nullptr;
		QLabel*                Avatar           = nullptr;
		QLabel*                LoginLabel       = nullptr;
		QLabel*                ConsequenceLabel = nullptr;
		QPushButton*           CliOfferButton   = nullptr;
		QLabel*                CliTrustWarning  = nullptr;
		QPushButton*           SignInButton     = nullptr;
		QPushButton*           SignOutButton    = nullptr;
		QPushButton*           ConnectButton    = nullptr;
		QFrame*                RowsSeparator    = nullptr;
		QWidget*               Rows             = nullptr;
		QWidget*               AccessRow        = nullptr;
		QWidget*               RateLimitRow     = nullptr;
		QWidget*               DetailsRow       = nullptr;
		std::array<QFrame*, 2> RowHairlines{ };
		QLabel*                AccessLabel      = nullptr;
		QLabel*                RateLimitLabel   = nullptr;
		usage_ring*            UsageIndicator   = nullptr;
		QToolButton*           DetailsButton    = nullptr;
		QLabel*                DetailsSummary   = nullptr;
		QWidget*               Details          = nullptr;
		std::array<QLabel*, 4> CapabilityValues{ };
		std::array<QLabel*, 3> RateLimitNames{ };
		std::array<QLabel*, 3> RateLimitValues{ };
		QLabel*                RateLimitsUnreported = nullptr;
		QLabel*                LastProblemName      = nullptr;
		QLabel*                LastProblemLabel     = nullptr;
		QPushButton*           CopyDetailsButton    = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SERVER_CARD_HPP
