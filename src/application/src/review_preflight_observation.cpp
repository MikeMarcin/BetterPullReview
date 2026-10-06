#include "bpr/application/review_preflight_observation.hpp"

#include "bpr/application/feedback_sources.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr std::size_t maximum_subject_bytes = 200U;

		/// The subject as written, else the first line, cut at a character.
		[[nodiscard]] auto subject_of(std::string_view aBody, const core::feedback_annotation* aAnnotation) -> std::string
		{
			auto text = aAnnotation != nullptr && aAnnotation->Subject ? core::feedback_text(aBody, *aAnnotation->Subject) : aBody;
			if (aAnnotation != nullptr && aAnnotation->UserBody && !aAnnotation->Subject) {
				text = core::feedback_text(aBody, *aAnnotation->UserBody);
			}
			text = text.substr(0U, text.find('\n'));
			while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) {
				text.remove_suffix(1U);
			}
			if (text.size() > maximum_subject_bytes) {
				auto end = maximum_subject_bytes;
				while (end > 0U && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
					--end;
				}
				text = text.substr(0U, end);
			}
			return std::string{ text };
		}

	} // namespace

	auto observe_review_preflight(const core::cached_review_key& aScope, const github_pull_request_snapshot& aSnapshot)
		-> review_preflight_observation
	{
		auto observation = review_preflight_observation{ .Snapshot  = aSnapshot.Snapshot,
														 .Account   = aSnapshot.Account,
														 .Head      = std::string{ aSnapshot.PullRequest.HeadOid.value() },
														 .Submitter = std::string{ aSnapshot.Viewer.Actor.value() } };
		// [[BPR-1087]]. The shared adapter; a tour index stays a top-level
		// comment here, as the preflight has always read it.
		const auto sources  = observe_feedback_sources(aSnapshot);
		const auto observed = feedback_post_observations(sources);
		auto       facts    = std::vector<core::feedback_conversation_facts>{ };
		auto       where    = std::map<std::string, std::string>{ };
		for (const auto& conversation : sources.Conversations) {
			observation.Authors.push_back({ .Conversation = conversation.Facts.Id, .RootAuthor = conversation.RootAuthor });
			facts.push_back(conversation.Facts);
			where.emplace(conversation.Facts.Id, conversation.Where);
		}
		auto refresh = core::refresh_feedback_annotations({ }, aScope, observed);
		if (refresh.Refusal != core::feedback_refresh_refusal::none) {
			return observation;
		}
		auto adopted = core::adopt_feedback_refresh({ }, std::move(refresh));
		if (!adopted) {
			return observation;
		}
		auto population = core::build_feedback_population(*adopted, facts, sources.Collection);
		if (population.Refusal != core::feedback_population_refusal::none) {
			return observation;
		}
		observation.Annotations = std::move(*adopted);
		observation.Population  = std::move(population.Population);
		observation.Admitted    = true;

		auto bodies = std::unordered_map<std::string_view, std::string_view>{ };
		for (const auto& post : sources.Posts) {
			bodies.emplace(post.Id, post.Body);
		}
		auto annotations = std::unordered_map<std::string_view, const core::feedback_annotation*>{ };
		for (const auto& post : observation.Annotations.Posts) {
			if (post.Annotation) {
				annotations.emplace(post.Id, &*post.Annotation);
			}
		}
		for (const auto& conversation : observation.Annotations.Conversations) {
			auto reference = review_preflight_reference{ .Where = where[conversation.Id] };
			if (conversation.Root) {
				const auto body       = bodies.find(*conversation.Root);
				const auto annotation = annotations.find(*conversation.Root);
				if (body != bodies.end()) {
					reference.Subject = subject_of(body->second, annotation == annotations.end() ? nullptr : annotation->second);
				}
			}
			observation.References.emplace(conversation.Id, std::move(reference));
		}
		return observation;
	}

	auto project_review_preflight(const review_preflight_observation&                 aObservation,
								  const std::span<const core::review_preflight_draft> aDrafts) -> std::optional<core::review_preflight>
	{
		if (!aObservation.Admitted) {
			return std::nullopt;
		}
		auto preflight = core::project_review_preflight(aObservation.Submitter, aObservation.Annotations, aObservation.Population, aObservation.Authors, aDrafts);
		if (preflight.Refusal != core::review_preflight_refusal::none) {
			return std::nullopt;
		}
		return preflight;
	}
}
