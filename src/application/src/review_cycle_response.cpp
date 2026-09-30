#include "bpr/application/review_cycle_response.hpp"

#include "bpr/application/review_cycle_round.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		auto refused(const core::cycle_response_problem aProblem, const std::optional<std::size_t> aEntry = std::nullopt) -> cycle_response_result
		{
			return { .Refusal = core::cycle_response_refusal{ .Problem = aProblem, .Entry = aEntry } };
		}

		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		/// Installs the canonical content and appends the record. A reused
		/// response identifier with other content is a refusal, not a failure.
		auto append(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_frozen_round& aRound, const core::cycle_response_channel aChannel,
					std::string aForeignId, const std::string_view aCanonical, const cycle_operation_id& aOperation, const std::stop_token& aStop)
			-> cycle_store_result<cycle_response_result>
		{
			auto object = aStore.install(bytes_of(aCanonical), aStop);
			if (!object) {
				return std::unexpected(object.error());
			}
			const auto commit = cycle_response_commit{
				.Subject     = aSubject,
				.Round       = aRound.Round.Round.Id.Id,
				.RoundDigest = aRound.Round.Round.Digest,
				.Channel     = aChannel,
				.ForeignId   = std::move(aForeignId),
				.Content     = *object,
			};
			auto receipt = aStore.append_response(aOperation, commit, aStop);
			if (!receipt) {
				if (receipt.error().Failure == cycle_store_failure::conflict) {
					return refused(core::cycle_response_problem::reused_identifier);
				}
				return std::unexpected(receipt.error());
			}
			return cycle_response_result{ .Receipt = *receipt };
		}
	}

	auto import_cycle_response(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, const std::span<const std::uint8_t> aDocument,
							   const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_response_result>
	{
		if (aDocument.size() > core::maximum_response_document_bytes) {
			return cycle_response_result{ .Decode = core::cycle_response_decode_error{ .Problem = core::cycle_response_decode_problem::bounds } };
		}
		auto decoded = core::decode_response_document({ reinterpret_cast<const char*>(aDocument.data()), aDocument.size() });
		if (!decoded) {
			return cycle_response_result{ .Decode = decoded.error() };
		}
		auto round = read_cycle_round(aStore, aSubject, aRound, aStop);
		if (!round) {
			if (round.error().Failure == cycle_store_failure::not_found) {
				return refused(core::cycle_response_problem::wrong_round);
			}
			return std::unexpected(round.error());
		}
		if (const auto refusal = core::check_response(decoded->Document, round->Content.Payload, round->Content.Digest)) {
			return cycle_response_result{ .Refusal = *refusal };
		}
		return append(aStore, aSubject, *round, core::cycle_response_channel::response_file, decoded->Document.ResponseId, decoded->Canonical, aOperation, aStop);
	}

	auto attach_manual_reply(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, core::cycle_manual_reply aReply,
							 const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_response_result>
	{
		auto round = read_cycle_round(aStore, aSubject, aRound, aStop);
		if (!round) {
			if (round.error().Failure == cycle_store_failure::not_found) {
				return refused(core::cycle_response_problem::wrong_round);
			}
			return std::unexpected(round.error());
		}
		aReply.RoundId = round->Content.Payload.RoundId;
		if (const auto refusal = core::check_manual_reply(aReply, round->Content.Payload)) {
			return cycle_response_result{ .Refusal = *refusal };
		}
		const auto canonical = core::encode_manual_reply(aReply);
		if (!canonical) {
			return failed(cycle_store_failure::bounds);
		}
		return append(aStore, aSubject, *round, core::cycle_response_channel::manual_entry, { }, *canonical, aOperation, aStop);
	}

	auto read_cycle_responses(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<cycle_response_entries>
	{
		auto entries = cycle_response_entries{ };
		auto after   = std::optional<core::cycle_response_id>{ };
		do {
			auto page = aStore.responses(aSubject, after, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			for (auto& record : page->Responses) {
				auto bytes = aStore.read_object({ .Digest = record.Content, .Size = record.Size }, aStop);
				if (!bytes) {
					return std::unexpected(bytes.error());
				}
				const auto text = std::string_view{ reinterpret_cast<const char*>(bytes->data()), bytes->size() };
				if (record.Channel == core::cycle_response_channel::response_file) {
					auto decoded = core::decode_response_document(text);
					if (!decoded || decoded->Canonical != text || decoded->Document.ResponseId != record.ForeignId) {
						return failed(cycle_store_failure::corrupt);
					}
					entries.emplace_back(std::move(record), std::move(decoded->Document));
				} else {
					auto reply = core::decode_manual_reply(text);
					if (!reply) {
						return failed(cycle_store_failure::corrupt);
					}
					entries.emplace_back(std::move(record), std::move(*reply));
				}
			}
			after = page->Next;
		} while (after);
		return entries;
	}
}
