#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DISCOVERY_DIAGNOSIS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DISCOVERY_DIAGNOSIS_HPP

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1680]]. What one Home discovery diagnostic line is about:
	/// a whole refresh, or one read of it.
	enum class discovery_diagnosis_scope : std::uint8_t {
		refresh,
		read,
	};

	/// \brief [[BPR-1680]]. Why a discovery read was made; `none` for a refresh.
	enum class discovery_diagnosis_reason : std::uint8_t {
		none,
		/// Metadata and submitted reviews of pull requests already in the queue.
		retained,
		/// Open pull requests the viewer authored.
		authored,
		/// Open pull requests that ask the viewer for a review.
		requested,
	};

	/// \brief [[BPR-1680]]. How a refresh ended; `none` while it runs and for
	/// a read.
	enum class discovery_diagnosis_state : std::uint8_t {
		none,
		/// Everything was read.
		current,
		/// Everything was read; a bound made some answer partial.
		partial,
		/// It ran to its end with a repository or pull request unreadable.
		stale,
		/// A session-wide failure (offline, authentication, rate limit) stopped it.
		stopped,
		/// The session changed or the profile closed while it ran.
		cancelled,
	};

	/// \brief [[BPR-1680]]. One Home discovery diagnostic line: enums,
	/// ordinals and counts only.
	///
	/// \details Personal discovery reads under the viewer's login and saved
	/// repositories, which a protected GitHub Enterprise Server must not see
	/// leave the machine, so nothing here names a host, repository, account,
	/// pull request or title. `Refresh` numbers the profile's refreshes from 1
	/// and `Read` numbers a refresh's reads from 1, so a read's two lines and
	/// its refresh's two lines correlate without a request identity.
	///
	/// A read's `Repositories` and `PullRequests` are what it asks about; its
	/// `Requests` are the wire requests it sent, `WaitMilliseconds` the time
	/// it waited for its turn on the provider's lane and `RunMilliseconds`
	/// the time it then ran. A refresh's start counts the saved repositories
	/// and retained pull requests it will read; its end counts the reads it
	/// submitted and the ones that failed, and sums its reads' requests and
	/// lane waits, and `RunMilliseconds` is the whole refresh.
	///
	/// [[BPR-1744]]. The timing split and answer counts say where a
	/// read's run went and why it may have been slow. `CredentialMilliseconds` is the run's
	/// start of its account operation (a GitHub CLI credential is a `gh`
	/// process of its own); `WireMilliseconds` is the time its requests spent
	/// in the transport, process and account lease checks included, so the
	/// rest of the run is local work; `DecisionMilliseconds` is how long its
	/// answer waited for the host's decision lane. `Forbidden`, `Throttled`
	/// and `ServerErrors` count its HTTP 403, 429 and 5xx answers, and
	/// `Timeouts` its requests the transport timed out, including those of
	/// optional reads that did not fail the read. A refresh's end sums them
	/// over its reads, and `BackoffSeconds` is how long a rate limit then
	/// keeps the next refresh waiting. These spans overlap the run and the
	/// refresh; they are not parts to add up.
	struct discovery_diagnosis
	{
		discovery_diagnosis_scope  Scope                  = discovery_diagnosis_scope::refresh;
		std::uint32_t              Refresh                = 0U;
		std::uint32_t              Read                   = 0U;
		discovery_diagnosis_reason Reason                 = discovery_diagnosis_reason::none;
		discovery_diagnosis_state  State                  = discovery_diagnosis_state::none;
		std::uint32_t              Repositories           = 0U;
		std::uint32_t              PullRequests           = 0U;
		std::uint32_t              Reads                  = 0U;
		std::uint32_t              Failed                 = 0U;
		std::uint32_t              Requests               = 0U;
		std::uint32_t              WaitMilliseconds       = 0U;
		std::uint32_t              RunMilliseconds        = 0U;
		std::uint32_t              CredentialMilliseconds = 0U;
		std::uint32_t              WireMilliseconds       = 0U;
		std::uint32_t              DecisionMilliseconds   = 0U;
		std::uint32_t              Forbidden              = 0U;
		std::uint32_t              Throttled              = 0U;
		std::uint32_t              ServerErrors           = 0U;
		std::uint32_t              Timeouts               = 0U;
		std::uint32_t              BackoffSeconds         = 0U;

		auto operator<=>(const discovery_diagnosis&) const = default;
	};

	/// \brief The stable lower-case names, empty outside each set.
	[[nodiscard]] auto name(discovery_diagnosis_scope aScope) noexcept -> std::string_view;
	[[nodiscard]] auto name(discovery_diagnosis_reason aReason) noexcept -> std::string_view;
	[[nodiscard]] auto name(discovery_diagnosis_state aState) noexcept -> std::string_view;

	/// \brief Whether a line is one the host writes: known enums, a refresh
	/// numbered from 1, a refresh line with no read ordinal or reason and a
	/// read line with both, no more failures than reads, and a read line with
	/// no refresh state, read count, failure count or backoff.
	[[nodiscard]] auto valid(const discovery_diagnosis& aDiagnosis) noexcept -> bool;

	/// \brief The line for the diagnostics log, in fixed key order:
	/// `discovery=SCOPE refresh=N read=N reason=REASON state=STATE
	/// repositories=N pull_requests=N reads=N failed=N requests=N wait_ms=N
	/// run_ms=N credential_ms=N wire_ms=N decision_ms=N http_403=N http_429=N
	/// http_5xx=N timeouts=N backoff_s=N`. Empty when the line is not valid.
	[[nodiscard]] auto describe(const discovery_diagnosis& aDiagnosis) -> std::string;

	/// \brief Reads exactly what `describe` writes; anything else is none.
	/// \details A line that ends at `run_ms`, as builds before the timing
	/// split wrote it, is also read, with the later fields zero, so their
	/// retained logs still export.
	[[nodiscard]] auto parse_discovery_diagnosis(std::string_view aText) -> std::optional<discovery_diagnosis>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DISCOVERY_DIAGNOSIS_HPP
