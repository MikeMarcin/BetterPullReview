#include "github_application/window_composition.hpp"

#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/conversation_timeline.hpp"
#include "bpr/core/review_queue_attention.hpp"
#include "github_application/queue_notifications.hpp"

#include <algorithm>

namespace bpr::app {
	/// \brief RFC-BPR-0089. The ledger file a selected diff's receipt is
	/// kept under: its own for a commit-filtered diff.
	auto github_application::implementation::receipt_file(const application::workbench_selected_diff_publication& aPublication) -> core::file_identity
	{
		return aPublication.Key.Filter ? core::filtered_receipt_file(aPublication.Request.File.File, *aPublication.Key.Filter)
									   : aPublication.Request.File.File;
	}

	auto github_application::implementation::observe_read_state() -> std::optional<observed_read_state>
	{
		if (Closing || Host == nullptr || ReviewLedgerHost == nullptr) {
			return std::nullopt;
		}
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAvailable) {
				return std::nullopt;
			}
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (!snapshot.has_value()) {
			return std::nullopt;
		}
		auto threads = std::vector<application::conversation_thread_observation>{ };
		auto names   = std::vector<std::pair<std::string, std::vector<std::string>>>{ };
		threads.reserve(snapshot->ReviewThreads.size());
		for (const auto& thread : snapshot->ReviewThreads) {
			++ConversationThreadsVisited;
			auto observation = application::conversation_thread_observation{
				.Conversation = core::review_conversation_id{
					defect_identity(thread_identity(thread)).Conversation },
			};
			auto posts = std::vector<std::string>{ };
			for (const auto& comment : thread.Comments) {
				++ConversationPostsVisited;
				if (comment.Deleted) {
					continue;
				}
				observation.Posts.push_back(post_observation(comment, snapshot->Viewer.Login));
				posts.push_back(comment_projection_id(comment));
			}
			names.emplace_back(thread_identity(thread), std::move(posts));
			threads.push_back(std::move(observation));
		}
		const auto appendPost = [&](const auto& aPost) {
			++ConversationPostsVisited;
			const auto id = comment_projection_id(aPost);
			threads.push_back(application::conversation_thread_observation{
				.Conversation = core::review_conversation_id{ defect_identity(id).Conversation },
				.Posts        = { post_observation(aPost, snapshot->Viewer.Login) },
			});
			names.emplace_back(id, std::vector<std::string>{ id });
		};
		for (const auto& review : snapshot->Reviews) {
			// [[BPR-1779]]. A review with no body is no comment: it is
			// never unread content, so no count can hold what no gesture
			// reaches.
			if (core::review_body_is_comment(review.Body)) {
				appendPost(review);
			}
		}
		for (const auto& comment : snapshot->IssueComments) {
			if (!comment.Deleted) {
				appendPost(comment);
			}
		}
		const auto state = ReviewLedgerHost->unread_conversations(threads);
		if (!state.has_value()) {
			ReadStateCache.reset();
			NotePaths.reset();
			DefectThreadNames.reset();
			request_conversation_receipts();
			return std::nullopt;
		}
		// [[BPR-0972]]. Each unread conversation finds its threads, and each
		// unread post its names, by binary search over one ordering, where
		// both were a walk of every thread and every post.
		auto conversations = std::vector<std::pair<core::review_conversation_id, std::size_t>>{ };
		conversations.reserve(threads.size());
		for (std::size_t index = 0U; index < threads.size(); ++index) {
			conversations.emplace_back(threads[index].Conversation, index);
		}
		std::ranges::sort(conversations);
		auto  observed  = observed_read_state{ };
		auto& presented = observed.Presented;
		auto  cache     = read_state_cache{
			.Snapshot    = snapshot->Snapshot,
			.ThreadCount = snapshot->ReviewThreads.size(),
			.Unread      = std::vector<std::uint32_t>(threads.size(), 0U),
			.Counted     = std::vector<std::uint32_t>(threads.size(), 0U),
		};
		for (const auto& projection : state->Threads) {
			const auto matching = std::ranges::equal_range(conversations, projection.Conversation, { }, &std::pair<core::review_conversation_id, std::size_t>::first);
			for (const auto& [conversation, index] : matching) {
				auto entry = qt::thread_read_state{ .Thread = names[index].first };
				append_unread_names(threads[index], names[index].second, projection.UnreadPosts, entry.UnreadPosts);
				// The queue counts what is newer than what I read: a thread
				// I never read has no cursor to be newer than.
				if (projection.SeenBefore) {
					observed.UnreadPosts += static_cast<std::uint32_t>(entry.UnreadPosts.size());
					cache.Counted[index] += static_cast<std::uint32_t>(entry.UnreadPosts.size());
				}
				cache.Unread[index] = static_cast<std::uint32_t>(entry.UnreadPosts.size());
				if (index >= cache.ThreadCount) {
					observed.ConversationUnreadPosts += cache.Unread[index];
				}
				presented.push_back(std::move(entry));
			}
		}
		// A conversation shared by two threads is joined as a cross product
		// above; a single thread's change cannot be told apart from it, so
		// such a review keeps the complete observation.
		cache.Unique = std::ranges::adjacent_find(conversations, { }, &std::pair<core::review_conversation_id, std::size_t>::first) == conversations.end();
		for (std::size_t index = 0U; index < names.size(); ++index) {
			cache.ByIdentity.emplace_back(names[index].first, index);
		}
		std::ranges::sort(cache.ByIdentity);
		cache.ConversationUnreadPosts = observed.ConversationUnreadPosts;
		cache.UnreadPosts             = observed.UnreadPosts;
		cache.Threads                 = std::move(threads);
		cache.Names                   = std::move(names);
		ReadStateCache                = std::move(cache);
		return observed;
	}

	/// \brief The cached conversation index for an identity, if the cache
	/// still describes the accepted snapshot and names it exactly once.
	auto github_application::implementation::cached_conversation(const std::string_view aIdentity) const -> std::optional<std::size_t>
	{
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (!ReadStateCache.has_value() || !snapshot.has_value()
			|| ReadStateCache->Snapshot != snapshot->Snapshot) {
			return std::nullopt;
		}
		const auto& byIdentity = ReadStateCache->ByIdentity;
		const auto  found      = std::ranges::equal_range(byIdentity, aIdentity, std::less<>{ }, &std::pair<std::string, std::size_t>::first);
		return found.size() == 1U ? std::optional{ found.front().second } : std::nullopt;
	}

	/// \brief Observes one thread's read state again and publishes that
	/// thread alone.
	/// \returns The pull request's unseen count, or nothing when the whole
	/// observation must be made again.
	auto github_application::implementation::publish_thread_read_state(const std::size_t aThread) -> std::optional<std::uint32_t>
	{
		if (!ReadStateCache.has_value() || !ReadStateCache->Unique || Window == nullptr) {
			return std::nullopt;
		}
		auto&      cache = *ReadStateCache;
		const auto state = ReviewLedgerHost->unread_conversations(std::span{ &cache.Threads[aThread], 1U });
		if (!state.has_value()) {
			ReadStateCache.reset();
			NotePaths.reset();
			DefectThreadNames.reset();
			return std::nullopt;
		}
		++ConversationThreadsVisited;
		ConversationPostsVisited += cache.Threads[aThread].Posts.size();
		auto unread  = std::vector<std::string>{ };
		auto counted = std::uint32_t{ 0U };
		if (!state->Threads.empty()) {
			const auto& projection = state->Threads.front();
			append_unread_names(cache.Threads[aThread], cache.Names[aThread].second, projection.UnreadPosts, unread);
			counted = projection.SeenBefore ? static_cast<std::uint32_t>(unread.size()) : 0U;
		}
		if (aThread >= cache.ThreadCount) {
			cache.ConversationUnreadPosts = cache.ConversationUnreadPosts - cache.Unread[aThread] + static_cast<std::uint32_t>(unread.size());
		}
		cache.Unread[aThread] = static_cast<std::uint32_t>(unread.size());
		Window->present_new_conversation_comments(cache.ConversationUnreadPosts);
		cache.UnreadPosts      = cache.UnreadPosts - cache.Counted[aThread] + counted;
		cache.Counted[aThread] = counted;
		++ReadStateUpdates;
		Window->update_read_state(cache.Names[aThread].first, std::move(unread));
		return cache.UnreadPosts;
	}

	/// \brief Appends the presentation names of a thread's unread posts.
	/// \details Every post carrying an unread comment identity is named, in
	/// the unread order and then post order, as the nested walk did.
	void github_application::implementation::append_unread_names(
		const application::conversation_thread_observation& aThread,
		const std::vector<std::string>&                     aNames,
		const std::vector<core::review_comment_id>&         aUnread,
		std::vector<std::string>&                           aOut)
	{
		auto posts = std::vector<std::pair<core::review_comment_id, std::size_t>>{ };
		posts.reserve(aThread.Posts.size());
		for (std::size_t post = 0U; post < aThread.Posts.size(); ++post) {
			posts.emplace_back(aThread.Posts[post].Comment, post);
		}
		std::ranges::sort(posts);
		for (const auto unread : aUnread) {
			for (const auto& [comment, post] : std::ranges::equal_range(posts, unread, { }, &std::pair<core::review_comment_id, std::size_t>::first)) {
				aOut.push_back(aNames[post]);
			}
		}
	}

	auto github_application::implementation::publish_read_state() -> std::optional<std::uint32_t>
	{
		auto observed = observe_read_state();
		if (!observed.has_value() || Window == nullptr) {
			return std::nullopt;
		}
		Window->present_new_conversation_comments(observed->ConversationUnreadPosts);
		Window->present_read_state(std::move(observed->Presented));
		return observed->UnreadPosts;
	}

	/// \brief Tells the queue how many comments of the open pull request
	/// I have not seen, from the same read state the cards draw
	/// ([[BPR-0659]]).
	void github_application::implementation::refresh_queue_unseen(const std::uint32_t aUnseen)
	{
		if (Closing || !QueueActivity.has_value() || QueueHost == nullptr
			|| QueueRefresh == nullptr
			|| QueueActivity->Activity.UnseenComments == aUnseen) {
			return;
		}
		QueueActivity->Activity.UnseenComments = aUnseen;
		QueueRefresh->retain(QueueActivity->Session, QueueActivity->Observation);
		QueueHost->refresh(QueueActivity->Ref, QueueActivity->Activity);
	}
#if defined(BPR_BUILD_TESTING)
	/// \brief [[BPR-1293]]. An attended look's read state: the thread
	/// read through \p aComment exactly as Mark read records it at its
	/// latest post, so the posts after it are new.
	auto github_application::implementation::mark_conversation_read_through(const std::string& aThread, const std::string& aComment) -> bool
	{
		if (Closing || ReviewLedgerHost == nullptr || !WindowRead || !WindowRead->accepted_snapshot()) {
			return false;
		}
		const auto* const thread = accepted_thread(aThread);
		if (thread == nullptr) {
			return false;
		}
		auto seen = std::optional<core::conversation_post_observation>{ };
		for (const auto& comment : thread->Comments) {
			if ("review-comment:" + std::string{ comment.Comment.value() } == aComment) {
				seen = post_observation(comment, WindowRead->accepted_snapshot()->Viewer.Login);
			}
		}
		if (!seen.has_value()) {
			return false;
		}
		ReviewLedgerHost->mark_conversation_read(
			core::conversation_observation{
				.Conversation  = core::review_conversation_id{ defect_identity(thread_identity(*thread)).Conversation },
				.LatestComment = seen->Comment,
				.LatestEdit    = seen->Edit,
			},
			ledger_answer([this, aThread](const std::expected<void, application::ledger_store_failure>& aMarked) {
				if (!aMarked.has_value()) {
					return;
				}
				const auto cached = cached_conversation(aThread);
				auto       unseen = cached.has_value() ? publish_thread_read_state(*cached) : std::nullopt;
				if (!unseen.has_value()) {
					unseen = publish_read_state();
				}
				if (unseen.has_value()) {
					refresh_queue_unseen(*unseen);
				}
			}));
		return true;
	}
#endif
	auto github_application::implementation::accepted_read_conversation(const std::string& aIdentity) const
		-> std::optional<application::conversation_thread_observation>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot    = *WindowRead->accepted_snapshot();
		auto        observation = application::conversation_thread_observation{
			.Conversation = core::review_conversation_id{ defect_identity(aIdentity).Conversation },
		};
		if (const auto* thread = accepted_thread(aIdentity); thread != nullptr) {
			for (const auto& comment : thread->Comments) {
				if (!comment.Deleted) {
					observation.Posts.push_back(post_observation(comment, snapshot.Viewer.Login));
				}
			}
			return observation;
		}
		for (const auto& review : snapshot.Reviews) {
			if (comment_projection_id(review) == aIdentity) {
				if (!core::review_body_is_comment(review.Body)) {
					return std::nullopt;
				}
				observation.Posts.push_back(post_observation(review, snapshot.Viewer.Login));
				return observation;
			}
		}
		for (const auto& comment : snapshot.IssueComments) {
			if (!comment.Deleted && comment_projection_id(comment) == aIdentity) {
				observation.Posts.push_back(post_observation(comment, snapshot.Viewer.Login));
				return observation;
			}
		}
		return std::nullopt;
	}

	void github_application::implementation::mark_conversation_read(const std::string& aThread)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			return;
		}
		const auto observation = accepted_read_conversation(aThread);
		if (!observation.has_value() || observation->Posts.empty()) {
			return;
		}
		const auto& latest = observation->Posts.back();
		// [[BPR-1211]]. The cursor is written off the decision lane and
		// the thread is presented read once it is durable.
		ReviewLedgerHost->mark_conversation_read(
			core::conversation_observation{
				.Conversation  = observation->Conversation,
				.LatestComment = latest.Comment,
				.LatestEdit    = latest.Edit,
			},
			ledger_answer([this, aThread](const std::expected<void, application::ledger_store_failure>& aMarked) {
				if (!aMarked.has_value()) {
					return;
				}
				const auto cached = cached_conversation(aThread);
				auto       unseen = cached.has_value() ? publish_thread_read_state(*cached) : std::nullopt;
				if (!unseen.has_value()) {
					unseen = publish_read_state();
				}
				if (unseen.has_value()) {
					refresh_queue_unseen(*unseen);
				}
			}));
	}

	/// \brief [[BPR-1699]]. Every conversation of the accepted
	/// snapshot, as the read state observes them.
	auto github_application::implementation::accepted_read_conversations() const
		-> std::optional<std::vector<application::conversation_thread_observation>>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto& viewer   = snapshot.Viewer.Login;
		auto        threads  = std::vector<application::conversation_thread_observation>{ };
		threads.reserve(snapshot.ReviewThreads.size() + snapshot.Reviews.size() + snapshot.IssueComments.size());
		for (const auto& thread : snapshot.ReviewThreads) {
			auto& observation        = threads.emplace_back();
			observation.Conversation = core::review_conversation_id{ defect_identity(thread_identity(thread)).Conversation };
			for (const auto& comment : thread.Comments) {
				if (!comment.Deleted) {
					observation.Posts.push_back(post_observation(comment, viewer));
				}
			}
		}
		const auto appendPost = [&](const auto& aPost) {
			threads.push_back(application::conversation_thread_observation{
				.Conversation = core::review_conversation_id{ defect_identity(comment_projection_id(aPost)).Conversation },
				.Posts        = { post_observation(aPost, viewer) },
			});
		};
		for (const auto& review : snapshot.Reviews) {
			if (core::review_body_is_comment(review.Body)) {
				appendPost(review);
			}
		}
		for (const auto& comment : snapshot.IssueComments) {
			if (!comment.Deleted) {
				appendPost(comment);
			}
		}
		return threads;
	}

	/// \brief [[BPR-1699]]. Mark all comments as read, or all from
	/// one author: the core decides over every conversation of the pull
	/// request, the ledger host commits the receipts it changes as one
	/// durable change, and the read state is presented once that is
	/// durable. Local only: nothing reaches GitHub.
	void github_application::implementation::mark_conversations_read(const std::optional<std::string>& aAuthor)
	{
		if (Closing || ReviewLedgerHost == nullptr || Window == nullptr) {
			return;
		}
		const auto scope = core::conversation_read_scope{
			.Author = aAuthor.has_value() ? core::review_author_of(*aAuthor) : core::review_author_id{ },
		};
		auto threads = accepted_read_conversations();
		if (!threads.has_value() || (aAuthor.has_value() && scope.Author == core::review_author_id{ })) {
			return;
		}
		ReviewLedgerHost->mark_conversations_read(
			std::move(*threads),
			scope,
			ledger_answer([this, aAuthor](const std::expected<core::conversation_bulk_read, application::ledger_store_failure>& aRead) {
				if (Window == nullptr) {
					return;
				}
				if (!aRead.has_value()) {
					Window->present_conversations_read(std::nullopt, aAuthor);
					return;
				}
				if (const auto unseen = publish_read_state(); unseen.has_value()) {
					refresh_queue_unseen(*unseen);
				}
				Window->present_conversations_read(qt::conversations_read_result{ .Marked = aRead->MarkedPosts, .Kept = aRead->KeptPosts }, aAuthor);
			}));
	}

	/// \brief [[BPR-1582]]. Mark as unread: the core moves the seen cursor
	/// back so the latest post by someone else is new again, and the
	/// thread is presented unread once that is durable.
	void github_application::implementation::mark_conversation_unread(const std::string& aThread)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			return;
		}
		const auto observation = accepted_read_conversation(aThread);
		if (!observation.has_value()) {
			return;
		}
		ReviewLedgerHost->mark_conversation_unread(
			*observation,
			ledger_answer([this, aThread](const std::expected<void, application::ledger_store_failure>& aMarked) {
				if (!aMarked.has_value()) {
					return;
				}
				const auto cached = cached_conversation(aThread);
				auto       unseen = cached.has_value() ? publish_thread_read_state(*cached) : std::nullopt;
				if (!unseen.has_value()) {
					unseen = publish_read_state();
				}
				if (unseen.has_value()) {
					refresh_queue_unseen(*unseen);
				}
			}));
	}

	/// \brief [[BPR-1211]]. Loads the conversation receipts off the
	/// decision lane, once, and presents the read state when they arrive.
	/// \details A failed load is not retried here; the next change that
	/// needs the read state asks again.
	void github_application::implementation::request_conversation_receipts()
	{
		if (ReviewLedgerHost == nullptr || ConversationReceiptsLoading) {
			return;
		}
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAvailable) {
				return;
			}
		}
		ConversationReceiptsLoading = true;
		ReviewLedgerHost->load_conversation_receipts(ledger_answer([this](const std::expected<void, application::ledger_store_failure>& aLoaded) {
			ConversationReceiptsLoading = false;
			if (!aLoaded.has_value()) {
				return;
			}
			if (const auto unseen = publish_read_state(); unseen.has_value()) {
				refresh_queue_unseen(*unseen);
			}
		}));
	}

	void github_application::implementation::publish_overview(const bool aLoadReceipts)
	{
		if (Closing || Window == nullptr || Host == nullptr) {
			return;
		}
		OverviewHead                = EpochHost != nullptr && EpochHost->state().AcceptedRevision.has_value()
			? std::optional{ EpochHost->state().AcceptedRevision->Head }
			: std::nullopt;
		OverviewChangedPathsPending = false;
		const auto& accepted        = WindowRead->accepted_snapshot();
		// RFC-BPR-0089. While a commit filter applies, the Overview is of
		// its file set; an exact filtered diff's receipt is its own.
		const auto filtered         = accepted.has_value() ? CommitFilter.files() : std::nullopt;
		auto       filteredSnapshot = std::optional<application::github_pull_request_snapshot>{ };
		auto       filteredEnds     = std::map<std::string_view, core::filtered_diff_endpoints>{ };
		// M3. A split file's row is as far read as its least read diff.
		auto filteredSegments = std::map<std::string_view, std::vector<core::filtered_diff_endpoints>>{ };
		if (filtered.has_value()) {
			filteredSnapshot = *accepted;
			filteredSnapshot->Files.clear();
			filteredSnapshot->Files.reserve(filtered->Files.size());
			for (const auto& file : filtered->Files) {
				filteredSnapshot->Files.push_back(file.Record);
			}
			for (const auto& file : filtered->Files) {
				if (file.Filter.has_value()) {
					filteredEnds.emplace(file.Record.Path, *file.Filter);
				}
				if (file.Split && !file.Segments.empty()) {
					auto& ends = filteredSegments[file.Record.Path];
					for (const auto& segment : file.Segments) {
						ends.push_back(segment.Endpoints);
					}
				}
			}
		}
		const auto& snapshot = filteredSnapshot.has_value() ? filteredSnapshot : accepted;
		if (!snapshot.has_value()) {
			PublishedOverview.reset();
			OverviewReceipts.clear();
			FeedbackSnapshot.reset();
			FeedbackDrillDown.reset();
			Window->clear_overview();
			return;
		}
		// [[BPR-1089]]. The label buckets follow the snapshot before the
		// overview is presented, so its rows are counted from the same one.
		observe_feedback_counters();
		++OverviewBuilds;
		OverviewReceiptRevision.reset();
		OverviewReceipts.clear();
		auto context    = application::overview_context{ };
		using readiness = application::overview_readiness;
		// The first accepted projection can precede attachment of its store.
		const auto available      = ReviewLedgerHost != nullptr ? ReviewLedgerHost->available() : std::optional<bool>{ };
		context.ReceiptsReadiness = available == false ? readiness::unavailable : readiness::loading;
		context.PrivateReadiness  = context.ReceiptsReadiness;
		auto missing              = std::vector<core::file_identity>{ };
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (ReviewLedgerAvailable && ReviewLedgerHost != nullptr) {
				const auto revision = ReviewLedgerHost->durable_receipt_revision();
				context.Receipts.reserve(snapshot->Files.size());
				for (const auto& file : snapshot->Files) {
					++OverviewReceiptVisits;
					const auto identity = core::file_identity_of(file.Path);
					if (const auto split = filteredSegments.find(file.Path); split != filteredSegments.end()) {
						auto parts = std::vector<std::optional<core::file_receipt>>{ };
						for (const auto& segment : split->second) {
							const auto key  = core::filtered_receipt_file(identity, segment);
							const auto held = ReviewLedgerHost->cached_receipt(key);
							if (!held.has_value()) {
								missing.push_back(key);
							}
							parts.push_back(held.has_value() ? *held : std::nullopt);
						}
						if (const auto combined = core::combined_split_receipt(identity, parts)) {
							context.Receipts.push_back(*combined);
						}
						continue;
					}
					const auto ends = filteredEnds.find(file.Path);
					const auto key  = ends != filteredEnds.end() ? core::filtered_receipt_file(identity, ends->second) : identity;
					const auto held = ReviewLedgerHost->cached_receipt(key);
					if (!held.has_value()) {
						missing.push_back(key);
					} else if (held->has_value()) {
						auto receipt = **held;
						receipt.File = identity;
						context.Receipts.push_back(receipt);
					}
				}
				if (missing.empty()) {
					OverviewReceiptRevision   = revision;
					context.ReceiptsReadiness = readiness::ready;
				} else if (OverviewReceiptsFailed) {
					context.ReceiptsReadiness = readiness::unavailable;
				}
			}
		}
		// [[BPR-1211]]. Receipts the ledger does not hold yet are read off
		// the decision lane, in one operation; the overview is published
		// now from what is held and again when they arrive.
		if (aLoadReceipts && !missing.empty() && !OverviewReceiptsLoading) {
			OverviewReceiptsLoading   = true;
			OverviewReceiptsFailed    = false;
			context.ReceiptsReadiness = readiness::loading;
			ReviewLedgerHost->load_receipts(std::move(missing), ledger_answer([this](const std::expected<void, application::ledger_store_failure>& aLoaded) {
												OverviewReceiptsLoading = false;
												OverviewReceiptsFailed  = !aLoaded.has_value();
												// Publish failure without retrying this same failed load.
												publish_overview(aLoaded.has_value());
											}));
		}
		if (ReviewLedgerHost != nullptr) {
			if (auto privateState = ReviewLedgerHost->private_observation();
				privateState.has_value()) {
				context.Private          = std::move(*privateState);
				context.PrivateReadiness = readiness::ready;
			} else if (PrivateRecordsFailed) {
				context.PrivateReadiness = readiness::unavailable;
			}
		}
		OverviewReviewedHead = context.Private ? context.Private->LastReviewedHead : std::nullopt;
		context.Updates      = EpochRows.rows();
		context.Head         = OverviewHead;
		if (context.Private.has_value() && context.Private->LastReviewedHead.has_value()
			&& context.Head.has_value()
			&& *context.Head != *context.Private->LastReviewedHead) {
			auto paths = std::vector<std::filesystem::path>{ };
			paths.reserve(snapshot->Files.size());
			for (const auto& file : snapshot->Files) {
				paths.emplace_back(file.Path);
			}
			context.ChangedSinceReviewPaths = changed_since_review(
				*context.Private->LastReviewedHead, *context.Head, std::move(paths));
		}
		context.ProjectedResolutions    = projected_overview_resolutions(*snapshot);
		context.ProjectedReviewRequests = projected_overview_review_requests(*snapshot);
		// [[BPR-1852]]. The viewer's draft-state change counts until a read agrees.
		context.ProjectedDraft      = projected_draft_state(*snapshot);
		OverviewReviewRequests      = context.ProjectedReviewRequests;
		PublishedOverview           = application::project_pull_request_overview(*snapshot, context);
		OverviewResolutions         = std::move(context.ProjectedResolutions);
		OverviewResolutionsSnapshot = snapshot->Snapshot;
		if (filtered.has_value()) {
			PublishedOverview->CommitFilterComparisonFiles = accepted->Files.size();
			PublishedOverview->FilteredByCommits           = filtered->CommitFiltered;
			PublishedOverview->FileFilteredFiles           = filtered->FileFiltered;
			PublishedOverview->GeneratedFiles              = filtered->SetAside.size();
		}
		if (accepted.has_value()) {
			PublishedOverview->GeneratedAttributes         = CommitFilter.generated_attributes();
			PublishedOverview->GeneratedAttributesExceeded = CommitFilter.generated_attributes_exceeded();
		}
		if (OverviewReceiptRevision.has_value()) {
			OverviewReceipts.adopt(*PublishedOverview,
								   context.Private.has_value() && context.Private->LastReviewedHead.has_value(),
								   !context.Receipts.empty());
		}
		Window->publish(*PublishedOverview);
	}

	/// \brief [[BPR-1841]]. The threads the viewer resolved or reopened
	/// since the read, by GitHub thread id, as the core projects them.
	auto github_application::implementation::projected_overview_resolutions(const application::github_pull_request_snapshot& aSnapshot) const
		-> std::map<std::string, bool, std::less<>>
	{
		auto projected = std::map<std::string, bool, std::less<>>{ };
		if (ConversationOverlay.Resolutions.empty()) {
			return projected;
		}
		for (const auto& thread : aSnapshot.ReviewThreads) {
			const auto shown = core::project_thread_resolution(thread.Resolved, ConversationOverlay, thread_identity(thread));
			if (shown.Resolved != thread.Resolved) {
				projected.emplace(std::string{ thread.Thread.value() }, shown.Resolved);
			}
		}
		return projected;
	}

	/// \brief [[BPR-1841]]. A resolve moves the Overview's counts for its
	/// one thread, in place: the file's unresolved count and mine. A read
	/// the overview has not been projected from yet is left to that
	/// projection.
	void github_application::implementation::present_overview_resolutions()
	{
		if (Closing || Window == nullptr || !PublishedOverview || !WindowRead || !WindowRead->accepted_snapshot()
			|| OverviewResolutionsSnapshot != std::optional{ WindowRead->accepted_snapshot()->Snapshot }) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		auto        now      = projected_overview_resolutions(snapshot);
		if (now == OverviewResolutions) {
			return;
		}
		auto changed = false;
		for (const auto& thread : snapshot.ReviewThreads) {
			const auto id     = thread.Thread.value();
			const auto before = OverviewResolutions.find(id);
			const auto after  = now.find(id);
			if (before == OverviewResolutions.end() && after == now.end()) {
				continue;
			}
			const auto shown    = before != OverviewResolutions.end() ? before->second : thread.Resolved;
			const auto resolved = after != now.end() ? after->second : thread.Resolved;
			changed             = application::apply_thread_resolution(*PublishedOverview, snapshot, thread, shown, resolved) || changed;
		}
		OverviewResolutions = std::move(now);
		if (changed) {
			++OverviewResolutionUpdates;
			Window->publish(*PublishedOverview);
		}
	}

	/// \brief [[BPR-1089]]. Adopts a newly accepted snapshot into the label
	/// buckets, and republishes the Overview's counts and any listed count.
	void github_application::implementation::observe_feedback_counters()
	{
		if (Closing || Window == nullptr || !WindowRead || !LastAcceptedHandoff) {
			return;
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (!snapshot.has_value() || FeedbackSnapshot == std::optional{ snapshot->Snapshot }) {
			return;
		}
		FeedbackSnapshot     = snapshot->Snapshot;
		const auto& handoff  = *LastAcceptedHandoff;
		const auto  observed = FeedbackCounters.observe(
			core::cached_review_key{ handoff.HostAuthority.bytes(), handoff.Profile, handoff.Host, handoff.Account, handoff.Locator, handoff.Number }, *snapshot);
		// [[BPR-1094]]. Counted here as well as in the model, so the
		// evidence would show a model made afresh.
		++FeedbackObservations;
		FeedbackBodiesParsed += observed.Reparsed;
		if (!observed.Admitted) {
			return;
		}
		// [[BPR-1293]]. Threads' label filter counts as the Overview's
		// chips do, from the same kept population.
		Window->set_conversation_labels(application::project_conversation_labels(FeedbackCounters));
		publish_overview_feedback();
		// A listed count follows the accepted population; the reviewer's
		// place in Discuss stays where it is.
		if (FeedbackDrillDown) {
			if (Window->conversation_listing_active()) {
				open_feedback_drill_down(FeedbackDrillDown->first, FeedbackDrillDown->second, false);
			} else {
				FeedbackDrillDown.reset();
			}
		}
	}

	auto github_application::implementation::overview_file_paths() const -> std::vector<std::string>
	{
		auto paths = std::vector<std::string>{ };
		if (WindowRead && WindowRead->accepted_snapshot()) {
			paths.reserve(WindowRead->accepted_snapshot()->Files.size());
			for (const auto& file : WindowRead->accepted_snapshot()->Files) {
				paths.push_back(file.Path);
			}
		}
		return paths;
	}

	void github_application::implementation::publish_overview_feedback()
	{
		if (Closing || Window == nullptr || !FeedbackRequest || !WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		if (auto projection = application::project_overview_feedback(FeedbackCounters, overview_file_paths(), *FeedbackRequest)) {
			++OverviewFeedbackPublications;
			Window->publish(*projection);
		}
	}

	void github_application::implementation::open_feedback_drill_down(const application::overview_drill_down& aDrillDown, const application::overview_feedback_request& aRequest, const bool aReveal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto listing = application::list_overview_drill_down(FeedbackCounters, overview_file_paths(), aRequest, aDrillDown);
		if (!listing) {
			return;
		}
		FeedbackDrillDown = std::pair{ aDrillDown, aRequest };
		Window->list_conversations(*listing, aReveal);
	}

	/// \brief Brings the published overview up to the ledger's durable receipts.
	///
	/// \details [[BPR-0972]]. When the ledger names the few files that moved
	/// since the overview last read it, only their rows, the progress and
	/// the one action are updated and published. Anything it cannot name,
	/// or a receipt that vanished, is answered by projecting again.
	void github_application::implementation::refresh_overview_receipts()
	{
		if (Closing || Window == nullptr || ReviewLedgerHost == nullptr) {
			return;
		}
		const auto revision = ReviewLedgerHost->durable_receipt_revision();
		if (OverviewReceiptRevision == std::optional{ revision }) {
			return;
		}
		constexpr auto largest = std::size_t{ 64U };
		const auto     changed = OverviewReceiptRevision.has_value() && PublishedOverview.has_value()
			? ReviewLedgerHost->durable_changes_since(*OverviewReceiptRevision)
			: std::nullopt;
		if (!changed.has_value() || changed->size() > largest) {
			publish_overview();
			return;
		}
		auto receipts = std::vector<core::file_receipt>{ };
		auto missing  = std::vector<core::file_identity>{ };
		receipts.reserve(changed->size());
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAvailable) {
				return;
			}
			for (const auto& file : *changed) {
				const auto held = ReviewLedgerHost->cached_receipt(file);
				if (!held.has_value()) {
					missing.push_back(file);
				} else if (held->has_value()) {
					receipts.push_back(**held);
				}
			}
		}
		if (!missing.empty()) {
			// [[BPR-1211]]. A sibling moved receipts this ledger no longer
			// holds: they are read off the decision lane, then the rows move.
			if (!OverviewReceiptsLoading) {
				OverviewReceiptsLoading = true;
				ReviewLedgerHost->load_receipts(std::move(missing), ledger_answer([this](const std::expected<void, application::ledger_store_failure>& aLoaded) {
													OverviewReceiptsLoading = false;
													if (aLoaded.has_value()) {
														refresh_overview_receipts();
													}
												}));
			}
			return;
		}
		const auto update = receipts.size() == changed->size()
			? OverviewReceipts.apply(*PublishedOverview, receipts)
			: std::nullopt;
		if (!update.has_value()) {
			publish_overview();
			return;
		}
		OverviewReceiptRevision = revision;
		++OverviewReceiptUpdates;
		OverviewReceiptRows += update->Rows.size();
		Window->publish(*update);
	}

	// [[BPR-0654]]. Answers from the kept resolution when its inputs are
	// the ones asked about; otherwise asks the epoch host once and answers
	// "not yet". The host looks each file up at both heads on the review's
	// cache actor, beside the selected comparison, and the completion
	// republishes the overview with the exact set. A refusal before
	// admission and a stale answer (the cache not yet serving the accepted
	// head) are asked again on the next publish; a failure is not, until an
	// input moves.
	auto github_application::implementation::changed_since_review(
		const core::git_commit_oid&        aReviewed,
		const core::git_commit_oid&        aHead,
		std::vector<std::filesystem::path> aPaths)
		-> std::optional<std::vector<std::string>>
	{
		auto authority = std::optional<application::workbench_pull_request_authority>{ };
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			authority       = ReviewLedgerAuthority;
		}
		if (EpochHost == nullptr || !authority.has_value()) {
			return std::nullopt;
		}
		auto request = application::changed_paths_request{
			.WorkbenchAuthority = *authority,
			.Left               = aReviewed,
			.Right              = aHead,
			.Paths              = std::move(aPaths),
		};
		if (ChangedSinceReview.has_value() && ChangedSinceReview->Request == request) {
			return ChangedSinceReview->Changed;
		}
		ChangedSinceReview  = changed_since_review_state{ .Request = request };
		const auto admitted = EpochHost->resolve_changed_paths(
			request,
			[this](const application::changed_paths_observation& aObservation) {
				if (Closing || !ChangedSinceReview.has_value()
					|| ChangedSinceReview->Request != aObservation.Request) {
					return;
				}
				if (aObservation.Status == application::changed_paths_status::stale) {
					ChangedSinceReview.reset();
					OverviewChangedPathsPending = true;
					return;
				}
				if (aObservation.Status != application::changed_paths_status::resolved) {
					return;
				}
				auto changed = std::vector<std::string>{ };
				changed.reserve(aObservation.Changed.size());
				for (const auto& path : aObservation.Changed) {
					changed.push_back(path.generic_string());
				}
				ChangedSinceReview->Changed = std::move(changed);
				publish_overview();
			});
		if (!admitted) {
			ChangedSinceReview.reset();
			OverviewChangedPathsPending = true;
		}
		return std::nullopt;
	}

	void github_application::implementation::publish(const application::receipt_projection& aProjection)
	{
		auto authority = std::optional<
			application::workbench_pull_request_authority>{ };
		auto actionabilityChanged = false;
		auto nowActionable        = true;
		{
			const auto lock      = std::scoped_lock{ ReviewLedgerStateMutex };
			nowActionable        = core::actionable(core::reason_for(
				core::actionability_input{ .Receipt = aProjection.Receipt }));
			actionabilityChanged = ReviewLedgerReceipt.has_value()
				? ReviewLedgerReceipt->File != aProjection.Receipt.File
					|| core::actionable(core::reason_for(core::actionability_input{
						   .Receipt = *ReviewLedgerReceipt,
					   }))
						!= nowActionable
				: !nowActionable;
			ReviewLedgerReceipt  = aProjection.Receipt;
			authority            = ReviewLedgerAuthority;
		}
		if (Window != nullptr) {
			Window->publish(application::project_file_receipt_actions(
				aProjection.Receipt));
			// [[BPR-0972]]. The overview shows durable receipts only. A
			// selection's in-memory receipt, or a reload that found nothing
			// new, leaves them as the last overview read them; a commit moves
			// only its own rows.
			// RFC-BPR-0089 M3. A split file's row combines its diffs' receipts.
			const auto split = WorkbenchReview.split_receipt(aProjection.Receipt.File);
			if (ReviewLedgerHost == nullptr || OverviewChangedPathsPending || split) {
				publish_overview();
			} else {
				refresh_overview_receipts();
			}
			present_split_strip();
			if (split) {
				return;
			}
		}
		if (WorkbenchHost != nullptr && authority.has_value()
			&& actionabilityChanged) {
			// [[BPR-0972]]. The retained receipt names one file; the host
			// moves that file and restores any it withheld, in place.
			WorkbenchHost->retain_actionability(*authority, WorkbenchReview.receipt_owner(aProjection.Receipt.File), nowActionable);
		}
	}

	void github_application::implementation::publish(const application::queue_landing_projection& aProjection)
	{
		if (Closing || !Window) {
			return;
		}
		Window->publish(aProjection);
		synchronize_setup_servers(aProjection);
	}

	void github_application::implementation::fail(
		const application::receipt_failure_publication& aFailure)
	{
		if (Window != nullptr) {
			Window->fail(aFailure);
		}
	}
}
