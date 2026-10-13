// [[BPR-1441]]. The C++ syntax classifier: KSyntaxHighlighting's complete-blob
// pass and the C++ identifier refinement behind it, through the public
// `packaged_syntax_classifier`, exactly as the appearance worker calls it.
//
// Input: byte 0 is reserved (keep it zero), bytes 1-2 are the first line and
// byte 3 the line count of the hunk this input also asks for (big-endian; a
// count of zero asks for no hunk). The rest is the blob.
//
// Checked, beyond the absence of a crash:
// - a result that is not `available` carries no spans;
// - an available result's spans lie inside the blob's lines, are nonempty,
//   in order, do not overlap, and carry a syntax role;
// - the same request gives the same answer again;
// - a hunk is exactly the whole-blob spans on its lines;
// - a span limit below the answer's span count gives `limit_exceeded` and no
//   spans, and at the count gives either the same answer or that refusal;
// - a stop requested before the call publishes no spans, and answers a
//   nonempty blob with something other than `available`;
// - a blob that is strictly valid UTF-8 is never `unavailable`.

#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "fuzz_support.hpp"

#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	namespace app    = bpr::application;
	namespace core   = bpr::core;
	namespace syntax = bpr::adapters::syntax;

	class fuzz_blob final : public syntax::blob_source
	{
	public:
		[[nodiscard]] auto blob_bytes(const core::git_blob_oid&) -> std::optional<std::string> override
		{
			return Bytes;
		}

		std::string Bytes;
	};

	auto catalog() -> const syntax::packaged_definition_catalog&
	{
		static const auto instance = [] {
			QStandardPaths::setTestModeEnabled(true);
			return syntax::packaged_definition_catalog{ };
		}();
		return instance;
	}

	[[nodiscard]] auto syntax_role(const core::appearance_role aRole) noexcept -> bool
	{
		const auto value = static_cast<unsigned>(aRole);
		return (value >= static_cast<unsigned>(core::appearance_role::syntax_normal)
				&& value <= static_cast<unsigned>(core::appearance_role::syntax_other))
			|| aRole == core::appearance_role::syntax_namespace || aRole == core::appearance_role::syntax_enum_member;
	}

	/// Byte length of every line, split at '\n' as the classifier splits.
	[[nodiscard]] auto line_lengths(const std::string_view aText) -> std::vector<std::size_t>
	{
		auto lengths = std::vector<std::size_t>{ };
		auto begin   = std::size_t{ 0U };
		while (true) {
			const auto end = aText.find('\n', begin);
			if (end == std::string_view::npos) {
				lengths.push_back(aText.size() - begin);
				return lengths;
			}
			lengths.push_back(end - begin);
			begin = end + 1U;
		}
	}

	/// RFC 3629 UTF-8: no overlong forms, no surrogates, nothing above U+10FFFF.
	[[nodiscard]] auto strict_utf8(const std::string_view aText) noexcept -> bool
	{
		auto at = std::size_t{ 0U };
		while (at < aText.size()) {
			const auto lead = static_cast<unsigned char>(aText[at]);
			if (lead < 0x80U) {
				++at;
				continue;
			}
			auto need = std::size_t{ 0U };
			auto low  = 0x80U;
			auto high = 0xBFU;
			if (lead >= 0xC2U && lead <= 0xDFU) {
				need = 1U;
			} else if (lead >= 0xE0U && lead <= 0xEFU) {
				need = 2U;
				low  = lead == 0xE0U ? 0xA0U : 0x80U;
				high = lead == 0xEDU ? 0x9FU : 0xBFU;
			} else if (lead >= 0xF0U && lead <= 0xF4U) {
				need = 3U;
				low  = lead == 0xF0U ? 0x90U : 0x80U;
				high = lead == 0xF4U ? 0x8FU : 0xBFU;
			} else {
				return false;
			}
			if (at + need >= aText.size()) {
				return false;
			}
			for (auto index = std::size_t{ 1U }; index <= need; ++index) {
				const auto next = static_cast<unsigned char>(aText[at + index]);
				const auto from = index == 1U ? low : 0x80U;
				const auto to   = index == 1U ? high : 0xBFU;
				if (next < from || next > to) {
					return false;
				}
			}
			at += need + 1U;
		}
		return true;
	}

	[[nodiscard]] auto blob_identity() -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = 0x5BU;
		bytes[19]  = 0xA5U;
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	void require_empty_unless_available(const app::syntax_classification& aResult)
	{
		bpr::fuzz::require(aResult.Status == app::syntax_classification_status::available || aResult.Spans.empty());
	}

	void require_well_formed(const app::token_span_list& aSpans, const std::vector<std::size_t>& aLines)
	{
		const auto* previous = static_cast<const app::token_span*>(nullptr);
		for (const auto& span : aSpans) {
			bpr::fuzz::require(span.Length > 0U && syntax_role(span.Role));
			bpr::fuzz::require(span.Line < aLines.size());
			bpr::fuzz::require(static_cast<std::size_t>(span.Column) + span.Length <= aLines[span.Line]);
			if (previous != nullptr) {
				bpr::fuzz::require(span.Line > previous->Line
								   || (span.Line == previous->Line && span.Column >= previous->Column + previous->Length));
			}
			previous = &span;
		}
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	auto       input      = bpr::fuzz::fuzz_input{ aData, aSize };
	const auto reserved   = input.byte();
	const auto firstLine  = input.u16();
	const auto lineCount  = input.byte();
	auto       blobs      = fuzz_blob{ };
	blobs.Bytes           = std::string{ input.text() };
	auto       classifier = syntax::packaged_syntax_classifier{ catalog(), blobs };
	const auto definition = catalog().definition_named("C++");
	bpr::fuzz::require(definition.has_value());
	static_cast<void>(reserved);

	auto request = app::syntax_classification_request{
		.Context      = { },
		.Key          = { .Blob = blob_identity(), .Definition = *definition },
		.MaximumSpans = 1'000'000U,
	};
	const auto whole = classifier.classify(request, { });
	require_empty_unless_available(whole);
	if (strict_utf8(blobs.Bytes)) {
		bpr::fuzz::require(whole.Status != app::syntax_classification_status::unavailable);
	}
	const auto lines = line_lengths(blobs.Bytes);
	if (whole.Status == app::syntax_classification_status::available) {
		require_well_formed(whole.Spans, lines);
	}

	const auto again = classifier.classify(request, { });
	bpr::fuzz::require(again.Status == whole.Status && again.Spans == whole.Spans);

	if (lineCount > 0U) {
		const auto range = syntax::blob_line_range{ firstLine, lineCount };
		const auto hunk  = classifier.classify_hunk(request, range, { });
		require_empty_unless_available(hunk);
		bpr::fuzz::require(hunk.Status == whole.Status);
		auto projected = app::token_span_list{ };
		std::copy_if(whole.Spans.begin(), whole.Spans.end(), std::back_inserter(projected), [&](const app::token_span& aSpan) {
			return aSpan.Line >= range.First && aSpan.Line < range.First + range.Count;
		});
		bpr::fuzz::require(hunk.Spans == projected);
	}

	if (whole.Status == app::syntax_classification_status::available && !whole.Spans.empty()) {
		request.MaximumSpans = whole.Spans.size() - 1U;
		const auto below     = classifier.classify(request, { });
		bpr::fuzz::require(below.Status == app::syntax_classification_status::limit_exceeded && below.Spans.empty());
		request.MaximumSpans = whole.Spans.size();
		const auto exact     = classifier.classify(request, { });
		bpr::fuzz::require((exact.Status == app::syntax_classification_status::available && exact.Spans == whole.Spans)
						   || (exact.Status == app::syntax_classification_status::limit_exceeded && exact.Spans.empty()));
		request.MaximumSpans = 1'000'000U;
	}

	auto stop = std::stop_source{ };
	stop.request_stop();
	const auto stopped = classifier.classify(request, stop.get_token());
	bpr::fuzz::require(stopped.Spans.empty());
	// An empty blob has nothing to stop: it is answered without spans.
	bpr::fuzz::require(blobs.Bytes.empty() || stopped.Status != app::syntax_classification_status::available);
	return 0;
}
