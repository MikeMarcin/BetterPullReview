#include "bpr/application/local_changes_host.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <utility>

namespace bpr::application {
	namespace {
		/// The largest side Local Changes compares or Edit opens past the editing bound.
		constexpr auto maximum_side_bytes = std::uint64_t{ 16U } * 1024U * 1024U;
		/// Lists longer than this keep their files and skip their line counts.
		constexpr auto maximum_counted_files = std::size_t{ 400U };

		using bytes = std::vector<std::uint8_t>;

		[[nodiscard]] auto compare(diff_line_matcher_port& aMatcher, const core::local_comparison aComparison, const std::string& aPath, const std::optional<bytes>& aLeft,
								   const std::optional<bytes>& aRight, std::stop_token aStop) -> core::material_diff_outcome
		{
			const auto options = core::diff_options{ };
			const auto left    = aLeft ? std::optional{ std::span<const std::uint8_t>{ *aLeft } } : std::nullopt;
			const auto right   = aRight ? std::optional{ std::span<const std::uint8_t>{ *aRight } } : std::nullopt;
			const auto key     = core::local_change_comparison(aComparison, aPath, left, right, options);
			if (!key) {
				return { .Reason = core::diff_reason_code::host_failure };
			}
			auto check    = stop_token_check{ aStop };
			auto matching = diff_line_matching{ aMatcher };
			auto run      = core::compute_material_diff({ .Key = *key, .Options = options, .LeftBytes = left, .RightBytes = right, .Limits = core::diff_compute_limits{ } },
														core::diff_compute_control{ .Stop = &check }, matching);
			if (!run.Outcome) {
				return { .Reason = core::diff_reason_code::host_failure };
			}
			return std::move(*run.Outcome);
		}

		[[nodiscard]] auto hunks_of(const core::material_diff_outcome& aOutcome) -> std::vector<core::local_change_hunk>
		{
			return aOutcome.Result ? core::change_hunks(*aOutcome.Result) : std::vector<core::local_change_hunk>{ };
		}

		/// What one observation of the worktree and GitHub's branch found.
		struct observation
		{
			core::local_worktree_status           Status;
			std::vector<core::local_status_entry> NotPushed;
			std::vector<core::local_status_entry> WholePullRequest;
			core::local_hosted_branch             Hosted;
			std::optional<std::string>            Url;
			std::optional<local_changes_failure>  Failure;
		};
	}

	auto name(const local_changes_operation aOperation) noexcept -> std::string_view
	{
		switch (aOperation) {
		case local_changes_operation::none:       return "none";
		case local_changes_operation::reading:    return "reading";
		case local_changes_operation::staging:    return "staging";
		case local_changes_operation::committing: return "committing";
		case local_changes_operation::pushing:    return "pushing";
		case local_changes_operation::checking:   return "checking";
		case local_changes_operation::suggesting: return "suggesting";
		case local_changes_operation::saving:     return "saving";
		}
		return "none";
	}

	struct local_changes_host::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(author_git_port& aGit, worktree_file_port& aFiles, std::shared_ptr<diff_line_matcher_port> aMatcher, effect_executor& aEffects, decision_executor& aDecisions,
					   local_changes_port& aPort, local_changes_consent aConsent)
			: Git{ aGit }
			, Files{ aFiles }
			, Matcher{ std::move(aMatcher) }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Port{ aPort }
			, Consent{ std::move(aConsent) }
		{
		}

		author_git_port&                        Git;
		worktree_file_port&                     Files;
		std::shared_ptr<diff_line_matcher_port> Matcher;
		effect_executor&                        Effects;
		decision_executor&                      Decisions;
		local_changes_port&                     Port;
		local_changes_consent                   Consent;
		local_commit_message_runner             Runner;
		effect_executor*                        AgentLane = nullptr;

		local_changes_projection Projection;
		std::uint64_t            Generation = 0U;
		std::size_t              Pending    = 0U;
		bool                     Stopped    = false;
		std::mutex               EffectMutex;
		std::stop_source         Stop;
		/// The press waiting for the one-time question.
		std::function<void()> Waiting;
		std::atomic_size_t    Outstanding{ 0U };

		void publish()
		{
			++Projection.Revision;
			Port.publish(Projection);
		}

		/// Runs `aEffect` on the effect lane, then `aThen` on the decision lane
		/// when the subject is still the one it ran for.
		template <typename TEffect, typename TThen>
		void run(const local_changes_operation aOperation, TEffect aEffect, TThen aThen, effect_executor* aLane = nullptr)
		{
			if (Stopped || !Projection.Active) {
				return;
			}
			auto self       = shared_from_this();
			auto generation = Generation;
			auto subjectNow = Projection.Subject;
			auto token      = Stop.get_token();
			++Pending;
			++Outstanding;
			Projection.Running = aOperation;
			publish();
			(aLane != nullptr ? *aLane : Effects).post([self, effect = std::move(aEffect), then = std::move(aThen), generation, subject = std::move(subjectNow), token]() mutable {
				auto result = [&] {
					const auto lock = std::scoped_lock{ self->EffectMutex };
					return effect(*self, subject, token);
				}();
				self->Decisions.post([host = self, finish = std::move(then), outcome = std::move(result), generation]() mutable {
					--host->Pending;
					--host->Outstanding;
					if (host->Stopped) {
						return;
					}
					if (host->Pending == 0U) {
						host->Projection.Running = local_changes_operation::none;
					}
					if (generation != host->Generation) {
						host->publish();
						return;
					}
					finish(*host, std::move(outcome));
				});
			});
		}

		// --- Observation (effect lane) ----------------------------------------------

		[[nodiscard]] auto url(const local_changes_subject& aSubject, std::stop_token aStop) -> std::string
		{
			if (!aSubject.Remote.empty()) {
				auto configured = Git.remote_url(aSubject.Clone.empty() ? aSubject.Worktree : aSubject.Clone, aSubject.Remote, aStop);
				// A local path names another clone, never GitHub: use the hosted URL.
				if (configured && *configured && !(*configured)->starts_with('/') && !(*configured)->starts_with('.') && !(*configured)->starts_with("file:")) {
					return **configured;
				}
			}
			return aSubject.FallbackUrl;
		}

		[[nodiscard]] auto side(const std::string& aWorktree, const std::optional<core::git_commit_oid>& aCommit, const std::string& aPath, std::stop_token aStop) -> std::optional<bytes>
		{
			auto blob = Git.read_blob(aWorktree, { .Commit = aCommit, .Path = aPath }, maximum_side_bytes, aStop);
			if (!blob) {
				return std::nullopt;
			}
			return std::move(*blob);
		}

		[[nodiscard]] auto disk(const std::string& aWorktree, const std::string& aPath) -> std::optional<bytes>
		{
			auto file = Files.read(aWorktree, aPath, maximum_side_bytes);
			return file && file->Exists && file->Regular ? std::optional{ std::move(file->Bytes) } : std::nullopt;
		}

		/// Counts each entry's lines against `aLeft` (a commit, or the index when none).
		void count(std::vector<core::local_status_entry>& aEntries, const std::string& aWorktree, const std::optional<core::git_commit_oid>& aLeft, const bool aRightIsIndex,
				   const core::local_comparison aComparison, std::stop_token aStop)
		{
			if (aEntries.size() > maximum_counted_files || !Matcher) {
				return;
			}
			for (auto& entry : aEntries) {
				if (aStop.stop_requested()) {
					return;
				}
				const auto left  = entry.Status == core::local_file_status::untracked ? std::nullopt : side(aWorktree, aLeft, entry.Original.empty() ? entry.Path : entry.Original, aStop);
				const auto right = aRightIsIndex ? side(aWorktree, std::nullopt, entry.Path, aStop) : disk(aWorktree, entry.Path);
				if (!left && !right) {
					continue;
				}
				const auto outcome = compare(*Matcher, aComparison, entry.Path, left, right, aStop);
				if (!outcome.Result || outcome.Result->Alignment.Content != core::diff_content_state::text) {
					entry.Binary = outcome.Result.has_value();
					continue;
				}
				const auto counts = core::line_counts(*outcome.Result);
				entry.Added       = counts.Added;
				entry.Removed     = counts.Removed;
			}
		}

		/// The paths that differ from a commit, untracked files included.
		[[nodiscard]] auto since(const std::string& aWorktree, const core::git_commit_oid& aCommit, const core::local_worktree_status& aStatus, std::stop_token aStop)
			-> std::vector<core::local_status_entry>
		{
			auto entries = std::vector<core::local_status_entry>{ };
			auto changed = Git.changed_since(aWorktree, aCommit, aStop);
			if (changed) {
				for (const auto& path : *changed) {
					entries.push_back({ .Path = path.Path, .Original = path.Original, .Status = path.Status });
				}
			}
			for (const auto& entry : aStatus.Unstaged) {
				if (entry.Status == core::local_file_status::untracked) {
					entries.push_back({ .Path = entry.Path, .Status = core::local_file_status::added });
				}
			}
			std::ranges::sort(entries, [](const auto& aLeft, const auto& aRight) { return aLeft.Path < aRight.Path; });
			return entries;
		}

		[[nodiscard]] auto hosted(const local_changes_subject& aSubject, const core::local_worktree_status& aStatus, const std::string& aUrl, std::stop_token aStop)
			-> std::pair<core::local_hosted_branch, std::optional<local_changes_failure>>
		{
			auto branch = core::local_hosted_branch{ .PullRequest = aSubject.PullRequest };
			if (aUrl.empty() || aSubject.Branch.empty()) {
				return { branch, std::nullopt };
			}
			auto tip = Git.hosted_tip(aSubject.Worktree, aUrl, aSubject.Branch, aStop);
			if (!tip) {
				return { branch, local_changes_failure{ .Operation = local_changes_operation::checking, .Failure = tip.error().Failure, .Message = tip.error().Message } };
			}
			branch.Observed = true;
			branch.Tip      = *tip;
			if (branch.Tip && aStatus.Head) {
				const auto known = Git.has_commit(aSubject.Worktree, *branch.Tip, aStop);
				branch.TipKnown  = known && *known;
				if (branch.TipKnown) {
					const auto ahead  = Git.count_commits(aSubject.Worktree, *aStatus.Head, *branch.Tip, aStop);
					const auto behind = Git.count_commits(aSubject.Worktree, *branch.Tip, *aStatus.Head, aStop);
					branch.Ahead      = ahead.value_or(0U);
					branch.Behind     = behind.value_or(0U);
				}
			}
			return { branch, std::nullopt };
		}

		[[nodiscard]] auto observe(const local_changes_subject& aSubject, const core::local_hosted_branch& aKnown, const bool aCheckHost, std::stop_token aStop) -> observation
		{
			auto found   = observation{ };
			auto status  = Git.status(aSubject.Worktree, aStop);
			found.Hosted = aKnown;
			if (!status) {
				found.Failure = local_changes_failure{ .Operation = local_changes_operation::reading, .Failure = status.error().Failure, .Message = status.error().Message };
				return found;
			}
			found.Status = std::move(*status);
			count(found.Status.Staged, aSubject.Worktree, found.Status.Head, true, core::local_comparison::uncommitted, aStop);
			count(found.Status.Unstaged, aSubject.Worktree, std::nullopt, false, core::local_comparison::uncommitted, aStop);
			if (aCheckHost) {
				found.Url              = url(aSubject, aStop);
				auto [branch, failure] = hosted(aSubject, found.Status, *found.Url, aStop);
				found.Hosted           = branch;
				found.Failure          = failure;
			} else if (found.Hosted.Observed && found.Hosted.Tip && found.Hosted.TipKnown && found.Status.Head) {
				// The tip is as last read; the head may have moved.
				found.Hosted.Ahead  = Git.count_commits(aSubject.Worktree, *found.Status.Head, *found.Hosted.Tip, aStop).value_or(found.Hosted.Ahead);
				found.Hosted.Behind = Git.count_commits(aSubject.Worktree, *found.Hosted.Tip, *found.Status.Head, aStop).value_or(found.Hosted.Behind);
			}
			if (found.Hosted.Tip && found.Hosted.TipKnown) {
				found.NotPushed = since(aSubject.Worktree, *found.Hosted.Tip, found.Status, aStop);
				count(found.NotPushed, aSubject.Worktree, found.Hosted.Tip, false, core::local_comparison::not_pushed, aStop);
			}
			if (aSubject.Base) {
				found.WholePullRequest = since(aSubject.Worktree, *aSubject.Base, found.Status, aStop);
				count(found.WholePullRequest, aSubject.Worktree, aSubject.Base, false, core::local_comparison::whole_pull_request, aStop);
			}
			return found;
		}

		[[nodiscard]] auto file(const local_changes_subject& aSubject, const core::local_comparison aComparison, const std::string& aPath, const core::local_worktree_status& aStatus,
								const core::local_hosted_branch& aHosted, std::stop_token aStop) -> local_changes_file
		{
			auto shown    = local_changes_file{ .Comparison = aComparison, .Path = aPath };
			auto worktree = Files.read(aSubject.Worktree, aPath, maximum_side_bytes);
			auto right    = std::optional<bytes>{ };
			auto facts    = core::local_edit_facts{ };
			if (worktree) {
				facts = { .Exists = worktree->Exists, .Regular = worktree->Regular, .Link = worktree->Link, .Inside = worktree->Inside, .Size = worktree->Bytes.size() };
				if (worktree->Exists && worktree->Regular) {
					right = worktree->Bytes;
				}
			} else {
				facts.Inside = worktree.error() != worktree_file_failure::outside;
				facts.Exists = worktree.error() != worktree_file_failure::missing;
				facts.Link   = worktree.error() == worktree_file_failure::link;
				facts.Size   = worktree.error() == worktree_file_failure::too_large ? maximum_side_bytes : 0U;
			}
			if (right && facts.Regular) {
				const auto filtered = Git.filtered(aSubject.Worktree, aPath, aStop);
				facts.Filtered      = filtered && *filtered;
			}
			shown.EditRefusal = core::decide_editable(facts, right ? std::span<const std::uint8_t>{ *right } : std::span<const std::uint8_t>{ });
			if (right) {
				shown.Disk = *right;
			}
			switch (aComparison) {
			case core::local_comparison::uncommitted:        shown.LeftCommit = aStatus.Head; break;
			case core::local_comparison::not_pushed:         shown.LeftCommit = aHosted.Tip; break;
			case core::local_comparison::whole_pull_request: shown.LeftCommit = aSubject.Base; break;
			}
			if (!shown.LeftCommit || !Matcher) {
				shown.Reason = core::diff_reason_code::host_failure;
				return shown;
			}
			const auto renamed  = std::ranges::find_if(aStatus.Staged, [&aPath](const auto& aEntry) { return aEntry.Path == aPath && !aEntry.Original.empty(); });
			const auto leftPath = renamed != aStatus.Staged.end() ? renamed->Original : aPath;
			const auto left     = side(aSubject.Worktree, shown.LeftCommit, leftPath, aStop);
			auto       outcome  = compare(*Matcher, aComparison, aPath, left, right, aStop);
			shown.Reason        = outcome.Reason;
			if (!outcome.Result) {
				return shown;
			}
			shown.Marks = core::line_marks(*outcome.Result);
			if (aComparison == core::local_comparison::uncommitted) {
				const auto index    = side(aSubject.Worktree, std::nullopt, aPath, aStop);
				const auto staged   = compare(*Matcher, core::local_comparison::uncommitted, aPath, left, index, aStop);
				const auto unstaged = compare(*Matcher, core::local_comparison::uncommitted, aPath, index, right, aStop);
				const auto showing  = core::change_hunks(*outcome.Result);
				shown.Bands         = core::uncommitted_bands(showing, hunks_of(staged), hunks_of(unstaged));
			}
			shown.Diff = std::make_shared<const core::computed_material_diff>(std::move(*outcome.Result));
			return shown;
		}

		// --- Decisions (decision lane) --------------------------------------------------

		void adopt(observation aFound, const std::optional<local_changes_operation> aAfter = std::nullopt)
		{
			Projection.Observed         = true;
			Projection.Status           = std::move(aFound.Status);
			Projection.NotPushed        = std::move(aFound.NotPushed);
			Projection.WholePullRequest = std::move(aFound.WholePullRequest);
			Projection.Hosted           = aFound.Hosted;
			if (aFound.Url) {
				Url = *aFound.Url;
			}
			Projection.NotPushedAvailable = Projection.Hosted.Tip.has_value() && Projection.Hosted.TipKnown;
			Projection.Push               = core::decide_push(Projection.Status, Projection.Hosted);
			Projection.HeadOnHost         = core::head_on_host(Projection.Status, Projection.Hosted);
			Projection.AmendOffered       = core::amend_offered(Projection.Status, Projection.HeadOnHost);
			Projection.Command            = Url.empty() ? std::nullopt : core::make_push_command(Projection.Subject.Worktree, Url, Projection.Subject.Branch);
			if (aFound.Failure) {
				Projection.Failure = aFound.Failure;
			} else if (aAfter) {
				Projection.Failure.reset();
			}
			reload_file();
		}

		void reload_file()
		{
			const auto path = Projection.File ? Projection.File->Path : first_path();
			if (path.empty()) {
				Projection.File.reset();
				publish();
				return;
			}
			show(path);
		}

		[[nodiscard]] auto first_path() const -> std::string
		{
			const auto& list = Projection.Comparison == core::local_comparison::uncommitted ? (Projection.Status.Unstaged.empty() ? Projection.Status.Staged : Projection.Status.Unstaged)
				: Projection.Comparison == core::local_comparison::not_pushed               ? Projection.NotPushed
																							: Projection.WholePullRequest;
			return list.empty() ? std::string{ } : list.front().Path;
		}

		void show(std::string aPath)
		{
			const auto comparison = Projection.Comparison;
			run(
				local_changes_operation::reading,
				[path = aPath, comparison, status = Projection.Status, hostedBranch = Projection.Hosted](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) {
					return aSelf.file(aSubject, comparison, path, status, hostedBranch, aStop);
				},
				[](implementation& aSelf, local_changes_file aFile) {
					aSelf.Projection.File = std::move(aFile);
					aSelf.publish();
				});
		}

		void observe_again(const bool aCheckHost, const std::optional<local_changes_operation> aAfter = std::nullopt)
		{
			run(
				aCheckHost ? local_changes_operation::checking : local_changes_operation::reading,
				[aCheckHost, known = Projection.Hosted](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) {
					return aSelf.observe(aSubject, known, aCheckHost, aStop);
				},
				[aAfter](implementation& aSelf, observation aFound) { aSelf.adopt(std::move(aFound), aAfter); });
		}

		/// A write, then a fresh observation; its failure is kept for the banner.
		template <typename TWrite>
		void write(const local_changes_operation aOperation, TWrite aWrite, const bool aCheckHost = false)
		{
			run(
				aOperation,
				[write = std::move(aWrite), aCheckHost, known = Projection.Hosted](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) {
					auto done  = write(aSelf, aSubject, aStop);
					auto found = aSelf.observe(aSubject, known, aCheckHost && done.has_value(), std::stop_token{ });
					return std::pair{ std::move(done), std::move(found) };
				},
				[aOperation](implementation& aSelf, std::pair<author_git_result<void>, observation> aOutcome) {
					auto& [done, found] = aOutcome;
					if (!done) {
						found.Failure = local_changes_failure{ .Operation = aOperation, .Failure = done.error().Failure, .Message = done.error().Message };
					}
					aSelf.adopt(std::move(found), aOperation);
				});
		}

		/// Runs `aPress` now when the subject is allowed, or asks first.
		void with_consent(std::function<void()> aPress)
		{
			if (Consent.Allowed && Consent.Allowed(Projection.Subject.ConsentKey)) {
				aPress();
				return;
			}
			Waiting                  = std::move(aPress);
			Projection.ConsentNeeded = true;
			publish();
		}

		std::string Url;
	};

	local_changes_host::local_changes_host(author_git_port& aGit, worktree_file_port& aFiles, std::shared_ptr<diff_line_matcher_port> aMatcher, effect_executor& aEffects,
										   decision_executor& aDecisions, local_changes_port& aPort, local_changes_consent aConsent)
		: Implementation{ std::make_shared<implementation>(aGit, aFiles, std::move(aMatcher), aEffects, aDecisions, aPort, std::move(aConsent)) }
	{
	}

	local_changes_host::~local_changes_host()
	{
		shutdown();
	}

	void local_changes_host::open(local_changes_subject aSubject)
	{
		auto& self = *Implementation;
		++self.Generation;
		self.Waiting    = { };
		self.Url        = aSubject.FallbackUrl;
		self.Projection = local_changes_projection{ .Active = true, .Subject = std::move(aSubject), .AgentAvailable = static_cast<bool>(self.Runner) };
		self.publish();
		// Opening reads the worktree only; GitHub is read on Check Again or Push.
		self.observe_again(false);
	}

	void local_changes_host::close()
	{
		auto& self = *Implementation;
		++self.Generation;
		self.Waiting    = { };
		self.Projection = local_changes_projection{ };
		self.publish();
	}

	void local_changes_host::refresh()
	{
		Implementation->observe_again(false);
	}

	void local_changes_host::choose(const core::local_comparison aComparison)
	{
		auto& self = *Implementation;
		if (self.Projection.Comparison == aComparison) {
			return;
		}
		self.Projection.Comparison = aComparison;
		self.Projection.File.reset();
		self.reload_file();
	}

	void local_changes_host::open_file(std::string aPath)
	{
		Implementation->show(std::move(aPath));
	}

	void local_changes_host::stage(std::vector<std::string> aPaths)
	{
		if (aPaths.empty()) {
			return;
		}
		Implementation->write(local_changes_operation::staging, [paths = std::move(aPaths)](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) {
			return aSelf.Git.stage_paths(aSubject.Worktree, paths, aStop);
		});
	}

	void local_changes_host::unstage(std::vector<std::string> aPaths)
	{
		if (aPaths.empty()) {
			return;
		}
		Implementation->write(local_changes_operation::staging, [paths = std::move(aPaths)](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) {
			return aSelf.Git.unstage_paths(aSubject.Worktree, paths, aStop);
		});
	}

	void local_changes_host::stage_change(std::string aPath, const core::local_change_hunk aChange)
	{
		Implementation->write(local_changes_operation::staging, [path = std::move(aPath), aChange](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) -> author_git_result<void> {
			// The index's text and the disk's, compared again now: a change that
			// is no longer one of them is refused, nothing written.
			const auto index = aSelf.side(aSubject.Worktree, std::nullopt, path, aStop);
			const auto disk  = aSelf.disk(aSubject.Worktree, path);
			if (!aSelf.Matcher) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::unavailable } };
			}
			const auto filtered = aSelf.Git.filtered(aSubject.Worktree, path, aStop);
			if (filtered && *filtered) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::failed, .Message = "This file has a Git filter, so only the whole file can be staged." } };
			}
			const auto outcome = compare(*aSelf.Matcher, core::local_comparison::uncommitted, path, index, disk, aStop);
			if (!outcome.Result) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::failed, .Message = "This file cannot be compared, so only the whole file can be staged." } };
			}
			const auto staged = core::apply_change(*outcome.Result, aChange);
			if (!staged) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::failed,
														  .Message = staged.error() == core::local_stage_refusal::stale ? "The file changed; nothing was staged." : "Only the whole file can be staged: its line endings differ." } };
			}
			return aSelf.Git.write_index_entry(aSubject.Worktree, path, *staged, aStop);
		});
	}

	void local_changes_host::unstage_change(std::string aPath, const core::local_change_hunk aChange)
	{
		Implementation->write(local_changes_operation::staging, [path = std::move(aPath), aChange](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) -> author_git_result<void> {
			auto status = aSelf.Git.status(aSubject.Worktree, aStop);
			if (!status) {
				return std::unexpected{ status.error() };
			}
			const auto head  = aSelf.side(aSubject.Worktree, status->Head, path, aStop);
			const auto index = aSelf.side(aSubject.Worktree, std::nullopt, path, aStop);
			if (!aSelf.Matcher || !index) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::failed, .Message = "Only the whole file can be unstaged." } };
			}
			const auto outcome = compare(*aSelf.Matcher, core::local_comparison::uncommitted, path, head, index, aStop);
			if (!outcome.Result) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::failed, .Message = "Only the whole file can be unstaged." } };
			}
			const auto reverted = core::revert_change(*outcome.Result, aChange);
			if (!reverted) {
				return std::unexpected{ author_git_error{ .Failure = author_git_failure::failed, .Message = "The staged change moved on; nothing was unstaged." } };
			}
			if (!head && reverted->empty()) {
				return aSelf.Git.unstage_paths(aSubject.Worktree, { path }, aStop);
			}
			return aSelf.Git.write_index_entry(aSubject.Worktree, path, *reverted, aStop);
		});
	}

	void local_changes_host::commit(core::local_commit_request aRequest)
	{
		auto& self    = *Implementation;
		auto  decided = core::decide_commit(self.Projection.Status, aRequest, self.Projection.HeadOnHost);
		if (!decided) {
			return;
		}
		self.with_consent([&self, accepted = std::move(*decided)] {
			self.write(local_changes_operation::committing, [plan = accepted](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) {
				return aSelf.Git.commit(aSubject.Worktree, plan, aStop);
			});
		});
	}

	void local_changes_host::push()
	{
		auto& self = *Implementation;
		if (!self.Projection.Command) {
			return;
		}
		if (!self.Projection.Push.Offered) {
			// Refused before anything runs: GitHub has commits the branch does not.
			const auto refused = self.Projection.Push.State == core::local_push_state::behind || self.Projection.Push.State == core::local_push_state::diverged
				|| self.Projection.Push.State == core::local_push_state::unknown_tip;
			if (refused) {
				self.Projection.Failure = local_changes_failure{ .Operation = local_changes_operation::pushing, .Failure = author_git_failure::failed, .RefusedBeforeRunning = true };
				self.publish();
			}
			return;
		}
		self.with_consent([&self, command = *self.Projection.Command] {
			self.write(
				local_changes_operation::pushing, [command](implementation& aSelf, const local_changes_subject&, std::stop_token aStop) { return aSelf.Git.push(command, aStop); }, true);
		});
	}

	void local_changes_host::check_again()
	{
		Implementation->observe_again(true);
	}

	void local_changes_host::answer_consent(const bool aAllow)
	{
		auto& self                    = *Implementation;
		self.Projection.ConsentNeeded = false;
		auto press                    = std::exchange(self.Waiting, { });
		if (!aAllow) {
			self.Projection.Declined = true;
			self.Projection.Commands.clear();
			self.Projection.Commands.push_back("git -C " + self.Projection.Subject.Worktree + " commit");
			if (self.Projection.Command) {
				self.Projection.Commands.push_back("git -C " + self.Projection.Subject.Worktree + " " + self.Projection.Command->Text.substr(4U));
			}
			self.publish();
			return;
		}
		self.Projection.Declined = false;
		if (self.Consent.Remember) {
			self.Consent.Remember(self.Projection.Subject.ConsentKey);
		}
		if (press) {
			press();
		} else {
			self.publish();
		}
	}

	void local_changes_host::save(std::string aPath, std::vector<std::uint8_t> aRead, std::vector<std::uint8_t> aEdited)
	{
		auto& self = *Implementation;
		self.run(
			local_changes_operation::saving,
			[path = aPath, read = std::move(aRead), edited = std::move(aEdited)](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token) {
				const auto now      = aSelf.disk(aSubject.Worktree, path).value_or(bytes{ });
				const auto decision = core::decide_save(read, now, edited);
				if (decision == core::local_save_decision::write) {
					const auto written = aSelf.Files.write(aSubject.Worktree, path, read, edited);
					if (!written) {
						return std::pair{ path, core::local_save_decision::changed_on_disk };
					}
				}
				return std::pair{ path, decision };
			},
			[](implementation& aSelf, std::pair<std::string, core::local_save_decision> aOutcome) {
				aSelf.Projection.ChangedOnDisk = aOutcome.second == core::local_save_decision::changed_on_disk ? aOutcome.first : std::string{ };
				aSelf.Projection.Saved         = aOutcome.second == core::local_save_decision::changed_on_disk ? std::string{ } : aOutcome.first;
				aSelf.observe_again(false);
			});
	}

	void local_changes_host::set_agent(local_commit_message_runner aRunner, effect_executor* const aLane)
	{
		auto& self                     = *Implementation;
		self.Runner                    = std::move(aRunner);
		self.AgentLane                 = aLane;
		self.Projection.AgentAvailable = static_cast<bool>(self.Runner);
		if (self.Projection.Active) {
			self.publish();
		}
	}

	void local_changes_host::suggest_message(local_commit_message_agent aAgent)
	{
		auto& self = *Implementation;
		if (!self.Runner || self.Projection.Status.Staged.empty()) {
			return;
		}
		self.Projection.SuggestFailure.clear();
		self.run(
			local_changes_operation::suggesting,
			[agent = std::move(aAgent)](implementation& aSelf, const local_changes_subject& aSubject, std::stop_token aStop) -> std::expected<core::commit_message_suggestion, std::string> {
				auto diff = aSelf.Git.staged_diff(aSubject.Worktree, core::maximum_commit_message_diff_bytes + 1U, aStop);
				if (!diff) {
					return std::unexpected{ std::string{ "The staged change could not be read." } };
				}
				const auto subjects = aSelf.Git.recent_subjects(aSubject.Worktree, core::maximum_commit_message_subjects, aStop);
				const auto prompt   = core::compose_commit_message_prompt({ .Instructions   = agent.Instructions,
																			.Branch         = aSubject.Branch,
																			.StagedDiff     = std::move(*diff),
																			.RecentSubjects = subjects.value_or(std::vector<std::string>{ }) });
				auto       answer   = aSelf.Runner(agent, prompt, aStop);
				if (!answer) {
					return std::unexpected{ answer.error() };
				}
				auto suggestion = core::read_commit_message_answer(*answer);
				if (!suggestion) {
					return std::unexpected{ std::string{ "The agent's answer had no subject." } };
				}
				return *suggestion;
			},
			[](implementation& aSelf, std::expected<core::commit_message_suggestion, std::string> aOutcome) {
				if (aOutcome) {
					aSelf.Projection.Suggestion = std::move(*aOutcome);
					++aSelf.Projection.SuggestionRevision;
				} else {
					aSelf.Projection.SuggestFailure = aOutcome.error();
				}
				aSelf.publish();
			},
			self.AgentLane);
	}

	void local_changes_host::stop()
	{
		auto& self = *Implementation;
		self.Stop.request_stop();
		self.Stop = std::stop_source{ };
	}

	auto local_changes_host::projection() const noexcept -> const local_changes_projection&
	{
		return Implementation->Projection;
	}

	void local_changes_host::shutdown()
	{
		if (!Implementation || Implementation->Stopped) {
			return;
		}
		Implementation->Stop.request_stop();
		Implementation->Stopped = true;
	}

	auto local_changes_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Outstanding.load() == 0U;
	}
}
