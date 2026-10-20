#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_PORT_HPP

#include "bpr/core/release_update.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <string_view>
#include <utility>

namespace bpr::application {

	/// \brief Bundle identity and resource bounds for the narrow staging capability.
	inline constexpr std::string_view release_update_bundle_identifier            = "com.gamecoretech.betterpullreview";
	inline constexpr std::string_view release_update_signing_team                 = "YTQKP2V2A8";
	inline constexpr std::uint64_t    release_update_maximum_metadata_bytes       = 1024U * 1024U;
	inline constexpr std::uint64_t    release_update_maximum_notes_bytes          = 32U * 1024U;
	inline constexpr std::uint64_t    release_update_maximum_expanded_bytes       = 2ULL * 1024U * 1024U * 1024U;
	inline constexpr std::uint64_t    release_update_maximum_entries              = 50'000U;
	inline constexpr std::uint32_t    release_update_metadata_timeout_seconds     = 30U;
	inline constexpr std::uint32_t    release_update_download_timeout_seconds     = 15U * 60U;
	inline constexpr std::uint32_t    release_update_verification_timeout_seconds = 5U * 60U;

	/// \brief Clock effects belong to the host; core receives these facts explicitly.
	class release_update_clock_port
	{
	public:
		virtual ~release_update_clock_port()                                   = default;
		[[nodiscard]] virtual auto now() noexcept -> core::release_update_time = 0;
	};

	/// \brief An owned archive capability. Paths and cleanup authority stay in its adapter.
	/// \details Destruction must not perform blocking cleanup on the decision lane;
	/// the adapter schedules any owned resource disposal on its effect service.
	class release_update_archive
	{
	public:
		explicit release_update_archive(core::release_update_candidate aCandidate)
			: Candidate{ std::move(aCandidate) }
		{ }
		virtual ~release_update_archive() = default;
		[[nodiscard]] auto candidate() const noexcept -> const core::release_update_candidate& { return Candidate; }

	private:
		core::release_update_candidate Candidate;
	};

	/// \brief A native-validated bundle capability; never constructed from a pathname by UI.
	/// \details Like archive capabilities, destruction only releases ownership and
	/// schedules bounded adapter cleanup. It cannot synchronously run filesystem work.
	class release_update_verified_bundle
	{
	public:
		explicit release_update_verified_bundle(core::release_update_candidate aCandidate)
			: Candidate{ std::move(aCandidate) }
		{ }
		virtual ~release_update_verified_bundle() = default;
		[[nodiscard]] auto candidate() const noexcept -> const core::release_update_candidate& { return Candidate; }

	private:
		core::release_update_candidate Candidate;
	};

	/// \brief Normalized release or typed failure, with no remote free-form diagnostic.
	struct release_update_check_result
	{
		core::release_update_reason  Failure = core::release_update_reason::none;
		core::release_update_release Release{ };
		std::uint64_t                RetryAfterSeconds = 0U;
		std::string                  Notes{ };
	};
	/// \brief Download terminal retains an owned archive capability and actual byte count.
	struct release_update_download_result
	{
		core::release_update_reason                   Failure = core::release_update_reason::none;
		std::shared_ptr<const release_update_archive> Archive{ };
		std::uint64_t                                 Bytes             = 0U;
		std::uint64_t                                 RetryAfterSeconds = 0U;
	};
	/// \brief Native validation returns a capability bound to the same candidate.
	struct release_update_verification_result
	{
		core::release_update_reason                           Failure = core::release_update_reason::none;
		std::shared_ptr<const release_update_verified_bundle> Bundle{ };
	};

	/// \brief An explicitly selected native GitHub.com release read lease, not a token.
	/// \details Composition acquires it through the existing native auth/vault boundary.
	/// Binding is an opaque live lease generation, not a profile or account login.
	/// binding()/kind() are nonblocking, thread-safe facts; revocation returns an
	/// unavailable binding and replacement must mint a fresh, never-reused identity. The
	/// adapter must enforce that authority and fixed repository on every request. Calls
	/// run only on the effect executor, are bounded/cancellable, and return one terminal.
	/// No progress callback may outlive download(). Credentials go only to API origin;
	/// redirects use the packaged HTTPS allowlist and strip credentials/cookies. No
	/// URLs, wire bodies or credentials belong in results or diagnostics. Bounded
	/// release notes are untrusted transient projection text, never persisted.
	class release_update_access_port
	{
	public:
		virtual ~release_update_access_port()                                                                                                             = default;
		[[nodiscard]] virtual auto binding() const noexcept -> core::release_update_access_id                                                             = 0;
		[[nodiscard]] virtual auto kind() const noexcept -> core::release_update_access_kind                                                              = 0;
		[[nodiscard]] virtual auto latest(std::stop_token aStop) -> release_update_check_result                                                           = 0;
		[[nodiscard]] virtual auto download(const core::release_update_candidate& aCandidate,
											std::stop_token aStop, const std::function<void(std::uint64_t)>& aProgress) -> release_update_download_result = 0;
	};

	/// \brief Bounded staging and complete native validation, isolated from core and Qt.
	/// \details Success requires safe archive layout, exact bundle/version/universal
	/// architectures, supplied digest, installed production designated requirement/team,
	/// system trust and notarization. No quarantine bypass. This port does not install.
	/// Revocation discards capabilities; any future install must refresh release identity
	/// under a valid access lease and revalidate the exact bundle immediately beforehand.
	class release_update_verification_port
	{
	public:
		virtual ~release_update_verification_port()                                                                                  = default;
		[[nodiscard]] virtual auto verify(std::shared_ptr<const release_update_archive> aArchive,
										  std::stop_token                               aStop) -> release_update_verification_result = 0;
	};

	/// \brief Qualified installation transaction boundary; never a UI pathname or process handle.
	/// \details Called on the effect lane after core accepts an exact refreshed release.
	/// The implementation revalidates staging, coordinates every installation process,
	/// and requires durable local-state shutdown before replacement. Cancellation before
	/// handoff leaves the installed application intact. Success means helper ownership,
	/// not successful installation; only its exact healthy-start receipt proves that.
	class release_update_install_port
	{
	public:
		virtual ~release_update_install_port()                                                                                             = default;
		[[nodiscard]] virtual auto install(std::shared_ptr<const release_update_verified_bundle> aBundle,
										   std::stop_token aStop, const std::function<bool()>& aAuthorized) -> core::release_update_reason = 0;
	};

	/// \brief Effect-lane byte sink, kept separate from native HTTPS and its secrets.
	class release_update_archive_writer
	{
	public:
		virtual ~release_update_archive_writer()                                             = default;
		[[nodiscard]] virtual auto append(std::span<const std::byte> aBytes) -> bool         = 0;
		[[nodiscard]] virtual auto finish() -> std::shared_ptr<const release_update_archive> = 0;
	};
	/// \brief Creates a private, bounded archive transaction. No pathname crosses inward.
	class release_update_archive_store
	{
	public:
		virtual ~release_update_archive_store() = default;
		[[nodiscard]] virtual auto begin(const core::release_update_candidate& aCandidate)
			-> std::unique_ptr<release_update_archive_writer> = 0;
	};
	/// \brief Durable application-wide preferences and opaque selected-account binding.
	struct release_update_settings
	{
		core::release_update_preferences Preferences;
		std::string                      AccountBinding;
		auto                             operator<=>(const release_update_settings&) const = default;
	};
	class release_update_settings_port
	{
	public:
		virtual ~release_update_settings_port()                                                                  = default;
		[[nodiscard]] virtual auto load() -> std::expected<release_update_settings, core::release_update_reason> = 0;
		[[nodiscard]] virtual auto save(const release_update_settings& aSettings) -> bool                        = 0;
	};
}

#endif
