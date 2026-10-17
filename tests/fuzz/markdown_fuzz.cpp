// [[BPR-0970]]. Markdown: link admission from a rendered body, and the tour
// codec's structural mapping, edit, insertion and rendering of Markdown it
// did not write.
//
// Input: [selector] then the payload. Selector 0 admits the payload as a link
// destination under a fixed observation; 1 does the same with the payload's
// first two parts as hostname and owner; 3 reads the payload as a body's image
// markup and as GitHub's rendered HTML, and admits what it finds (RFC-BPR-0084);
// 4 scans the payload's fenced code, names each block's language, marks its
// lines as a diff and lays it out as a suggestion (RFC-BPR-0036 amendment 2);
// anything else maps the payload as a tour document.

#include "bpr/core/markdown_code.hpp"
#include "bpr/core/markdown_image.hpp"
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

	/// RFC-BPR-0084. Image markup, served HTML and the admissions over them.
	auto images(const std::string_view aRaw) -> void
	{
		const auto spans = core::scan_html_images(aRaw);
		fuzz::require(spans.size() <= core::markdown_image_references_per_body);
		auto previous = std::size_t{ 0 };
		for (const auto& span : spans) {
			// Spans are ordered, disjoint, start at a tag and never decode longer.
			fuzz::require(span.Begin >= previous && span.Begin < span.End && span.End <= aRaw.size() && aRaw[span.Begin] == '<');
			fuzz::require(span.Source.size() <= span.End - span.Begin && span.Alt.size() <= span.End - span.Begin);
			previous = span.End;
		}
		for (const auto& linked : core::markdown_linked_images(aRaw)) {
			fuzz::require(!linked.Source.empty() && !linked.Link.empty() && aRaw.find(linked.Link) != std::string_view::npos);
		}
		// RFC-BPR-0084 owner amendment 1. SVG admission never admits a
		// document that is not one or that names anything outside itself.
		if (const auto svg = core::admit_markdown_svg(aRaw); svg) {
			fuzz::require(core::markdown_svg_signature(aRaw) && aRaw.size() <= core::markdown_image_svg_bytes);
			auto lower = std::string{ aRaw };
			for (auto& character : lower) {
				character = character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) : character;
			}
			for (const auto* refused : { "<script", "<!entity", "@import", "url(http", "href=\"http", "href='http", "href=\"file", "href='file" }) {
				fuzz::require(lower.find(refused) == std::string::npos);
			}
		} else if (!core::markdown_svg_signature(aRaw)) {
			fuzz::require(svg.error() == core::markdown_image_state::unsupported);
		}
		const auto served = core::served_markdown_images(aRaw);
		fuzz::require(!served || served->size() <= core::markdown_image_served_tags_per_body);
		const auto dotcom = core::markdown_image_fetch_policy{ .Contract = core::markdown_image_host_contract::github_com, .Hostname = "github.com", .CredentialAvailable = true };
		const auto ghes   = core::markdown_image_fetch_policy{ .Contract = core::markdown_image_host_contract::ghes, .Hostname = "ghe.example.invalid", .CredentialAvailable = true };
		const auto check  = [&](const std::string_view aAddress) {
			for (const auto* policy : { &dotcom, &ghes }) {
				const auto hop = core::admit_markdown_image_fetch(aAddress, *policy);
				if (!hop) {
					continue;
				}
				// An admitted hop is HTTPS and printable; GitHub.com never sends the credential.
				auto scheme = std::string{ hop->Address.substr(0U, 8U) };
				for (auto& character : scheme) {
					character = character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) : character;
				}
				fuzz::require(scheme == "https://");
				fuzz::require(policy->Contract == core::markdown_image_host_contract::ghes || !hop->Credential);
				const auto next = core::admit_markdown_image_redirect(*hop, aRaw.substr(0U, 512U), *policy);
				if (next) {
					fuzz::require(next->Hop == hop->Hop + 1U && (hop->Credential || !next->Credential));
					fuzz::require(policy->Contract == core::markdown_image_host_contract::ghes || !next->Credential);
				}
			}
		};
		for (const auto& span : spans) {
			const auto source = core::admit_markdown_image_source(span.Source, "github.com");
			if (source && source->Kind == core::markdown_image_source_kind::attachment) {
				fuzz::require(source->Canonical.size() == 36U);
			}
			if (source && served) {
				if (const auto match = core::match_served_markdown_image(*source, *served)) {
					check(*match);
				}
			}
			check(span.Source);
		}
	}

	/// [[BPR-1562]]..[[BPR-1564]]. Fenced code: blocks in order and bounded,
	/// their lines inside the body, marks inside their line and ordered, and
	/// a suggestion's rows its two sides with changed words inside each row.
	auto fenced(const std::string_view aRaw, const std::uint8_t aChoice) -> void
	{
		const auto blocks = core::scan_markdown_fenced_code(aRaw);
		fuzz::require(blocks.size() <= core::markdown_code_blocks_per_body);
		auto total = std::size_t{ 0U };
		for (std::size_t index = 0U; index < blocks.size(); ++index) {
			const auto& block = blocks[index];
			fuzz::require(block.Ordinal == index && block.Info.size() <= aRaw.size());
			for (const auto& line : block.Lines) {
				fuzz::require(line.find('\n') == std::string::npos && line.size() <= aRaw.size());
				total += line.size();
			}
			fuzz::require(total <= aRaw.size());
			const auto language = core::markdown_code_language_of(block.Info);
			fuzz::require((language.Kind == core::markdown_code_kind::language) == !language.Selector.empty());
			for (const auto& line : block.Lines) {
				auto end = std::size_t{ 0U };
				for (const auto& mark : core::markdown_diff_marks(line)) {
					fuzz::require(mark.Begin >= end && mark.Length > 0U && mark.Begin + mark.Length <= line.size());
					end = mark.Begin + mark.Length;
				}
			}
			const auto context = (aChoice & 1U) != 0U
				? std::optional{ core::markdown_suggestion_context{ .FirstLine = 1U + aChoice, .Lines = { block.Lines.rbegin(), block.Lines.rend() } } }
				: std::nullopt;
			const auto rows    = core::markdown_suggestion_rows(block.Lines, context);
			fuzz::require(rows.size() == block.Lines.size() + (context ? context->Lines.size() : 0U));
			for (const auto& row : rows) {
				fuzz::require(row.ChangedBegin <= row.ChangedEnd && row.ChangedEnd <= row.Text.size());
				fuzz::require(row.Number.has_value() == context.has_value());
			}
		}
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
	switch (selector % 5U) {
	case 0U:
		admit(input.text(), "example.invalid", "owner");
		break;
	case 1U: {
		const auto [host, rest]         = input.split(fuzz::part_separator);
		const auto [owner, destination] = fuzz::fuzz_input{ reinterpret_cast<const std::uint8_t*>(rest.data()), rest.size() }.split(fuzz::part_separator);
		admit(destination, host, owner);
		break;
	}
	case 3U:
		images(input.text());
		break;
	case 4U:
		fenced(input.text(), choice);
		break;
	default:
		tour(input.text(), choice);
		break;
	}
	return 0;
}
