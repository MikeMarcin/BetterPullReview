#include "bpr/core/local_draft_evidence.hpp"

#include "bpr/core/hex.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <ranges>
#include <set>
#include <utility>

namespace bpr::core::evidence_wire {
	// --- Stored form ---------------------------------------------------------------
	struct origin
	{
		std::string   Source;
		std::string   Producer;
		std::string   Version;
		std::uint64_t Import = 0;
	};
	struct claim
	{
		std::uint64_t            Id = 0;
		std::string              Kind;
		std::string              Text;
		std::vector<std::string> Earlier;
		std::string              Disposition;
		origin                   Origin;
		bool                     Selected = false;
	};
	struct formal
	{
		std::string Property;
		std::string Subject;
		std::string Tool;
		std::string Assumptions;
		std::string Bounds;
		std::string Correspondence;
	};
	struct check
	{
		std::uint64_t              Id = 0;
		std::string                Method;
		std::string                Title;
		std::string                Command;
		std::vector<std::uint64_t> Claims;
		std::optional<formal>      Formal;
		std::string                Disposition;
		origin                     Origin;
	};
	struct working
	{
		std::string                Path;
		std::optional<std::string> Object;
	};
	struct material
	{
		std::string          Head;
		std::vector<working> Working;
	};
	struct artifact
	{
		std::string                Name;
		std::string                State;
		std::string                Reference;
		std::optional<std::string> Object;
		std::uint64_t              Size = 0;
	};
	struct result
	{
		std::uint64_t            Id    = 0;
		std::uint64_t            Check = 0;
		std::string              Outcome;
		std::string              Class;
		origin                   Origin;
		std::optional<material>  Material;
		std::uint64_t            Candidate = 0;
		std::vector<std::string> Inputs;
		std::string              Environment;
		std::string              Expected;
		std::string              Actual;
		std::string              Limitations;
		std::vector<artifact>    Artifacts;
		std::int64_t             Recorded = 0;
		bool                     Selected = false;
	};
	struct question
	{
		std::uint64_t Id = 0;
		std::string   Text;
		std::uint64_t Claim    = 0;
		bool          Resolved = false;
		std::string   Disposition;
		origin        Origin;
		bool          Selected = false;
	};
	struct import_record
	{
		std::uint64_t Ordinal = 0;
		origin        Origin;
		std::string   Digest;
		std::uint64_t Claims    = 0;
		std::uint64_t Checks    = 0;
		std::uint64_t Results   = 0;
		std::uint64_t Questions = 0;
		std::int64_t  At        = 0;
	};
	struct evidence
	{
		std::vector<claim>         Claims;
		std::vector<check>         Checks;
		std::vector<result>        Results;
		std::vector<question>      Questions;
		std::vector<import_record> Imports;
		std::uint64_t              NextId = 1;
	};

	// --- The import document ---------------------------------------------------------
	struct head
	{
		std::string   Format;
		std::uint32_t Version = 0;
	};
	struct producer
	{
		std::string                Kind;
		std::string                Name;
		std::optional<std::string> Version;
	};
	struct in_claim
	{
		std::string Key;
		std::string Kind;
		std::string Text;
	};
	struct in_formal
	{
		std::optional<std::string> Property;
		std::optional<std::string> Subject;
		std::optional<std::string> Tool;
		std::optional<std::string> Assumptions;
		std::optional<std::string> Bounds;
		std::optional<std::string> Correspondence;
	};
	struct in_check
	{
		std::string                Key;
		std::string                Method;
		std::string                Title;
		std::optional<std::string> Command;
		std::vector<std::string>   Claims;
		std::optional<in_formal>   Formal;
	};
	struct in_working
	{
		std::string                Path;
		std::optional<std::string> Object;
		bool                       Deleted = false;
	};
	struct in_material
	{
		std::string             Head;
		std::vector<in_working> Working;
	};
	struct in_artifact
	{
		std::string                Name;
		std::optional<std::string> Path;
		std::optional<std::string> Url;
	};
	struct in_result
	{
		std::optional<std::string>   Check;
		std::optional<std::uint64_t> CheckId;
		std::string                  Outcome;
		std::string                  Class;
		std::optional<in_material>   Material;
		std::vector<std::string>     Inputs;
		std::optional<std::string>   Environment;
		std::optional<std::string>   Expected;
		std::optional<std::string>   Actual;
		std::optional<std::string>   Limitations;
		std::vector<in_artifact>     Artifacts;
	};
	struct in_question
	{
		std::string                Text;
		std::optional<std::string> Claim;
	};
	struct document
	{
		std::string              Format;
		std::uint32_t            Version = 0;
		producer                 Producer;
		std::vector<in_claim>    Claims;
		std::vector<in_check>    Checks;
		std::vector<in_result>   Results;
		std::vector<in_question> Questions;
	};
} // namespace bpr::core::evidence_wire

namespace ew = bpr::core::evidence_wire;

template <>
struct glz::meta<ew::origin>
{
	using value_type            = ew::origin;
	static constexpr auto value = object("source", &value_type::Source, "producer", &value_type::Producer, "version", &value_type::Version, "import", &value_type::Import);
};
template <>
struct glz::meta<ew::claim>
{
	using value_type            = ew::claim;
	static constexpr auto value = object("id", &value_type::Id, "kind", &value_type::Kind, "text", &value_type::Text, "earlier", &value_type::Earlier, "disposition", &value_type::Disposition, "origin", &value_type::Origin, "selected", &value_type::Selected);
};
template <>
struct glz::meta<ew::formal>
{
	using value_type            = ew::formal;
	static constexpr auto value = object("property", &value_type::Property, "subject", &value_type::Subject, "tool", &value_type::Tool, "assumptions", &value_type::Assumptions, "bounds", &value_type::Bounds, "correspondence",
										 &value_type::Correspondence);
};
template <>
struct glz::meta<ew::check>
{
	using value_type            = ew::check;
	static constexpr auto value = object("id", &value_type::Id, "method", &value_type::Method, "title", &value_type::Title, "command", &value_type::Command, "claims", &value_type::Claims, "formal", &value_type::Formal, "disposition",
										 &value_type::Disposition, "origin", &value_type::Origin);
};
template <>
struct glz::meta<ew::working>
{
	using value_type            = ew::working;
	static constexpr auto value = object("path", &value_type::Path, "object", &value_type::Object);
};
template <>
struct glz::meta<ew::material>
{
	using value_type            = ew::material;
	static constexpr auto value = object("head", &value_type::Head, "working", &value_type::Working);
};
template <>
struct glz::meta<ew::artifact>
{
	using value_type            = ew::artifact;
	static constexpr auto value = object("name", &value_type::Name, "state", &value_type::State, "reference", &value_type::Reference, "object", &value_type::Object, "size", &value_type::Size);
};
template <>
struct glz::meta<ew::result>
{
	using value_type            = ew::result;
	static constexpr auto value = object("id", &value_type::Id, "check", &value_type::Check, "outcome", &value_type::Outcome, "class", &value_type::Class, "origin", &value_type::Origin, "material", &value_type::Material, "candidate",
										 &value_type::Candidate, "inputs", &value_type::Inputs, "environment", &value_type::Environment, "expected", &value_type::Expected, "actual", &value_type::Actual, "limitations",
										 &value_type::Limitations, "artifacts", &value_type::Artifacts, "recorded", &value_type::Recorded, "selected", &value_type::Selected);
};
template <>
struct glz::meta<ew::question>
{
	using value_type            = ew::question;
	static constexpr auto value = object("id", &value_type::Id, "text", &value_type::Text, "claim", &value_type::Claim, "resolved", &value_type::Resolved, "disposition", &value_type::Disposition, "origin", &value_type::Origin, "selected",
										 &value_type::Selected);
};
template <>
struct glz::meta<ew::import_record>
{
	using value_type            = ew::import_record;
	static constexpr auto value = object("ordinal", &value_type::Ordinal, "origin", &value_type::Origin, "digest", &value_type::Digest, "claims", &value_type::Claims, "checks", &value_type::Checks, "results", &value_type::Results,
										 "questions", &value_type::Questions, "at", &value_type::At);
};
template <>
struct glz::meta<ew::evidence>
{
	using value_type            = ew::evidence;
	static constexpr auto value = object("claims", &value_type::Claims, "checks", &value_type::Checks, "results", &value_type::Results, "questions", &value_type::Questions, "imports", &value_type::Imports, "next_id", &value_type::NextId);
};
template <>
struct glz::meta<ew::head>
{
	using value_type            = ew::head;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version);
};
template <>
struct glz::meta<ew::producer>
{
	using value_type            = ew::producer;
	static constexpr auto value = object("kind", &value_type::Kind, "name", &value_type::Name, "version", &value_type::Version);
};
template <>
struct glz::meta<ew::in_claim>
{
	using value_type            = ew::in_claim;
	static constexpr auto value = object("key", &value_type::Key, "kind", &value_type::Kind, "text", &value_type::Text);
};
template <>
struct glz::meta<ew::in_formal>
{
	using value_type            = ew::in_formal;
	static constexpr auto value = object("property", &value_type::Property, "subject", &value_type::Subject, "tool", &value_type::Tool, "assumptions", &value_type::Assumptions, "bounds", &value_type::Bounds, "correspondence",
										 &value_type::Correspondence);
};
template <>
struct glz::meta<ew::in_check>
{
	using value_type            = ew::in_check;
	static constexpr auto value = object("key", &value_type::Key, "method", &value_type::Method, "title", &value_type::Title, "command", &value_type::Command, "claims", &value_type::Claims, "formal", &value_type::Formal);
};
template <>
struct glz::meta<ew::in_working>
{
	using value_type            = ew::in_working;
	static constexpr auto value = object("path", &value_type::Path, "object", &value_type::Object, "deleted", &value_type::Deleted);
};
template <>
struct glz::meta<ew::in_material>
{
	using value_type            = ew::in_material;
	static constexpr auto value = object("head", &value_type::Head, "working", &value_type::Working);
};
template <>
struct glz::meta<ew::in_artifact>
{
	using value_type            = ew::in_artifact;
	static constexpr auto value = object("name", &value_type::Name, "path", &value_type::Path, "url", &value_type::Url);
};
template <>
struct glz::meta<ew::in_result>
{
	using value_type            = ew::in_result;
	static constexpr auto value = object("check", &value_type::Check, "check_id", &value_type::CheckId, "outcome", &value_type::Outcome, "class", &value_type::Class, "material", &value_type::Material, "inputs", &value_type::Inputs,
										 "environment", &value_type::Environment, "expected", &value_type::Expected, "actual", &value_type::Actual, "limitations", &value_type::Limitations, "artifacts", &value_type::Artifacts);
};
template <>
struct glz::meta<ew::in_question>
{
	using value_type            = ew::in_question;
	static constexpr auto value = object("text", &value_type::Text, "claim", &value_type::Claim);
};
template <>
struct glz::meta<ew::document>
{
	using value_type            = ew::document;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version, "producer", &value_type::Producer, "claims", &value_type::Claims, "checks", &value_type::Checks, "results", &value_type::Results,
										 "questions", &value_type::Questions);
};

namespace bpr::core {
	namespace {
		constexpr auto import_format         = std::string_view{ "bpr.evidence-import" };
		constexpr auto import_format_version = std::uint32_t{ 1U };
		constexpr auto reported_class        = std::string_view{ "reported" };
		constexpr auto strict                = glz::opts{ .error_on_unknown_keys = true };
		constexpr auto lenient               = glz::opts{ .error_on_unknown_keys = false };

		constexpr auto all_sources      = std::array{ evidence_source::author, evidence_source::tool, evidence_source::agent };
		constexpr auto all_dispositions = std::array{ evidence_disposition::proposed, evidence_disposition::accepted, evidence_disposition::rejected };
		constexpr auto all_kinds        = std::array{ claim_kind::bug_fix, claim_kind::preserved_behavior, claim_kind::compatibility, claim_kind::performance, claim_kind::formal_property,
													  claim_kind::other };
		constexpr auto all_methods      = std::array{ check_method::test, check_method::benchmark, check_method::fuzz, check_method::property_test,
													  check_method::inspection, check_method::model_check, check_method::proof, check_method::other };
		constexpr auto all_outcomes     = std::array{ evidence_outcome::pass, evidence_outcome::fail, evidence_outcome::inconclusive, evidence_outcome::not_run, evidence_outcome::interrupted };
		constexpr auto all_states       = std::array{ artifact_state::stored, artifact_state::link, artifact_state::missing, artifact_state::too_large, artifact_state::refused };

		template <typename TEnum, std::size_t TCount>
		[[nodiscard]] auto enum_from(const std::string_view aName, const std::array<TEnum, TCount>& aValues) -> std::optional<TEnum>
		{
			for (const auto value : aValues) {
				if (name(value) == aName) {
					return value;
				}
			}
			return std::nullopt;
		}

		/// Text within a bound, valid UTF-8, without control bytes (line
		/// breaks and tabs only where `aLines`).
		[[nodiscard]] auto plain_text(const std::string_view aText, const std::size_t aLimit, const bool aLines) -> bool
		{
			return aText.size() <= aLimit && utf8::valid(aText) && std::ranges::none_of(aText, [aLines](const char aByte) {
					   const auto byte = static_cast<unsigned char>(aByte);
					   return (byte < 0x20U && !(aLines && (aByte == '\n' || aByte == '\t' || aByte == '\r'))) || byte == 0x7FU;
				   });
		}

		[[nodiscard]] auto valid_path(const std::string_view aPath) -> bool
		{
			if (aPath.empty() || aPath.size() > 4096U || aPath.front() == '/' || aPath.back() == '/' || !utf8::valid(aPath)) {
				return false;
			}
			for (const auto part : std::views::split(aPath, '/')) {
				const auto segment = std::string_view{ part.begin(), part.end() };
				if (segment.empty() || segment == "." || segment == ".." || segment == ".git") {
					return false;
				}
			}
			return std::ranges::none_of(aPath, [](const char aByte) { return aByte == '\0' || aByte == '\\'; });
		}

		/// A path, or a directory ending in `/`.
		[[nodiscard]] auto valid_input(const std::string_view aInput) -> bool
		{
			return aInput.ends_with('/') ? valid_path(aInput.substr(0U, aInput.size() - 1U)) : valid_path(aInput);
		}

		[[nodiscard]] auto valid_text(const std::string_view aText) -> bool
		{
			return plain_text(aText, maximum_evidence_text_bytes, true);
		}

		[[nodiscard]] auto valid_name(const std::string_view aText) -> bool
		{
			return plain_text(aText, maximum_evidence_name_bytes, false);
		}

		[[nodiscard]] auto advanced(local_draft aDraft, const std::int64_t aNow) -> local_draft
		{
			++aDraft.Version;
			aDraft.Updated = std::max(aDraft.Updated, aNow);
			return aDraft;
		}

		[[nodiscard]] auto find_candidate(const local_draft& aDraft, const std::uint64_t aOrdinal) -> const local_draft_candidate*
		{
			const auto found = std::ranges::find(aDraft.Candidates, aOrdinal, &local_draft_candidate::Ordinal);
			return found == aDraft.Candidates.end() ? nullptr : &*found;
		}

		template <typename TRecord>
		[[nodiscard]] auto find_record(std::vector<TRecord>& aRecords, const std::uint64_t aId) -> TRecord*
		{
			const auto found = std::ranges::find(aRecords, aId, &TRecord::Id);
			return found == aRecords.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto has_claim(const draft_evidence& aEvidence, const std::uint64_t aId) -> bool
		{
			return std::ranges::find(aEvidence.Claims, aId, &evidence_claim::Id) != aEvidence.Claims.end();
		}

		[[nodiscard]] auto has_check(const draft_evidence& aEvidence, const std::uint64_t aId) -> bool
		{
			return std::ranges::find(aEvidence.Checks, aId, &evidence_check::Id) != aEvidence.Checks.end();
		}

		[[nodiscard]] auto valid_origin(const evidence_origin& aOrigin, const std::size_t aImports) -> bool
		{
			return plain_text(aOrigin.Producer, maximum_evidence_name_bytes, false) && plain_text(aOrigin.Version, maximum_evidence_name_bytes, false) && aOrigin.Import <= aImports
				&& (aOrigin.Import != 0U || (aOrigin.Source == evidence_source::author && aOrigin.Producer.empty())
					// RFC-BPR-0110 AC07: a claim or check an agent proposed in a
					// conversation and the author accepted, with no import.
					|| (aOrigin.Source == evidence_source::agent && !aOrigin.Producer.empty()));
		}

		[[nodiscard]] auto valid_formal(const formal_scope& aFormal) -> bool
		{
			return valid_text(aFormal.Property) && valid_text(aFormal.Subject) && valid_name(aFormal.Tool) && valid_text(aFormal.Assumptions) && valid_text(aFormal.Bounds)
				&& valid_text(aFormal.Correspondence);
		}

		[[nodiscard]] auto valid_material(const evidence_material& aMaterial) -> bool
		{
			if (!aMaterial.Head.valid() || aMaterial.Working.size() > maximum_local_draft_working_files) {
				return false;
			}
			const auto* previous = static_cast<const std::string*>(nullptr);
			for (const auto& file : aMaterial.Working) {
				if (!valid_path(file.Path) || (previous != nullptr && !(*previous < file.Path)) || (file.Object && !file.Object->valid())) {
					return false;
				}
				previous = &file.Path;
			}
			return true;
		}

		[[nodiscard]] auto valid_artifact(const evidence_artifact& aArtifact) -> bool
		{
			return !aArtifact.Name.empty() && valid_name(aArtifact.Name) && plain_text(aArtifact.Reference, 4096U, false)
				&& (aArtifact.State == artifact_state::stored) == aArtifact.Object.has_value() && (!aArtifact.Object || aArtifact.Object->valid())
				&& aArtifact.Size <= maximum_evidence_artifact_bytes;
		}

		[[nodiscard]] auto take_id(draft_evidence& aEvidence) -> std::uint64_t
		{
			return aEvidence.NextId++;
		}

		[[nodiscard]] auto lower_hex_text(const std::string_view aText) -> bool
		{
			return std::ranges::all_of(aText, [](const char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); });
		}

		// --- Codec helpers ------------------------------------------------------------

		[[nodiscard]] auto to_wire(const evidence_origin& aOrigin) -> ew::origin
		{
			return { .Source = std::string{ name(aOrigin.Source) }, .Producer = aOrigin.Producer, .Version = aOrigin.Version, .Import = aOrigin.Import };
		}

		[[nodiscard]] auto from_wire(const ew::origin& aOrigin) -> std::optional<evidence_origin>
		{
			const auto source = enum_from(aOrigin.Source, all_sources);
			if (!source) {
				return std::nullopt;
			}
			return evidence_origin{ .Source = *source, .Producer = aOrigin.Producer, .Version = aOrigin.Version, .Import = aOrigin.Import };
		}

		[[nodiscard]] auto to_wire(const evidence_material& aMaterial) -> ew::material
		{
			auto wire = ew::material{ .Head = std::string{ canonical_hex(aMaterial.Head).view() } };
			for (const auto& file : aMaterial.Working) {
				wire.Working.push_back({ .Path = file.Path, .Object = file.Object ? std::optional{ std::string{ canonical_hex(*file.Object).view() } } : std::nullopt });
			}
			return wire;
		}

		[[nodiscard]] auto from_wire(const ew::material& aMaterial) -> std::optional<evidence_material>
		{
			const auto head = parse_git_commit_oid(aMaterial.Head);
			if (!head) {
				return std::nullopt;
			}
			auto material = evidence_material{ .Head = *head };
			for (const auto& file : aMaterial.Working) {
				auto object = std::optional<git_entry_oid>{ };
				if (file.Object) {
					object = parse_git_entry_oid(*file.Object);
					if (!object) {
						return std::nullopt;
					}
				}
				material.Working.push_back({ .Path = file.Path, .Object = object });
			}
			return material;
		}

		[[nodiscard]] auto refused(const evidence_import_refusal aReason, std::string aDetail = { }) -> std::unexpected<evidence_import_refused>
		{
			return std::unexpected{ evidence_import_refused{ .Reason = aReason, .Detail = std::move(aDetail) } };
		}

		[[nodiscard]] auto at(const std::string_view aList, const std::size_t aIndex, const std::string_view aField = { }) -> std::string
		{
			auto text = std::string{ aList } + "[" + std::to_string(aIndex) + "]";
			if (!aField.empty()) {
				text += ".";
				text += aField;
			}
			return text;
		}

		/// Paths the candidate's change touches under one input, with what
		/// it makes of each.
		[[nodiscard]] auto changes_under(const local_draft_candidate& aCandidate, const std::string_view aInput)
			-> std::map<std::string, std::optional<std::pair<std::uint32_t, git_entry_oid>>, std::less<>>
		{
			auto touched = std::map<std::string, std::optional<std::pair<std::uint32_t, git_entry_oid>>, std::less<>>{ };
			for (const auto& change : aCandidate.Changes) {
				const auto inside = aInput.ends_with('/') ? std::string_view{ change.Path }.starts_with(aInput) : change.Path == aInput;
				if (inside) {
					touched.emplace(change.Path, change.Right ? std::optional{ std::pair{ change.Right->Mode, change.Right->Object } } : std::nullopt);
				}
			}
			return touched;
		}
	}

	auto name(const evidence_source aSource) noexcept -> std::string_view
	{
		switch (aSource) {
		case evidence_source::author: return "author";
		case evidence_source::tool:   return "tool";
		case evidence_source::agent:  return "agent";
		}
		return "unknown";
	}

	auto name(const evidence_disposition aDisposition) noexcept -> std::string_view
	{
		switch (aDisposition) {
		case evidence_disposition::proposed: return "proposed";
		case evidence_disposition::accepted: return "accepted";
		case evidence_disposition::rejected: return "rejected";
		}
		return "unknown";
	}

	auto name(const claim_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case claim_kind::bug_fix:            return "bug_fix";
		case claim_kind::preserved_behavior: return "preserved_behavior";
		case claim_kind::compatibility:      return "compatibility";
		case claim_kind::performance:        return "performance";
		case claim_kind::formal_property:    return "formal_property";
		case claim_kind::other:              return "other";
		}
		return "unknown";
	}

	auto name(const check_method aMethod) noexcept -> std::string_view
	{
		switch (aMethod) {
		case check_method::test:          return "test";
		case check_method::benchmark:     return "benchmark";
		case check_method::fuzz:          return "fuzz";
		case check_method::property_test: return "property_test";
		case check_method::inspection:    return "inspection";
		case check_method::model_check:   return "model_check";
		case check_method::proof:         return "proof";
		case check_method::other:         return "other";
		}
		return "unknown";
	}

	auto formal(const check_method aMethod) noexcept -> bool
	{
		return aMethod == check_method::model_check || aMethod == check_method::proof;
	}

	auto name(const evidence_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case evidence_outcome::pass:         return "pass";
		case evidence_outcome::fail:         return "fail";
		case evidence_outcome::inconclusive: return "inconclusive";
		case evidence_outcome::not_run:      return "not_run";
		case evidence_outcome::interrupted:  return "interrupted";
		}
		return "unknown";
	}

	auto name(const artifact_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case artifact_state::stored:    return "stored";
		case artifact_state::link:      return "link";
		case artifact_state::missing:   return "missing";
		case artifact_state::too_large: return "too_large";
		case artifact_state::refused:   return "refused";
		}
		return "unknown";
	}

	auto name(const evidence_import_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case evidence_import_refusal::too_large:     return "too_large";
		case evidence_import_refusal::malformed:     return "malformed";
		case evidence_import_refusal::format:        return "format";
		case evidence_import_refusal::version:       return "version";
		case evidence_import_refusal::unknown_field: return "unknown_field";
		case evidence_import_refusal::observed:      return "observed";
		case evidence_import_refusal::invalid_value: return "invalid_value";
		case evidence_import_refusal::reference:     return "reference";
		case evidence_import_refusal::bounds:        return "bounds";
		case evidence_import_refusal::duplicate:     return "duplicate";
		case evidence_import_refusal::empty:         return "empty";
		case evidence_import_refusal::stale:         return "stale";
		}
		return "unknown";
	}

	auto name(const evidence_currency aCurrency) noexcept -> std::string_view
	{
		switch (aCurrency) {
		case evidence_currency::current:       return "current";
		case evidence_currency::carried:       return "carried";
		case evidence_currency::needs_recheck: return "needs_recheck";
		case evidence_currency::unbound:       return "unbound";
		case evidence_currency::later:         return "later";
		}
		return "unknown";
	}

	auto name(const evidence_reach aReach) noexcept -> std::string_view
	{
		switch (aReach) {
		case evidence_reach::material:    return "material";
		case evidence_reach::environment: return "environment";
		case evidence_reach::model:       return "model";
		case evidence_reach::property:    return "property";
		case evidence_reach::reading:     return "reading";
		}
		return "unknown";
	}

	auto reach(const check_method aMethod) noexcept -> evidence_reach
	{
		switch (aMethod) {
		case check_method::benchmark:     return evidence_reach::environment;
		case check_method::model_check:   return evidence_reach::model;
		case check_method::proof:         return evidence_reach::property;
		case check_method::inspection:    return evidence_reach::reading;
		case check_method::test:
		case check_method::fuzz:
		case check_method::property_test:
		case check_method::other:         return evidence_reach::material;
		}
		return evidence_reach::material;
	}

	// --- Validity -------------------------------------------------------------------

	auto valid(const draft_evidence& aEvidence) -> bool
	{
		if (aEvidence.Claims.size() > maximum_evidence_claims || aEvidence.Checks.size() > maximum_evidence_checks || aEvidence.Results.size() > maximum_evidence_results
			|| aEvidence.Questions.size() > maximum_evidence_questions || aEvidence.Imports.size() > maximum_evidence_imports || aEvidence.NextId == 0U) {
			return false;
		}
		auto       ids     = std::set<std::uint64_t>{ };
		const auto imports = aEvidence.Imports.size();
		const auto fresh   = [&](const std::uint64_t aId) { return aId != 0U && aId < aEvidence.NextId && ids.insert(aId).second; };
		for (const auto& claim : aEvidence.Claims) {
			if (!fresh(claim.Id) || claim.Text.empty() || !valid_text(claim.Text) || claim.Earlier.size() > maximum_evidence_claim_revisions
				|| !std::ranges::all_of(claim.Earlier, [](const std::string& aText) { return valid_text(aText); }) || !valid_origin(claim.Origin, imports)) {
				return false;
			}
		}
		for (const auto& check : aEvidence.Checks) {
			if (!fresh(check.Id) || check.Title.empty() || !valid_name(check.Title) || !valid_text(check.Command) || !valid_formal(check.Formal) || !valid_origin(check.Origin, imports)
				|| check.Claims.size() > maximum_evidence_claims || !std::ranges::all_of(check.Claims, [&](const std::uint64_t aClaim) { return has_claim(aEvidence, aClaim); })) {
				return false;
			}
		}
		for (const auto& result : aEvidence.Results) {
			if (!fresh(result.Id) || !has_check(aEvidence, result.Check) || !valid_origin(result.Origin, imports) || (result.Material && !valid_material(*result.Material))
				|| (result.Candidate != 0U && !result.Material) || result.Inputs.size() > maximum_evidence_inputs
				|| !std::ranges::all_of(result.Inputs, [](const std::string& aInput) { return valid_input(aInput); }) || !valid_text(result.Environment) || !valid_text(result.Expected)
				|| !valid_text(result.Actual) || !valid_text(result.Limitations) || result.Artifacts.size() > maximum_evidence_artifacts
				|| !std::ranges::all_of(result.Artifacts, [](const evidence_artifact& aArtifact) { return valid_artifact(aArtifact); })) {
				return false;
			}
		}
		for (const auto& question : aEvidence.Questions) {
			if (!fresh(question.Id) || question.Text.empty() || !valid_text(question.Text) || (question.Claim != 0U && !has_claim(aEvidence, question.Claim))
				|| !valid_origin(question.Origin, imports)) {
				return false;
			}
		}
		for (std::size_t index = 0; index < imports; ++index) {
			const auto& record = aEvidence.Imports[index];
			if (record.Ordinal != index + 1U || record.Digest.size() != 64U || !lower_hex_text(record.Digest) || record.Origin.Import != record.Ordinal
				|| !valid_origin(record.Origin, imports)) {
				return false;
			}
		}
		return true;
	}

	auto evidence_objects(const draft_evidence& aEvidence) -> std::vector<git_entry_oid>
	{
		auto objects = std::vector<git_entry_oid>{ };
		for (const auto& result : aEvidence.Results) {
			for (const auto& artifact : result.Artifacts) {
				if (artifact.Object) {
					objects.push_back(*artifact.Object);
				}
			}
		}
		return objects;
	}

	// --- Entered in the workbench ---------------------------------------------------

	auto add_claim(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const claim_kind aKind, std::string aText, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aText.empty() || !valid_text(aText)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		if (aDraft.Evidence.Claims.size() >= maximum_evidence_claims) {
			return std::unexpected{ draft_refusal::evidence_full };
		}
		auto next = advanced(aDraft, aNow);
		auto id   = take_id(next.Evidence);
		next.Evidence.Claims.push_back({ .Id = id, .Kind = aKind, .Text = std::move(aText), .Disposition = evidence_disposition::accepted });
		return next;
	}

	auto revise_claim(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aClaim, std::string aText, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aText.empty() || !valid_text(aText)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		auto  next  = advanced(aDraft, aNow);
		auto* claim = find_record(next.Evidence.Claims, aClaim);
		if (claim == nullptr) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		if (claim->Text == aText) {
			return std::unexpected{ draft_refusal::unchanged };
		}
		if (claim->Earlier.size() >= maximum_evidence_claim_revisions) {
			return std::unexpected{ draft_refusal::evidence_full };
		}
		claim->Earlier.push_back(std::exchange(claim->Text, std::move(aText)));
		return next;
	}

	auto add_check(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const check_method aMethod, std::string aTitle, std::string aCommand,
				   std::vector<std::uint64_t> aClaims, formal_scope aFormal, const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		std::ranges::sort(aClaims);
		const auto [first, last] = std::ranges::unique(aClaims);
		aClaims.erase(first, last);
		if (aTitle.empty() || !valid_name(aTitle) || !valid_text(aCommand) || !valid_formal(aFormal)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		if (!std::ranges::all_of(aClaims, [&aDraft](const std::uint64_t aClaim) { return has_claim(aDraft.Evidence, aClaim); })) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		if (aDraft.Evidence.Checks.size() >= maximum_evidence_checks) {
			return std::unexpected{ draft_refusal::evidence_full };
		}
		if (!formal(aMethod)) {
			aFormal = { };
		}
		auto next = advanced(aDraft, aNow);
		auto id   = take_id(next.Evidence);
		next.Evidence.Checks.push_back({ .Id          = id,
										 .Method      = aMethod,
										 .Title       = std::move(aTitle),
										 .Command     = std::move(aCommand),
										 .Claims      = std::move(aClaims),
										 .Formal      = std::move(aFormal),
										 .Disposition = evidence_disposition::accepted });
		return next;
	}

	auto record_result(const local_draft& aDraft, const std::uint64_t aExpectedVersion, manual_result aResult, const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (!has_check(aDraft.Evidence, aResult.Check)) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		const auto* candidate = aResult.Candidate == 0U ? nullptr : find_candidate(aDraft, aResult.Candidate);
		if ((aResult.Candidate != 0U && candidate == nullptr) || aResult.Inputs.size() > maximum_evidence_inputs
			|| !std::ranges::all_of(aResult.Inputs, [](const std::string& aInput) { return valid_input(aInput); }) || !valid_text(aResult.Environment)
			|| !valid_text(aResult.Expected) || !valid_text(aResult.Actual) || !valid_text(aResult.Limitations)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		if (aDraft.Evidence.Results.size() >= maximum_evidence_results) {
			return std::unexpected{ draft_refusal::evidence_full };
		}
		auto next = advanced(aDraft, aNow);
		auto id   = take_id(next.Evidence);
		next.Evidence.Results.push_back({ .Id          = id,
										  .Check       = aResult.Check,
										  .Outcome     = aResult.Outcome,
										  .Material    = candidate == nullptr ? std::nullopt : std::optional{ candidate_material(*candidate) },
										  .Candidate   = aResult.Candidate,
										  .Inputs      = std::move(aResult.Inputs),
										  .Environment = std::move(aResult.Environment),
										  .Expected    = std::move(aResult.Expected),
										  .Actual      = std::move(aResult.Actual),
										  .Limitations = std::move(aResult.Limitations),
										  .Recorded    = aNow });
		return next;
	}

	auto add_question(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aText, const std::uint64_t aClaim, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aText.empty() || !valid_text(aText)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		if (aClaim != 0U && !has_claim(aDraft.Evidence, aClaim)) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		if (aDraft.Evidence.Questions.size() >= maximum_evidence_questions) {
			return std::unexpected{ draft_refusal::evidence_full };
		}
		auto next = advanced(aDraft, aNow);
		auto id   = take_id(next.Evidence);
		next.Evidence.Questions.push_back({ .Id = id, .Text = std::move(aText), .Claim = aClaim, .Disposition = evidence_disposition::accepted });
		return next;
	}

	auto resolve_question(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aQuestion, const bool aResolved, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		auto  next     = advanced(aDraft, aNow);
		auto* question = find_record(next.Evidence.Questions, aQuestion);
		if (question == nullptr) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		if (question->Resolved == aResolved) {
			return std::unexpected{ draft_refusal::unchanged };
		}
		question->Resolved = aResolved;
		return next;
	}

	auto decide_evidence(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aRecord, const evidence_disposition aDisposition, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aDisposition == evidence_disposition::proposed) {
			return std::unexpected{ draft_refusal::invalid };
		}
		auto  next        = advanced(aDraft, aNow);
		auto* disposition = static_cast<evidence_disposition*>(nullptr);
		if (auto* claim = find_record(next.Evidence.Claims, aRecord); claim != nullptr) {
			disposition = &claim->Disposition;
			if (aDisposition == evidence_disposition::rejected) {
				claim->Selected = false;
			}
		} else if (auto* check = find_record(next.Evidence.Checks, aRecord); check != nullptr) {
			disposition = &check->Disposition;
			if (aDisposition == evidence_disposition::rejected) {
				for (auto& result : next.Evidence.Results) {
					result.Selected = result.Selected && result.Check != aRecord;
				}
			}
		} else if (auto* question = find_record(next.Evidence.Questions, aRecord); question != nullptr) {
			disposition = &question->Disposition;
			if (aDisposition == evidence_disposition::rejected) {
				question->Selected = false;
			}
		}
		if (disposition == nullptr) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		if (*disposition == aDisposition) {
			return std::unexpected{ draft_refusal::unchanged };
		}
		*disposition = aDisposition;
		return next;
	}

	auto select_evidence(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aRecord, const bool aSelected, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ draft_refusal::stale };
		}
		auto  next     = advanced(aDraft, aNow);
		auto* selected = static_cast<bool*>(nullptr);
		auto  eligible = true;
		if (auto* claim = find_record(next.Evidence.Claims, aRecord); claim != nullptr) {
			selected = &claim->Selected;
			eligible = claim->Disposition == evidence_disposition::accepted;
		} else if (auto* result = find_record(next.Evidence.Results, aRecord); result != nullptr) {
			selected        = &result->Selected;
			const auto* its = find_record(next.Evidence.Checks, result->Check);
			eligible        = its != nullptr && its->Disposition == evidence_disposition::accepted;
		} else if (auto* question = find_record(next.Evidence.Questions, aRecord); question != nullptr) {
			selected = &question->Selected;
			eligible = question->Disposition == evidence_disposition::accepted;
		}
		if (selected == nullptr) {
			return std::unexpected{ draft_refusal::unknown_record };
		}
		// Only what the author accepted can be selected for others to read.
		if (aSelected && !eligible) {
			return std::unexpected{ draft_refusal::invalid };
		}
		if (*selected == aSelected) {
			return std::unexpected{ draft_refusal::unchanged };
		}
		*selected = aSelected;
		return next;
	}

	// --- The import -------------------------------------------------------------------

	auto parse_evidence_import(const std::string_view aDocument) -> std::expected<evidence_import_document, evidence_import_refused>
	{
		if (aDocument.size() > maximum_evidence_import_bytes) {
			return refused(evidence_import_refusal::too_large);
		}
		auto opening = ew::head{ };
		if (glz::read<lenient>(opening, aDocument)) {
			return refused(evidence_import_refusal::malformed);
		}
		if (opening.Format != import_format) {
			return refused(evidence_import_refusal::format, "format");
		}
		if (opening.Version != import_format_version) {
			return refused(evidence_import_refusal::version, "version " + std::to_string(opening.Version));
		}
		auto wire = ew::document{ };
		if (const auto error = glz::read<strict>(wire, aDocument); error) {
			return refused(error.ec == glz::error_code::unknown_key ? evidence_import_refusal::unknown_field : evidence_import_refusal::malformed);
		}
		if (wire.Claims.empty() && wire.Checks.empty() && wire.Results.empty() && wire.Questions.empty()) {
			return refused(evidence_import_refusal::empty);
		}
		if (wire.Claims.size() > maximum_evidence_claims || wire.Checks.size() > maximum_evidence_checks || wire.Results.size() > maximum_evidence_results
			|| wire.Questions.size() > maximum_evidence_questions) {
			return refused(evidence_import_refusal::bounds);
		}
		auto document = evidence_import_document{ };
		{
			const auto source = enum_from(wire.Producer.Kind, all_sources);
			if (!source) {
				return refused(evidence_import_refusal::invalid_value, "producer.kind");
			}
			if (wire.Producer.Name.empty() || !valid_name(wire.Producer.Name) || !valid_name(wire.Producer.Version.value_or(""))) {
				return refused(evidence_import_refusal::invalid_value, "producer.name");
			}
			document.Origin = { .Source = *source, .Producer = wire.Producer.Name, .Version = wire.Producer.Version.value_or("") };
		}
		auto claimKeys = std::set<std::string, std::less<>>{ };
		for (std::size_t index = 0; index < wire.Claims.size(); ++index) {
			const auto& claim = wire.Claims[index];
			const auto  kind  = enum_from(claim.Kind, all_kinds);
			if (claim.Key.empty() || !valid_name(claim.Key) || !claimKeys.insert(claim.Key).second) {
				return refused(evidence_import_refusal::invalid_value, at("claims", index, "key"));
			}
			if (!kind) {
				return refused(evidence_import_refusal::invalid_value, at("claims", index, "kind"));
			}
			if (claim.Text.empty() || !valid_text(claim.Text)) {
				return refused(evidence_import_refusal::invalid_value, at("claims", index, "text"));
			}
			document.Claims.push_back({ .Key = claim.Key, .Kind = *kind, .Text = claim.Text });
		}
		auto checkKeys = std::set<std::string, std::less<>>{ };
		for (std::size_t index = 0; index < wire.Checks.size(); ++index) {
			const auto& check  = wire.Checks[index];
			const auto  method = enum_from(check.Method, all_methods);
			if (check.Key.empty() || !valid_name(check.Key) || !checkKeys.insert(check.Key).second) {
				return refused(evidence_import_refusal::invalid_value, at("checks", index, "key"));
			}
			if (!method) {
				return refused(evidence_import_refusal::invalid_value, at("checks", index, "method"));
			}
			auto entry = evidence_import_document::check{ .Key = check.Key, .Method = *method, .Title = check.Title, .Command = check.Command.value_or("") };
			if (check.Formal) {
				const auto& in = *check.Formal;
				entry.Formal   = { .Property       = in.Property.value_or(""),
								   .Subject        = in.Subject.value_or(""),
								   .Tool           = in.Tool.value_or(""),
								   .Assumptions    = in.Assumptions.value_or(""),
								   .Bounds         = in.Bounds.value_or(""),
								   .Correspondence = in.Correspondence.value_or("") };
			}
			if (entry.Title.empty() || !valid_name(entry.Title) || !valid_text(entry.Command) || !valid_formal(entry.Formal) || (check.Formal && !formal(*method))) {
				return refused(evidence_import_refusal::invalid_value, at("checks", index));
			}
			for (const auto& key : check.Claims) {
				if (!claimKeys.contains(key)) {
					return refused(evidence_import_refusal::reference, at("checks", index, "claims"));
				}
			}
			entry.Claims = check.Claims;
			document.Checks.push_back(std::move(entry));
		}
		for (std::size_t index = 0; index < wire.Results.size(); ++index) {
			const auto& result = wire.Results[index];
			if (result.Class != reported_class) {
				return refused(result.Class == "observed" ? evidence_import_refusal::observed : evidence_import_refusal::invalid_value, at("results", index, "class"));
			}
			const auto outcome = enum_from(result.Outcome, all_outcomes);
			if (!outcome) {
				return refused(evidence_import_refusal::invalid_value, at("results", index, "outcome"));
			}
			if (result.Check.has_value() == result.CheckId.has_value() || (result.Check && !checkKeys.contains(*result.Check))) {
				return refused(evidence_import_refusal::reference, at("results", index, "check"));
			}
			auto entry = evidence_import_document::result{ .CheckKey    = result.Check.value_or(""),
														   .CheckId     = result.CheckId.value_or(0U),
														   .Outcome     = *outcome,
														   .Inputs      = result.Inputs,
														   .Environment = result.Environment.value_or(""),
														   .Expected    = result.Expected.value_or(""),
														   .Actual      = result.Actual.value_or(""),
														   .Limitations = result.Limitations.value_or("") };
			if (!valid_text(entry.Environment) || !valid_text(entry.Expected) || !valid_text(entry.Actual) || !valid_text(entry.Limitations)) {
				return refused(evidence_import_refusal::invalid_value, at("results", index));
			}
			if (entry.Inputs.size() > maximum_evidence_inputs || !std::ranges::all_of(entry.Inputs, [](const std::string& aInput) { return valid_input(aInput); })) {
				return refused(evidence_import_refusal::invalid_value, at("results", index, "inputs"));
			}
			if (result.Material) {
				const auto head = parse_git_commit_oid(result.Material->Head);
				if (!head) {
					return refused(evidence_import_refusal::invalid_value, at("results", index, "material.head"));
				}
				auto material = evidence_material{ .Head = *head };
				for (const auto& file : result.Material->Working) {
					auto object = file.Object ? parse_git_entry_oid(*file.Object) : std::nullopt;
					if (!valid_path(file.Path) || file.Object.has_value() == file.Deleted || (file.Object && (!object || object->algorithm() != head->algorithm()))) {
						return refused(evidence_import_refusal::invalid_value, at("results", index, "material.working"));
					}
					material.Working.push_back({ .Path = file.Path, .Object = object });
				}
				std::ranges::sort(material.Working, { }, &evidence_working_file::Path);
				if (std::ranges::adjacent_find(material.Working, { }, &evidence_working_file::Path) != material.Working.end()
					|| material.Working.size() > maximum_local_draft_working_files) {
					return refused(evidence_import_refusal::invalid_value, at("results", index, "material.working"));
				}
				entry.Material = std::move(material);
			}
			if (result.Artifacts.size() > maximum_evidence_artifacts) {
				return refused(evidence_import_refusal::bounds, at("results", index, "artifacts"));
			}
			for (const auto& artifact : result.Artifacts) {
				const auto local = artifact.Path.has_value();
				if (artifact.Name.empty() || !valid_name(artifact.Name) || local == artifact.Url.has_value() || (local && !valid_path(*artifact.Path))
					|| (!local && (!plain_text(*artifact.Url, 4096U, false) || !(artifact.Url->starts_with("https://") || artifact.Url->starts_with("http://"))))) {
					return refused(evidence_import_refusal::invalid_value, at("results", index, "artifacts"));
				}
				entry.Artifacts.push_back({ .Name = artifact.Name, .Path = artifact.Path.value_or(""), .Url = artifact.Url.value_or("") });
			}
			document.Results.push_back(std::move(entry));
		}
		for (std::size_t index = 0; index < wire.Questions.size(); ++index) {
			const auto& question = wire.Questions[index];
			if (question.Text.empty() || !valid_text(question.Text)) {
				return refused(evidence_import_refusal::invalid_value, at("questions", index, "text"));
			}
			if (question.Claim && !claimKeys.contains(*question.Claim)) {
				return refused(evidence_import_refusal::reference, at("questions", index, "claim"));
			}
			document.Questions.push_back({ .Text = question.Text, .ClaimKey = question.Claim.value_or("") });
		}
		const auto digest = sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aDocument.data()), aDocument.size() });
		document.Digest   = lower_hex(std::span<const std::uint8_t>{ digest });
		return document;
	}

	auto evidence_artifact_requests(const evidence_import_document& aDocument) -> std::vector<evidence_artifact_request>
	{
		auto requests = std::vector<evidence_artifact_request>{ };
		for (const auto& result : aDocument.Results) {
			requests.insert(requests.end(), result.Artifacts.begin(), result.Artifacts.end());
		}
		return requests;
	}

	auto import_evidence(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const evidence_import_document& aDocument, const std::span<const evidence_artifact> aArtifacts,
						 const std::int64_t aNow) -> std::expected<local_draft, evidence_import_refused>
	{
		if (aDraft.Version != aExpectedVersion) {
			return refused(evidence_import_refusal::stale);
		}
		const auto& existing = aDraft.Evidence;
		if (std::ranges::find(existing.Imports, aDocument.Digest, &evidence_import::Digest) != existing.Imports.end()) {
			return refused(evidence_import_refusal::duplicate);
		}
		if (existing.Imports.size() >= maximum_evidence_imports || existing.Claims.size() + aDocument.Claims.size() > maximum_evidence_claims
			|| existing.Checks.size() + aDocument.Checks.size() > maximum_evidence_checks || existing.Results.size() + aDocument.Results.size() > maximum_evidence_results
			|| existing.Questions.size() + aDocument.Questions.size() > maximum_evidence_questions) {
			return refused(evidence_import_refusal::bounds);
		}
		if (aArtifacts.size() != evidence_artifact_requests(aDocument).size() || !std::ranges::all_of(aArtifacts, [](const evidence_artifact& aArtifact) { return valid_artifact(aArtifact); })) {
			return refused(evidence_import_refusal::invalid_value, "artifacts");
		}
		for (std::size_t index = 0; index < aDocument.Results.size(); ++index) {
			const auto id = aDocument.Results[index].CheckId;
			if (id != 0U && !has_check(existing, id)) {
				return refused(evidence_import_refusal::reference, at("results", index, "check_id"));
			}
		}
		auto       next     = advanced(aDraft, aNow);
		auto&      evidence = next.Evidence;
		const auto ordinal  = static_cast<std::uint64_t>(evidence.Imports.size()) + 1U;
		auto       origin   = aDocument.Origin;
		origin.Import       = ordinal;
		auto claims         = std::map<std::string, std::uint64_t, std::less<>>{ };
		for (const auto& claim : aDocument.Claims) {
			const auto id = take_id(evidence);
			claims.emplace(claim.Key, id);
			evidence.Claims.push_back({ .Id = id, .Kind = claim.Kind, .Text = claim.Text, .Disposition = evidence_disposition::proposed, .Origin = origin });
		}
		auto checks = std::map<std::string, std::uint64_t, std::less<>>{ };
		for (const auto& check : aDocument.Checks) {
			const auto id = take_id(evidence);
			checks.emplace(check.Key, id);
			auto linked = std::vector<std::uint64_t>{ };
			for (const auto& key : check.Claims) {
				linked.push_back(claims.at(key));
			}
			std::ranges::sort(linked);
			const auto [first, last] = std::ranges::unique(linked);
			linked.erase(first, last);
			evidence.Checks.push_back({ .Id          = id,
										.Method      = check.Method,
										.Title       = check.Title,
										.Command     = check.Command,
										.Claims      = std::move(linked),
										.Formal      = formal(check.Method) ? check.Formal : formal_scope{ },
										.Disposition = evidence_disposition::proposed,
										.Origin      = origin });
		}
		auto artifact = aArtifacts.begin();
		for (const auto& result : aDocument.Results) {
			const auto id    = take_id(evidence);
			auto       entry = evidence_result{ .Id          = id,
												.Check       = result.CheckId != 0U ? result.CheckId : checks.at(result.CheckKey),
												.Outcome     = result.Outcome,
												.Origin      = origin,
												.Material    = result.Material,
												.Candidate   = result.Material ? bind_material(aDraft, *result.Material) : 0U,
												.Inputs      = result.Inputs,
												.Environment = result.Environment,
												.Expected    = result.Expected,
												.Actual      = result.Actual,
												.Limitations = result.Limitations,
												.Recorded    = aNow };
			for (std::size_t index = 0; index < result.Artifacts.size(); ++index) {
				entry.Artifacts.push_back(*artifact++);
			}
			evidence.Results.push_back(std::move(entry));
		}
		for (const auto& question : aDocument.Questions) {
			const auto id = take_id(evidence);
			evidence.Questions.push_back({ .Id          = id,
										   .Text        = question.Text,
										   .Claim       = question.ClaimKey.empty() ? 0U : claims.at(question.ClaimKey),
										   .Disposition = evidence_disposition::proposed,
										   .Origin      = origin });
		}
		evidence.Imports.push_back({ .Ordinal   = ordinal,
									 .Origin    = origin,
									 .Digest    = aDocument.Digest,
									 .Claims    = aDocument.Claims.size(),
									 .Checks    = aDocument.Checks.size(),
									 .Results   = aDocument.Results.size(),
									 .Questions = aDocument.Questions.size(),
									 .At        = aNow });
		if (!valid(evidence)) {
			return refused(evidence_import_refusal::invalid_value);
		}
		return next;
	}

	// --- Binding and currency -----------------------------------------------------------

	auto candidate_material(const local_draft_candidate& aCandidate) -> evidence_material
	{
		auto material = evidence_material{ .Head = aCandidate.Head };
		for (const auto& path : aCandidate.Included) {
			const auto change = std::ranges::find(aCandidate.Changes, path, &draft_path_change::Path);
			if (change == aCandidate.Changes.end()) {
				continue;
			}
			material.Working.push_back({ .Path = path, .Object = change->Right ? std::optional{ change->Right->Object } : std::nullopt });
		}
		std::ranges::sort(material.Working, { }, &evidence_working_file::Path);
		return material;
	}

	auto bind_material(const local_draft& aDraft, const evidence_material& aMaterial) -> std::uint64_t
	{
		for (const auto& candidate : std::views::reverse(aDraft.Candidates)) {
			if (candidate.Head != aMaterial.Head) {
				continue;
			}
			// An included file the comparison does not list (its working bytes
			// equal the base's) has no identity here to compare: not bound.
			const auto listed = std::ranges::all_of(candidate.Included, [&candidate](const std::string& aPath) {
				return std::ranges::find(candidate.Changes, aPath, &draft_path_change::Path) != candidate.Changes.end();
			});
			if (listed && candidate_material(candidate) == aMaterial) {
				return candidate.Ordinal;
			}
		}
		return 0U;
	}

	auto currency(const local_draft& aDraft, const evidence_result& aResult, const std::uint64_t aCandidate) -> evidence_currency
	{
		if (aResult.Candidate == 0U) {
			return evidence_currency::unbound;
		}
		if (aResult.Candidate == aCandidate) {
			return evidence_currency::current;
		}
		if (aResult.Candidate > aCandidate) {
			return evidence_currency::later;
		}
		const auto* from = find_candidate(aDraft, aResult.Candidate);
		const auto* to   = find_candidate(aDraft, aCandidate);
		// Without its inputs, any later candidate may change what it tested; a
		// new base may change behaviour even when the diff reads the same.
		if (from == nullptr || to == nullptr || aResult.Inputs.empty() || from->Base != to->Base) {
			return evidence_currency::needs_recheck;
		}
		for (const auto& input : aResult.Inputs) {
			if (changes_under(*from, input) != changes_under(*to, input)) {
				return evidence_currency::needs_recheck;
			}
		}
		return evidence_currency::carried;
	}

	auto evidence_for_claim(const local_draft& aDraft, const std::uint64_t aClaim, const std::uint64_t aCandidate) -> claim_evidence
	{
		auto counts = claim_evidence{ };
		for (const auto& check : aDraft.Evidence.Checks) {
			if (check.Disposition != evidence_disposition::accepted || std::ranges::find(check.Claims, aClaim) == check.Claims.end()) {
				continue;
			}
			auto any = false;
			for (const auto& result : aDraft.Evidence.Results) {
				if (result.Check != check.Id) {
					continue;
				}
				const auto state = currency(aDraft, result, aCandidate);
				if (state == evidence_currency::later) {
					continue;
				}
				any = true;
				if (state == evidence_currency::unbound) {
					++counts.Unbound;
				} else if (state == evidence_currency::needs_recheck) {
					++counts.NeedsRecheck;
				} else if (result.Outcome == evidence_outcome::pass) {
					++counts.Passing;
				} else if (result.Outcome == evidence_outcome::fail) {
					++counts.Failing;
				} else {
					++counts.Other;
				}
			}
			if (!any) {
				++counts.Planned;
			}
		}
		return counts;
	}

	// --- Selection ------------------------------------------------------------------------

	auto selected_evidence(const local_draft& aDraft) -> evidence_selection
	{
		auto        selection = evidence_selection{ };
		const auto& evidence  = aDraft.Evidence;
		for (const auto& claim : evidence.Claims) {
			if (claim.Selected && claim.Disposition == evidence_disposition::accepted) {
				auto shared    = claim;
				shared.Earlier = { };
				selection.Claims.push_back(std::move(shared));
			}
		}
		auto checks = std::set<std::uint64_t>{ };
		for (const auto& result : evidence.Results) {
			const auto check = std::ranges::find(evidence.Checks, result.Check, &evidence_check::Id);
			if (result.Selected && check != evidence.Checks.end() && check->Disposition == evidence_disposition::accepted) {
				selection.Results.push_back(result);
				checks.insert(result.Check);
			}
		}
		for (const auto& check : evidence.Checks) {
			if (checks.contains(check.Id)) {
				auto shared = check;
				// A link to a claim that is not shared is not shared either.
				std::erase_if(shared.Claims, [&selection](const std::uint64_t aClaim) {
					return std::ranges::find(selection.Claims, aClaim, &evidence_claim::Id) == selection.Claims.end();
				});
				selection.Checks.push_back(std::move(shared));
			}
		}
		for (const auto& question : evidence.Questions) {
			if (question.Selected && question.Disposition == evidence_disposition::accepted) {
				auto shared = question;
				if (std::ranges::find(selection.Claims, shared.Claim, &evidence_claim::Id) == selection.Claims.end()) {
					shared.Claim = 0U;
				}
				selection.Questions.push_back(std::move(shared));
			}
		}
		return selection;
	}

	// --- Stored form ------------------------------------------------------------------------

	auto encode_draft_evidence(const draft_evidence& aEvidence) -> std::string
	{
		auto wire = ew::evidence{ .NextId = aEvidence.NextId };
		for (const auto& claim : aEvidence.Claims) {
			wire.Claims.push_back({ .Id          = claim.Id,
									.Kind        = std::string{ name(claim.Kind) },
									.Text        = claim.Text,
									.Earlier     = claim.Earlier,
									.Disposition = std::string{ name(claim.Disposition) },
									.Origin      = to_wire(claim.Origin),
									.Selected    = claim.Selected });
		}
		for (const auto& check : aEvidence.Checks) {
			auto entry = ew::check{ .Id          = check.Id,
									.Method      = std::string{ name(check.Method) },
									.Title       = check.Title,
									.Command     = check.Command,
									.Claims      = check.Claims,
									.Disposition = std::string{ name(check.Disposition) },
									.Origin      = to_wire(check.Origin) };
			if (check.Formal != formal_scope{ }) {
				const auto& in = check.Formal;
				entry.Formal   = ew::formal{ .Property       = in.Property,
											 .Subject        = in.Subject,
											 .Tool           = in.Tool,
											 .Assumptions    = in.Assumptions,
											 .Bounds         = in.Bounds,
											 .Correspondence = in.Correspondence };
			}
			wire.Checks.push_back(std::move(entry));
		}
		for (const auto& result : aEvidence.Results) {
			auto entry = ew::result{ .Id          = result.Id,
									 .Check       = result.Check,
									 .Outcome     = std::string{ name(result.Outcome) },
									 .Class       = std::string{ reported_class },
									 .Origin      = to_wire(result.Origin),
									 .Material    = result.Material ? std::optional{ to_wire(*result.Material) } : std::nullopt,
									 .Candidate   = result.Candidate,
									 .Inputs      = result.Inputs,
									 .Environment = result.Environment,
									 .Expected    = result.Expected,
									 .Actual      = result.Actual,
									 .Limitations = result.Limitations,
									 .Recorded    = result.Recorded,
									 .Selected    = result.Selected };
			for (const auto& artifact : result.Artifacts) {
				entry.Artifacts.push_back({ .Name      = artifact.Name,
											.State     = std::string{ name(artifact.State) },
											.Reference = artifact.Reference,
											.Object    = artifact.Object ? std::optional{ std::string{ canonical_hex(*artifact.Object).view() } } : std::nullopt,
											.Size      = artifact.Size });
			}
			wire.Results.push_back(std::move(entry));
		}
		for (const auto& question : aEvidence.Questions) {
			wire.Questions.push_back({ .Id          = question.Id,
									   .Text        = question.Text,
									   .Claim       = question.Claim,
									   .Resolved    = question.Resolved,
									   .Disposition = std::string{ name(question.Disposition) },
									   .Origin      = to_wire(question.Origin),
									   .Selected    = question.Selected });
		}
		for (const auto& record : aEvidence.Imports) {
			wire.Imports.push_back({ .Ordinal   = record.Ordinal,
									 .Origin    = to_wire(record.Origin),
									 .Digest    = record.Digest,
									 .Claims    = record.Claims,
									 .Checks    = record.Checks,
									 .Results   = record.Results,
									 .Questions = record.Questions,
									 .At        = record.At });
		}
		auto text = std::string{ };
		if (glz::write_json(wire, text)) {
			return { };
		}
		return text;
	}

	auto decode_draft_evidence(const std::string_view aText) -> std::optional<draft_evidence>
	{
		auto wire = ew::evidence{ };
		if (glz::read<strict>(wire, aText)) {
			return std::nullopt;
		}
		auto evidence = draft_evidence{ .NextId = wire.NextId };
		for (const auto& claim : wire.Claims) {
			const auto kind        = enum_from(claim.Kind, all_kinds);
			const auto disposition = enum_from(claim.Disposition, all_dispositions);
			const auto origin      = from_wire(claim.Origin);
			if (!kind || !disposition || !origin) {
				return std::nullopt;
			}
			evidence.Claims.push_back(
				{ .Id = claim.Id, .Kind = *kind, .Text = claim.Text, .Earlier = claim.Earlier, .Disposition = *disposition, .Origin = *origin, .Selected = claim.Selected });
		}
		for (const auto& check : wire.Checks) {
			const auto method      = enum_from(check.Method, all_methods);
			const auto disposition = enum_from(check.Disposition, all_dispositions);
			const auto origin      = from_wire(check.Origin);
			if (!method || !disposition || !origin) {
				return std::nullopt;
			}
			auto entry = evidence_check{
				.Id = check.Id, .Method = *method, .Title = check.Title, .Command = check.Command, .Claims = check.Claims, .Disposition = *disposition, .Origin = *origin
			};
			if (check.Formal) {
				const auto& in = *check.Formal;
				entry.Formal   = { .Property       = in.Property,
								   .Subject        = in.Subject,
								   .Tool           = in.Tool,
								   .Assumptions    = in.Assumptions,
								   .Bounds         = in.Bounds,
								   .Correspondence = in.Correspondence };
			}
			evidence.Checks.push_back(std::move(entry));
		}
		for (const auto& result : wire.Results) {
			// Only reported results are stored; anything else is not this build's record.
			const auto outcome = enum_from(result.Outcome, all_outcomes);
			const auto origin  = from_wire(result.Origin);
			if (result.Class != reported_class || !outcome || !origin) {
				return std::nullopt;
			}
			auto entry = evidence_result{ .Id          = result.Id,
										  .Check       = result.Check,
										  .Outcome     = *outcome,
										  .Origin      = *origin,
										  .Candidate   = result.Candidate,
										  .Inputs      = result.Inputs,
										  .Environment = result.Environment,
										  .Expected    = result.Expected,
										  .Actual      = result.Actual,
										  .Limitations = result.Limitations,
										  .Recorded    = result.Recorded,
										  .Selected    = result.Selected };
			if (result.Material) {
				entry.Material = from_wire(*result.Material);
				if (!entry.Material) {
					return std::nullopt;
				}
			}
			for (const auto& artifact : result.Artifacts) {
				const auto state  = enum_from(artifact.State, all_states);
				auto       object = artifact.Object ? parse_git_entry_oid(*artifact.Object) : std::nullopt;
				if (!state || (artifact.Object && !object)) {
					return std::nullopt;
				}
				entry.Artifacts.push_back({ .Name = artifact.Name, .State = *state, .Reference = artifact.Reference, .Object = object, .Size = artifact.Size });
			}
			evidence.Results.push_back(std::move(entry));
		}
		for (const auto& question : wire.Questions) {
			const auto disposition = enum_from(question.Disposition, all_dispositions);
			const auto origin      = from_wire(question.Origin);
			if (!disposition || !origin) {
				return std::nullopt;
			}
			evidence.Questions.push_back({ .Id          = question.Id,
										   .Text        = question.Text,
										   .Claim       = question.Claim,
										   .Resolved    = question.Resolved,
										   .Disposition = *disposition,
										   .Origin      = *origin,
										   .Selected    = question.Selected });
		}
		for (const auto& record : wire.Imports) {
			const auto origin = from_wire(record.Origin);
			if (!origin) {
				return std::nullopt;
			}
			evidence.Imports.push_back({ .Ordinal   = record.Ordinal,
										 .Origin    = *origin,
										 .Digest    = record.Digest,
										 .Claims    = record.Claims,
										 .Checks    = record.Checks,
										 .Results   = record.Results,
										 .Questions = record.Questions,
										 .At        = record.At });
		}
		if (!valid(evidence)) {
			return std::nullopt;
		}
		return evidence;
	}
}
