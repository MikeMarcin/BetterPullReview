#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <compare>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
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

		enrollment_departure Departed = enrollment_departure::none;

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

	/// \brief Retains enrollments and projects the queue from them.
	///
	/// \details Holds no provider, no clock, and no database. The durable store
	/// is a separate concern; this is the policy that decides what is retained
	/// and what the queue shows.
	class queue_enrollment_register final
	{
	public:
		/// \brief Enrolls or re-enrolls one pull request.
		///
		/// \details Enrolling something already present updates its membership
		/// only when it gained a GitHub role. A rule match must not overwrite a
		/// role, and a role must not overwrite the rule that first brought the
		/// item in — the reviewer is owed both reasons.
		void enroll(
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
		/// quiet.
		[[nodiscard]] auto project(
			std::span<const queue_activity_report> aActivity) const
			-> std::vector<core::queue_item>;

	private:
		[[nodiscard]] auto find(const core::pull_request_ref& aRef)
			-> enrollment_record*;

		std::vector<enrollment_record> Records;
	};

	/// \brief Names one durable queue-store failure without exposing SQLite.
	enum class queue_store_failure : std::uint8_t {
		unavailable,
		integrity_failure,
		cancelled,
		invalid_rule,
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

		auto operator<=>(const queue_landing_projection&) const = default;
	};

	class queue_landing_projection_port
	{
	public:
		virtual ~queue_landing_projection_port()                          = default;
		virtual void publish(const queue_landing_projection& aProjection) = 0;
	};

	struct review_queue_host_statistics
	{
		std::uint64_t RuleIntents            = 0U;
		std::uint64_t PreferenceIntents      = 0U;
		std::uint64_t SavedRepositoryIntents = 0U;
		std::uint64_t SavedServerIntents     = 0U;
		std::uint64_t PersistenceRequests    = 0U;
		std::uint64_t RefreshRequests        = 0U;
		std::uint64_t ConditionalRequests    = 0U;
		std::uint64_t AppliedRefreshes       = 0U;
		std::uint64_t StaleCompletions       = 0U;
		std::uint64_t CancelledCompletions   = 0U;
		std::uint64_t Publications           = 0U;
		std::uint64_t Failures               = 0U;

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
		/// \brief [[BPR-0660]]. Remembers or forgets one enterprise server, durably before it is shown.
		void apply(saved_server_intent aIntent);
		void adopt(queue_session_id aSession);
		void refresh(
			const core::pull_request_ref&    aRef,
			core::queue_activity_observation aActivity = { });
		void cancel();
		void shutdown();

		[[nodiscard]] auto rules() const noexcept
			-> std::span<const core::observer_rule>;
		[[nodiscard]] auto saved_repositories() const noexcept
			-> std::span<const saved_repository>;
		[[nodiscard]] auto saved_servers() const noexcept
			-> std::span<const saved_server>;
		[[nodiscard]] auto queue() const -> std::vector<core::queue_item>;
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
