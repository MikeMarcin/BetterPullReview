#include "bpr/application/review_mutation_recovery_port.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/core/sha256.hpp"
#include <algorithm>
#include <span>

namespace bpr::application {
	namespace {
		using state        = core::review_publication_state;
		using source       = review_fallback_authorization_source;
		using transmission = review_mutation_recovery_transmission;
		[[nodiscard]] auto digest_present(const std::array<std::uint8_t, 32>& aDigest) noexcept -> bool
		{
			return std::ranges::any_of(aDigest, [](const auto aByte) { return aByte != 0U; });
		}
		[[nodiscard]] auto key_valid(const std::string_view aKey) noexcept -> bool
		{
			return aKey.size() == 64U && std::ranges::all_of(aKey, [](const char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); });
		}
		[[nodiscard]] auto child_valid(const review_publication_recovery_record& aParent, const std::optional<review_mutation_recovery_record>& aChild, const core::review_publication_representation aRepresentation) -> bool
		{
			if (!aChild) {
				return true;
			}
			return valid(*aChild)
				&& (aChild->State == review_mutation_recovery_state::unresolved || aChild->State == review_mutation_recovery_state::succeeded)
				&& aChild->IdempotencyKey == review_publication_child_key(aParent.Key, aRepresentation)
				&& digest_present(aChild->RequestFingerprint) && digest_present(aChild->AuthorityFingerprint)
				&& aChild->BaselineExactCommentIds && (!aChild->Receipt || aChild->Receipt->Representation == aRepresentation);
		}
		[[nodiscard]] auto child_at(const std::optional<review_mutation_recovery_record>& aChild, const transmission aTransmission, const bool aSucceeded = false) -> bool
		{
			return aChild && aChild->Transmission == aTransmission
				&& (aChild->State == review_mutation_recovery_state::succeeded) == aSucceeded
				&& aChild->Receipt.has_value() == aSucceeded;
		}
	}
	auto review_publication_child_key(const std::string_view aParent, const core::review_publication_representation aRepresentation) -> std::string
	{
		const auto material = std::string{ "bpr-review-child-v1:" } + std::string{ aParent } + (aRepresentation == core::review_publication_representation::native_line ? ":native-wire-v1" : ":file-wire-v1:permalink-v1:body-v1");
		const auto digest   = core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(material.data()), material.size() });
		return core::lower_hex(digest);
	}
	auto valid(const review_publication_recovery_record& aRecord) noexcept -> bool
	{
		try {
			if (!key_valid(aRecord.Key) || !key_valid(aRecord.LegacyKey) || !digest_present(aRecord.Fingerprint)
				|| !child_valid(aRecord, aRecord.Native, core::review_publication_representation::native_line)
				|| !child_valid(aRecord, aRecord.Fallback, core::review_publication_representation::file_comment_with_permalink)) {
				return false;
			}
			const auto& auth = aRecord.Authorization;
			// [[BPR-1706]]. An unresolvable line's admission is first-attempt
			// like scoped evidence and unbounded in time like a native rejection.
			const bool firstAttempt = auth.Source == source::scoped_evidence || auth.Source == source::unresolvable_line;
			const bool authorized   = auth.Source != source::none && auth.Source <= source::unresolvable_line
				&& digest_present(auth.ContractDigest) && digest_present(auth.ScopeDigest)
				&& (auth.Source != source::scoped_evidence
						? auth.Generation == 0U && auth.ObservedAt == 0U && auth.ExpiresAt == 0U
						: auth.Generation != 0U && auth.ExpiresAt > auth.ObservedAt && auth.ExpiresAt - auth.ObservedAt <= 300U);
			if (auth.Source == source::none && auth != review_fallback_authorization{ }) {
				return false;
			}
			if (aRecord.Fallback && aRecord.Native && (aRecord.Fallback->Anchor != aRecord.Native->Anchor || aRecord.Fallback->AuthorityFingerprint != aRecord.Native->AuthorityFingerprint)) {
				return false;
			}
			switch (aRecord.State) {
			case state::native_prepared:                 return !authorized && auth.Source == source::none && !aRecord.Fallback && child_at(aRecord.Native, transmission::prepared);
			case state::native_possibly_transmitted:     return !authorized && auth.Source == source::none && !aRecord.Fallback && child_at(aRecord.Native, transmission::possibly_transmitted);
			case state::succeeded_native:                return !authorized && auth.Source == source::none && !aRecord.Fallback && child_at(aRecord.Native, transmission::possibly_transmitted, true);
			case state::native_definitively_unsupported: return authorized && auth.Source == source::native_rejection && !aRecord.Fallback && child_at(aRecord.Native, transmission::possibly_transmitted);
			case state::unsupported_evidence_admitted:   return authorized && auth.Source == source::scoped_evidence && !aRecord.Native && !aRecord.Fallback;
			case state::unresolvable_line_admitted:      return authorized && auth.Source == source::unresolvable_line && !aRecord.Native && !aRecord.Fallback;
			case state::fallback_prepared:
			case state::fallback_possibly_transmitted:
			case state::succeeded_fallback:
				return authorized && (firstAttempt ? !aRecord.Native : child_at(aRecord.Native, transmission::possibly_transmitted))
					&& child_at(aRecord.Fallback, aRecord.State == state::fallback_prepared ? transmission::prepared : transmission::possibly_transmitted, aRecord.State == state::succeeded_fallback);
			default: return false;
			}
		} catch (...) {
			return false;
		}
	}
	auto valid_publication_transition(const std::optional<review_publication_recovery_record>& aExpected, const review_publication_recovery_record& aNext) -> bool
	{
		if (!valid(aNext)) {
			return false;
		}
		if (!aExpected) {
			return aNext.State == state::native_prepared || aNext.State == state::unsupported_evidence_admitted || aNext.State == state::unresolvable_line_admitted;
		}
		if (!valid(*aExpected)) {
			return false;
		}
		auto permitted = *aExpected;
		using event    = core::review_publication_event;
		std::optional<event> transition;
		switch (aNext.State) {
		case state::native_possibly_transmitted:
			if (!permitted.Native) {
				return false;
			}
			permitted.Native->Transmission = transmission::possibly_transmitted;
			transition                     = event::native_transmission_witness;
			break;
		case state::native_definitively_unsupported:
			permitted.Authorization = aNext.Authorization;
			transition              = event::native_definitively_unsupported;
			break;
		case state::fallback_prepared:
			permitted.Fallback = aNext.Fallback;
			transition         = event::prepare_fallback;
			break;
		case state::fallback_possibly_transmitted:
			if (!permitted.Fallback) {
				return false;
			}
			permitted.Fallback->Transmission = transmission::possibly_transmitted;
			transition                       = event::fallback_transmission_witness;
			break;
		case state::succeeded_native:
		case state::succeeded_fallback: {
			auto&       child     = aNext.State == state::succeeded_native ? permitted.Native : permitted.Fallback;
			const auto& nextChild = aNext.State == state::succeeded_native ? aNext.Native : aNext.Fallback;
			if (!child || !nextChild || !nextChild->Receipt || std::ranges::binary_search(*child->BaselineExactCommentIds, nextChild->Receipt->ServerCommentId)) {
				return false;
			}
			child->State   = review_mutation_recovery_state::succeeded;
			child->Receipt = nextChild->Receipt;
			transition     = aNext.State == state::succeeded_native ? event::native_created : event::fallback_created;
			break;
		}
		default: return false;
		}
		const auto advanced = core::advance_review_publication({ .State = aExpected->State, .Eligible = true }, *transition);
		if (!advanced || *advanced != aNext.State) {
			return false;
		}
		permitted.State = *advanced;
		return permitted == aNext;
	}
}
