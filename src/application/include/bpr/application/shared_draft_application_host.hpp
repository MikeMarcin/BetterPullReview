#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SHARED_DRAFT_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SHARED_DRAFT_APPLICATION_HOST_HPP
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include <expected>
#include <functional>
#include <memory>
#include <vector>
namespace bpr::application {
	inline constexpr std::size_t maximum_shared_drafts          = 64;
	inline constexpr std::size_t maximum_shared_draft_consumers = 32;
	struct shared_draft_authority
	{
		core::bpr_profile_id             BprProfile;
		github_profile_authority         ProfileAuthority;
		core::host_authority_fingerprint HostAuthority;
		core::application_session_id     Session;
		core::github_identity_generation Generation;
		core::github_account_id          Account;
		core::github_access_mode         Mode;
		core::review_mutation_target     Target;
		auto                             operator==(const shared_draft_authority&) const -> bool = default;
	};
	using shared_draft_consumer = core::strong_id<struct shared_draft_consumer_tag>;
	enum class shared_draft_failure : std::uint8_t { invalid_authority,
													 retired,
													 capacity,
													 exhausted,
													 unknown_draft,
													 conflict,
													 policy,
													 closing,
													 durable_required };
	struct shared_draft_result
	{
		std::optional<core::revisioned_review_draft> Accepted = { };
		std::optional<shared_draft_failure>          Failure  = { };
		core::revisioned_draft_refusal               Refusal  = core::revisioned_draft_refusal::none;
	};
	class shared_draft_context_port
	{
	public:
		virtual ~shared_draft_context_port()                                                                = default;
		[[nodiscard]] virtual auto current(const core::bpr_window_context& aContext) const noexcept -> bool = 0;
	};
	/// Decision-lane owner. No persistence or publication effects. Keep the context port alive through shutdown.
	class shared_draft_application_host final
	{
	public:
		using publication = std::function<void(const core::revisioned_review_draft&)>;
		shared_draft_application_host(shared_draft_authority aAuthority, core::review_draft_scope_id aScope, shared_draft_context_port& aContexts);
		~shared_draft_application_host();
		shared_draft_application_host(const shared_draft_application_host&)                                  = delete;
		auto               operator=(const shared_draft_application_host&) -> shared_draft_application_host& = delete;
		[[nodiscard]] auto attach(const shared_draft_authority& aAuthority, core::bpr_window_context aWindow, publication aPublish) -> std::expected<shared_draft_consumer, shared_draft_failure>;
		void               detach(shared_draft_consumer aConsumer);
		[[nodiscard]] auto allocate(shared_draft_consumer aConsumer, core::source_line_ref aAnchor, std::string aBody = { }, std::optional<std::string> aReply = { }) -> shared_draft_result;
		/// Reuses an active draft at the exact anchor/reply target without changing its revision.
		[[nodiscard]] auto select(shared_draft_consumer aConsumer, core::source_line_ref aAnchor, std::optional<std::string> aReply = { }) -> shared_draft_result;
		[[nodiscard]] auto read(shared_draft_consumer aConsumer, core::review_draft_id aDraft) const -> shared_draft_result;
		[[nodiscard]] auto apply(shared_draft_consumer aConsumer, const core::revisioned_review_draft_command& aCommand) -> shared_draft_result;
		/// Host-only exact terminal path; it does not depend on an editor remaining attached.
		[[nodiscard]] auto complete(const shared_draft_authority& aAuthority, core::review_draft_id aDraft, std::uint64_t aPublishingRevision, bool aSuccess) -> shared_draft_result;
		/// \brief Adopts an acknowledged store transaction on the decision lane, then fans out.
		/// Durable records can only change through storage; ordinary synchronous apply refuses them.
		[[nodiscard]] auto adopt(const shared_draft_authority&, const durable_draft_commit&) -> std::expected<void, shared_draft_failure>;
		void               shutdown();
		[[nodiscard]] auto authority() const noexcept -> const shared_draft_authority&;
		/// \brief Captures retained private drafts for an orderly update; grants no publication authority.
		[[nodiscard]] auto recovery_snapshot() const -> std::vector<core::revisioned_review_draft>;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
