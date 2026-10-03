#include "bpr/core/review_composition.hpp"

#include <algorithm>
#include <type_traits>

namespace bpr::core {
	namespace {
		auto valid_key(const private_source_key& aKey) -> bool
		{
			return std::visit([](const auto& aValue) {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<value_type, private_turn_source>) {
					return valid_private_id(aValue.Thread) && valid_private_id(aValue.Turn) && valid_private_id(aValue.Revision);
				} else if constexpr (std::is_same_v<value_type, private_finding_source>) {
					return valid_private_id(aValue.Run);
				} else {
					return valid_private_id(aValue.Composition) && valid_private_id(aValue.Revision);
				}
			},
							  aKey);
		}
		auto same_object(const private_source_key& aLeft, const private_source_key& aRight) -> bool
		{
			if (aLeft.index() != aRight.index()) {
				return false;
			}
			return std::visit([&](const auto& aValue) {
				using value_type  = std::decay_t<decltype(aValue)>;
				const auto& right = std::get<value_type>(aRight);
				if constexpr (std::is_same_v<value_type, private_turn_source>) {
					return aValue.Thread == right.Thread && aValue.Turn == right.Turn;
				} else if constexpr (std::is_same_v<value_type, private_finding_source>) {
					return aValue == right;
				} else {
					return aValue.Composition == right.Composition;
				}
			},
							  aLeft);
		}
		// Fixed-width integers and length-framed bytes avoid concatenation aliases.
		class identity_encoder
		{
		public:
			explicit identity_encoder(std::string_view aDomain)
			{
				text(aDomain);
				number(1);
			}
			void number(std::uint64_t aValue)
			{
				for (unsigned shift = 0; shift < 64; shift += 8) {
					Bytes.push_back(static_cast<char>((aValue >> shift) & 255U));
				}
			}
			void text(std::string_view aValue)
			{
				number(aValue.size());
				Bytes.append(aValue);
			}
			template <typename TIdentity>
			void fixed(const TIdentity& aValue)
			{
				const auto& bytes = aValue.bytes();
				text({ reinterpret_cast<const char*>(bytes.data()), bytes.size() });
			}
			void key(const private_source_key& aKey)
			{
				number(aKey.index());
				std::visit([&](const auto& aValue) {
					using value_type = std::decay_t<decltype(aValue)>;
					if constexpr (std::is_same_v<value_type, private_turn_source>) {
						number(aValue.Thread.value());
						number(aValue.Turn.value());
						number(aValue.Revision.value());
					} else if constexpr (std::is_same_v<value_type, private_finding_source>) {
						number(aValue.Run.value());
						number(aValue.Ordinal);
					} else {
						number(aValue.Composition.value());
						number(aValue.Revision.value());
					}
				},
						   aKey);
			}
			void scope(const private_review_scope& aScope)
			{
				number(aScope.Profile.value());
				fixed(aScope.Incarnation);
				number(aScope.Host.value());
				number(aScope.Account.value());
				number(aScope.Repository.value());
				number(aScope.PullRequest.value());
			}
			void source(const private_source_reference& aSource)
			{
				scope(aSource.Scope);
				key(aSource.Key);
				number(aSource.Order.value());
				fixed(aSource.Digest);
				number(aSource.BodyBytes);
			}
			auto digest() const -> private_body_digest { return private_digest(Bytes); }
			auto size() const -> std::size_t { return Bytes.size(); }

		private:
			std::string Bytes;
		};
		auto target_shape(const private_composition_target& aTarget) -> bool
		{
			return valid_private_binding(aTarget.Binding) && aTarget.Binding.Comparison
				&& aTarget.Line.Comparison == *aTarget.Binding.Comparison && aTarget.Line.Side == diff_side::right;
		}
		auto valid_source_actor(const private_source_observation& aSource) -> bool
		{
			if (const auto* finding = std::get_if<private_finding_source>(&aSource.Source.Key)) {
				const auto* actor = std::get_if<private_agent_actor>(&aSource.Actor);
				return actor && actor->Run == finding->Run && aSource.FindingVerdict
					&& aSource.FindingVerdict->Refusal != agent_finding_refusal::malformed
					&& aSource.FindingVerdict->State <= agent_finding_anchor_state::stale
					&& aSource.FindingVerdict->Refusal <= agent_finding_refusal::malformed
					&& aSource.FindingVerdict->AnalyzedBaseOid.valid() && aSource.FindingVerdict->AnalyzedHeadOid.valid()
					&& (!aSource.Target || aSource.FindingVerdict->anchored());
			}
			if (aSource.FindingVerdict) {
				return false;
			}
			if (const auto* reviewer = std::get_if<private_reviewer_actor>(&aSource.Actor)) {
				return !reviewer->Account || (valid_private_id(*reviewer->Account) && *reviewer->Account == aSource.Source.Scope.Account);
			}
			if (const auto* agent = std::get_if<private_agent_actor>(&aSource.Actor)) {
				return valid_private_id(agent->Run);
			}
			return true;
		}

		auto source_observation(std::span<const private_source_observation> aObserved, const private_source_reference& aSource) -> const private_source_observation*
		{
			const auto found = std::ranges::find_if(aObserved, [&](const auto& aValue) { return aValue.Source.Scope == aSource.Scope && aValue.Source.Key == aSource.Key; });
			return found == aObserved.end() ? nullptr : &*found;
		}
		auto validate_selected_source(const private_review_scope& aScope, const private_source_selection& aSelected, const private_source_observation& aObserved) -> std::optional<private_review_refusal>
		{
			if (aSelected.Source.Scope != aScope || aObserved.Source.Scope != aScope) {
				return private_review_refusal::wrong_scope;
			}
			if (aObserved.Source.Digest != aSelected.Source.Digest) {
				return private_review_refusal::digest_mismatch;
			}
			if (aObserved.Source != aSelected.Source || aObserved.Latest != aSelected.ExpectedLatest) {
				return private_review_refusal::source_changed;
			}
			if (aObserved.Completion != private_revision_completion::complete) {
				return private_review_refusal::incomplete_source;
			}
			if (!valid_source_actor(aObserved)) {
				return private_review_refusal::invalid_actor;
			}
			if (aObserved.Availability > private_subject_availability::unplaceable) {
				return private_review_refusal::invalid_subject;
			}
			if (aObserved.Target && (!target_shape(*aObserved.Target) || aObserved.Target->Binding.Scope != aScope)) {
				return private_review_refusal::invalid_target;
			}
			if (aObserved.CopyBody && (!aObserved.CopyBody->Digest.valid() || aObserved.CopyBody->Bytes > maximum_private_assistant_body_bytes)) {
				return private_review_refusal::bounds;
			}
			if (!std::holds_alternative<private_finding_source>(aObserved.Source.Key) && aObserved.CopyBody
				&& (aObserved.CopyBody->Digest != aObserved.Source.Digest || aObserved.CopyBody->Bytes != aObserved.Source.BodyBytes)) {
				return private_review_refusal::digest_mismatch;
			}
			return std::nullopt;
		}
		auto valid_scalar(std::string_view aValue, std::size_t aLimit) -> bool
		{
			if (aValue.empty() || aValue.size() > aLimit) {
				return false;
			}
			for (std::size_t i = 0; i < aValue.size();) {
				const auto first = static_cast<unsigned char>(aValue[i++]);
				if (first < 128) {
					if (first < 32 || first == 127) {
						return false;
					}
					continue;
				}
				unsigned      count   = 0;
				std::uint32_t scalar  = 0;
				std::uint32_t minimum = 0;
				if (first >= 0xC2 && first <= 0xDF) {
					count   = 1;
					scalar  = first & 31U;
					minimum = 128;
				} else if (first >= 0xE0 && first <= 0xEF) {
					count   = 2;
					scalar  = first & 15U;
					minimum = 2048;
				} else if (first >= 0xF0 && first <= 0xF4) {
					count   = 3;
					scalar  = first & 7U;
					minimum = 65536;
				} else {
					return false;
				}
				if (aValue.size() - i < count) {
					return false;
				}
				for (unsigned n = 0; n < count; ++n) {
					const auto part = static_cast<unsigned char>(aValue[i++]);
					if ((part & 0xC0U) != 0x80U) {
						return false;
					}
					scalar = (scalar << 6U) | (part & 63U);
				}
				if (scalar < minimum || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF) || (scalar >= 0x80 && scalar <= 0x9F)) {
					return false;
				}
			}
			return true;
		}
		struct located_fact
		{
			agent_fact_field    Field;
			agent_fact_location Location;
			std::string_view    Value;
		};
		auto add_fact(std::vector<located_fact>& aFacts, std::size_t& aBytes, const std::optional<std::string>& aFact, agent_fact_field aField, agent_fact_location aLocation) -> bool
		{
			if (!aFact) {
				return true;
			}
			const auto limit = aField <= agent_fact_field::tool_version ? maximum_runtime_fact_bytes : maximum_remote_run_id_bytes;
			if (!valid_scalar(*aFact, limit)) {
				return false;
			}
			aBytes += aFact->size();
			aFacts.push_back({ aField, aLocation, *aFact });
			return aBytes <= maximum_runtime_metadata_bytes;
		}
		auto add_settings(std::vector<located_fact>& aFacts, std::size_t& aBytes, const agent_setting_facts& aSettings, agent_fact_location aLocation) -> bool
		{
			return add_fact(aFacts, aBytes, aSettings.Model, agent_fact_field::model, aLocation)
				&& add_fact(aFacts, aBytes, aSettings.Effort, agent_fact_field::effort, aLocation);
		}
		auto add_reports(std::vector<located_fact>& aFacts, std::size_t& aBytes, std::span<const agent_reported_facts> aReports) -> bool
		{
			for (std::size_t i = 0; i < aReports.size(); ++i) {
				const auto& report = aReports[i];
				if (!valid_scalar(report.Basis, maximum_runtime_fact_bytes)) {
					return false;
				}
				aBytes += report.Basis.size();
				const auto location = agent_fact_location{ agent_fact_basis::protocol_reported, i };
				if (!add_settings(aFacts, aBytes, report.Settings, location)
					|| !add_fact(aFacts, aBytes, report.ToolVersion, agent_fact_field::tool_version, location)
					|| !add_fact(aFacts, aBytes, report.RemoteSession, agent_fact_field::remote_session, location)
					|| !add_fact(aFacts, aBytes, report.RemoteThread, agent_fact_field::remote_thread, location)
					|| !add_fact(aFacts, aBytes, report.RemoteTurn, agent_fact_field::remote_turn, location)
					|| !add_fact(aFacts, aBytes, report.RemoteRun, agent_fact_field::remote_run, location)) {
					return false;
				}
			}
			return aBytes <= maximum_runtime_metadata_bytes;
		}
		auto conflicts(std::span<const located_fact> aFacts) -> std::vector<agent_provenance_conflict>
		{
			auto result = std::vector<agent_provenance_conflict>{ };
			for (std::size_t i = 0; i < aFacts.size(); ++i) {
				for (std::size_t j = i + 1; j < aFacts.size(); ++j) {
					if (aFacts[i].Field == aFacts[j].Field && aFacts[i].Value != aFacts[j].Value) {
						result.push_back({ aFacts[i].Field, aFacts[i].Location, aFacts[j].Location });
					}
				}
			}
			return result;
		}
		auto valid_run_sources(const agent_run_provenance& aRun) -> bool
		{
			std::uint64_t bytes = 0;
			for (const auto& selected : aRun.SelectedPrivateSources) {
				const auto& source = selected.Source;
				if (source.Scope != aRun.Binding.Scope || source.Order >= aRun.Order
					|| source.BodyBytes > maximum_selected_private_source_bytes - bytes) {
					return false;
				}
				bytes += source.BodyBytes;
			}
			return true;
		}

		auto run_terminal_valid(const agent_run_provenance& aRun) -> bool
		{
			if (aRun.Outcome > private_run_outcome::interrupted || aRun.Completion > private_revision_completion::cancelled) {
				return false;
			}
			switch (aRun.Outcome) {
			case private_run_outcome::admitted:         return aRun.Completion == private_revision_completion::incomplete && !aRun.OutputDigest;
			case private_run_outcome::succeeded:        return aRun.Completion == private_revision_completion::complete && aRun.OutputDigest && aRun.OutputDigest->valid();
			case private_run_outcome::cancelled:        return aRun.Completion == private_revision_completion::cancelled || aRun.Completion == private_revision_completion::incomplete;
			case private_run_outcome::failed:
			case private_run_outcome::timed_out:
			case private_run_outcome::protocol_failure:
			case private_run_outcome::interrupted:      return aRun.Completion == private_revision_completion::failed || aRun.Completion == private_revision_completion::incomplete;
			}
			return false;
		}
		void encode_finding_anchor(identity_encoder& aEncoder, const agent_finding_anchor& aAnchor)
		{
			aEncoder.number(aAnchor.Line.has_value());
			if (aAnchor.Line) {
				aEncoder.fixed(aAnchor.Line->Comparison);
				aEncoder.number(static_cast<unsigned>(aAnchor.Line->Side));
				aEncoder.number(aAnchor.Line->Line);
			}
			aEncoder.number(aAnchor.Boundary.has_value());
			if (aAnchor.Boundary) {
				aEncoder.fixed(aAnchor.Boundary->Comparison);
				aEncoder.number(static_cast<unsigned>(aAnchor.Boundary->Side));
				aEncoder.number(aAnchor.Boundary->Boundary);
			}
			aEncoder.number(aAnchor.Segment.has_value());
			if (aAnchor.Segment) {
				aEncoder.fixed(*aAnchor.Segment);
			}
			aEncoder.number(aAnchor.Group.has_value());
			if (aAnchor.Group) {
				aEncoder.fixed(*aAnchor.Group);
			}
			aEncoder.number(aAnchor.Move.has_value());
			if (aAnchor.Move) {
				aEncoder.fixed(*aAnchor.Move);
			}
		}
		auto finding_encoding(const agent_finding& aFinding, const agent_finding_verdict& aVerdict) -> identity_encoder
		{
			auto encoder = identity_encoder{ "bpr.private.finding" };
			encoder.number(aFinding.Provider.value());
			encoder.number(static_cast<unsigned>(aFinding.Severity));
			encoder.number(static_cast<unsigned>(aFinding.Confidence));
			encoder.number(static_cast<unsigned>(aFinding.Category));
			encoder.text(aFinding.Path);
			encoder.number(aFinding.Lines.Begin);
			encoder.number(aFinding.Lines.End);
			encode_finding_anchor(encoder, aFinding.Anchor);
			encoder.text(aFinding.Claim);
			encoder.text(aFinding.Evidence);
			encoder.text(aFinding.Consequence);
			encoder.number(aFinding.SuggestedComment.has_value());
			if (aFinding.SuggestedComment) {
				encoder.text(*aFinding.SuggestedComment);
			}
			encoder.text(aFinding.RequiredContext);
			encoder.text(canonical_hex(aFinding.AnalyzedBaseOid).view());
			encoder.text(canonical_hex(aFinding.AnalyzedHeadOid).view());
			encoder.number(aFinding.AnalyzedCommits.size());
			for (const auto& commit : aFinding.AnalyzedCommits) {
				encoder.text(canonical_hex(commit).view());
			}
			encoder.number(static_cast<unsigned>(aVerdict.State));
			encoder.number(static_cast<unsigned>(aVerdict.Refusal));
			encoder.text(canonical_hex(aVerdict.AnalyzedBaseOid).view());
			encoder.text(canonical_hex(aVerdict.AnalyzedHeadOid).view());
			return encoder;
		}
		auto valid_workspace(const review_composition_state& aState) -> bool
		{
			return valid_private_id(aState.Id) && valid_private_binding(aState.Binding)
				&& aState.Mode <= review_composition_mode::agent_assisted && aState.Lifecycle <= review_composition_lifecycle::abandoned
				&& aState.SourceCount > 0 && aState.SourceCount <= maximum_private_sources && aState.SourceSetDigest.valid()
				&& (!aState.Target || (target_shape(*aState.Target) && aState.Target->Binding.Scope == aState.Binding.Scope));
		}
		auto valid_candidate(const review_composition_state& aState, const review_composition_revision& aRevision) -> bool
		{
			const auto* key   = std::get_if<private_composition_source>(&aRevision.Identity.Key);
			const auto  actor = private_source_observation{ .Source = aRevision.Identity, .Actor = aRevision.Actor };
			if (!valid_private_source(aRevision.Identity) || !key || key->Composition != aState.Id
				|| aRevision.Identity.Scope != aState.Binding.Scope || aState.Candidate != aRevision.Identity
				|| aRevision.SourceSetDigest != aState.SourceSetDigest || aRevision.Body.size() > maximum_private_reviewer_body_bytes
				|| aRevision.Identity.BodyBytes != aRevision.Body.size() || aRevision.Identity.Digest != private_digest(aRevision.Body)
				|| !valid_source_actor(actor) || aRevision.Derivation.empty() || aRevision.Derivation.size() > maximum_private_sources) {
				return false;
			}
			return std::ranges::all_of(aRevision.Derivation, [&](const auto& aEdge) {
				return aEdge.Kind <= private_derivation_kind::adopted_from_incomplete_run && valid_private_source(aEdge.Source)
					&& aEdge.Source.Scope == aRevision.Identity.Scope && aEdge.Source.Key != aRevision.Identity.Key
					&& aEdge.Source.Order < aRevision.Identity.Order;
			});
		}

		auto revalidate_workspace(const review_composition_state& aState, std::span<const private_source_selection> aSelected,
								  std::span<const private_source_observation> aObserved) -> std::optional<private_review_refusal>
		{
			if (!valid_workspace(aState)) {
				return private_review_refusal::invalid_identity;
			}
			if (aState.Lifecycle != review_composition_lifecycle::editing) {
				return private_review_refusal::invalid_transition;
			}
			auto admitted = admit_composition_sources(aState.Binding.Scope, aSelected, aObserved);
			if (!admitted) {
				return admitted.error();
			}
			if (aState.SourceCount != aSelected.size() || aState.SourceSetDigest != *admitted) {
				return private_review_refusal::source_changed;
			}
			return { };
		}
		auto proposal_actor(const review_composition_state& aState, std::string_view aBody,
							std::span<const private_source_observation> aObserved, const agent_run_provenance* aRun,
							const review_composition_revision* aPredecessor) -> std::expected<private_review_actor, private_review_refusal>
		{
			if (aPredecessor) {
				if (!valid_candidate(aState, *aPredecessor) || aRun) {
					return std::unexpected(private_review_refusal::conflict);
				}
				return private_reviewer_actor{ };
			}
			if (aState.Candidate) {
				return std::unexpected(private_review_refusal::conflict);
			}
			if (aState.Mode == review_composition_mode::agent_assisted) {
				if (!aRun || !valid_run_provenance(*aRun) || aRun->Purpose != private_run_purpose::composition
					|| aRun->EntryPoint != agent_entry_point::workshop_comment || aRun->Binding != aState.Binding
					|| aRun->InputSelectionDigest != aState.SourceSetDigest || aRun->Outcome != private_run_outcome::succeeded
					|| aRun->OutputDigest != private_digest(aBody)) {
					return std::unexpected(private_review_refusal::invalid_provenance);
				}
				return private_agent_actor{ aRun->Run };
			}
			if (aRun) {
				return std::unexpected(private_review_refusal::invalid_mode);
			}
			if (aState.Mode == review_composition_mode::exact_copy) {
				if (aObserved.size() != 1 || !aObserved.front().CopyBody) {
					return std::unexpected(private_review_refusal::invalid_mode);
				}
				const auto& copy = *aObserved.front().CopyBody;
				if (copy.Bytes != aBody.size() || copy.Digest != private_digest(aBody)) {
					return std::unexpected(private_review_refusal::digest_mismatch);
				}
				return aObserved.front().Actor;
			}
			return private_reviewer_actor{ };
		}
	}
	auto source_of(const private_review_revision& aRevision) -> private_source_reference
	{
		const auto& id = aRevision.Identity;
		return { id.Scope, private_turn_source{ id.Thread, id.Turn, id.Revision }, id.Order, id.Digest, aRevision.Body.size() };
	}
	auto valid_private_source(const private_source_reference& aSource) noexcept -> bool
	{
		return valid_private_scope(aSource.Scope) && valid_key(aSource.Key) && valid_private_id(aSource.Order)
			&& aSource.Digest.valid() && aSource.BodyBytes <= maximum_private_assistant_body_bytes;
	}
	auto make_composition_target(const private_review_binding& aBinding, source_line_ref aLine, const diff_semantic_index& aIndex)
		-> std::expected<private_composition_target, private_review_refusal>
	{
		if (aLine.Side != diff_side::right || !valid_private_subject({ aBinding, aLine }, &aIndex)) {
			return std::unexpected(private_review_refusal::invalid_target);
		}
		return private_composition_target{ aBinding, aLine };
	}
	auto private_selection_digest(std::span<const private_source_selection> aSelected) -> std::expected<private_body_digest, private_review_refusal>
	{
		if (aSelected.size() > maximum_private_sources) {
			return std::unexpected(private_review_refusal::bounds);
		}
		auto sorted = std::vector<private_source_selection>{ aSelected.begin(), aSelected.end() };
		std::ranges::sort(sorted, [](const auto& aLeft, const auto& aRight) { return std::tie(aLeft.Source.Scope, aLeft.Source.Key) < std::tie(aRight.Source.Scope, aRight.Source.Key); });
		auto encoder = identity_encoder{ "bpr.private.selection" };
		encoder.number(sorted.size());
		for (std::size_t i = 0; i < sorted.size(); ++i) {
			const auto& selected = sorted[i];
			if (!valid_private_source(selected.Source) || !valid_key(selected.ExpectedLatest)
				|| !same_object(selected.Source.Key, selected.ExpectedLatest) || (!selected.Historical && selected.Source.Key != selected.ExpectedLatest)) {
				return std::unexpected(private_review_refusal::invalid_identity);
			}
			if (i && sorted[i - 1].Source.Scope == selected.Source.Scope && sorted[i - 1].Source.Key == selected.Source.Key) {
				return std::unexpected(private_review_refusal::duplicate_source);
			}
			encoder.source(selected.Source);
			encoder.key(selected.ExpectedLatest);
			encoder.number(selected.Historical);
		}
		return encoder.digest();
	}
	auto admit_composition_sources(const private_review_scope& aScope, std::span<const private_source_selection> aSelected,
								   std::span<const private_source_observation> aObserved) -> std::expected<private_body_digest, private_review_refusal>
	{
		if (!valid_private_scope(aScope)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (aSelected.empty() || aSelected.size() != aObserved.size()) {
			return std::unexpected(private_review_refusal::missing_source);
		}
		auto digest = private_selection_digest(aSelected);
		if (!digest) {
			return std::unexpected(digest.error());
		}
		for (const auto& selected : aSelected) {
			const auto* observed = source_observation(aObserved, selected.Source);
			if (!observed) {
				return std::unexpected(private_review_refusal::missing_source);
			}
			if (auto refusal = validate_selected_source(aScope, selected, *observed)) {
				return std::unexpected(*refusal);
			}
		}
		return digest;
	}
	auto admit_private_derivation(const private_source_reference& aDestination, bool aDestinationExists,
								  std::span<const private_derivation_edge> aEdges, std::span<const private_source_reference> aExistingSources)
		-> std::expected<std::vector<private_derivation_edge>, private_review_refusal>
	{
		if (!valid_private_source(aDestination)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (aDestinationExists) {
			return std::unexpected(private_review_refusal::conflict);
		}
		if (aEdges.size() > maximum_private_sources || aEdges.size() != aExistingSources.size()) {
			return std::unexpected(private_review_refusal::bounds);
		}
		for (std::size_t i = 0; i < aEdges.size(); ++i) {
			const auto& edge = aEdges[i];
			if (edge.Kind > private_derivation_kind::adopted_from_incomplete_run || !valid_private_source(edge.Source)) {
				return std::unexpected(private_review_refusal::invalid_identity);
			}
			if (edge.Source.Scope != aDestination.Scope) {
				return std::unexpected(private_review_refusal::wrong_scope);
			}
			if (edge.Source.Key == aDestination.Key || edge.Source.Order >= aDestination.Order) {
				return std::unexpected(private_review_refusal::cycle);
			}
			if (std::ranges::find(aExistingSources, edge.Source) == aExistingSources.end()) {
				return std::unexpected(private_review_refusal::missing_source);
			}
			if (std::ranges::any_of(aEdges.first(i), [&](const auto& aPrior) { return aPrior.Source.Key == edge.Source.Key; })) {
				return std::unexpected(private_review_refusal::duplicate_source);
			}
		}
		return std::vector<private_derivation_edge>{ aEdges.begin(), aEdges.end() };
	}
	auto inspect_runtime_metadata(const agent_runtime_metadata& aMetadata) -> std::expected<std::vector<agent_provenance_conflict>, private_review_refusal>
	{
		if (aMetadata.ProtocolReported.size() > maximum_runtime_reports) {
			return std::unexpected(private_review_refusal::bounds);
		}
		auto        facts = std::vector<located_fact>{ };
		std::size_t bytes = 0;
		if (!add_settings(facts, bytes, aMetadata.Requested, { agent_fact_basis::requested, 0 })
			|| !add_settings(facts, bytes, aMetadata.Configured, { agent_fact_basis::configured, 0 })
			|| !add_settings(facts, bytes, aMetadata.ContentSelfReported, { agent_fact_basis::content_self_reported, 0 })
			|| !add_reports(facts, bytes, aMetadata.ProtocolReported)) {
			return std::unexpected(private_review_refusal::invalid_provenance);
		}
		return conflicts(facts);
	}
	auto append_runtime_report(const agent_runtime_metadata& aMetadata, const agent_reported_facts& aReport) -> std::expected<agent_runtime_metadata, private_review_refusal>
	{
		if (!inspect_runtime_metadata(aMetadata)) {
			return std::unexpected(private_review_refusal::invalid_provenance);
		}
		auto result = aMetadata;
		if (std::ranges::find(result.ProtocolReported, aReport) == result.ProtocolReported.end()) {
			result.ProtocolReported.push_back(aReport);
		}
		auto checked = inspect_runtime_metadata(result);
		if (!checked) {
			return std::unexpected(checked.error());
		}
		return result;
	}
	auto admit_private_run_sources(const private_review_binding& aBinding, std::span<const private_source_selection> aSelected, std::span<const private_source_observation> aObserved) -> std::expected<private_body_digest, private_review_refusal>
	{
		if (!valid_private_binding(aBinding)) {
			return std::unexpected(private_review_refusal::invalid_binding);
		}
		if (aSelected.empty() && aObserved.empty()) {
			return private_selection_digest(aSelected);
		}
		auto result = admit_composition_sources(aBinding.Scope, aSelected, aObserved);
		if (!result) {
			return result;
		}
		for (const auto& observed : aObserved) {
			if (observed.Binding && (observed.Binding->Revision != aBinding.Revision || observed.Binding->HostAuthority != aBinding.HostAuthority || observed.Binding->Access != aBinding.Access || observed.Binding->LeftAnchor != aBinding.LeftAnchor || observed.Binding->RightAnchor != aBinding.RightAnchor)) {
				return std::unexpected(private_review_refusal::invalid_binding);
			}
		}
		return result;
	}
	auto admit_workshop_run_sources(std::span<const private_source_selection> aSelected, std::span<const private_source_observation> aObserved) -> std::expected<void, private_review_refusal>
	{
		if (aSelected.size() != 1 || aObserved.size() != 1) {
			return std::unexpected(private_review_refusal::invalid_provenance);
		}
		const auto& selected = aSelected.front();
		const auto& observed = aObserved.front();
		if (!std::holds_alternative<private_turn_source>(selected.Source.Key) || selected.Historical
			|| observed.Source.Key != selected.Source.Key || observed.Latest != selected.Source.Key
			|| !std::holds_alternative<private_reviewer_actor>(observed.Actor)
			|| observed.Completion != private_revision_completion::complete) {
			return std::unexpected(private_review_refusal::invalid_provenance);
		}
		return { };
	}
	auto valid_run_provenance(const agent_run_provenance& aRun) -> bool
	{
		if (!valid_private_id(aRun.Run) || !valid_private_id(aRun.Session) || !valid_private_id(aRun.Order) || !valid_private_id(aRun.Profile)
			|| !valid_scalar(aRun.ProfileDisplayName, maximum_runtime_fact_bytes) || aRun.Protocol > agent_protocol::external_terminal
			|| !declared(aRun.EntryPoint) || aRun.Purpose > private_run_purpose::findings || !valid_private_binding(aRun.Binding)
			|| !run_terminal_valid(aRun) || (aRun.OutputDigest && !aRun.OutputDigest->valid()) || !inspect_runtime_metadata(aRun.Metadata)) {
			return false;
		}
		if (aRun.Purpose == private_run_purpose::composition && aRun.EntryPoint != agent_entry_point::workshop_comment) {
			return false;
		}
		if (aRun.OutputItemBytes.size() > 256 || (!aRun.OutputItemBytes.empty() && !aRun.OutputDigest)) {
			return false;
		}
		std::uint64_t bytes = 0;
		for (const auto count : aRun.OutputItemBytes) {
			if (count > maximum_private_assistant_body_bytes - bytes) {
				return false;
			}
			bytes += count;
		}
		const auto digest = private_selection_digest(aRun.SelectedPrivateSources);
		if (!digest || *digest != aRun.InputSelectionDigest) {
			return false;
		}
		return valid_run_sources(aRun);
	}
	auto authorize_private_run(const agent_run_provenance& aRun) -> std::expected<private_run_authorization, private_review_refusal>
	{
		if (!valid_run_provenance(aRun) || aRun.Purpose != private_run_purpose::conversation || aRun.Outcome == private_run_outcome::admitted) {
			return std::unexpected(private_review_refusal::invalid_provenance);
		}
		return private_run_authorization{ aRun.Binding.Scope, aRun.Run, aRun.Order, aRun.Binding, aRun.Completion, aRun.OutputDigest };
	}
	auto valid_retained_agent_finding(const retained_agent_finding& aFinding) -> bool
	{
		if (!valid_private_source(aFinding.Source) || !std::holds_alternative<private_finding_source>(aFinding.Source.Key)
			|| aFinding.Verdict.State > agent_finding_anchor_state::stale || aFinding.Verdict.Refusal >= agent_finding_refusal::malformed
			|| aFinding.Verdict.AnalyzedBaseOid != aFinding.Finding.AnalyzedBaseOid || aFinding.Verdict.AnalyzedHeadOid != aFinding.Finding.AnalyzedHeadOid
			|| validate_agent_finding(aFinding.Finding, { }).Refusal == agent_finding_refusal::malformed) {
			return false;
		}
		const auto bytes = finding_encoding(aFinding.Finding, aFinding.Verdict);
		return aFinding.Source.Digest == bytes.digest() && aFinding.Source.BodyBytes == bytes.size();
	}

	auto retain_agent_finding(const agent_run_provenance& aRun, std::uint32_t aOrdinal, private_vertex_order aOrder,
							  const agent_finding& aFinding, const agent_finding_snapshot& aSnapshot) -> std::expected<retained_agent_finding, private_review_refusal>
	{
		if (!valid_run_provenance(aRun) || aRun.Outcome != private_run_outcome::succeeded || aFinding.Provider != aRun.Profile
			|| aSnapshot.Binding != aRun.Binding.Revision || !valid_private_id(aOrder) || aOrder <= aRun.Order) {
			return std::unexpected(private_review_refusal::invalid_provenance);
		}
		const auto verdict = validate_agent_finding(aFinding, aSnapshot);
		if (verdict.Refusal == agent_finding_refusal::malformed) {
			return std::unexpected(private_review_refusal::invalid_subject);
		}
		const auto encoded = finding_encoding(aFinding, verdict);
		return retained_agent_finding{ { aRun.Binding.Scope, private_finding_source{ aRun.Run, aOrdinal }, aOrder, encoded.digest(), encoded.size() }, aFinding, verdict };
	}
	auto partition_composition_targets(std::span<const private_source_observation> aSources) -> std::expected<composition_target_partitions, private_review_refusal>
	{
		if (aSources.size() > maximum_private_sources) {
			return std::unexpected(private_review_refusal::bounds);
		}
		auto result = composition_target_partitions{ };
		for (std::size_t i = 0; i < aSources.size(); ++i) {
			const auto& source = aSources[i];
			if (!source.Target || source.Availability == private_subject_availability::unplaceable) {
				result.Unassigned.push_back(i);
				continue;
			}
			if (!target_shape(*source.Target) || source.Target->Binding.Scope != source.Source.Scope) {
				return std::unexpected(private_review_refusal::invalid_target);
			}
			auto found = std::ranges::find_if(result.Assigned, [&](const auto& aPartition) { return aPartition.Target == *source.Target; });
			if (found == result.Assigned.end()) {
				result.Assigned.push_back({ *source.Target, { i } });
			} else {
				found->Sources.push_back(i);
			}
		}
		return result;
	}
	auto create_review_composition(review_composition_id aId, const private_review_binding& aBinding, review_composition_mode aMode,
								   std::span<const private_source_selection> aSelected, std::span<const private_source_observation> aObserved,
								   std::optional<private_composition_target> aExplicitTarget) -> std::expected<review_composition_state, private_review_refusal>
	{
		if (!valid_private_id(aId) || !valid_private_binding(aBinding)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (aMode > review_composition_mode::agent_assisted || (aMode == review_composition_mode::exact_copy && aSelected.size() != 1)) {
			return std::unexpected(private_review_refusal::invalid_mode);
		}
		auto digest = admit_composition_sources(aBinding.Scope, aSelected, aObserved);
		if (!digest) {
			return std::unexpected(digest.error());
		}
		auto partitions = partition_composition_targets(aObserved);
		if (!partitions) {
			return std::unexpected(partitions.error());
		}
		if (aExplicitTarget && (!target_shape(*aExplicitTarget) || aExplicitTarget->Binding.Scope != aBinding.Scope)) {
			return std::unexpected(private_review_refusal::invalid_target);
		}
		if (!aExplicitTarget && partitions->Assigned.size() == 1 && partitions->Unassigned.empty()) {
			aExplicitTarget = partitions->Assigned.front().Target;
		}
		return review_composition_state{ aId, aBinding, aMode, review_composition_lifecycle::editing, *digest, aSelected.size(), aExplicitTarget, { } };
	}
	auto propose_composition_revision(const review_composition_state& aState, review_composition_revision_id aRevision,
									  private_vertex_order aOrder, std::string_view aBody, std::span<const private_source_selection> aSelected,
									  std::span<const private_source_observation> aObserved, const agent_run_provenance* aRun,
									  const review_composition_revision* aPredecessor) -> std::expected<composition_revision_proposal, private_review_refusal>
	{
		if (auto refusal = revalidate_workspace(aState, aSelected, aObserved)) {
			return std::unexpected(*refusal);
		}
		if (!valid_private_id(aRevision) || !valid_private_id(aOrder)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (aBody.size() > maximum_private_reviewer_body_bytes || aBody.empty()) {
			return std::unexpected(private_review_refusal::bounds);
		}
		auto actor = proposal_actor(aState, aBody, aObserved, aRun, aPredecessor);
		if (!actor) {
			return std::unexpected(actor.error());
		}
		if (aRun && aRun->Order >= aOrder) {
			return std::unexpected(private_review_refusal::cycle);
		}
		const auto identity = private_source_reference{ aState.Binding.Scope, private_composition_source{ aState.Id, aRevision }, aOrder, private_digest(aBody), aBody.size() };
		auto       edges    = std::vector<private_derivation_edge>{ };
		auto       existing = std::vector<private_source_reference>{ };
		if (aPredecessor) {
			edges.push_back({ private_derivation_kind::edited_from, aPredecessor->Identity });
			existing.push_back(aPredecessor->Identity);
		} else {
			for (const auto& selected : aSelected) {
				edges.push_back({ aState.Mode == review_composition_mode::exact_copy ? private_derivation_kind::copied_from : private_derivation_kind::composed_from, selected.Source });
				existing.push_back(selected.Source);
			}
		}
		auto admitted = admit_private_derivation(identity, false, edges, existing);
		if (!admitted) {
			return std::unexpected(admitted.error());
		}
		auto state      = aState;
		state.Candidate = identity;
		return composition_revision_proposal{ state, { identity, aState.SourceSetDigest, *actor, std::string{ aBody }, std::move(*admitted) } };
	}
	auto accept_composition_as_draft(const review_composition_state& aState, const review_composition_revision& aCandidate,
									 std::span<const private_source_selection> aSelected, std::span<const private_source_observation> aObserved,
									 const diff_semantic_index& aTargetIndex, const std::optional<revisioned_review_draft>& aExistingDraft)
		-> std::expected<accept_composition_as_draft_request, private_review_refusal>
	{
		if (auto refusal = revalidate_workspace(aState, aSelected, aObserved)) {
			return std::unexpected(*refusal);
		}
		if (!valid_candidate(aState, aCandidate)) {
			return std::unexpected(private_review_refusal::conflict);
		}
		if (!aState.Target) {
			return std::unexpected(private_review_refusal::ambiguous_target);
		}
		if (!make_composition_target(aState.Target->Binding, aState.Target->Line, aTargetIndex)) {
			return std::unexpected(private_review_refusal::invalid_target);
		}
		if (!std::ranges::any_of(aCandidate.Body, [](char aChar) { return aChar != ' ' && aChar != '\t' && aChar != '\n' && aChar != '\r'; })) {
			return std::unexpected(private_review_refusal::bounds);
		}
		if (aExistingDraft) {
			const auto* key  = std::get_if<private_composition_source>(&aCandidate.Identity.Key);
			const auto  link = review_composition_link{ aState.Binding.Scope, aState.Id, key->Revision, aCandidate.Identity.Digest };
			const auto  edit = revisioned_review_draft_command{
				.Command          = { aExistingDraft->Draft.Id, review_draft_command_kind::edit, aCandidate.Body },
				.ExpectedRevision = aExistingDraft->Revision,
				.Scope            = aExistingDraft->Scope,
				.Composition      = link
			};
			if (aExistingDraft->Draft.Anchor != aState.Target->Line || aExistingDraft->Draft.ReplyToThread
				|| apply(*aExistingDraft, edit).Refusal != revisioned_draft_refusal::none) {
				return std::unexpected(private_review_refusal::conflict);
			}
		}

		return accept_composition_as_draft_request{ aCandidate.Identity, *aState.Target, aCandidate.Body,
													aExistingDraft ? std::optional{ aExistingDraft->Draft.Id } : std::nullopt, aExistingDraft ? aExistingDraft->Revision : 0 };
	}
	auto acknowledge_composition_draft(const review_composition_state& aState, const private_source_reference& aAcceptedCandidate)
		-> std::expected<review_composition_state, private_review_refusal>
	{
		if (!valid_workspace(aState) || aState.Lifecycle != review_composition_lifecycle::editing || !valid_private_source(aAcceptedCandidate) || aState.Candidate != aAcceptedCandidate || !aState.Target) {
			return std::unexpected(private_review_refusal::conflict);
		}
		auto state      = aState;
		state.Lifecycle = review_composition_lifecycle::accepted_as_draft;
		return state;
	}
	auto reopen_review_composition(const review_composition_state& aState) -> std::expected<review_composition_state, private_review_refusal>
	{
		if (!valid_workspace(aState) || aState.Lifecycle != review_composition_lifecycle::accepted_as_draft || !aState.Candidate) {
			return std::unexpected(private_review_refusal::invalid_transition);
		}
		auto state      = aState;
		state.Lifecycle = review_composition_lifecycle::editing;
		return state;
	}
	auto abandon_review_composition(const review_composition_state& aState) -> std::expected<review_composition_state, private_review_refusal>
	{
		if (!valid_workspace(aState) || aState.Lifecycle != review_composition_lifecycle::editing) {
			return std::unexpected(private_review_refusal::invalid_transition);
		}
		auto state      = aState;
		state.Lifecycle = review_composition_lifecycle::abandoned;
		return state;
	}
	auto duplicate_composition_publication(const review_composition_state& aState, const review_composition_revision& aCandidate,
										   std::span<const composition_publication_observation> aPublished) noexcept -> bool
	{
		if (!valid_workspace(aState) || !valid_candidate(aState, aCandidate) || !aState.Target || aPublished.size() > maximum_private_sources) {
			return false;
		}
		return std::ranges::any_of(aPublished, [&](const auto& aPublication) {
			return aPublication.Scope == aState.Binding.Scope && aPublication.SourceSetDigest == aState.SourceSetDigest
				&& aPublication.BodyDigest == aCandidate.Identity.Digest && aPublication.Target == *aState.Target;
		});
	}
}
