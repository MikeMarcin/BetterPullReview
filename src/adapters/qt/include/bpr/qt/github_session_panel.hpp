#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_SESSION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_SESSION_PANEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/qt/usage_ring.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QAction;
class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QSpinBox;
class QToolButton;

namespace bpr::qt {
	/// \brief Records directly observable session-chrome activity.
	struct github_session_panel_statistics
	{
		std::size_t CommandUpdates    = 0;
		std::size_t StatusUpdates     = 0;
		std::size_t IdentityUpdates   = 0;
		std::size_t CapabilityUpdates = 0;
		std::size_t SessionUpdates    = 0;
		std::size_t DiagnosticUpdates = 0;
	};

	/// \brief What the session chrome needs from the review content beside it.
	///
	/// \details Two facts, named rather than reached for. `HasPullRequest` is
	/// read because the status line says something different while a pull
	/// request is on screen; `PresentationChanged` is called because a status
	/// or diagnostic change can alter which view state applies.
	///
	/// They exist because the seam is not as clean as it looks. See the note on
	/// `github_session_panel` below.
	struct github_session_environment
	{
		std::function<bool()> HasPullRequest;
		std::function<void()> PresentationChanged;
	};

	/// \brief Presents one GitHub session: host profile, locator, and status.
	///
	/// \details Extracted from `github_window` by BPR-0195 so that BPR-0191 can
	/// move the session chrome into the workbench shell without also moving the
	/// pull-request content in the same commit.
	///
	/// **This panel deliberately does not implement `github_projection_port`,
	/// which BPR-0195 asked for.** A presentation update cannot be split into a
	/// chrome half and a content half that apply independently: inside one
	/// transition the order is commands, identity, capabilities, *clear the
	/// pull request*, status, diagnostics — and `apply_status` reads whether a
	/// pull request is on screen, so a content mutation sits between two chrome
	/// applications. A `publish` here would have to either re-implement that
	/// ordering rule or be wrong, and a second copy of an ordering rule is how
	/// the two windows this RFC is merging came to disagree in the first place.
	///
	/// So the composing window keeps the routing unchanged and calls the
	/// appliers below in the order it already used. That is also what makes
	/// this slice behaviour-preserving: the sequence a projection is applied in
	/// is not re-derived anywhere.
	///
	/// The panel holds no presentation state beyond what the projection told
	/// it, exactly as the window did.
	class github_session_panel final : public QWidget
	{
	public:
		github_session_panel(
			std::vector<application::github_profile_option> aProfiles,
			application::github_intent_dispatch             aDispatch,
			QAction*                                        aConnectAction,
			QAction*                                        aOpenAction,
			QAction*                                        aRefreshAction,
			github_session_environment                      aEnvironment,
			QWidget*                                        aParent = nullptr);

		/// \brief Applies one command projection if it names a chrome command.
		///
		/// \returns Whether this panel owns the command. The composing window
		/// handles the rest, so a command belonging to neither is a compile-time
		/// omission rather than a silently dropped projection.
		auto apply_command(const core::github_command_projection& aCommand) -> bool;
		void apply_identity(const core::github_identity_delta& aIdentity);
		void apply_status(core::github_status aStatus);
		void apply_diagnostic(
			core::github_reason_code     aReason,
			std::optional<std::uint32_t> aRetryAfter = std::nullopt,
			const std::optional<
				application::github_rate_limit_projection>& aRateLimit  = std::nullopt,
			bool                                            aHostDetail = false);
		void apply_session(
			const application::github_session_projection& aSession);
		void apply_capabilities(const core::github_capabilities& aCapabilities);
		void apply_rate_limits(
			const application::github_rate_limits_projection& aLimits);
		/// \brief Adopts one public-preview presentation.
		///
		/// \details Under the signed-out mode this names the mode beside the
		/// host, fills the usage ring, and states the refusal that ended the
		/// last read. Under the signed-in mode the strip is removed entirely
		/// rather than blanked, because an empty mode banner in an
		/// authenticated session is the ambiguity the mode exists to avoid.
		/// `aNowUnixSeconds` is the instant the composition measured; the
		/// panel reads no clock.
		void apply_public_preview(
			const application::public_preview_presentation& aPresentation,
			std::uint64_t                                   aNowUnixSeconds);
		/// \brief Adopts the sink the ring's sign-in gesture leaves through.
		void set_public_preview_dispatch(
			application::public_preview_gesture_dispatch aDispatch);
		/// \brief Re-reads the selected profile after the reviewer changed it.
		void update_selected_profile();
		void update_connect_enabled();
		void refresh_native_appearance();
		/// \brief Lays the locator actions out for the available width.
		void apply_compact_layout(bool aCompact);

		[[nodiscard]] auto profile_selector() const noexcept -> QComboBox*;
		/// \brief The consumed-budget ring, shown only under public preview.
		[[nodiscard]] auto usage_indicator() const noexcept -> usage_ring*;
		/// \brief The label naming the access mode beside the active host.
		[[nodiscard]] auto access_mode_label() const noexcept -> QLabel*;
		/// \brief The label stating the refusal that ended the last read.
		[[nodiscard]] auto refusal_label() const noexcept -> QLabel*;
		[[nodiscard]] auto public_preview_signed_out() const noexcept -> bool;
		[[nodiscard]] auto owner_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto repository_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto pull_request_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto connect_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto open_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto refresh_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto connection_details_button() const noexcept -> QToolButton*;
		/// \brief Whether the banner is currently showing a diagnostic.
		///
		/// \details Distinct from `diagnostic_text`, which reports the text the
		/// label holds whether or not it is on screen. Both callers exist and
		/// they want different answers.
		[[nodiscard]] auto showing_diagnostic() const -> bool;
		/// \brief The banner an assistive announcement is attributed to.
		[[nodiscard]] auto status_banner() const noexcept -> QFrame*;
		/// \brief The status line and any diagnostic under it, as one sentence.
		[[nodiscard]] auto summary_text() const -> QString;
		[[nodiscard]] auto status_text() const -> std::string;
		[[nodiscard]] auto diagnostic_text() const -> std::string;
		[[nodiscard]] auto active_host_text() const -> std::string;
		[[nodiscard]] auto active_account_text() const -> std::string;
		[[nodiscard]] auto status() const noexcept -> core::github_status;
		[[nodiscard]] auto selected_profile() const
			-> std::optional<core::host_profile_id>;
		[[nodiscard]] auto statistics() const noexcept
			-> github_session_panel_statistics;

	private:
		[[nodiscard]] auto profile_option(core::host_profile_id aProfile) const
			-> const application::github_profile_option*;
		[[nodiscard]] auto profile_index(core::host_profile_id aProfile) const
			-> int;

		application::github_intent_dispatch             Dispatch;
		std::vector<application::github_profile_option> Profiles;
		github_session_environment                      Environment;

		QAction*     ConnectAction           = nullptr;
		QAction*     OpenAction              = nullptr;
		QAction*     RefreshAction           = nullptr;
		QToolButton* ConnectButton           = nullptr;
		QToolButton* OpenButton              = nullptr;
		QToolButton* RefreshButton           = nullptr;
		QToolButton* ConnectionDetailsButton = nullptr;
		QComboBox*   ProfileSelector         = nullptr;
		QLineEdit*   OwnerEditor             = nullptr;
		QLineEdit*   RepositoryEditor        = nullptr;
		QSpinBox*    PullRequestEditor       = nullptr;
		QLabel*      HostLabel               = nullptr;
		QLabel*      TransportLabel          = nullptr;
		QLabel*      AccountLabel            = nullptr;
		QLabel*      CapabilityLabel         = nullptr;
		QLabel*      RateLimitLabel          = nullptr;
		QLabel*      DiagnosticLabel         = nullptr;
		QLabel*      StatusLabel             = nullptr;
		QFrame*      StatusBanner            = nullptr;
		QWidget*     ConnectionDetails       = nullptr;
		QWidget*     PublicPreviewStrip      = nullptr;
		QLabel*      AccessModeLabel         = nullptr;
		QLabel*      RefusalLabel            = nullptr;
		usage_ring*  UsageIndicator          = nullptr;
		QWidget*     LocatorActions          = nullptr;
		QGridLayout* LocatorLayout           = nullptr;

		std::optional<core::host_profile_id>    ActiveProfile;
		std::optional<core::github_account_id>  SessionAccount;
		std::optional<core::github_reason_code> LastDiagnosticReason;
		bool                                    LastDiagnosticHasHostDetail = false;
		bool                                    ConnectCommandEnabled       = true;
		core::github_reason_code                ConnectDisabledReason       = core::github_reason_code::none;
		core::github_status                     CurrentStatus               = core::github_status::no_profile;

		github_session_panel_statistics Statistics;
		// The picker entry the mode wording replaced, so leaving the mode puts
		// the profile's own label back rather than a second guess at it.
		QString ReplacedProfileLabel;
		int     ReplacedProfileIndex = -1;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_SESSION_PANEL_HPP
