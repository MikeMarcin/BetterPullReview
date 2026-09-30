#include "bpr/core/review_cycle_bundle.hpp"

#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_cycle_workspace.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <utility>

namespace bpr::core {
	namespace {
		using json = cycle_json_value;

		constexpr auto manifest_schema = std::string_view{ "bpr.cycle.bundle-manifest.v1" };
		constexpr auto manifest_name   = std::string_view{ "manifest.json" };
		constexpr auto round_name      = std::string_view{ "round.json" };
		constexpr auto review_name     = std::string_view{ "review.md" };
		constexpr auto objects_prefix  = std::string_view{ "objects/" };
		/// A manifest lists at most every admitted member.
		constexpr std::size_t maximum_manifest_bytes = 4U * 1024U * 1024U;

		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto hex_digest(const std::string_view aBytes) -> std::string
		{
			return cycle_hex(sha256(bytes_of(aBytes)));
		}

		auto failure(const cycle_bundle_problem aProblem, std::string aMember = { }) -> std::unexpected<cycle_bundle_error>
		{
			return std::unexpected(cycle_bundle_error{ .Problem = aProblem, .Member = std::move(aMember) });
		}

		auto hex_value(const char aSymbol) noexcept -> int
		{
			if (aSymbol >= '0' && aSymbol <= '9') {
				return aSymbol - '0';
			}
			if (aSymbol >= 'a' && aSymbol <= 'f') {
				return aSymbol - 'a' + 10;
			}
			if (aSymbol >= 'A' && aSymbol <= 'F') {
				return aSymbol - 'A' + 10;
			}
			return -1;
		}

		/// One level of percent-decoding; malformed escapes stay as written.
		auto percent_decoded(const std::string_view aText) -> std::string
		{
			auto result = std::string{ };
			for (std::size_t index = 0; index < aText.size(); ++index) {
				if (aText[index] == '%' && index + 2U < aText.size()) {
					const auto high = hex_value(aText[index + 1U]);
					const auto low  = hex_value(aText[index + 2U]);
					if (high >= 0 && low >= 0) {
						result.push_back(static_cast<char>(high * 16 + low));
						index += 2U;
						continue;
					}
				}
				result.push_back(aText[index]);
			}
			return result;
		}

		auto reserved_device(std::string_view aSegment) -> bool
		{
			aSegment     = aSegment.substr(0, aSegment.find('.'));
			auto lowered = std::string{ aSegment };
			std::ranges::transform(lowered, lowered.begin(), [](const char aSymbol) { return aSymbol >= 'A' && aSymbol <= 'Z' ? static_cast<char>(aSymbol - 'A' + 'a') : aSymbol; });
			if (lowered == "con" || lowered == "prn" || lowered == "aux" || lowered == "nul") {
				return true;
			}
			return lowered.size() == 4U && (lowered.starts_with("com") || lowered.starts_with("lpt")) && lowered[3] >= '1' && lowered[3] <= '9';
		}

		auto object_name(const std::string_view aName) -> bool
		{
			return aName.starts_with(objects_prefix) && parse_cycle_sha256(aName.substr(objects_prefix.size())).has_value();
		}

		auto member_type(const std::string_view aName) -> std::string_view
		{
			if (aName == round_name) {
				return "application/json";
			}
			if (aName == review_name) {
				return "text/markdown";
			}
			return "application/octet-stream";
		}

		/// Every byte digest the round itself names.
		auto referenced_digests(const cycle_round_payload& aPayload) -> std::set<std::string>
		{
			auto result = std::set<std::string>{ };
			for (const auto& source : aPayload.Sources) {
				result.insert(hex_digest(source.Bytes));
			}
			for (const auto& item : aPayload.Items) {
				for (const auto& suggestion : item.Suggestions) {
					result.insert(hex_digest(suggestion.Original));
					result.insert(hex_digest(suggestion.Replacement));
				}
			}
			return result;
		}

		struct listed
		{
			std::string   Type;
			std::uint64_t Bytes = 0;
			std::string   Sha;
		};

		auto read_manifest(const std::string_view aText) -> std::optional<std::map<std::string, listed>>
		{
			const auto parsed = parse_cycle_json(aText, maximum_manifest_bytes);
			if (!parsed) {
				return std::nullopt;
			}
			static constexpr auto keys    = std::array<std::string_view, 2>{ "schema", "files" };
			const auto*           members = std::get_if<json::object>(&parsed->Value);
			if (!members || !has_exactly(*members, keys)) {
				return std::nullopt;
			}
			const auto* schema = std::get_if<std::string>(&find_member(*members, "schema")->Value);
			const auto* files  = std::get_if<json::array>(&find_member(*members, "files")->Value);
			if (!schema || *schema != manifest_schema || !files) {
				return std::nullopt;
			}
			auto result = std::map<std::string, listed>{ };
			for (const auto& file : *files) {
				static constexpr auto fileKeys = std::array<std::string_view, 4>{ "path", "type", "bytes", "sha256" };
				const auto*           fields   = std::get_if<json::object>(&file.Value);
				if (!fields || !has_exactly(*fields, fileKeys)) {
					return std::nullopt;
				}
				const auto* path  = std::get_if<std::string>(&find_member(*fields, "path")->Value);
				const auto* type  = std::get_if<std::string>(&find_member(*fields, "type")->Value);
				const auto* bytes = std::get_if<std::int64_t>(&find_member(*fields, "bytes")->Value);
				const auto* sha   = std::get_if<std::string>(&find_member(*fields, "sha256")->Value);
				if (!path || !type || !bytes || *bytes < 0 || !sha || !parse_cycle_sha256(*sha) || *path == manifest_name) {
					return std::nullopt;
				}
				if (!result.emplace(*path, listed{ .Type = *type, .Bytes = static_cast<std::uint64_t>(*bytes), .Sha = *sha }).second) {
					return std::nullopt;
				}
			}
			return result;
		}

		auto manifest_document(const std::vector<cycle_zip_member>& aListed) -> std::string
		{
			auto files = json::array{ };
			for (const auto& member : aListed) {
				files.push_back(json{ json::object{
					{ .Key = "path", .Value = json{ member.Name } },
					{ .Key = "type", .Value = json{ std::string{ member_type(member.Name) } } },
					{ .Key = "bytes", .Value = json{ static_cast<std::int64_t>(member.Bytes.size()) } },
					{ .Key = "sha256", .Value = json{ hex_digest(member.Bytes) } },
				} });
			}
			auto document = canonical_cycle_json(json{ json::object{ { .Key = "schema", .Value = json{ std::string{ manifest_schema } } }, { .Key = "files", .Value = json{ std::move(files) } } } });
			return document.value_or(std::string{ }) + "\n";
		}

		/// Names: portable, recognized, and distinct on every platform.
		auto check_names(const std::vector<cycle_zip_member>& aMembers) -> std::optional<cycle_bundle_error>
		{
			auto keys = std::set<std::string>{ };
			for (const auto& member : aMembers) {
				if (!portable_member_name(member.Name)) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::member_name, .Member = member.Name };
				}
				if (!keys.insert(workspace_collision_key(member.Name)).second) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::alias, .Member = member.Name };
				}
			}
			for (const auto& member : aMembers) {
				if (member.Name != manifest_name && member.Name != round_name && member.Name != review_name && !object_name(member.Name)) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::unexpected_member, .Member = member.Name };
				}
			}
			for (const auto required : { manifest_name, round_name, review_name }) {
				if (std::ranges::find(aMembers, required, &cycle_zip_member::Name) == aMembers.end()) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::missing_member, .Member = std::string{ required } };
				}
			}
			return std::nullopt;
		}

		/// Exactly the listed members, each as listed.
		auto check_listing(const std::vector<cycle_zip_member>& aMembers, const std::map<std::string, listed>& aListing) -> std::optional<cycle_bundle_error>
		{
			for (const auto& [path, expected] : aListing) {
				const auto found = std::ranges::find(aMembers, path, &cycle_zip_member::Name);
				if (found == aMembers.end()) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::unlisted_member, .Member = path };
				}
				if (found->Bytes.size() != expected.Bytes || hex_digest(found->Bytes) != expected.Sha || expected.Type != member_type(path)) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::member_mismatch, .Member = path };
				}
			}
			for (const auto& member : aMembers) {
				if (member.Name != manifest_name && !aListing.contains(member.Name)) {
					return cycle_bundle_error{ .Problem = cycle_bundle_problem::unlisted_member, .Member = member.Name };
				}
			}
			return std::nullopt;
		}
	}

	auto stable_name(const cycle_bundle_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_bundle_problem::archive:             return "archive";
		case cycle_bundle_problem::member_name:         return "member_name";
		case cycle_bundle_problem::alias:               return "alias";
		case cycle_bundle_problem::missing_member:      return "missing_member";
		case cycle_bundle_problem::unexpected_member:   return "unexpected_member";
		case cycle_bundle_problem::unlisted_member:     return "unlisted_member";
		case cycle_bundle_problem::manifest:            return "manifest";
		case cycle_bundle_problem::member_mismatch:     return "member_mismatch";
		case cycle_bundle_problem::round:               return "round";
		case cycle_bundle_problem::derived_mismatch:    return "derived_mismatch";
		case cycle_bundle_problem::object_mismatch:     return "object_mismatch";
		case cycle_bundle_problem::unreferenced_object: return "unreferenced_object";
		}
		return "archive";
	}

	auto portable_member_name(const std::string_view aName) -> bool
	{
		if (aName.empty() || aName.size() > 4096U) {
			return false;
		}
		auto decoded = std::string{ aName };
		auto settled = false;
		for (int level = 0; level < 8 && !settled; ++level) {
			auto next = percent_decoded(decoded);
			settled   = next == decoded;
			decoded   = std::move(next);
		}
		if (!settled || !valid_cycle_utf8(decoded)) {
			return false;
		}
		const auto unsafe = std::ranges::any_of(decoded, [](const char aSymbol) { return static_cast<unsigned char>(aSymbol) < 0x20U || aSymbol == 0x7F || aSymbol == '\\' || aSymbol == ':'; });
		if (unsafe || decoded.front() == '/') {
			return false;
		}
		for (std::size_t start = 0; start <= decoded.size();) {
			const auto end     = decoded.find('/', start);
			const auto segment = std::string_view{ decoded }.substr(start, end == std::string::npos ? std::string_view::npos : end - start);
			if (segment.empty() || segment == "." || segment == ".." || segment.back() == '.' || segment.back() == ' ' || reserved_device(segment)) {
				return false;
			}
			if (end == std::string::npos) {
				break;
			}
			start = end + 1U;
		}
		return true;
	}

	auto build_round_bundle(const cycle_decoded_round& aRound) -> std::string
	{
		auto listed = std::vector<cycle_zip_member>{
			{ .Name = std::string{ round_name }, .Bytes = encode_round_envelope(aRound.Payload).value_or(std::string{ }) },
			{ .Name = std::string{ review_name }, .Bytes = render_round_markdown(aRound) },
		};
		auto objects = std::map<std::string, std::string>{ };
		for (const auto& source : aRound.Payload.Sources) {
			objects.emplace(std::string{ objects_prefix } + hex_digest(source.Bytes), source.Bytes);
		}
		for (auto& [name, bytes] : objects) {
			listed.push_back({ .Name = name, .Bytes = std::move(bytes) });
		}
		auto members = std::vector<cycle_zip_member>{ { .Name = std::string{ manifest_name }, .Bytes = manifest_document(listed) } };
		members.insert(members.end(), std::make_move_iterator(listed.begin()), std::make_move_iterator(listed.end()));
		return write_cycle_zip(members);
	}

	auto open_round_bundle(const std::span<const std::uint8_t> aArchive) -> std::expected<cycle_opened_bundle, cycle_bundle_error>
	{
		if (aArchive.size() > maximum_bundle_archive_bytes) {
			return std::unexpected(cycle_bundle_error{ .Problem = cycle_bundle_problem::archive, .Zip = cycle_zip_problem::size });
		}
		auto members = read_cycle_zip(aArchive);
		if (!members) {
			return std::unexpected(cycle_bundle_error{ .Problem = cycle_bundle_problem::archive, .Zip = members.error() });
		}
		if (auto problem = check_names(*members)) {
			return std::unexpected(std::move(*problem));
		}
		const auto& manifest = *std::ranges::find(*members, manifest_name, &cycle_zip_member::Name);
		const auto  listing  = read_manifest(manifest.Bytes);
		if (!listing) {
			return failure(cycle_bundle_problem::manifest, std::string{ manifest_name });
		}
		if (auto problem = check_listing(*members, *listing)) {
			return std::unexpected(std::move(*problem));
		}
		auto round = decode_round_envelope(std::ranges::find(*members, round_name, &cycle_zip_member::Name)->Bytes);
		if (!round) {
			return std::unexpected(cycle_bundle_error{ .Problem = cycle_bundle_problem::round, .Round = round.error(), .Member = std::string{ round_name } });
		}
		auto result = cycle_opened_bundle{ .Round = std::move(*round), .Review = std::ranges::find(*members, review_name, &cycle_zip_member::Name)->Bytes };
		if (result.Review != render_round_markdown(result.Round)) {
			return failure(cycle_bundle_problem::derived_mismatch, std::string{ review_name });
		}
		const auto named = referenced_digests(result.Round.Payload);
		for (auto& member : *members) {
			if (!object_name(member.Name)) {
				continue;
			}
			const auto hex = member.Name.substr(objects_prefix.size());
			if (hex_digest(member.Bytes) != hex) {
				return failure(cycle_bundle_problem::object_mismatch, member.Name);
			}
			if (!named.contains(hex)) {
				return failure(cycle_bundle_problem::unreferenced_object, member.Name);
			}
			result.Objects.emplace_back(cycle_material_digest{ *parse_cycle_sha256(hex) }, std::move(member.Bytes));
		}
		std::ranges::sort(result.Objects, { }, &std::pair<cycle_material_digest, std::string>::first);
		return result;
	}
}
