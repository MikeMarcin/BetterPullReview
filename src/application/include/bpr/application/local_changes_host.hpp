#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_CHANGES_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_CHANGES_HOST_HPP

#include "bpr/application/author_git_port.hpp"
#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/local_changes.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief Author state's host (RFC-BPR-0113 and RFC-BPR-0114 as amended
/// 2026-11-12, [[BPR-2062]], [[BPR-2065]], [[BPR-2066]]): Local Changes'
/// three comparisons, staging, the commit, the push, Check Again, Save and
/// the agent's commit message, for one window on the author's own worktree.
/// \details Every Git, file and agent call runs on the effect lane; every
/// projection is published on the decision lane. After any write the host
/// observes the worktree again and never repeats a write by itself. A late
/// completion for another subject is dropped.
namespace bpr::application {
	/// \brief What the window is on: the author's worktree and the branch's place on GitHub.
	struct local_changes_subject
	{
		std::string Worktree;
		std::string Branch;
		/// The clone whose remote names the hosted repository, and that remote.
		std::string Clone;
		std::string Remote;
		/// The hosted repository's URL when the remote's own cannot be read.
		std::string FallbackUrl;
		/// The target branch's name ("main") and the merge base Whole PR starts from.
		std::string                         Target;
		std::optional<core::git_commit_oid> Base;
		/// The pull request, when one exists.
		std::optional<std::uint64_t> PullRequest;
		/// Identifies the subject for the one-time question.
		std::string ConsentKey;
		auto        operator<=>(const local_changes_subject&) const = default;
	};

	/// \brief The file shown in one comparison.
	struct local_changes_file
	{
		core::local_comparison Comparison = core::local_comparison::uncommitted;
		std::string            Path;
		/// Null when a side could not be read or compared.
		std::shared_ptr<const core::computed_material_diff> Diff;
		core::diff_reason_code                              Reason = core::diff_reason_code::none;
		/// Uncommitted only: what each change's band offers.
		std::vector<core::local_change_band> Bands;
		/// The left side's commit, none for the index; the label's short form.
		std::optional<core::git_commit_oid> LeftCommit;
		/// The worktree's bytes, and why Edit refuses them.
		std::vector<std::uint8_t>               Disk;
		std::optional<core::local_edit_refusal> EditRefusal;
		std::vector<core::local_line_mark>      Marks;
		bool                                    operator==(const local_changes_file&) const = default;
	};

	enum class local_changes_operation : std::uint8_t {
		none,
		reading,
		staging,
		committing,
		pushing,
		checking,
		suggesting,
		saving,
	};
	[[nodiscard]] auto name(local_changes_operation aOperation) noexcept -> std::string_view;

	/// \brief What stopped, in Git's and any hook's words.
	struct local_changes_failure
	{
		local_changes_operation Operation = local_changes_operation::none;
		author_git_failure      Failure   = author_git_failure::failed;
		std::string             Message;
		/// The push was refused before anything ran: GitHub has commits the branch lacks.
		bool RefusedBeforeRunning                           = false;
		bool operator==(const local_changes_failure&) const = default;
	};

	struct local_changes_projection
	{
		bool                        Active = false;
		local_changes_subject       Subject;
		core::local_worktree_status Status;
		/// The worktree was read at least once.
		bool Observed = false;
		/// Not pushed and Whole PR, with their counts.
		std::vector<core::local_status_entry> NotPushed;
		std::vector<core::local_status_entry> WholePullRequest;
		/// Not pushed has a left side (GitHub's branch exists).
		bool                                    NotPushedAvailable = false;
		core::local_comparison                  Comparison         = core::local_comparison::uncommitted;
		std::optional<local_changes_file>       File;
		core::local_hosted_branch               Hosted;
		core::local_push_decision               Push;
		std::optional<core::local_push_command> Command;
		bool                                    AmendOffered = false;
		bool                                    HeadOnHost   = false;
		local_changes_operation                 Running      = local_changes_operation::none;
		std::optional<local_changes_failure>    Failure;
		/// Commit or Push was pressed before the one-time question was answered.
		bool ConsentNeeded = false;
		/// The question was declined: these are the commands to run instead.
		bool                     Declined = false;
		std::vector<std::string> Commands;
		/// The agent's last suggestion, which the fields take as an editable suggestion.
		std::optional<core::commit_message_suggestion> Suggestion;
		std::uint64_t                                  SuggestionRevision = 0U;
		std::string                                    SuggestFailure;
		bool                                           AgentAvailable = false;
		/// The last Save found the file changed on disk: Reload, nothing written.
		std::string ChangedOnDisk;
		/// A Save completed for this path.
		std::string   Saved;
		std::uint64_t Revision                                          = 0U;
		bool          operator==(const local_changes_projection&) const = default;
	};

	class local_changes_port
	{
	public:
		virtual ~local_changes_port()                                     = default;
		virtual void publish(const local_changes_projection& aProjection) = 0;
	};

	/// \brief The footer's agent for the commit message: one fresh, tool-free run.
	struct local_commit_message_agent
	{
		std::string   Agent;
		std::string   AgentName;
		std::uint64_t Profile = 0;
		std::string   Instructions;
	};
	/// \brief Runs one prompt; blocking, stoppable. Returns the answer text or a failure sentence.
	using local_commit_message_runner = std::function<std::expected<std::string, std::string>(const local_commit_message_agent&, const std::string&, std::stop_token)>;

	/// \brief The one-time question's answers, kept by the composition.
	struct local_changes_consent
	{
		std::function<bool(const std::string&)> Allowed;
		std::function<void(const std::string&)> Remember;
	};

	class local_changes_host final
	{
	public:
		local_changes_host(author_git_port& aGit, worktree_file_port& aFiles, std::shared_ptr<diff_line_matcher_port> aMatcher, effect_executor& aEffects, decision_executor& aDecisions,
						   local_changes_port& aPort, local_changes_consent aConsent);
		~local_changes_host();
		local_changes_host(const local_changes_host&)                    = delete;
		auto operator=(const local_changes_host&) -> local_changes_host& = delete;
		local_changes_host(local_changes_host&&)                         = delete;
		auto operator=(local_changes_host&&) -> local_changes_host&      = delete;

		/// \brief Enters author state for a worktree and reads it.
		void open(local_changes_subject aSubject);
		void close();
		void refresh();
		void choose(core::local_comparison aComparison);
		void open_file(std::string aPath);
		void stage(std::vector<std::string> aPaths);
		void unstage(std::vector<std::string> aPaths);
		void stage_change(std::string aPath, core::local_change_hunk aChange);
		void unstage_change(std::string aPath, core::local_change_hunk aChange);
		void commit(core::local_commit_request aRequest);
		void push();
		/// \brief Reads GitHub's branch through the author's Git.
		void check_again();
		/// \brief The one-time question's answer; a yes is remembered for the subject.
		void answer_consent(bool aAllow);
		/// \brief Writes the edited text when the disk still holds what was read.
		void save(std::string aPath, std::vector<std::uint8_t> aRead, std::vector<std::uint8_t> aEdited);
		void set_agent(local_commit_message_runner aRunner, effect_executor* aLane);
		void suggest_message(local_commit_message_agent aAgent);
		/// \brief Stops the running operation; what happened is then observed.
		void stop();

		[[nodiscard]] auto projection() const noexcept -> const local_changes_projection&;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
