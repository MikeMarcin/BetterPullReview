#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_PRIVATE_REVIEW_CODEC_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_PRIVATE_REVIEW_CODEC_HPP
#include "bpr/application/private_review_store.hpp"
#include <type_traits>
namespace bpr::adapters::sqlite::detail {
	/// \brief Versioned exact-byte codec, admitted by the typed store before encoding.
	/// Every allocation is bounded by both payload size and field count on decoding.
	class private_codec
	{
	public:
		std::vector<std::uint8_t>     Bytes;
		std::span<const std::uint8_t> Input;
		bool                          Reading = false;
		bool                          Good    = true;
		void                          octet(std::uint8_t& aValue)
		{
			if (!Good) {
				return;
			}
			if (!Reading) {
				Bytes.push_back(aValue);
				return;
			}
			if (Input.empty()) {
				Good = false;
				return;
			}
			aValue = Input.front();
			Input  = Input.subspan(1);
		}
		template <typename T>
		void value(T& aValue)
		{
			if (!Good) {
				return;
			}
			if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, core::private_pr_subject> || std::is_same_v<T, core::private_legacy_unknown_actor>) {
			} else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
				std::uint64_t result = 0;
				for (unsigned i = 0; i < 8; ++i) {
					auto byte = static_cast<std::uint8_t>(static_cast<std::uint64_t>(aValue) >> ((7 - i) * 8));
					octet(byte);
					result = (result << 8) | byte;
				}
				if constexpr (std::is_integral_v<T>) {
					if (result > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
						Good = false;
					}
				}
				if (Reading) {
					aValue = static_cast<T>(result);
				}
			} else if constexpr (requires { aValue.algorithm(); aValue.bytes(); }) {
				auto algorithm = aValue.algorithm();
				value(algorithm);
				auto bytes = aValue.bytes();
				for (auto& byte : bytes) {
					octet(byte);
				}
				if (Reading) {
					aValue = T{ algorithm, bytes };
				}
			} else if constexpr (requires { aValue.bytes(); }) {
				auto bytes = aValue.bytes();
				for (auto& byte : bytes) {
					octet(byte);
				}
				if (Reading) {
					aValue = T{ bytes };
				}
			} else if constexpr (requires { aValue.value(); }) {
				auto scalar = aValue.value();
				value(scalar);
				if (Reading) {
					aValue = T{ scalar };
				}
			} else {
				fields(*this, aValue);
			}
		}
		void value(std::string& aValue)
		{
			auto size = aValue.size();
			value(size);
			if (!Good || size > core::maximum_private_assistant_body_bytes || (Reading && size > Input.size())) {
				Good = false;
				return;
			}
			if (Reading) {
				aValue.assign(reinterpret_cast<const char*>(Input.data()), size);
				Input = Input.subspan(size);
			} else {
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
		}
		template <typename T>
		void value(std::optional<T>& aValue)
		{
			auto present = aValue.has_value();
			value(present);
			if (!Good) {
				return;
			}
			if (!present) {
				aValue.reset();
				return;
			}
			if (!aValue) {
				aValue.emplace();
			}
			value(*aValue);
		}
		template <typename T>
		void value(std::vector<T>& aValues)
		{
			auto count = aValues.size();
			value(count);
			if (!Good || count > core::maximum_private_sources) {
				Good = false;
				return;
			}
			if (Reading) {
				aValues.resize(count);
			}
			for (auto& item : aValues) {
				value(item);
			}
		}
		template <std::size_t I = 0, typename... T>
		void alternative(std::variant<T...>& aValue, std::size_t aIndex)
		{
			if constexpr (I < sizeof...(T)) {
				if (I == aIndex) {
					aValue.template emplace<I>();
					value(std::get<I>(aValue));
				} else {
					alternative<I + 1>(aValue, aIndex);
				}
			} else {
				Good = false;
			}
		}
		template <typename... T>
		void value(std::variant<T...>& aValue)
		{
			auto index = aValue.index();
			value(index);
			if (!Good) {
				return;
			}
			if (Reading) {
				alternative(aValue, index);
			} else {
				std::visit([&](auto& aItem) { value(aItem); }, aValue);
			}
		}
		template <typename... T>
		void values(T&... aValues) { (value(aValues), ...); }
	};
	inline void fields(private_codec& aCodec, core::private_review_scope& aValue) { aCodec.values(aValue.Profile, aValue.Incarnation, aValue.Host, aValue.Account, aValue.Repository, aValue.PullRequest); }
	inline void fields(private_codec& aCodec, core::agent_revision_binding& aValue) { aCodec.values(aValue.Host, aValue.Repository, aValue.PullRequest, aValue.BaseOid, aValue.HeadOid, aValue.LeftEpoch, aValue.RightEpoch); }
	inline void fields(private_codec& aCodec, core::comparison_anchor_identity& aValue) { aCodec.values(aValue.Authority, aValue.Incarnation, aValue.Kind, aValue.Epoch, aValue.Review); }
	inline void fields(private_codec& aCodec, core::private_review_binding& aValue) { aCodec.values(aValue.Scope, aValue.Access, aValue.HostAuthority, aValue.Revision, aValue.LeftAnchor, aValue.RightAnchor, aValue.Comparison); }
	inline void fields(private_codec& aCodec, core::private_file_subject& aValue) { aCodec.values(aValue.File, aValue.Comparison); }
	inline void fields(private_codec& aCodec, core::source_line_ref& aValue) { aCodec.values(aValue.Comparison, aValue.Side, aValue.Line); }
	inline void fields(private_codec& aCodec, core::source_boundary_ref& aValue) { aCodec.values(aValue.Comparison, aValue.Side, aValue.Boundary); }
	inline void fields(private_codec& aCodec, core::line_span& aValue) { aCodec.values(aValue.Begin, aValue.End); }
	inline void fields(private_codec& aCodec, core::source_span_ref& aValue) { aCodec.values(aValue.Comparison, aValue.Side, aValue.Lines); }
	inline void fields(private_codec& aCodec, core::private_group_subject& aValue) { aCodec.values(aValue.Comparison, aValue.Group); }
	inline void fields(private_codec& aCodec, core::private_segment_subject& aValue) { aCodec.values(aValue.Comparison, aValue.Segment); }
	inline void fields(private_codec& aCodec, core::private_move_subject& aValue) { aCodec.values(aValue.Comparison, aValue.Move); }
	inline void fields(private_codec& aCodec, core::private_finding_reference& aValue) { aCodec.values(aValue.Run, aValue.Ordinal, aValue.Digest); }
	inline void fields(private_codec& aCodec, core::legacy_retained_subject& aValue) { aCodec.values(aValue.Scope, aValue.Attachment); }
	inline void fields(private_codec& aCodec, core::exact_private_subject& aValue) { aCodec.values(aValue.Binding, aValue.Subject); }
	inline void fields(private_codec& aCodec, core::private_reviewer_actor& aValue) { aCodec.values(aValue.Account); }
	inline void fields(private_codec& aCodec, core::private_agent_actor& aValue) { aCodec.values(aValue.Run); }
	inline void fields(private_codec& aCodec, core::private_review_thread& aValue) { aCodec.values(aValue.Id, aValue.Binding, aValue.Version, aValue.LastOrder, aValue.Lifecycle); }
	inline void fields(private_codec& aCodec, core::private_revision_reference& aValue) { aCodec.values(aValue.Scope, aValue.Thread, aValue.Turn, aValue.Revision, aValue.Order, aValue.Digest); }
	inline void fields(private_codec& aCodec, core::private_review_revision& aValue) { aCodec.values(aValue.Identity, aValue.Actor, aValue.Body, aValue.Completion, aValue.Predecessor, aValue.ReplyTo, aValue.EditDerivation); }
	inline void fields(private_codec& aCodec, core::private_turn_source& aValue) { aCodec.values(aValue.Thread, aValue.Turn, aValue.Revision); }
	inline void fields(private_codec& aCodec, core::private_finding_source& aValue) { aCodec.values(aValue.Run, aValue.Ordinal); }
	inline void fields(private_codec& aCodec, core::private_composition_source& aValue) { aCodec.values(aValue.Composition, aValue.Revision); }
	inline void fields(private_codec& aCodec, core::private_source_reference& aValue) { aCodec.values(aValue.Scope, aValue.Key, aValue.Order, aValue.Digest, aValue.BodyBytes); }
	inline void fields(private_codec& aCodec, core::private_source_selection& aValue) { aCodec.values(aValue.Source, aValue.ExpectedLatest, aValue.Historical); }
	inline void fields(private_codec& aCodec, core::private_composition_target& aValue) { aCodec.values(aValue.Binding, aValue.Line); }
	inline void fields(private_codec& aCodec, core::private_derivation_edge& aValue) { aCodec.values(aValue.Kind, aValue.Source); }
	inline void fields(private_codec& aCodec, core::agent_setting_facts& aValue) { aCodec.values(aValue.Model, aValue.Effort); }
	inline void fields(private_codec& aCodec, core::agent_reported_facts& aValue) { aCodec.values(aValue.Basis, aValue.Settings, aValue.ToolVersion, aValue.RemoteSession, aValue.RemoteThread, aValue.RemoteTurn, aValue.RemoteRun); }
	inline void fields(private_codec& aCodec, core::agent_runtime_metadata& aValue) { aCodec.values(aValue.Requested, aValue.Configured, aValue.ProtocolReported, aValue.ContentSelfReported); }
	inline void fields(private_codec& aCodec, core::agent_run_provenance& aValue) { aCodec.values(aValue.Run, aValue.Session, aValue.Order, aValue.Profile, aValue.ProfileDisplayName, aValue.Protocol, aValue.EntryPoint, aValue.Purpose, aValue.Binding, aValue.Metadata, aValue.SelectedPrivateSources, aValue.InputSelectionDigest, aValue.OutputDigest, aValue.Outcome, aValue.Completion); }
	inline void fields(private_codec& aCodec, core::agent_finding_anchor& aValue) { aCodec.values(aValue.Line, aValue.Boundary, aValue.Segment, aValue.Group, aValue.Move); }
	inline void fields(private_codec& aCodec, core::agent_finding& aValue) { aCodec.values(aValue.Provider, aValue.Severity, aValue.Confidence, aValue.Category, aValue.Path, aValue.Lines, aValue.Anchor, aValue.Claim, aValue.Evidence, aValue.Consequence, aValue.SuggestedComment, aValue.RequiredContext, aValue.AnalyzedBaseOid, aValue.AnalyzedHeadOid, aValue.AnalyzedCommits); }
	inline void fields(private_codec& aCodec, core::agent_finding_verdict& aValue) { aCodec.values(aValue.State, aValue.Refusal, aValue.AnalyzedBaseOid, aValue.AnalyzedHeadOid); }
	inline void fields(private_codec& aCodec, core::retained_agent_finding& aValue) { aCodec.values(aValue.Source, aValue.Finding, aValue.Verdict); }
	inline void fields(private_codec& aCodec, core::review_composition_state& aValue) { aCodec.values(aValue.Id, aValue.Binding, aValue.Mode, aValue.Lifecycle, aValue.SourceSetDigest, aValue.SourceCount, aValue.Target, aValue.Candidate); }
	inline void fields(private_codec& aCodec, core::review_composition_revision& aValue) { aCodec.values(aValue.Identity, aValue.SourceSetDigest, aValue.Actor, aValue.Body, aValue.Derivation); }
	inline void fields(private_codec& aCodec, core::review_composition_link& aValue) { aCodec.values(aValue.Scope, aValue.Composition, aValue.Revision, aValue.BodyDigest); }
	inline void fields(private_codec& aCodec, core::review_pending_mark& aValue) { aCodec.values(aValue.Path, aValue.Head, aValue.Submission); }
	inline void fields(private_codec& aCodec, core::review_comment_draft& aValue) { aCodec.values(aValue.Id, aValue.Anchor, aValue.Body, aValue.State, aValue.ReplyToThread); }
	inline void fields(private_codec& aCodec, core::revisioned_review_draft& aValue) { aCodec.values(aValue.Draft, aValue.Revision, aValue.Scope, aValue.Composition); }
	inline void fields(private_codec& aCodec, application::private_composition_record& aValue) { aCodec.values(aValue.State, aValue.Version, aValue.Sources); }
	inline void fields(private_codec& aCodec, application::private_draft_record& aValue) { aCodec.values(aValue.Binding, aValue.Draft); }
	inline void fields(private_codec& aCodec, application::private_publication_record& aValue) { aCodec.values(aValue.Id, aValue.Draft, aValue.DraftRevision, aValue.Composition, aValue.Target, aValue.Actor, aValue.ApprovedProseDigest, aValue.WireBodyDigest, aValue.LogicalPublication, aValue.Representation, aValue.ServerComment, aValue.ReconciledIdentity); }
	template <typename T>
	[[nodiscard]] auto encode_private(T aValue) -> std::optional<std::vector<std::uint8_t>>
	{
		private_codec codec;
		std::uint64_t version = (std::is_same_v<T, core::agent_run_provenance> || std::is_same_v<T, application::private_publication_record>) ? 2 : 1;
		// [[BPR-1212]]. A draft on several lines is version 2, with its first
		// line after the version 1 fields; a single-line draft keeps exactly
		// its version 1 bytes, and every stored version 1 row reads as one line.
		// [[BPR-1345]]. A draft pending in a review is version 3: its optional
		// first line, then its pending mark. Every other draft keeps its
		// version 1 or 2 bytes, and every stored one reads as not pending.
		if constexpr (std::is_same_v<T, application::private_draft_record>) {
			version = aValue.Draft.Draft.InReview ? 3 : aValue.Draft.Draft.StartLine ? 2
																					 : 1;
		}
		codec.value(version);
		codec.value(aValue);
		if constexpr (std::is_same_v<T, core::agent_run_provenance>) {
			codec.value(aValue.OutputItemBytes);
		}
		if constexpr (std::is_same_v<T, application::private_draft_record>) {
			if (version == 2) {
				codec.value(*aValue.Draft.Draft.StartLine);
			}
			if (version == 3) {
				codec.value(aValue.Draft.Draft.StartLine);
				codec.value(*aValue.Draft.Draft.InReview);
			}
		}
		if constexpr (std::is_same_v<T, application::private_publication_record>) {
			codec.values(aValue.IdempotencyKey, aValue.ReceiptReserve, aValue.ActorNode, aValue.ActorDatabaseId, aValue.ActorLogin, aValue.Path, aValue.AcceptedHead);
		}
		if (!codec.Good || codec.Bytes.size() > 2U * 1024U * 1024U) {
			return { };
		}
		return std::move(codec.Bytes);
	}
	template <typename T>
	[[nodiscard]] auto decode_private(std::span<const std::uint8_t> aBytes) -> std::optional<T>
	{
		if (aBytes.size() > 2U * 1024U * 1024U) {
			return { };
		}
		private_codec codec;
		codec.Reading         = true;
		codec.Input           = aBytes;
		std::uint64_t version = 0;
		codec.value(version);
		if (version != 1 && !((std::is_same_v<T, core::agent_run_provenance> || std::is_same_v<T, application::private_publication_record> || std::is_same_v<T, application::private_draft_record>) && version == 2)
			&& !(std::is_same_v<T, application::private_draft_record> && version == 3)) {
			return { };
		}
		T result{ };
		codec.value(result);
		if constexpr (std::is_same_v<T, core::agent_run_provenance>) {
			if (version == 2) {
				codec.value(result.OutputItemBytes);
			}
		}
		if constexpr (std::is_same_v<T, application::private_publication_record>) {
			if (version == 2) {
				codec.values(result.IdempotencyKey, result.ReceiptReserve, result.ActorNode, result.ActorDatabaseId, result.ActorLogin, result.Path, result.AcceptedHead);
			}
		}
		if constexpr (std::is_same_v<T, application::private_draft_record>) {
			if (version == 2) {
				auto start = std::uint32_t{ 0U };
				codec.value(start);
				result.Draft.Draft.StartLine = start;
			}
			if (version == 3) {
				codec.value(result.Draft.Draft.StartLine);
				auto mark = core::review_pending_mark{ };
				codec.value(mark);
				result.Draft.Draft.InReview = std::move(mark);
			}
		}
		if (!codec.Good || !codec.Input.empty()) {
			return { };
		}
		return result;
	}
}
#endif
