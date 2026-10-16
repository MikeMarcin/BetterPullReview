#include "bpr/core/markdown_image.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto pixels(const std::uint32_t aValue) -> markdown_image_length
	{
		return { .Unit = markdown_image_length::unit::pixels, .Value = aValue };
	}

	auto percent(const std::uint32_t aValue) -> markdown_image_length
	{
		return { .Unit = markdown_image_length::unit::percent, .Value = aValue };
	}

	auto attachment_uuid() -> std::string { return "25addea2-357a-42c0-a1c8-fba0163382cc"; }

	void lengths()
	{
		expect(parse_markdown_image_length("40") == pixels(40), "bare pixels");
		expect(parse_markdown_image_length(" 40px ") == pixels(40), "px suffix and spaces");
		expect(parse_markdown_image_length("40PX") == pixels(40), "px is case-insensitive");
		expect(parse_markdown_image_length("50%") == percent(50), "percentage");
		expect(parse_markdown_image_length("250%") == percent(100), "percentage capped at 100");
		expect(parse_markdown_image_length("99999") == pixels(markdown_image_side_pixels), "pixels capped at the largest side");
		expect(parse_markdown_image_length("12.5") == pixels(12), "fraction dropped");
		for (const auto* text : { "", "0", "0%", "-4", "abc", "4em", "4 px x", ".5", "12.", "1234567", "%" }) {
			expect(!parse_markdown_image_length(text), std::string{ "no hint from " } + text);
		}
	}

	void tag_scanning()
	{
		// GitHub's own attachment markup, as the drag-and-drop editor writes it.
		const auto upload = std::string{ "Before <img width=\"1252\" height=\"625\" alt=\"Image\" src=\"https://github.com/user-attachments/assets/" } + attachment_uuid() + "\" /> after";
		auto       spans  = scan_html_images(upload);
		expect(spans.size() == 1U, "one upload image");
		expect(spans[0].Source == "https://github.com/user-attachments/assets/" + attachment_uuid() && spans[0].Alt == "Image", "upload source and alt");
		expect(spans[0].Hints.Width == pixels(1252) && spans[0].Hints.Height == pixels(625), "upload size hints");
		expect(upload.substr(spans[0].Begin, 4) == "<img" && upload.substr(spans[0].End) == " after", "span covers exactly the tag");

		spans = scan_html_images("<IMG SRC='a.png' ALT=\"x &amp; y\" width=50%>");
		expect(spans.size() == 1U && spans[0].Source == "a.png" && spans[0].Alt == "x & y" && spans[0].Hints.Width == percent(50), "case, single quotes, references, unquoted value");

		spans = scan_html_images("<img src=\"https://e.example/a.png?x=1&amp;y=2\" alt=\"&#60;&#x3E;&bogus;\">");
		expect(spans.size() == 1U && spans[0].Source == "https://e.example/a.png?x=1&y=2" && spans[0].Alt == "<>&bogus;", "numeric references decode; unknown ones stay");

		spans = scan_html_images("<a href=\"https://example.org\">\n  <img src=\"b.png\">\n</a>");
		expect(spans.size() == 1U && spans[0].Link == std::string{ "https://example.org" } && spans[0].Source == "b.png", "a linked image is one span");

		const auto picture = std::string{ "<picture>\xE2\x80\xA8<source media=\"(prefers-color-scheme: dark)\" srcset=\"d.png 1x\"> <source media=\"(prefers-color-scheme:light)\" srcset=\"l.png\"><img src=\"f.png\" alt=\"p\"></picture>" };
		spans              = scan_html_images(picture);
		expect(spans.size() == 1U && spans[0].DarkSource == std::string{ "d.png" } && spans[0].LightSource == std::string{ "l.png" } && spans[0].Source == "f.png" && spans[0].End == picture.size(), "colour-scheme picture with a line separator inside");

		spans = scan_html_images("<picture><source media=\"(min-width: 600px)\" srcset=\"w.png\"><img src=\"f.png\"></picture>");
		expect(spans.size() == 1U && spans[0].Source == "f.png" && !spans[0].DarkSource && spans[0].Begin > 0U, "an unknown media query leaves picture markup as text; its img still shows");

		spans = scan_html_images("<a href=\"x\">text <img src=\"c.png\"></a>");
		expect(spans.size() == 1U && !spans[0].Link, "an anchor with more than an image is not a linked image");

		for (const auto* text : { "<img src=\"unterminated>", "< img src=a>", "<imgsrc=a>", "<img src=a", "<img/src=a>", "<span>no</span>", "<img src=\"a\"alt=\"b\">", "a < b > c" }) {
			expect(scan_html_images(text).empty(), std::string{ "not an image: " } + text);
		}
		std::string many;
		for (int index = 0; index < 40; ++index) {
			many += "<img src=\"a.png\">";
		}
		expect(scan_html_images(many).size() == markdown_image_references_per_body, "scan is bounded per body");
		expect(scan_html_images("<img " + std::string(9000, 'a') + "=\"1\" src=\"a.png\">").empty(), "a tag over 8 KiB is not recognized");
		auto attributes = std::string{ "<img" };
		for (int index = 0; index < 40; ++index) {
			attributes += " a" + std::to_string(index) + "=1";
		}
		expect(scan_html_images(attributes + " src=x>").empty(), "attribute count is bounded");
		spans = scan_html_images("<img src=\"first.png\" src=\"second.png\">");
		expect(spans.size() == 1U && spans[0].Source == "first.png", "the first duplicate attribute wins");
	}

	void served()
	{
		const auto html = std::string{ "<p><a href=\"x\"><img src=\"https://camo.githubusercontent.com/abc/def\" alt=\"badge\" data-canonical-src=\"https://img.shields.io/a.svg?x=1&amp;y=2\"></a>" }
			+ "<img src=\"https://private-user-images.githubusercontent.com/1/604055368-" + attachment_uuid() + ".png?jwt=e.y.z\">"
			+ "<source srcset=\"https://camo.githubusercontent.com/dark/1 2x\" data-canonical-src=\"https://e.example/d.png\">"
			+ "<img alt=\"stripped\"><code>&lt;img src=\"x\"&gt;</code></p>";
		const auto list = served_markdown_images(html);
		expect(list && list->size() == 3U, "img and source tags with an address are served; stripped and escaped ones are not");
		expect((*list)[0].Canonical == "https://img.shields.io/a.svg?x=1&y=2", "canonical source decodes references");
		expect((*list)[2].Source == "https://camo.githubusercontent.com/dark/1", "srcset first candidate");
		expect(!served_markdown_images(std::string(markdown_image_body_html_bytes + 1U, 'a')), "oversized HTML refuses");
		std::string many;
		for (std::size_t index = 0; index <= markdown_image_served_tags_per_body; ++index) {
			many += "<img src=\"a\">";
		}
		expect(!served_markdown_images(many), "too many image tags refuses");

		const auto camo = admit_markdown_image_source("https://img.shields.io/a.svg?x=1&y=2", "github.com");
		expect(camo && camo->Kind == markdown_image_source_kind::absolute_web, "external source admitted");
		expect(match_served_markdown_image(*camo, *list) == std::string{ "https://camo.githubusercontent.com/abc/def" }, "external source matches its camo canonical");
		const auto attachment = admit_markdown_image_source("https://github.com/user-attachments/assets/" + attachment_uuid(), "github.com");
		expect(attachment && attachment->Kind == markdown_image_source_kind::attachment && attachment->Canonical == attachment_uuid(), "own attachment recognized by UUID");
		expect(match_served_markdown_image(*attachment, *list)->starts_with("https://private-user-images.githubusercontent.com/1/604055368-"), "attachment matches its signed served address");
		const auto other = admit_markdown_image_source("https://github.com/user-attachments/assets/00000000-357a-42c0-a1c8-fba0163382cc", "github.com");
		expect(other && !match_served_markdown_image(*other, *list), "a different UUID does not match");
		const auto unserved = admit_markdown_image_source("https://e.example/not-served.png", "github.com");
		expect(unserved && !match_served_markdown_image(*unserved, *list), "a source GitHub does not serve has no address");
		const auto upper = admit_markdown_image_source("https://GitHub.com/user-attachments/assets/" + attachment_uuid(), "github.com");
		expect(upper && upper->Kind == markdown_image_source_kind::attachment, "host comparison is case-insensitive");
		const auto foreign = admit_markdown_image_source("https://evil.example/user-attachments/assets/" + attachment_uuid(), "github.com");
		expect(foreign && foreign->Kind == markdown_image_source_kind::absolute_web, "another host's attachment path is just an address");
		const auto ghes = admit_markdown_image_source("https://ghe.example/user-attachments/assets/" + attachment_uuid(), "ghe.example");
		expect(ghes && ghes->Kind == markdown_image_source_kind::attachment, "GHES attachment on its own host");

		const auto relative = admit_markdown_image_source("./docs/../img/x.png", "github.com");
		expect(relative && relative->Kind == markdown_image_source_kind::repository_relative, "relative path admitted for GitHub to resolve");
		const auto relativeServed = std::vector<markdown_served_image>{ { .Source = "https://github.com/o/r/raw/main/img/x.png?raw=true", .Canonical = { } } };
		const auto matched        = match_served_markdown_image(*relative, relativeServed);
		expect(matched == relativeServed[0].Source, "relative path matches its resolved served address");
		expect(resolved_markdown_image_source(*relative, *matched).Canonical == "https://github.com/o/r/raw/main/img/x.png", "relative identity is its served address without query");
		expect(resolved_markdown_image_source(*attachment, "https://x/y?jwt=1").Canonical == attachment_uuid(), "other identities are unchanged");
		const auto unresolved = std::vector<markdown_served_image>{ { .Source = "img/y.png", .Canonical = { } } };
		expect(!match_served_markdown_image(*relative, unresolved), "an unresolved relative served address does not match another path");

		for (const auto* source : { "", "   ", "data:image/png;base64,AAAA", "file:///etc/passwd", "javascript:alert(1)", "ftp://e.example/a.png", "//evil.example/a.png", "../outside.png", "a/../../b.png", "https://user@e.example/a.png", "https://[::1]/a.png", "https://e.example:0/a.png", "https://e.example/a b.png", "https://e.example/\\a.png", "qrc:/a.png", "https:///a.png" }) {
			const auto admitted = admit_markdown_image_source(source, "github.com");
			expect(!admitted && admitted.error() == markdown_image_state::blocked, std::string{ "blocked source: " } + source);
		}
		expect(!admit_markdown_image_source("https://e.example/" + std::string(markdown_image_source_bytes, 'a'), "github.com"), "source length is bounded");
	}

	void fetch_policy()
	{
		const auto dotcom = markdown_image_fetch_policy{ .Contract = markdown_image_host_contract::github_com, .Hostname = "github.com", .CredentialAvailable = true };
		for (const auto* address : { "https://camo.githubusercontent.com/a/b", "https://private-user-images.githubusercontent.com/1/2-x.png?jwt=a.b.c", "https://github.com/user-attachments/assets/x", "https://raw.githubusercontent.com/o/r/main/a.png", "https://user-images.githubusercontent.com/1/a.png", "https://avatars.githubusercontent.com/u/1?v=4", "HTTPS://CAMO.GITHUBUSERCONTENT.COM/a", "https://camo.githubusercontent.com:443/a" }) {
			const auto hop = admit_markdown_image_fetch(address, dotcom);
			expect(hop && !hop->Credential && hop->Hop == 0U, std::string{ "GitHub image host without credential: " } + address);
		}
		for (const auto* address : { "https://e.example/a.png", "http://camo.githubusercontent.com/a", "https://camo.githubusercontent.com:8443/a", "https://github-production-user-asset-6210df.s3.amazonaws.com/a", "https://camo.githubusercontent.com.evil.example/a", "https://evilgithub.com/a", "a.png", "https://camo.githubusercontent.com/a b" }) {
			const auto hop = admit_markdown_image_fetch(address, dotcom);
			expect(!hop && hop.error() == markdown_image_state::blocked, std::string{ "GitHub.com refuses: " } + address);
		}
		const auto first = admit_markdown_image_fetch("https://github.com/user-attachments/assets/x#frag", dotcom);
		expect(first && first->Address == "https://github.com/user-attachments/assets/x", "fragment removed");
		const auto storage = admit_markdown_image_redirect(*first, "https://github-production-user-asset-6210df.s3.amazonaws.com/1/a.png?X-Amz-Signature=s", dotcom);
		expect(storage && !storage->Credential && storage->Hop == 1U, "storage only as a redirect, signed query kept");
		for (const auto* location : { "https://github-production-user-asset-.s3.amazonaws.com/a", "https://evil-github-production-user-asset-1.s3.amazonaws.com/a", "https://github-production-user-asset-1.s3.amazonaws.com.evil.example/a", "https://github-production-user-asset-A_B.s3.amazonaws.com/a", "http://github-production-user-asset-1.s3.amazonaws.com/a", "https://e.example/a" }) {
			expect(!admit_markdown_image_redirect(*first, location, dotcom), std::string{ "redirect refused: " } + location);
		}
		const auto rooted = admit_markdown_image_redirect(*first, "/other/path.png", dotcom);
		expect(rooted && rooted->Address == "https://github.com/other/path.png", "root-relative location resolves on the same host");
		expect(!admit_markdown_image_redirect(*first, "other/path.png", dotcom), "path-relative location refused");
		auto hop = *first;
		for (std::uint32_t index = 0; index < markdown_image_redirects; ++index) {
			const auto next = admit_markdown_image_redirect(hop, "https://camo.githubusercontent.com/a", dotcom);
			expect(next.has_value(), "redirects within the bound");
			hop = *next;
		}
		const auto fourth = admit_markdown_image_redirect(hop, "https://camo.githubusercontent.com/a", dotcom);
		expect(!fourth && fourth.error() == markdown_image_state::failed, "a fourth redirect fails");

		const auto ghes = markdown_image_fetch_policy{ .Contract = markdown_image_host_contract::ghes, .Hostname = "ghe.example", .CredentialAvailable = true };
		const auto own  = admit_markdown_image_fetch("https://ghe.example/user-attachments/assets/x", ghes);
		expect(own && own->Credential, "GHES own host carries the credential");
		const auto media = admit_markdown_image_fetch("https://media.ghe.example/user/1/files/a.png", ghes);
		expect(media && media->Credential, "GHES media host carries the credential");
		for (const auto* address : { "https://e.example/a.png", "https://evil.ghe.example/a.png", "https://ghe.example.evil/a.png", "https://xmedia.ghe.example/a.png" }) {
			const auto external = admit_markdown_image_fetch(address, ghes);
			expect(external && !external->Credential, std::string{ "GHES other HTTPS host without credential: " } + address);
		}
		expect(!admit_markdown_image_fetch("http://ghe.example/a.png", ghes), "GHES refuses plain HTTP");
		expect(!admit_markdown_image_fetch("https://ghe.example:8443/a.png", ghes), "a port other than 443 is refused");
		auto noCredential                = ghes;
		noCredential.CredentialAvailable = false;
		expect(!admit_markdown_image_fetch("https://ghe.example/a.png", noCredential)->Credential, "no credential when the transport holds none");
		const auto away = admit_markdown_image_redirect(*own, "https://storage.example/blob?sig=1", ghes);
		expect(away && !away->Credential, "redirect to a foreign host drops the credential");
		const auto back = admit_markdown_image_redirect(*away, "https://ghe.example/again", ghes);
		expect(back && !back->Credential, "a dropped credential is never reattached");
		const auto stay = admit_markdown_image_redirect(*own, "/user-attachments/files/a.png", ghes);
		expect(stay && stay->Credential, "same-host redirect keeps the credential");
	}

	void sizing()
	{
		const auto natural = markdown_image_size{ 1252, 625 };
		expect(markdown_image_display_size(natural, { }, 2000) == natural, "natural size when it fits");
		expect(markdown_image_display_size(natural, { }, 626) == markdown_image_size{ 626, 313 }, "clamped to width with aspect ratio");
		expect(markdown_image_display_size(natural, { .Width = pixels(200), .Height = std::nullopt }, 2000) == markdown_image_size{ 200, 100 }, "width hint sets height by aspect");
		expect(markdown_image_display_size(natural, { .Width = std::nullopt, .Height = pixels(20) }, 2000) == markdown_image_size{ 40, 20 }, "height hint sets width by aspect");
		expect(markdown_image_display_size(natural, { .Width = pixels(300), .Height = pixels(300) }, 2000) == markdown_image_size{ 300, 300 }, "both hints are exact");
		expect(markdown_image_display_size(natural, { .Width = pixels(1000), .Height = pixels(500) }, 500) == markdown_image_size{ 500, 250 }, "hinted size also clamps to width");
		expect(markdown_image_display_size(natural, { .Width = percent(50), .Height = std::nullopt }, 800) == markdown_image_size{ 400, 200 }, "percentage width of the reader");
		expect(markdown_image_display_size(natural, { .Width = std::nullopt, .Height = percent(50) }, 2000) == natural, "percentage height is ignored");
		expect(markdown_image_display_size({ 0, 0 }, { }, 0) == markdown_image_size{ 1, 1 }, "degenerate sizes stay at least one pixel");
		expect(markdown_image_display_size({ 1, 16384 }, { }, 100) == markdown_image_size{ 1, 16384 }, "a tall thin image keeps its height");
		expect(markdown_image_reserved_size({ .Width = pixels(1252), .Height = pixels(625) }, 626) == markdown_image_size{ 626, 313 }, "reserved box from both hints, clamped");
		expect(!markdown_image_reserved_size({ .Width = pixels(10), .Height = std::nullopt }, 626), "no reserved box without both hints");
		expect(!markdown_image_reserved_size({ .Width = pixels(10), .Height = percent(10) }, 626), "no reserved box for a percentage height");
	}

	void admission()
	{
		const auto context     = markdown_context{ { default_bpr_profile_id, bpr_window_id{ 2 }, bpr_window_generation{ 3 } }, application_session_id{ 4 }, github_identity_generation{ 5 }, provider_snapshot_id{ 6 }, 7 };
		const auto observation = markdown_image_observation{ .Context = context, .BodyAvailable = true, .Available = true, .Hostname = "github.com" };
		auto       intent      = markdown_image_intent{ .Context = context, .Body = { }, .References = { } };
		intent.References.push_back({ 0, "https://e.example/a.png" });
		intent.References.push_back({ 1, "data:image/png;base64,AA" });
		intent.References.push_back({ 1, "https://e.example/duplicate.png" });
		intent.References.push_back({ static_cast<std::uint32_t>(markdown_image_references_per_body), "https://e.example/over.png" });
		const auto admitted = admit_markdown_images(intent, observation);
		expect(admitted && admitted->size() == 3U, "duplicate ordinal ignored");
		expect((*admitted)[0].Source.has_value(), "admitted reference");
		expect(!(*admitted)[1].Source && (*admitted)[1].Source.error() == markdown_image_state::blocked, "blocked reference keeps its ordinal");
		expect(!(*admitted)[2].Source && (*admitted)[2].Source.error() == markdown_image_state::over_limit, "beyond the per-body bound");

		auto stale = intent;
		++stale.Context.Revision;
		expect(admit_markdown_images(stale, observation).error() == markdown_image_refusal::stale, "a changed publication revision is stale");
		stale                       = intent;
		stale.Context.Window.Window = bpr_window_id{ 9 };
		expect(admit_markdown_images(stale, observation).error() == markdown_image_refusal::stale, "another window is stale");
		auto absent          = observation;
		absent.BodyAvailable = false;
		expect(admit_markdown_images(intent, absent).error() == markdown_image_refusal::stale, "an absent body is stale");
		absent         = observation;
		absent.Context = std::nullopt;
		expect(admit_markdown_images(intent, absent).error() == markdown_image_refusal::stale, "no accepted context is stale");
		auto unavailable      = observation;
		unavailable.Available = false;
		expect(admit_markdown_images(intent, unavailable).error() == markdown_image_refusal::unavailable, "no image capability is explicit");

		auto flood = intent;
		flood.References.clear();
		for (std::uint32_t index = 0; index < 1000U; ++index) {
			flood.References.push_back({ index, "https://e.example/a.png" });
		}
		expect(admit_markdown_images(flood, observation)->size() == 4U * markdown_image_references_per_body, "admission work is bounded");

		const auto source = *admit_markdown_image_source("https://e.example/a.png", "github.com");
		expect(markdown_image_cache_key("GitHub.com", 7, source) == markdown_image_cache_key("github.com", 7, source), "key host is case-insensitive");
		expect(markdown_image_cache_key("github.com", 7, source) != markdown_image_cache_key("github.com", 8, source), "keys are isolated per account");
		expect(markdown_image_cache_key("github.com", 7, source) != markdown_image_cache_key("ghe.example", 7, source), "keys are isolated per host");
	}
} // namespace

int main()
{
	lengths();
	tag_scanning();
	served();
	fetch_policy();
	sizing();
	admission();
	return EXIT_SUCCESS;
}
