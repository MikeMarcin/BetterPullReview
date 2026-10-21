#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/repository_addition_input.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/home_ignore.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/repository_addition.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <compare>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace bpr::application {
	/// \brief Maximum admitted durable queue collection sizes, including departed enrollments.
	inline constexpr std::size_t maximum_queue_rules        = 1024U;
	inline constexpr std::size_t maximum_saved_repositories = 1024U;
	inline constexpr std::size_t maximum_saved_servers      = 64U;
	inline constexpr std::size_t maximum_queue_enrollments  = 16384U;

	/// \brief Why an enrolled pull request left the queue.
	///
	/// \details A pull request that simply disappeared would leave the reviewer
	/// wondering whether they had imagined it, so every departure has a reason
	/// they can read.
	enum class enrollment_departure : std::uint8_t {
		// Still here.
		none,
		// The pull request was closed or merged.
		closed,
		// This account can no longer read it.
		access_lost,
		// It no longer exists under this identity — transferred, or deleted.
		gone,
		// The reviewer removed it.
		removed_by_reviewer,
		// [[BPR-0989]]. A complete search of its saved repository no longer
		// finds it open with the reviewer's name on it, and nothing else keeps
		// it here. Whether it closed, merged or the request was withdrawn was
		// not read, so none of those is claimed.
		undiscovered,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(enrollment_departure aDeparture) noexcept
		-> std::string_view;

	/// \brief Returns the sentence a reviewer reads about a departure.
	[[nodiscard]] auto explanation(enrollment_departure aDeparture) noexcept
		-> std::string_view;

	/// \brief One enrolled pull request, as the host retains it.
	///
	/// \details `Membership` is kept as it was when the item was enrolled, not
	/// recomputed on every refresh. That is what makes enrollment survive the
	/// path that matched: a reviewer watching `src/parser` keeps the pull
	/// request when the last parser file leaves it, because the reason they are
	/// watching has not stopped being true — it is why they wanted to see the
	/// change in the first place.
	///
	/// A recomputing queue would drop exactly the pull requests a path rule
	/// exists to catch.
	struct enrollment_record
	{
		core::pull_request_ref Ref;
		core::queue_membership Membership;

		enrollment_departure    Departed     = enrollment_departure::none;
		std::optional<bool>     Draft        = std::nullopt;
		core::github_account_id DraftAccount = { };
		core::queue_item_facts  LastFacts;
		core::github_account_id FactsAccount = { };

		[[nodiscard]] auto present() const noexcept -> bool
		{
			return Departed == enrollment_departure::none;
		}

		auto operator<=>(const enrollment_record&) const = default;
	};

	/// \brief One pull request's read activity, keyed to the ref it describes.
	///
	/// \details `project` once paired activity to enrollments positionally,
	/// which obliged every caller to reproduce the register's private record
	/// order exactly and would have misattributed one pull request's activity
	/// to another on any mismatch. Carrying the ref makes the pairing explicit
	/// and order-free.
	struct queue_activity_report
	{
		core::pull_request_ref           Ref;
		core::queue_activity_observation Observation;
		/// [[BPR-0659]]. The title and author the refresh read; the unseen
		/// count is the observation's and is not repeated here.
		core::queue_item_facts Facts;

		auto operator<=>(const queue_activity_report&) const = default;
	};

	/// \brief Work performed while producing complete queue projections.
	struct queue_projection_work
	{
		std::uint64_t RecordsVisited      = 0U;
		std::uint64_t ActivityComparisons = 0U;
		/// Register output-vector storage acquisitions, not all allocator calls.
		std::uint64_t CapacityGrowths                                 = 0U;
		auto          operator<=>(const queue_projection_work&) const = default;
	};

	/// \brief [[BPR-0989]]. One pull request a personal discovery answer found.
	struct queue_discovered_pull_request
	{
		std::int64_t              Number = 0;
		std::string               Title;
		std::string               Author;
		bool                      Detailed         = false;
		std::uint32_t             ReviewersAwaited = 0U;
		std::optional<bool>       Draft            = std::nullopt;
		core::queue_waiting_facts Waiting;
		/// [[BPR-1407]]. Unknown where the pull request itself was not read.
		core::queue_change_facts Change;

		auto operator<=>(const queue_discovered_pull_request&) const = default;
	};

	/// \brief [[BPR-0989]]. What one discovery answer establishes about one
	/// saved repository under one session.
	///
	/// \details `Complete` is true only when the answer can say what is absent:
	/// then a reason it no longer finds is withdrawn. A partial answer adds and
	/// refreshes, and withdraws nothing.
	struct queue_discovery_answer
	{
		queue_session_id                           Session;
		std::string                                Repository;
		bool                                       Requested = false;
		bool                                       Complete  = false;
		std::vector<queue_discovered_pull_request> PullRequests;
		std::uint64_t                              FactRevision = 0;

		auto operator<=>(const queue_discovery_answer&) const = default;
	};

	/// \brief [[BPR-1379]]. Bounded server metadata for an already enrolled row.
	/// Only title, author, submitted-review/head facts and awaited reviewers are refreshed;
	/// local unseen/defect facts remain intact. This cannot enroll a new pull request.
	struct queue_metadata_answer
	{
		queue_session_id                 Session;
		core::pull_request_ref           Ref;
		core::queue_item_facts           Facts;
		core::queue_activity_observation Activity;
		bool                             Closed = false;
		std::optional<bool>              Authored;

		auto operator<=>(const queue_metadata_answer&) const = default;
	};

	/// \brief Retains enrollments and projects the queue from them.
	///
	/// \details Holds no provider, no clock, and no database. The durable store
	/// is a separate concern; this is the policy that decides what is retained
	/// and what the queue shows.
	class queue_enrollment_register final
	{
	public:
		/// Retains a known observation for an existing exact queue identity.
		[[nodiscard]] auto observe_draft(const core::pull_request_ref& aRef, std::optional<bool> aDraft, core::github_account_id aAccount) -> bool;
		/// \brief Enrolls or re-enrolls one pull request.
		///
		/// \details Enrolling something already present updates its membership
		/// only when it gained a GitHub role. A rule match must not overwrite a
		/// role, and a role must not overwrite the rule that first brought the
		/// item in — the reviewer is owed both reasons.
		[[nodiscard]] auto observe_facts(const core::pull_request_ref& aRef, const core::queue_item_facts& aFacts, core::github_account_id aAccount) -> bool;
		void               enroll(
			const core::pull_request_ref& aRef,
			const core::queue_membership& aMembership);

		/// \brief Replaces only the rule-derived reason after a local rule edit.
		///
		/// \details Provider refreshes use `enroll`, which preserves the rule
		/// that originally matched. A reviewer editing that exact local rule is
		/// different: the rule reason must be recomputed while any GitHub role
		/// remains authoritative.
		void reconcile_rule_change(
			const core::pull_request_ref& aRef,
			const core::queue_membership& aMembership);

		/// \brief Records that an item left, and why.
		void depart(
			const core::pull_request_ref& aRef,
			enrollment_departure          aDeparture);

		/// \brief [[BPR-0989]]. Applies one discovery answer; returns the refs
		/// whose record changed.
		///
		/// \details Each found pull request gains the answer's reason. A
		/// complete answer withdraws that reason from every pull request of
		/// the same host, account and repository it no longer finds; one that
		/// is then held by nothing else departs as `undiscovered`, while a
		/// reviewed or followed one stays. Nothing is withdrawn by a partial
		/// answer.
		[[nodiscard]] auto observe_discovery(const queue_discovery_answer& aAnswer) -> std::vector<core::pull_request_ref>;

		/// \brief [[BPR-0989]]. Sets or clears the reviewer's local follow;
		/// returns whether the record changed.
		[[nodiscard]] auto follow(const core::pull_request_ref& aRef, bool aFollowed) -> bool;

		/// \brief [[BPR-0989]]. Marks an enrolled pull request as one the
		/// reviewer has reviewed; returns whether the record changed.
		[[nodiscard]] auto reviewed(const core::pull_request_ref& aRef) -> bool;

		/// \brief Applies what a refresh concluded about one item.
		///
		/// \details `withdrawn` is the only disposition that moves an item out,
		/// and it needs the status to say which departure it was: forbidden and
		/// gone are different facts with different next steps.
		void observe(
			const queue_refresh_completion& aCompletion,
			refresh_disposition             aDisposition);

		/// \brief Every retained enrollment, departed ones included.
		[[nodiscard]] auto records() const noexcept
			-> std::span<const enrollment_record>;

		/// \brief The record for one pull request, if it is retained.
		[[nodiscard]] auto record_for(const core::pull_request_ref& aRef) const
			-> std::optional<enrollment_record>;

		/// \brief Builds the ordered queue from what is present.
		///
		/// \details Departed items are retained but not projected. They are
		/// kept so the reviewer can be told what happened, and dropped from the
		/// queue because a closed pull request is not asking for attention.
		/// Reports pair to enrollments by ref in any order: a report about an
		/// unenrolled ref moves nothing, and an enrollment without a report is
		/// quiet. [[BPR-1683]]: an item in `aIgnored` is marked ignored and
		/// ordered after the rest.
		[[nodiscard]] auto project(
			std::span<const queue_activity_report> aActivity,
			queue_projection_work* aWork = nullptr, const queue_session_id* aSession = nullptr,
			const core::home_ignore_list* aIgnored = nullptr) const
			-> std::vector<core::queue_item>;

	private:
		[[nodiscard]] auto find(const core::pull_request_ref& aRef)
			-> enrollment_record*;

		std::vector<enrollment_record>                Records;
		std::map<core::pull_request_ref, std::size_t> RecordIndex;
	};

	/// \brief Names one durable queue-store failure without exposing SQLite.
	enum class queue_store_failure : std::uint8_t {
		unavailable,
		integrity_failure,
		cancelled,
		invalid_rule,
		collection_limit,
	};

	/// \brief Selects which quiet/current rows the Qt landing surface presents.
	enum class queue_quiet_filter : std::uint8_t {
		all,
		attention,
		quiet,
	};

	/// \brief Names the explicit presentation order selected by the reviewer.
	///
	/// \details `core_priority` preserves the functional core's exact order.
	/// The other values are presentation-only alternatives and never rewrite
	/// queue membership or attention policy.
	enum class queue_sort_order : std::uint8_t {
		core_priority,
		repository,
		pull_request_number,
	};

	/// \brief [[BPR-0989]]. Where personal discovery stands for one server and account.
	enum class queue_discovery_state : std::uint8_t {
		/// No discovery has run under this session yet.
		idle,
		refreshing,
		/// The last refresh read every saved repository completely.
		current,
		/// The last refresh answered, but some answer was partial.
		partial,
		/// The last refresh failed; earlier rows are kept.
		stale,
		/// Discovery cannot run: no account session, or no saved repository.
		unavailable,
	};

	/// \brief [[BPR-0989]]. Why discovery is partial, stale or unavailable.
	enum class queue_discovery_problem : std::uint8_t {
		none,
		incomplete_results,
		result_limit,
		detail_limit,
		rate_limited,
		secondary_rate_limited,
		offline,
		authentication,
		repository_unavailable,
		failed,
		no_account,
		no_saved_repository,
		store_unavailable,
	};

	/// \brief [[BPR-0989]]. The discovery status the profile's windows show.
	///
	/// \details `LastSuccess` is the host clock's reading, in seconds, when a
	/// refresh last answered for every saved repository; it lasts for the
	/// session. `RetryAfterSeconds` is how long a rate limit asked to wait.
	struct queue_discovery_status
	{
		std::string                  Host;
		std::string                  Account;
		queue_discovery_state        State   = queue_discovery_state::idle;
		queue_discovery_problem      Problem = queue_discovery_problem::none;
		std::optional<std::int64_t>  LastSuccess;
		std::optional<std::uint32_t> RetryAfterSeconds;
		std::uint32_t                Repositories = 0U;

		auto operator<=>(const queue_discovery_status&) const = default;
	};

	/// \brief [[BPR-0989]]. The reviewer's local follow or unfollow of one pull request.
	struct queue_follow_intent
	{
		core::pull_request_ref Ref;
		std::string            Title;
		bool                   Follow = true;

		auto operator<=>(const queue_follow_intent&) const = default;
	};

	/// \brief [[BPR-0990]]. What a window asks of its profile's discovery.
	enum class queue_discovery_intent : std::uint8_t {
		/// Home was shown, or its window activated while showing Home:
		/// refresh only when the list is older than the freshness window.
		activate,
		/// The explicit Refresh.
		refresh,
	};

	/// \brief The private reviewer preferences applied above the decided queue.
	struct queue_presentation_preferences
	{
		std::optional<core::github_role>        Role;
		std::optional<core::observer_rule_kind> ObserverReason;
		std::optional<core::attention_bucket>   Bucket;
		std::string                             Repository;
		queue_quiet_filter                      Quiet = queue_quiet_filter::all;
		queue_sort_order                        Sort  = queue_sort_order::core_priority;
		core::notification_preferences          Notifications;

		auto operator<=>(const queue_presentation_preferences&) const = default;
	};

	/// \brief Checks the bounded serialized preference vocabulary.
	[[nodiscard]] auto valid(
		const queue_presentation_preferences& aPreferences) noexcept -> bool;

	/// \brief Names the edited domain so the host can merge rapid independent gestures.
	enum class queue_preference_field : std::uint8_t {
		all,
		role,
		observer,
		bucket,
		repository,
		quiet,
		sort,
		notifications_enabled,
		notifications_paused,
		notifications,
		queue_filters,
	};

	/// \brief One filter or sorting gesture translated by Qt.
	struct queue_preferences_intent
	{
		queue_presentation_preferences Preferences;
		queue_preference_field         Field = queue_preference_field::all;

		auto operator<=>(const queue_preferences_intent&) const = default;
	};

	/// \brief A repository the reviewer keeps in the repositories drawer.
	///
	/// \details [[BPR-0659]]. Per account: the host and account the queue
	/// session names, and the repository as "owner/name". Saving a repository
	/// enrolls nothing and changes nothing on GitHub; the drawer lists it
	/// even while no pull request of interest is in it.
	struct saved_repository
	{
		std::string Host;
		std::string Account;
		std::string Repository;

		auto operator<=>(const saved_repository&) const = default;
	};

	/// \brief Reports whether a saved repository can enter the store.
	[[nodiscard]] auto valid(const saved_repository& aRepository) noexcept -> bool;

	enum class saved_repository_action : std::uint8_t {
		add,
		remove,
	};

	/// \brief One save or forget gesture translated by Qt.
	struct saved_repository_intent
	{
		saved_repository_action Action = saved_repository_action::add;
		saved_repository        Repository;

		auto operator<=>(const saved_repository_intent&) const = default;
	};

	/// \brief One deliberate Add gesture bound to its displayed accepted destination.
	/// \details Composition supplies Source; Gesture is the originating draft's nonzero
	/// correlation token. Setup/sign-in completion alone never produces this intent.
	struct repository_addition_intent
	{
		core::bpr_window_context Source;
		std::uint64_t            Gesture = 0U;
		queue_session_id         Destination;
		std::string              Text;
	};

	/// \brief Exact progress/result for one originating Add gesture, never a network claim.
	struct repository_addition_result
	{
		core::bpr_window_context                         Source;
		std::uint64_t                                    Gesture = 0U;
		queue_session_id                                 Destination;
		core::repository_addition_status                 Status = core::repository_addition_status::unavailable;
		std::optional<repository_addition_target>        Target;
		std::optional<repository_addition_input_problem> InputProblem;
		std::optional<queue_store_failure>               StoreFailure;
		auto                                             operator==(const repository_addition_result&) const -> bool = default;
		auto                                             operator<=>(const repository_addition_result& aOther) const
		{
			return std::tie(Source.Profile, Source.Window, Source.Generation, Gesture, Destination, Status, Target, InputProblem, StoreFailure)
				<=> std::tie(aOther.Source.Profile, aOther.Source.Window, aOther.Source.Generation, aOther.Gesture, aOther.Destination, aOther.Status, aOther.Target, aOther.InputProblem, aOther.StoreFailure);
		}
	};

	/// \brief One enterprise server the reviewer keeps ([[BPR-0660]]).
	///
	/// \details The hostname as the reviewer typed it and the account name
	/// signed in there, if any. Remembering a server stores no token: the
	/// next launch offers the server's profiles and the reviewer signs in.
	struct saved_server
	{
		std::string Hostname;
		std::string Account;

		auto operator<=>(const saved_server&) const = default;
	};

	/// \brief Reports whether a saved server can enter the store.
	[[nodiscard]] auto valid(const saved_server& aServer) noexcept -> bool;

	enum class saved_server_action : std::uint8_t {
		add,
		remove,
	};

	/// \brief One remember or forget gesture translated by Qt.
	struct saved_server_intent
	{
		saved_server_action Action = saved_server_action::add;
		saved_server        Server;

		auto operator<=>(const saved_server_intent&) const = default;
	};

	/// \brief The complete bounded private queue state restored at launch.
	struct durable_queue_state
	{
		std::vector<core::observer_rule> Rules;
		std::vector<enrollment_record>   Enrollments;
		queue_presentation_preferences   Preferences;
		std::vector<saved_repository>    SavedRepositories;
		/// [[BPR-0660]].
		std::vector<saved_server> SavedServers;
		/// [[BPR-1683]]. The pull requests ignored on Home, by whole identity.
		std::vector<core::pull_request_ref> Ignored;
	};

	enum class queue_rule_action : std::uint8_t {
		add,
		remove,
	};

	/// \brief One local-rule gesture translated by Qt.
	struct queue_rule_intent
	{
		queue_rule_action   Action = queue_rule_action::add;
		core::observer_rule Rule;

		auto operator<=>(const queue_rule_intent&) const = default;
	};

	/// \brief Persists observer rules and their exact enrollment reasons.
	class queue_enrollment_store_port
	{
	public:
		virtual ~queue_enrollment_store_port() = default;

		[[nodiscard]] virtual auto load(std::stop_token aStop)
			-> std::expected<durable_queue_state, queue_store_failure> = 0;
		[[nodiscard]] virtual auto apply_rule_change(
			const queue_rule_intent&           aIntent,
			std::span<const enrollment_record> aEnrollments,
			std::stop_token                    aStop)
			-> std::expected<void, queue_store_failure> = 0;
		[[nodiscard]] virtual auto store_enrollment(
			const enrollment_record& aRecord,
			std::stop_token          aStop)
			-> std::expected<void, queue_store_failure> = 0;

		/// \brief [[BPR-0989]]. Writes several enrollments together.
		///
		/// \details A durable store writes them in one transaction; this
		/// default writes them in order and stops at the first refusal.
		[[nodiscard]] virtual auto store_enrollments(
			std::span<const enrollment_record> aRecords,
			std::stop_token                    aStop)
			-> std::expected<void, queue_store_failure>
		{
			for (const auto& record : aRecords) {
				if (auto stored = store_enrollment(record, aStop); !stored) {
					return stored;
				}
			}
			return { };
		}
		[[nodiscard]] virtual auto store_preferences(
			const queue_presentation_preferences& aPreferences,
			std::stop_token                       aStop)
			-> std::expected<void, queue_store_failure> = 0;

		/// \brief Saves or forgets one repository durably ([[BPR-0659]]).
		[[nodiscard]] virtual auto apply_saved_repository_change(
			const saved_repository_intent& aIntent,
			std::stop_token                aStop)
			-> std::expected<void, queue_store_failure> = 0;

		/// \brief Remembers or forgets one enterprise server durably ([[BPR-0660]]).
		[[nodiscard]] virtual auto apply_saved_server_change(
			const saved_server_intent& aIntent,
			std::stop_token            aStop)
			-> std::expected<void, queue_store_failure> = 0;

		/// \brief [[BPR-1683]]. Records ignores, unignores and prunes together.
		///
		/// \details One transaction: `ignored` adds the pull request, every other
		/// change removes it. Removing one that is not there succeeds.
		[[nodiscard]] virtual auto apply_home_ignores(
			std::span<const core::home_ignore_delta> aDeltas,
			std::stop_token                          aStop)
			-> std::expected<void, queue_store_failure> = 0;
	};

	/// \brief Performs one queue refresh away from the serialized decision lane.
	class queue_refresh_effect_port
	{
	public:
		virtual ~queue_refresh_effect_port() = default;

		[[nodiscard]] virtual auto refresh(
			const queue_refresh_request& aRequest,
			std::stop_token              aStop) -> queue_refresh_completion = 0;
	};

	enum class queue_landing_status : std::uint8_t {
		restoring,
		ready,
		unavailable,
	};

	/// \brief One accepted queue projection for the shipped landing surface.
	struct queue_landing_projection
	{
		queue_landing_status             Status = queue_landing_status::restoring;
		std::vector<core::observer_rule> Rules;
		std::vector<core::queue_item>    Items;
		queue_presentation_preferences   Preferences;
		queue_session_id                 Session;
		bool                             Online         = true;
		std::uint32_t                    BackoffSeconds = 0U;
		/// [[BPR-0659]]. Every saved repository, by host, account and name.
		std::vector<saved_repository> SavedRepositories;
		/// [[BPR-0660]]. Every remembered enterprise server, by hostname.
		std::vector<saved_server> SavedServers;

		std::optional<queue_store_failure> Failure;

		/// [[BPR-0989]]. Personal discovery for the profile's account session.
		queue_discovery_status Discovery;

		/// Last correlated addition result; consumers act only on their exact Source/Gesture.
		std::optional<repository_addition_result> Addition;

		/// One existing item changed. Items is empty; attachment replay supplies a
		/// complete projection when a consumer has not received the preceding state.
		std::optional<core::queue_item> UpdatedItem;

		auto operator<=>(const queue_landing_projection&) const = default;
	};

	class queue_landing_projection_port
	{
	public:
		virtual ~queue_landing_projection_port()                          = default;
		virtual void publish(const queue_landing_projection& aProjection) = 0;
	};

	struct queue_consumer_id_tag;
	using queue_consumer_id = core::strong_id<queue_consumer_id_tag>;

	enum class queue_attachment_failure : std::uint8_t {
		closing,
		invalid_consumer,
		stale_window,
		capacity,
		identity_exhausted,
	};

	/// \brief Reads exact live-window admission from the profile's core catalog.
	/// \details The owner keeps this read-only port alive through host shutdown. It must
	/// not reenter the host. Admission is checked on attach, replay and every delivery.
	class queue_window_context_port
	{
	public:
		virtual ~queue_window_context_port()                                                                = default;
		[[nodiscard]] virtual auto current(const core::bpr_window_context& aContext) const noexcept -> bool = 0;
	};

	/// \brief Receives one profile queue through an exact live-window attachment.
	class queue_window_projection_port
	{
	public:
		virtual ~queue_window_projection_port()                                                                     = default;
		virtual void publish(const core::bpr_window_context& aContext, const queue_landing_projection& aProjection) = 0;
	};

	struct review_queue_host_statistics
	{
		queue_projection_work Projection;
		std::uint64_t         RuleIntents            = 0U;
		std::uint64_t         PreferenceIntents      = 0U;
		std::uint64_t         SavedRepositoryIntents = 0U;
		std::uint64_t         SavedServerIntents     = 0U;
		std::uint64_t         PersistenceRequests    = 0U;
		std::uint64_t         RefreshRequests        = 0U;
		std::uint64_t         ConditionalRequests    = 0U;
		std::uint64_t         AppliedRefreshes       = 0U;
		std::uint64_t         StaleCompletions       = 0U;
		std::uint64_t         CancelledCompletions   = 0U;
		std::uint64_t         Publications           = 0U;
		std::uint64_t         ItemPublications       = 0U;
		std::uint64_t         Failures               = 0U;
		/// [[BPR-0989]].
		std::uint64_t DiscoveryAnswers      = 0U;
		std::uint64_t StaleDiscoveryAnswers = 0U;
		std::uint64_t FollowIntents         = 0U;
		/// [[BPR-1683]].
		std::uint64_t IgnoreIntents = 0U;
		std::uint64_t IgnoresPruned = 0U;

		auto operator<=>(const review_queue_host_statistics&) const = default;
	};

	/// \brief Composes durable local rules, refresh effects, and queue projection.
	class review_queue_application_host final
	{
	public:
		review_queue_application_host(
			queue_enrollment_store_port&   aStore,
			queue_refresh_effect_port&     aRefresh,
			effect_executor&               aEffects,
			decision_executor&             aDecisionLane,
			queue_landing_projection_port& aProjection,
			queue_session_id               aSession);
		/// \brief Constructs a profile owner with no permanent first-window dependency.
		/// \details All calls run on the decision lane. Ports and executors outlive the
		/// owner through quiescence. The optional profile observer receives each fresh
		/// publication once, never attachment replay. Window callbacks may attach/detach
		/// or issue gestures, but must keep the owner alive until the callback returns.
		review_queue_application_host(queue_enrollment_store_port& aStore, queue_refresh_effect_port& aRefresh,
									  effect_executor& aEffects, decision_executor& aDecisionLane, core::bpr_profile_id aProfile,
									  queue_window_context_port& aWindows, queue_session_id aSession,
									  std::shared_ptr<queue_landing_projection_port> aProfileObserver = { });

		/// \brief Attaches one exact live window and replays its latest accepted projection.
		[[nodiscard]] auto attach(core::bpr_window_context aContext, std::shared_ptr<queue_window_projection_port> aProjection)
			-> std::expected<queue_consumer_id, queue_attachment_failure>;
		/// \brief Retires only this attachment token; repeated or stale detach is harmless.
		void               detach(queue_consumer_id aConsumer);
		[[nodiscard]] auto attached_consumers() const noexcept -> std::size_t;
		/// \brief Reports callback failure or observer feedback exceeding 32 pending publications.
		/// \details A failing consumer is detached; a failing/overflowing observer is retired.
		/// Other live window consumers continue receiving the latest queue.
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

		~review_queue_application_host();

		review_queue_application_host(const review_queue_application_host&) = delete;
		auto operator=(const review_queue_application_host&)
			-> review_queue_application_host&                          = delete;
		review_queue_application_host(review_queue_application_host&&) = delete;
		auto operator=(review_queue_application_host&&)
			-> review_queue_application_host& = delete;

		void restore();
		void apply(queue_rule_intent aIntent);
		void apply(queue_preferences_intent aIntent);
		/// \brief Saves or forgets a repository; the store answers first.
		void apply(saved_repository_intent aIntent);
		/// \brief Normalizes and admits one exact-window/session addition through the same durable store.
		void apply(repository_addition_intent aIntent);
		/// \brief [[BPR-0660]]. Remembers or forgets one enterprise server, durably before it is shown.
		void apply(saved_server_intent aIntent);
		/// \brief [[BPR-0989]]. Applies one personal discovery answer.
		///
		/// \details An answer under a session other than the one adopted is
		/// refused. Changed records are stored together; a refused write
		/// restores the records as they were and is reported.
		void apply(queue_discovery_answer aAnswer);
		/// \brief Refreshes exact retained-row metadata without loading a review.
		void apply(queue_metadata_answer aAnswer);
		/// \brief [[BPR-0989]]. The reviewer's local follow; changes nothing on GitHub.
		void apply(queue_follow_intent aIntent);
		/// \brief [[BPR-1683]]. Ignores or unignores one Home row; changes nothing on GitHub.
		///
		/// \details The core decides from the row's retained enrollment; the
		/// store answers before the change is kept. An ignored pull request the
		/// queue later observes closed or off Home is pruned the same way.
		void apply(core::home_ignore_intent aIntent);
		/// \brief [[BPR-0989]]. Publishes the discovery status with the queue.
		void present_discovery(queue_discovery_status aStatus);
		void adopt(queue_session_id aSession);
		void refresh(
			const core::pull_request_ref&    aRef,
			core::queue_activity_observation aActivity = { });
		/// \brief Reserves profile-wide dispatch order for background and window observations.
		[[nodiscard]] auto begin_observation() -> std::uint64_t;
		void               cancel();
		void               shutdown();

		[[nodiscard]] auto rules() const noexcept
			-> std::span<const core::observer_rule>;
		[[nodiscard]] auto saved_repositories() const noexcept
			-> std::span<const saved_repository>;
		[[nodiscard]] auto saved_servers() const noexcept
			-> std::span<const saved_server>;
		[[nodiscard]] auto queue() const -> std::vector<core::queue_item>;
		/// \brief [[BPR-0989]]. Every retained record, departed ones included.
		[[nodiscard]] auto enrollments() const noexcept -> std::span<const enrollment_record>;
		/// \brief [[BPR-1683]]. The pull requests ignored on Home.
		[[nodiscard]] auto ignored() const noexcept -> const core::home_ignore_list&;
		/// \brief [[BPR-0989]]. The session refreshes and answers are issued under.
		[[nodiscard]] auto session() const noexcept -> const queue_session_id&;
		[[nodiscard]] auto preferences() const noexcept -> const queue_presentation_preferences&;
		[[nodiscard]] auto statistics() const noexcept
			-> review_queue_host_statistics;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP
