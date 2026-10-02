#include "bpr/core/review_cycle_response.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
#include <set>
#include <span>
#include <system_error>
#include <utility>

namespace bpr::core {
	namespace {
		using json = cycle_json_value;

		constexpr auto response_schema = std::string_view{ "bpr.cycle.response.v1" };
		constexpr auto reply_schema    = std::string_view{ "bpr.cycle.manual-reply.v1" };

		constexpr auto dispositions = std::array{
			cycle_response_disposition::addressed,
			cycle_response_disposition::partially_addressed,
			cycle_response_disposition::disputed,
			cycle_response_disposition::blocked,
			cycle_response_disposition::discussion_only,
		};
		constexpr auto author_kinds   = std::array{ cycle_claimed_author_kind::human, cycle_claimed_author_kind::agent, cycle_claimed_author_kind::unknown };
		constexpr auto evidence_kinds = std::array{ cycle_evidence_kind::text, cycle_evidence_kind::path, cycle_evidence_kind::commit, cycle_evidence_kind::url, cycle_evidence_kind::test_report };

		template <typename TEnum, std::size_t N>
		auto enum_from(const std::string_view aText, const std::array<TEnum, N>& aValues) -> std::optional<TEnum>
		{
			const auto found = std::ranges::find_if(aValues, [&](const TEnum aValue) { return stable_name(aValue) == aText; });
			return found == aValues.end() ? std::nullopt : std::optional{ *found };
		}

		auto text(std::string aValue) -> json
		{
			return json{ .Value = std::move(aValue) };
		}

		auto optional_text(const std::optional<std::string>& aValue) -> json
		{
			return aValue ? text(*aValue) : json{ };
		}

		auto member(std::string aKey, json aValue) -> cycle_json_member
		{
			return { .Key = std::move(aKey), .Value = std::move(aValue) };
		}

		auto revision_text(const std::uint64_t aValue) -> std::string
		{
			return std::to_string(aValue);
		}

		auto parse_revision(const std::string_view aText) -> std::optional<std::uint64_t>
		{
			if (aText.empty() || aText.size() > 19U || aText.front() == '0') {
				return std::nullopt;
			}
			auto       value  = std::uint64_t{ 0 };
			const auto parsed = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != aText.data() + aText.size() || value == 0U || value > maximum_cycle_identity) {
				return std::nullopt;
			}
			return value;
		}

		/// Reads one document's fields, recording the first problem.
		class reader
		{
		public:
			std::optional<cycle_response_decode_error> Error;
			std::optional<std::size_t>                 Entry;

			template <typename T>
			auto fail(const cycle_response_decode_problem aProblem) -> std::optional<T>
			{
				if (!Error) {
					Error = cycle_response_decode_error{ .Problem = aProblem, .Entry = Entry };
				}
				return std::nullopt;
			}

			auto object(const json* aValue, const std::span<const std::string_view> aKeys) -> const json::object*
			{
				const auto* value = aValue != nullptr ? std::get_if<json::object>(&aValue->Value) : nullptr;
				if (value == nullptr || !has_exactly(*value, aKeys)) {
					static_cast<void>(fail<bool>(cycle_response_decode_problem::shape));
					return nullptr;
				}
				return value;
			}

			auto string(const json* aValue, const std::size_t aLimit) -> std::optional<std::string>
			{
				const auto* value = aValue != nullptr ? std::get_if<std::string>(&aValue->Value) : nullptr;
				if (value == nullptr) {
					return fail<std::string>(cycle_response_decode_problem::shape);
				}
				if (value->size() > aLimit || !valid_cycle_utf8(*value)) {
					return fail<std::string>(cycle_response_decode_problem::bounds);
				}
				return *value;
			}

			auto nullable_string(const json* aValue, const std::size_t aLimit) -> std::optional<std::optional<std::string>>
			{
				if (aValue != nullptr && std::holds_alternative<std::monostate>(aValue->Value)) {
					return std::optional<std::string>{ };
				}
				auto value = string(aValue, aLimit);
				if (!value) {
					return std::nullopt;
				}
				return std::optional<std::optional<std::string>>{ std::in_place, std::move(value) };
			}

			auto identifier(const json* aValue) -> std::optional<std::string>
			{
				auto value = string(aValue, maximum_response_text_bytes);
				if (value && !valid_cycle_wire_identifier(*value)) {
					return fail<std::string>(cycle_response_decode_problem::identifier);
				}
				return value;
			}

			template <typename TEnum, std::size_t N>
			auto enumerated(const json* aValue, const std::array<TEnum, N>& aValues) -> std::optional<TEnum>
			{
				const auto value = string(aValue, maximum_response_text_bytes);
				if (!value) {
					return std::nullopt;
				}
				const auto parsed = enum_from(*value, aValues);
				if (!parsed) {
					return fail<TEnum>(cycle_response_decode_problem::shape);
				}
				return parsed;
			}

			auto array(const json* aValue, const std::size_t aLimit) -> const json::array*
			{
				const auto* value = aValue != nullptr ? std::get_if<json::array>(&aValue->Value) : nullptr;
				if (value == nullptr) {
					static_cast<void>(fail<bool>(cycle_response_decode_problem::shape));
					return nullptr;
				}
				if (value->size() > aLimit) {
					static_cast<void>(fail<bool>(cycle_response_decode_problem::bounds));
					return nullptr;
				}
				return value;
			}
		};

		auto author_json(const cycle_claimed_author& aAuthor) -> json
		{
			return json{ .Value = json::object{
							 member("display_name", text(aAuthor.DisplayName)),
							 member("kind", text(std::string{ stable_name(aAuthor.Kind) })),
							 member("provider", optional_text(aAuthor.Provider)),
							 member("model", optional_text(aAuthor.Model)),
							 member("effort", optional_text(aAuthor.Effort)),
						 } };
		}

		auto entry_json(const cycle_response_entry& aEntry) -> json
		{
			auto evidence = json::array{ };
			for (const auto& item : aEntry.Evidence) {
				evidence.push_back(json{ .Value = json::object{ member("kind", text(std::string{ stable_name(item.Kind) })), member("value", text(item.Value)) } });
			}
			return json{ .Value = json::object{
							 member("item_id", text(aEntry.Item)),
							 member("item_revision", text(revision_text(aEntry.ItemRevision))),
							 member("disposition", text(std::string{ stable_name(aEntry.Disposition) })),
							 member("body", text(aEntry.Body)),
							 member("evidence", json{ .Value = std::move(evidence) }),
						 } };
		}

		/// The round item a name refers to, if the round holds it.
		auto round_item(const cycle_round_payload& aRound, const std::string_view aItem) -> const cycle_wire_item*
		{
			const auto found = std::ranges::find(aRound.Items, aItem, &cycle_wire_item::Id);
			return found == aRound.Items.end() ? nullptr : &*found;
		}
	}

	auto stable_name(const cycle_response_disposition aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_response_disposition::addressed:           return "addressed";
		case cycle_response_disposition::partially_addressed: return "partially_addressed";
		case cycle_response_disposition::disputed:            return "disputed";
		case cycle_response_disposition::blocked:             return "blocked";
		case cycle_response_disposition::discussion_only:     return "discussion_only";
		}
		return "unknown";
	}

	auto stable_name(const cycle_claimed_author_kind aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_claimed_author_kind::human:   return "human";
		case cycle_claimed_author_kind::agent:   return "agent";
		case cycle_claimed_author_kind::unknown: return "unknown";
		}
		return "unknown";
	}

	auto stable_name(const cycle_evidence_kind aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_evidence_kind::text:        return "text";
		case cycle_evidence_kind::path:        return "path";
		case cycle_evidence_kind::commit:      return "commit";
		case cycle_evidence_kind::url:         return "url";
		case cycle_evidence_kind::test_report: return "test_report";
		}
		return "unknown";
	}

	auto stable_name(const cycle_response_decode_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_response_decode_problem::json:       return "json";
		case cycle_response_decode_problem::version:    return "version";
		case cycle_response_decode_problem::shape:      return "shape";
		case cycle_response_decode_problem::identifier: return "identifier";
		case cycle_response_decode_problem::bounds:     return "bounds";
		case cycle_response_decode_problem::duplicate:  return "duplicate";
		}
		return "unknown";
	}

	auto stable_name(const cycle_response_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_response_problem::wrong_cycle:       return "wrong_cycle";
		case cycle_response_problem::wrong_round:       return "wrong_round";
		case cycle_response_problem::wrong_digest:      return "wrong_digest";
		case cycle_response_problem::unknown_item:      return "unknown_item";
		case cycle_response_problem::stale_item:        return "stale_item";
		case cycle_response_problem::empty:             return "empty";
		case cycle_response_problem::reused_identifier: return "reused_identifier";
		}
		return "unknown";
	}

	auto stable_name(const cycle_response_channel aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_response_channel::response_file:   return "response_file";
		case cycle_response_channel::manual_entry:    return "manual_entry";
		case cycle_response_channel::local_interface: return "local_interface";
		}
		return "unknown";
	}

	auto structured_response(const cycle_response_channel aValue) noexcept -> bool
	{
		return aValue == cycle_response_channel::response_file || aValue == cycle_response_channel::local_interface;
	}

	auto encode_response_document(const cycle_response_document& aDocument) -> std::optional<std::string>
	{
		auto entries = json::array{ };
		entries.reserve(aDocument.Entries.size());
		for (const auto& entry : aDocument.Entries) {
			entries.push_back(entry_json(entry));
		}
		const auto value = json{ .Value = json::object{
									 member("schema", text(std::string{ response_schema })),
									 member("response_id", text(aDocument.ResponseId)),
									 member("cycle_id", text(aDocument.CycleId)),
									 member("round_id", text(aDocument.RoundId)),
									 member("round_payload_sha256", text(cycle_hex(aDocument.RoundDigest.bytes()))),
									 member("claimed_author", author_json(aDocument.ClaimedAuthor)),
									 member("result_source_revision_id", optional_text(aDocument.ResultRevision)),
									 member("items", json{ .Value = std::move(entries) }),
								 } };
		return canonical_cycle_json(value);
	}

	auto decode_response_document(const std::string_view aDocument) -> std::expected<cycle_decoded_response, cycle_response_decode_error>
	{
		const auto parsed = parse_cycle_json(aDocument, maximum_response_document_bytes);
		if (!parsed) {
			return std::unexpected(cycle_response_decode_error{ .Problem = cycle_response_decode_problem::json, .Json = parsed.error() });
		}
		constexpr auto top     = std::array<std::string_view, 8>{ "schema", "response_id", "cycle_id", "round_id", "round_payload_sha256", "claimed_author", "result_source_revision_id", "items" };
		constexpr auto author  = std::array<std::string_view, 5>{ "display_name", "kind", "provider", "model", "effort" };
		constexpr auto entry   = std::array<std::string_view, 5>{ "item_id", "item_revision", "disposition", "body", "evidence" };
		constexpr auto support = std::array<std::string_view, 2>{ "kind", "value" };
		const auto*    root    = std::get_if<json::object>(&parsed->Value);
		const auto*    schema  = root != nullptr ? find_member(*root, "schema") : nullptr;
		if (schema == nullptr || !std::holds_alternative<std::string>(schema->Value)) {
			return std::unexpected(cycle_response_decode_error{ .Problem = cycle_response_decode_problem::shape });
		}
		if (std::get<std::string>(schema->Value) != response_schema) {
			return std::unexpected(cycle_response_decode_error{ .Problem = cycle_response_decode_problem::version });
		}
		auto        read   = reader{ };
		const auto* fields = read.object(&*parsed, top);
		auto        result = cycle_response_document{ };
		if (fields != nullptr) {
			auto response = read.identifier(find_member(*fields, "response_id"));
			auto cycle    = read.identifier(find_member(*fields, "cycle_id"));
			auto round    = read.identifier(find_member(*fields, "round_id"));
			auto digest   = read.string(find_member(*fields, "round_payload_sha256"), 64U);
			if (digest && !parse_cycle_sha256(*digest)) {
				static_cast<void>(read.fail<bool>(cycle_response_decode_problem::identifier));
			}
			auto claimed = read.nullable_string(find_member(*fields, "result_source_revision_id"), maximum_response_text_bytes);
			if (claimed && *claimed && !valid_cycle_wire_identifier(**claimed)) {
				static_cast<void>(read.fail<bool>(cycle_response_decode_problem::identifier));
			}
			if (const auto* who = read.object(find_member(*fields, "claimed_author"), author)) {
				auto name     = read.string(find_member(*who, "display_name"), maximum_response_text_bytes);
				auto kind     = read.enumerated(find_member(*who, "kind"), author_kinds);
				auto provider = read.nullable_string(find_member(*who, "provider"), maximum_response_text_bytes);
				auto model    = read.nullable_string(find_member(*who, "model"), maximum_response_text_bytes);
				auto effort   = read.nullable_string(find_member(*who, "effort"), maximum_response_text_bytes);
				if (name && kind && provider && model && effort) {
					result.ClaimedAuthor = { .DisplayName = std::move(*name), .Kind = *kind, .Provider = std::move(*provider), .Model = std::move(*model), .Effort = std::move(*effort) };
				}
			}
			auto seen = std::set<std::string, std::less<>>{ };
			if (const auto* items = read.array(find_member(*fields, "items"), maximum_response_items)) {
				for (std::size_t index = 0; index < items->size() && !read.Error; ++index) {
					read.Entry        = index;
					const auto* value = read.object(&(*items)[index], entry);
					if (value == nullptr) {
						break;
					}
					auto item        = read.identifier(find_member(*value, "item_id"));
					auto revision    = read.string(find_member(*value, "item_revision"), 20U);
					auto disposition = read.enumerated(find_member(*value, "disposition"), dispositions);
					auto body        = read.string(find_member(*value, "body"), maximum_response_body_bytes);
					auto parsedRev   = revision ? parse_revision(*revision) : std::nullopt;
					if (revision && !parsedRev) {
						static_cast<void>(read.fail<bool>(cycle_response_decode_problem::identifier));
					}
					auto evidence = std::vector<cycle_response_evidence>{ };
					if (const auto* list = read.array(find_member(*value, "evidence"), maximum_response_evidence)) {
						for (const auto& piece : *list) {
							const auto* fact = read.object(&piece, support);
							if (fact == nullptr) {
								break;
							}
							auto kind  = read.enumerated(find_member(*fact, "kind"), evidence_kinds);
							auto words = read.string(find_member(*fact, "value"), maximum_response_text_bytes);
							if (kind && words) {
								evidence.push_back({ .Kind = *kind, .Value = std::move(*words) });
							}
						}
					}
					if (item && !seen.insert(*item).second) {
						static_cast<void>(read.fail<bool>(cycle_response_decode_problem::duplicate));
					}
					if (item && parsedRev && disposition && body && !read.Error) {
						result.Entries.push_back({ .Item = std::move(*item), .ItemRevision = *parsedRev, .Disposition = *disposition, .Body = std::move(*body), .Evidence = std::move(evidence) });
					}
				}
				read.Entry.reset();
			}
			if (response && cycle && round && digest && claimed && !read.Error) {
				result.ResponseId     = std::move(*response);
				result.CycleId        = std::move(*cycle);
				result.RoundId        = std::move(*round);
				result.RoundDigest    = cycle_round_digest{ *parse_cycle_sha256(*digest) };
				result.ResultRevision = std::move(*claimed);
			}
		}
		if (read.Error) {
			return std::unexpected(*read.Error);
		}
		auto canonical = encode_response_document(result);
		if (!canonical) {
			return std::unexpected(cycle_response_decode_error{ .Problem = cycle_response_decode_problem::bounds });
		}
		const auto bytes  = std::span{ reinterpret_cast<const std::uint8_t*>(canonical->data()), canonical->size() };
		const auto digest = cycle_material_digest{ sha256(bytes) };
		return cycle_decoded_response{ .Document = std::move(result), .Canonical = std::move(*canonical), .Digest = digest };
	}

	auto check_response(const cycle_response_document& aResponse, const cycle_round_payload& aRound, const cycle_round_digest& aDigest) -> std::optional<cycle_response_refusal>
	{
		if (aResponse.CycleId != aRound.CycleId) {
			return cycle_response_refusal{ .Problem = cycle_response_problem::wrong_cycle };
		}
		if (aResponse.RoundId != aRound.RoundId) {
			return cycle_response_refusal{ .Problem = cycle_response_problem::wrong_round };
		}
		if (aResponse.RoundDigest != aDigest) {
			return cycle_response_refusal{ .Problem = cycle_response_problem::wrong_digest };
		}
		for (std::size_t index = 0; index < aResponse.Entries.size(); ++index) {
			const auto& entry = aResponse.Entries[index];
			const auto* item  = round_item(aRound, entry.Item);
			if (item == nullptr) {
				return cycle_response_refusal{ .Problem = cycle_response_problem::unknown_item, .Entry = index };
			}
			if (item->Revision != entry.ItemRevision) {
				return cycle_response_refusal{ .Problem = cycle_response_problem::stale_item, .Entry = index };
			}
		}
		return std::nullopt;
	}

	auto check_manual_reply(const cycle_manual_reply& aReply, const cycle_round_payload& aRound) -> std::optional<cycle_response_refusal>
	{
		if (aReply.RoundId != aRound.RoundId) {
			return cycle_response_refusal{ .Problem = cycle_response_problem::wrong_round };
		}
		const auto blank = std::ranges::all_of(aReply.Body, [](const char aSymbol) { return aSymbol == ' ' || aSymbol == '\t' || aSymbol == '\r' || aSymbol == '\n'; });
		if (blank) {
			return cycle_response_refusal{ .Problem = cycle_response_problem::empty };
		}
		auto seen = std::set<std::string_view>{ };
		for (std::size_t index = 0; index < aReply.Items.size(); ++index) {
			if (round_item(aRound, aReply.Items[index]) == nullptr || !seen.insert(aReply.Items[index]).second) {
				return cycle_response_refusal{ .Problem = cycle_response_problem::unknown_item, .Entry = index };
			}
		}
		return std::nullopt;
	}

	auto encode_manual_reply(const cycle_manual_reply& aReply) -> std::optional<std::string>
	{
		if (aReply.Body.size() > maximum_response_body_bytes || aReply.From.size() > maximum_response_text_bytes || aReply.Items.size() > maximum_response_items
			|| !valid_cycle_wire_identifier(aReply.RoundId) || !std::ranges::all_of(aReply.Items, valid_cycle_wire_identifier)) {
			return std::nullopt;
		}
		auto items = json::array{ };
		for (const auto& item : aReply.Items) {
			items.push_back(text(item));
		}
		return canonical_cycle_json(json{ .Value = json::object{
											  member("schema", text(std::string{ reply_schema })),
											  member("round_id", text(aReply.RoundId)),
											  member("from", text(aReply.From)),
											  member("items", json{ .Value = std::move(items) }),
											  member("body", text(aReply.Body)),
										  } });
	}

	auto decode_manual_reply(const std::string_view aCanonical) -> std::optional<cycle_manual_reply>
	{
		const auto parsed = parse_cycle_json(aCanonical, maximum_response_body_bytes * 2U);
		if (!parsed) {
			return std::nullopt;
		}
		constexpr auto keys   = std::array<std::string_view, 5>{ "schema", "round_id", "from", "items", "body" };
		auto           read   = reader{ };
		const auto*    fields = read.object(&*parsed, keys);
		if (fields == nullptr) {
			return std::nullopt;
		}
		const auto schema = read.string(find_member(*fields, "schema"), 64U);
		auto       reply  = cycle_manual_reply{ };
		auto       round  = read.identifier(find_member(*fields, "round_id"));
		auto       from   = read.string(find_member(*fields, "from"), maximum_response_text_bytes);
		auto       body   = read.string(find_member(*fields, "body"), maximum_response_body_bytes);
		if (const auto* items = read.array(find_member(*fields, "items"), maximum_response_items)) {
			for (const auto& item : *items) {
				if (auto name = read.identifier(&item)) {
					reply.Items.push_back(std::move(*name));
				}
			}
		}
		if (read.Error || !schema || *schema != reply_schema || !round || !from || !body) {
			return std::nullopt;
		}
		reply.RoundId = std::move(*round);
		reply.From    = std::move(*from);
		reply.Body    = std::move(*body);
		// Only the canonical form is a stored reply.
		if (encode_manual_reply(reply) != aCanonical) {
			return std::nullopt;
		}
		return reply;
	}

	auto item_statements(const std::vector<std::pair<cycle_response_record, cycle_response_content>>& aResponses, const std::string_view aItem) -> std::vector<cycle_item_statement>
	{
		auto statements = std::vector<cycle_item_statement>{ };
		for (const auto& [record, content] : aResponses) {
			if (const auto* document = std::get_if<cycle_response_document>(&content)) {
				for (const auto& entry : document->Entries) {
					if (entry.Item == aItem) {
						statements.push_back({ .Response      = record.Id.Id,
											   .Round         = record.Round,
											   .Channel       = record.Channel,
											   .From          = document->ClaimedAuthor.DisplayName,
											   .ClaimedAuthor = document->ClaimedAuthor,
											   .Disposition   = entry.Disposition,
											   .Body          = entry.Body,
											   .Evidence      = entry.Evidence });
					}
				}
				continue;
			}
			const auto& reply = std::get<cycle_manual_reply>(content);
			if (std::ranges::find(reply.Items, aItem) != reply.Items.end()) {
				statements.push_back({ .Response = record.Id.Id, .Round = record.Round, .Channel = record.Channel, .From = reply.From, .Body = reply.Body });
			}
		}
		return statements;
	}

	auto resolve_claimed_result(const review_cycle_state& aCycle, const std::optional<std::string>& aClaimed) -> std::optional<cycle_source_revision_id>
	{
		if (!aClaimed || aClaimed->size() < 2U || aClaimed->front() != 'R' || !aCycle.Latest) {
			return std::nullopt;
		}
		const auto value = parse_revision(std::string_view{ *aClaimed }.substr(1));
		if (!value || *value > aCycle.Latest->Id.Id.value()) {
			return std::nullopt;
		}
		return cycle_source_revision_id{ *value };
	}
}
