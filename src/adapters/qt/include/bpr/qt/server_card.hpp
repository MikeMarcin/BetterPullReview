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

class QEvent;
class QFrame;
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
		/// Choose this host, then connect it through the existing connect intent.
		std::function<void(core::host_profile_id)> Connect;
		/// Begin signing in on this host: its device flow where it has one,
		/// otherwise the way in Connect opens.
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

	/// \brief One configured host on Accounts & hosts: a header, an account row and rows.
	///
	/// \details [[BPR-0771]], RFC-BPR-0032. The header names the host, its kind
	/// and transport, and the connection state in one word. The account row is
	/// who is signed in and on what terms, or the one way in. The rows are
	/// Access, Rate limit and a Details disclosure holding the capabilities,
	/// the rate limits and the last problem with Copy details.
	///
	/// Presentation only. The card reaches no store, vault, provider or
	/// process; Connect, Sign in and Sign out leave through
	/// `server_card_gestures`, and what it shows arrives as a
	/// `server_card_presentation`. A GitHub CLI host carries RFC-BPR-0015's
	/// trust warning in its account row, where the opt-in is made.
	class server_card final : public QWidget
	{
	public:
		server_card(
			application::github_profile_option aOption,
			server_card_gestures               aGestures,
			QWidget*                           aParent = nullptr);

		/// \brief Adopts one decision, changing only what differs from the last.
		void apply(const server_card_presentation& aPresentation);
		/// \brief Adopts the sink the public-preview ring's sign-in gesture leaves through.
		void set_public_preview_dispatch(
			application::public_preview_gesture_dispatch aDispatch);

		[[nodiscard]] auto profile() const noexcept -> core::host_profile_id;
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
		/// \brief RFC-BPR-0015's configured-CLI trust warning; null unless this is a CLI host.
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
		void apply_header();
		void apply_account_row();
		void apply_rows();
		void refresh_native_appearance();

		application::github_profile_option Option;
		server_card_gestures               Gestures;
		server_card_presentation           Presentation;
		server_card_statistics             Statistics;
		bool                               Applied = false;

		QLabel*                HostLabel        = nullptr;
		QLabel*                KindLabel        = nullptr;
		QLabel*                StateLabel       = nullptr;
		QLabel*                Avatar           = nullptr;
		QLabel*                LoginLabel       = nullptr;
		QLabel*                ConsequenceLabel = nullptr;
		QLabel*                CliTrustWarning  = nullptr;
		QPushButton*           SignInButton     = nullptr;
		QPushButton*           SignOutButton    = nullptr;
		QPushButton*           ConnectButton    = nullptr;
		QFrame*                RowsSeparator    = nullptr;
		QWidget*               Rows             = nullptr;
		QLabel*                AccessName       = nullptr;
		QLabel*                AccessLabel      = nullptr;
		QLabel*                RateLimitName    = nullptr;
		QLabel*                RateLimitLabel   = nullptr;
		usage_ring*            UsageIndicator   = nullptr;
		QToolButton*           DetailsButton    = nullptr;
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
