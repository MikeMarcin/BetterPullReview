#include "bpr/application/review_cycle_promotion.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		/// The largest file promotion reads back to prove a note's lines.
		constexpr auto maximum_promotion_file_bytes = std::uint64_t{ 16U * 1024U * 1024U };
		/// Reads of a draft that changed under one request before it is left unanswered.
		constexpr auto maximum_promotion_attempts = 3U;

		auto lower(std::string_view aText) -> std::string
		{
			auto result = std::string{ aText };
			std::ranges::transform(result, result.begin(), [](const char aCharacter) { return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter; });
			return result;
		}

		struct prepared_note
		{
			review_cycle_promotion_row                                          Row;
			std::expected<core::cycle_promotion, core::cycle_promotion_refusal> Promotion = std::unexpected(core::cycle_promotion_refusal::mismatch);
		};

		/// On the effect lane: the native plan the link came from, the bytes of
		/// each note's file that may prove its lines, and each promotion.
		auto prepare(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_pull_association& aLink, const std::vector<core::cycle_promotion_note>& aNotes)
			-> std::vector<prepared_note>
		{
			auto plan = std::optional<core::publication_plan>{ };
			if (aLink.Origin == core::cycle_pull_origin::published && aLink.Publication) {
				auto stored = aStore.load_publication(aSubject, *aLink.Publication, { });
				if (stored && *stored && (*stored)->Journal.Link) {
					plan = (*stored)->Plan;
				}
			}
			auto result = std::vector<prepared_note>{ };
			result.reserve(aNotes.size());
			for (const auto& note : aNotes) {
				auto file  = std::optional<std::vector<std::uint8_t>>{ };
				auto bytes = std::optional<std::span<const std::uint8_t>>{ };
				if (core::promotion_needs_file(aLink, plan ? &*plan : nullptr, note)) {
					const auto& anchor = std::get<core::cycle_source_anchor>(note.Primary);
					const auto* blob   = core::published_blob(*plan, anchor.Path);
					if (blob != nullptr && blob->Size <= maximum_promotion_file_bytes) {
						auto read = aStore.read_object({ .Digest = blob->Content, .Size = blob->Size }, { });
						if (read) {
							file  = std::move(*read);
							bytes = std::span<const std::uint8_t>{ *file };
						}
					}
				}
				auto prepared      = prepared_note{ .Row = { .Item = note.Item, .Primary = note.Primary, .Body = note.Body } };
				prepared.Promotion = core::promote_note(aSubject, aLink, plan ? &*plan : nullptr, note, bytes);
				if (prepared.Promotion) {
					prepared.Row.Source = prepared.Promotion->Source;
					prepared.Row.Lines  = prepared.Promotion->Stop.Destination;
				} else {
					prepared.Row.Refusal = prepared.Promotion.error();
				}
				result.push_back(std::move(prepared));
			}
			return result;
		}

		auto count_words(const std::size_t aCount) -> std::string
		{
			return aCount == 1U ? std::string{ "1 note" } : std::to_string(aCount) + " notes";
		}
	}

	auto promotion_note(const review_cycle_note_entry& aEntry) -> core::cycle_promotion_note
	{
		return { .Item = aEntry.Item, .Primary = aEntry.Primary, .Body = aEntry.Body };
	}

	auto names_pull_request(const tour_saved_target& aTarget, const core::cycle_pull_address& aPull) -> bool
	{
		return lower(aTarget.Hostname) == lower(aPull.Host) && lower(aTarget.Repository.Owner) == lower(aPull.Owner) && lower(aTarget.Repository.Name) == lower(aPull.Repository)
			&& aTarget.Number.value() == aPull.Number;
	}

	struct review_cycle_promoter::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(std::unique_ptr<review_cycle_store> aStore, review_cycle_tour_port& aTours, effect_executor& aEffects, decision_executor& aDecisions, publication aPublish)
			: Store{ std::move(aStore) }
			, Tours{ aTours }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Publish{ std::move(aPublish) }
		{
		}

		std::unique_ptr<review_cycle_store>               Store;
		review_cycle_tour_port&                           Tours;
		effect_executor&                                  Effects;
		decision_executor&                                Decisions;
		publication                                       Publish;
		std::optional<review_cycle_promotion_view>        View;
		std::optional<core::local_cycle_subject>          Subject;
		std::vector<core::cycle_pull_association>         Links;
		std::vector<core::cycle_promotion_note>           Notes;
		std::vector<std::optional<core::cycle_promotion>> Promotions;
		std::optional<tour_saved_target>                  Target;
		/// Rows still to add, in order, and the current one's attempts.
		std::vector<std::size_t> Pending;
		unsigned                 Attempts = 0;
		/// Each open and close starts a new generation; a late answer for an
		/// earlier one is dropped.
		std::uint64_t Generation  = 0;
		std::size_t   Outstanding = 0;
		bool          Closing     = false;

		void publish()
		{
			if (Publish && !Closing) {
				Publish(View);
			}
		}

		/// Runs `aWork` on the decision lane for this generation only.
		template <typename TWork>
		auto guarded(TWork aWork)
		{
			++Outstanding;
			return [self = shared_from_this(), generation = Generation, work = std::move(aWork)](auto aValue) mutable {
				self->Decisions.post([host = self, expected = generation, task = std::move(work), value = std::move(aValue)]() mutable {
					--host->Outstanding;
					if (host->Closing || expected != host->Generation || !host->View) {
						return;
					}
					task(*host, std::move(value));
				});
			};
		}

		void open(core::local_cycle_subject aSubject, std::vector<core::cycle_pull_association> aLinks, const std::vector<review_cycle_note_entry>& aNotes)
		{
			Subject = aSubject;
			Links   = std::move(aLinks);
			Notes.clear();
			for (const auto& entry : aNotes) {
				Notes.push_back(promotion_note(entry));
			}
			View = review_cycle_promotion_view{ .Link = Links.back() };
			find_tour();
		}

		/// Which linked pull request's tour, then the notes against it.
		void find_tour()
		{
			++Generation;
			Promotions.clear();
			Target.reset();
			Pending.clear();
			View->Rows.clear();
			View->Busy   = true;
			View->Target = review_cycle_promotion_target::looking;
			View->Status = "Reading the notes and finding the pull request's Review Tour…";
			publish();
			Tours.targets(guarded([](implementation& aSelf, std::expected<std::vector<tour_saved_target>, review_cycle_tour_failure> aTargets) {
				auto& view = *aSelf.View;
				if (!aTargets) {
					view.Busy   = false;
					view.Target = review_cycle_promotion_target::unavailable;
					view.Status = "The pull request's Review Tour could not be read. Nothing was added.";
					aSelf.publish();
					return;
				}
				// The most recent link whose pull request has a tour on this
				// profile; the latest link when none has one yet.
				auto matching = std::vector<tour_saved_target>{ };
				for (auto link = aSelf.Links.rbegin(); link != aSelf.Links.rend() && matching.empty(); ++link) {
					std::ranges::copy_if(*aTargets, std::back_inserter(matching), [&](const auto& aTarget) { return names_pull_request(aTarget, link->Address); });
					if (!matching.empty()) {
						view.Link = *link;
					}
				}
				// The account that read the pull request decides between accounts.
				if (matching.size() > 1U) {
					std::erase_if(matching, [&](const auto& aTarget) { return aTarget.Owner.Account != view.Link.Account; });
				}
				if (matching.size() == 1U) {
					aSelf.Target = matching.front();
				} else {
					view.Target = matching.empty() ? review_cycle_promotion_target::not_opened : review_cycle_promotion_target::ambiguous;
				}
				aSelf.prepare_notes();
			}));
		}

		void prepare_notes()
		{
			auto done = guarded([](implementation& aSelf, std::vector<prepared_note> aPrepared) {
				for (auto& prepared : aPrepared) {
					aSelf.View->Rows.push_back(std::move(prepared.Row));
					aSelf.Promotions.push_back(prepared.Promotion ? std::optional{ std::move(*prepared.Promotion) } : std::nullopt);
				}
				aSelf.read_tour();
			});
			try {
				Effects.post([self = shared_from_this(), subject = *Subject, link = View->Link, notes = Notes, finished = std::move(done)]() mutable {
					auto prepared = std::vector<prepared_note>{ };
					try {
						prepared = prepare(*self->Store, subject, link, notes);
					} catch (...) {
						prepared.clear();
					}
					finished(std::move(prepared));
				});
			} catch (...) {
				--Outstanding;
				View->Busy   = false;
				View->Target = review_cycle_promotion_target::unavailable;
				View->Status = "The notes could not be read. Nothing was added.";
				publish();
			}
		}

		void read_tour()
		{
			auto& view = *View;
			if (!Target) {
				view.Busy   = false;
				view.Status = view.Target == review_cycle_promotion_target::not_opened
					? "This pull request has no Review Tour on this profile yet. Open it signed in, which starts its tour, then add notes here."
					: "More than one account has a Review Tour for this pull request. Open it with the account you mean, then add notes here.";
				publish();
				return;
			}
			Tours.load(*Target, { }, guarded([](implementation& aSelf, const review_cycle_tour_result& aLoaded) {
				auto& shown = *aSelf.View;
				shown.Busy  = false;
				if (!aLoaded) {
					shown.Target = review_cycle_promotion_target::unavailable;
					shown.Status = "The pull request's Review Tour could not be read. Nothing was added.";
					aSelf.publish();
					return;
				}
				shown.Target = review_cycle_promotion_target::ready;
				aSelf.observe(aLoaded->Draft);
				shown.Status = "Choose the notes to add. Adding sends nothing to GitHub.";
				aSelf.publish();
			}));
		}

		/// The tour's head decides what its policy makes of each stop.
		void observe(const core::review_tour& aDraft)
		{
			auto&      view = *View;
			const auto head = aDraft.ObservedHead.value_or(aDraft.Head);
			view.TourHead   = head;
			for (std::size_t index = 0; index < view.Rows.size(); ++index) {
				view.Rows[index].Planned = Promotions[index] ? std::optional{ core::promoted_destination(Promotions[index]->Stop, head, aDraft.Base) } : std::nullopt;
			}
		}

		void choose(const core::cycle_feedback_item_id aItem, const bool aChosen)
		{
			if (!View || View->Busy) {
				return;
			}
			const auto found = std::ranges::find(View->Rows, aItem, [](const auto& aRow) { return aRow.Item.Id; });
			if (found == View->Rows.end() || !found->Source || found->Chosen == aChosen) {
				return;
			}
			found->Chosen = aChosen;
			publish();
		}

		void add()
		{
			if (!View || View->Busy || View->Target != review_cycle_promotion_target::ready || !Target) {
				return;
			}
			Pending.clear();
			for (std::size_t index = 0; index < View->Rows.size(); ++index) {
				auto& row = View->Rows[index];
				if (row.Chosen && Promotions[index]) {
					row.Outcome = review_cycle_promotion_outcome::none;
					Pending.push_back(index);
				}
			}
			if (Pending.empty()) {
				View->Status = "Choose at least one note to add.";
				publish();
				return;
			}
			View->Busy   = true;
			View->Status = "Adding " + count_words(Pending.size()) + " to the Review Tour…";
			publish();
			Attempts = 0;
			step();
		}

		void step()
		{
			if (Pending.empty()) {
				finish();
				return;
			}
			// The tour's receipts are read first: a request it already answered
			// (its answer lost, or its process killed) is never sent again.
			Tours.load(*Target, Promotions[Pending.front()]->RequestId, guarded([](implementation& aSelf, const review_cycle_tour_result& aLoaded) {
				if (!aLoaded) {
					aSelf.unanswered();
					return;
				}
				aSelf.observe(aLoaded->Draft);
				if (aLoaded->Receipt) {
					auto& row   = aSelf.View->Rows[aSelf.Pending.front()];
					row.Outcome = aLoaded->Receipt->Status <= static_cast<std::uint8_t>(tour_service_status::unchanged) ? review_cycle_promotion_outcome::already : review_cycle_promotion_outcome::refused;
					row.Chosen  = row.Outcome != review_cycle_promotion_outcome::already;
					aSelf.next();
					return;
				}
				aSelf.request(aLoaded->Draft);
			}));
		}

		void request(const core::review_tour& aDraft)
		{
			const auto  index     = Pending.front();
			const auto& promotion = *Promotions[index];
			auto        request   = tour_service_request{ };
			request.Command       = { .Draft = aDraft.Id, .Owner = aDraft.Owner, .ExpectedRevision = aDraft.Revision, .Kind = core::tour_command_kind::insert, .Value = promotion.Stop, .Position = aDraft.Stops.size() };
			request.Operation     = tour_service_operation::edit;
			request.Origin        = tour_presentation_origin::desktop;
			request.RequestId     = promotion.RequestId;
			request.Digest        = promotion.Digest;
			Tours.execute(*Target, std::move(request), guarded([read = aDraft.Revision](implementation& aSelf, const review_cycle_tour_result& aDone) {
				auto& row = aSelf.View->Rows[aSelf.Pending.front()];
				if (aDone) {
					// A receipt from before this read answers an earlier request: the stop was there already.
					row.Outcome = aDone->Receipt && aDone->Receipt->Revision <= read ? review_cycle_promotion_outcome::already : review_cycle_promotion_outcome::added;
					row.Chosen  = false;
					aSelf.observe(aDone->Draft);
					aSelf.next();
					return;
				}
				switch (aDone.error()) {
				case review_cycle_tour_failure::conflict:
					// Another edit came first; nothing was recorded for this request.
					if (++aSelf.Attempts < maximum_promotion_attempts) {
						aSelf.step();
						return;
					}
					aSelf.unanswered();
					return;
				case review_cycle_tour_failure::refused:
					row.Outcome = review_cycle_promotion_outcome::refused;
					aSelf.next();
					return;
				case review_cycle_tour_failure::unanswered: aSelf.unanswered(); return;
				}
			}));
		}

		void next()
		{
			Pending.erase(Pending.begin());
			Attempts = 0;
			step();
		}

		/// The current note's answer is unknown: stop, and say how to finish.
		void unanswered()
		{
			for (const auto index : Pending) {
				View->Rows[index].Outcome = review_cycle_promotion_outcome::unanswered;
			}
			Pending.clear();
			finish();
		}

		void finish()
		{
			auto&      view    = *View;
			const auto counted = [&](const review_cycle_promotion_outcome aOutcome) { return static_cast<std::size_t>(std::ranges::count(view.Rows, aOutcome, &review_cycle_promotion_row::Outcome)); };
			const auto added = counted(review_cycle_promotion_outcome::added), already = counted(review_cycle_promotion_outcome::already), refused = counted(review_cycle_promotion_outcome::refused),
					   open  = counted(review_cycle_promotion_outcome::unanswered);
			auto       words = std::string{ };
			if (added > 0U) {
				words = "Added " + count_words(added) + " to the Review Tour of #" + std::to_string(view.Link.Address.Number) + ". "
					+ (added == 1U ? "It waits" : "They wait") + " for you to complete " + (added == 1U ? "it" : "them") + " in the pull request; nothing was sent to GitHub.";
			}
			if (already > 0U) {
				words += std::string{ words.empty() ? "" : " " } + (already == 1U ? "1 note was" : std::to_string(already) + " notes were") + " already in the tour; nothing changed for "
					+ (already == 1U ? "it." : "them.");
			}
			if (refused > 0U) {
				words += std::string{ words.empty() ? "" : " " } + "The tour refused " + count_words(refused) + ": it may be full or read-only. Nothing else changed.";
			}
			if (open > 0U) {
				words += std::string{ words.empty() ? "" : " " } + "BPR did not hear whether the tour took " + count_words(open)
					+ ". Add Notes again: a note the tour already holds is never added twice.";
			}
			view.Busy   = false;
			view.Status = words.empty() ? "Nothing was added." : words;
			publish();
		}

		void close()
		{
			++Generation;
			View.reset();
			Pending.clear();
			Target.reset();
			publish();
		}
	};

	review_cycle_promoter::review_cycle_promoter(std::unique_ptr<review_cycle_store> aStore, review_cycle_tour_port& aTours, effect_executor& aEffects, decision_executor& aDecisions, publication aPublish)
		: Implementation{ std::make_shared<implementation>(std::move(aStore), aTours, aEffects, aDecisions, std::move(aPublish)) }
	{
	}

	review_cycle_promoter::~review_cycle_promoter()
	{
		Implementation->Closing = true;
	}

	void review_cycle_promoter::open(core::local_cycle_subject aSubject, std::vector<core::cycle_pull_association> aLinks, const std::vector<review_cycle_note_entry>& aNotes)
	{
		if (!Implementation->Closing && !aLinks.empty()) {
			Implementation->open(aSubject, std::move(aLinks), aNotes);
		}
	}

	void review_cycle_promoter::choose(const core::cycle_feedback_item_id aItem, const bool aChosen)
	{
		Implementation->choose(aItem, aChosen);
	}

	void review_cycle_promoter::add()
	{
		Implementation->add();
	}

	void review_cycle_promoter::refresh()
	{
		auto& self = *Implementation;
		if (!self.Closing && self.View && !self.View->Busy) {
			self.find_tour();
		}
	}

	void review_cycle_promoter::close()
	{
		Implementation->close();
	}

	auto review_cycle_promoter::view() const noexcept -> const std::optional<review_cycle_promotion_view>&
	{
		return Implementation->View;
	}

	void review_cycle_promoter::shutdown()
	{
		Implementation->Closing = true;
		++Implementation->Generation;
	}

	auto review_cycle_promoter::is_quiescent() const noexcept -> bool
	{
		return Implementation->Outstanding == 0U;
	}
}
