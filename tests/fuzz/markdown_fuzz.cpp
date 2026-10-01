// [[BPR-0970]]. Markdown: link admission from a rendered body, and the tour
// codec's structural mapping, edit, insertion and rendering of Markdown it
// did not write.
//
// Input: [selector] then the payload. Selector 0 admits the payload as a link
// destination under a fixed observation; 1 does the same with the payload's
// first two parts as hostname and owner; anything else maps the payload as a
// tour document.

#include "bpr/core/markdown_navigation.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "fuzz_support.hpp"

#include <string>
#include <vector>

namespace {
	using namespace bpr;

	auto admit(const std::string_view aDestination, const std::string_view aHostname, const std::string_view aOwner) -> void
	{
		const core::markdown_context context{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 3 } },
			core::application_session_id{ 4 },
			core::github_identity_generation{ 5 },
			core::provider_snapshot_id{ 6 },
			7,
		};
		core::markdown_link_observation observation{
			context, true, true, aHostname, aOwner, "repository", "2222222222222222222222222222222222222222", core::pull_request_number{ 42 }
		};
		const core::markdown_link_intent intent{ context, { }, std::string{ aDestination } };
		const auto                       admitted = core::admit_markdown_link(intent, observation);
		if (admitted) {
			const auto& target = admitted->Target;
			fuzz::require(!target.empty() && target.size() <= aDestination.size());
			switch (admitted->Kind) {
			case core::markdown_destination_kind::absolute_web:
			case core::markdown_destination_kind::pull_request_fragment:
				fuzz::require(target == aDestination);
				break;
			case core::markdown_destination_kind::repository_path: {
				// A relative destination resolves inside the repository root:
				// its path is not rooted, has no parent segment and no escape
				// left to decode. A query or fragment after it is kept as written.
				const auto path = target.substr(0U, target.find_first_of("?#"));
				fuzz::require(!path.empty() && !path.starts_with('/') && path.find('%') == std::string::npos);
				fuzz::require(("/" + path + "/").find("/../") == std::string::npos);
				break;
			}
			}
			// Nothing outside printable ASCII is ever admitted.
			for (const auto character : target) {
				fuzz::require(static_cast<unsigned char>(character) > 32U && static_cast<unsigned char>(character) < 127U);
			}
		}
		// The same gesture without a browser, or against a changed body, is
		// never admitted.
		observation.BrowserAvailable = false;
		const auto absent            = core::admit_markdown_link(intent, observation);
		fuzz::require(!absent && absent.error() == core::markdown_link_result::unavailable);
		observation.Context.reset();
		const auto stale = core::admit_markdown_link(intent, observation);
		fuzz::require(!stale && stale.error() == core::markdown_link_result::stale);
	}

	auto within(const core::tour_text_span aSpan, const std::string_view aRaw) -> bool
	{
		return aSpan.Begin <= aSpan.End && aSpan.End <= aRaw.size();
	}

	auto tour(const std::string_view aRaw, const std::uint8_t aChoice) -> void
	{
		const auto map = core::map_tour_markdown(aRaw);
		for (const auto& section : map.Sections) {
			fuzz::require(within(section.Content, aRaw));
		}
		for (const auto& list : map.Lists) {
			fuzz::require(within(list.Source, aRaw));
		}
		const auto stop = core::map_tour_stop(aRaw);
		for (const auto& span : { stop.Heading, stop.Narrative, stop.Footer }) {
			fuzz::require(!span || within(*span, aRaw));
		}

		// Replacing what the map found with a fixed text keeps every byte the
		// edit did not select.
		std::vector<core::tour_text_edit> edits;
		for (const auto& section : map.Sections) {
			if (edits.size() < 4U && (edits.empty() || section.Content.Begin >= edits.back().Span.End)) {
				const auto span = section.Content;
				edits.push_back({ span, std::string{ aRaw.substr(span.Begin, span.End - span.Begin) }, "replacement\n" });
			}
		}
		if (const auto edited = core::edit_tour_markdown(aRaw, edits); edited && !edits.empty()) {
			const auto& first = edits.front().Span;
			fuzz::require(edited->starts_with(aRaw.substr(0U, first.Begin)));
			fuzz::require(edited->ends_with(aRaw.substr(edits.back().Span.End)));
		}
		static_cast<void>(core::insert_tour_brief(aRaw, aChoice % (aRaw.size() + 1U), "problem", "solution", (aChoice & 1U) != 0U ? "\r\n" : "\n"));

		// Labels and links the map read are rendered back without injecting
		// Markdown, and the rendering maps again.
		const auto label = core::tour_markdown_label(aRaw.substr(0U, 256U));
		static_cast<void>(label);
		if (const auto index = core::render_tour_index(map.Links)) {
			static_cast<void>(core::map_tour_markdown(*index));
		}
		if (const auto rendered = core::render_tour_stop(stop.Title, aChoice, aRaw.substr(0U, 512U), map.Links)) {
			static_cast<void>(core::map_tour_stop(*rendered));
		}
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       selector = input.byte();
	const auto       choice   = input.byte();
	if (input.rest().size() > 16384U) {
		return 0;
	}
	switch (selector % 3U) {
	case 0U:
		admit(input.text(), "example.invalid", "owner");
		break;
	case 1U: {
		const auto [host, rest]         = input.split(fuzz::part_separator);
		const auto [owner, destination] = fuzz::fuzz_input{ reinterpret_cast<const std::uint8_t*>(rest.data()), rest.size() }.split(fuzz::part_separator);
		admit(destination, host, owner);
		break;
	}
	default:
		tour(input.text(), choice);
		break;
	}
	return 0;
}
