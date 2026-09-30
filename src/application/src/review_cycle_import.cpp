#include "bpr/application/review_cycle_import.hpp"

#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}
	}

	auto import_round_bundle(review_cycle_store& aStore, const cycle_operation_id& aOperation, const std::span<const std::uint8_t> aBundle, std::stop_token aStop)
		-> cycle_store_result<cycle_import_result>
	{
		auto opened = core::open_round_bundle(aBundle);
		if (!opened) {
			return cycle_import_result{ .Refusal = std::move(opened.error()) };
		}
		// Only a completely validated bundle reaches the store.
		auto pieces = std::vector<std::span<const std::uint8_t>>{ bytes_of(opened->Round.Canonical) };
		for (const auto& [digest, content] : opened->Objects) {
			pieces.push_back(bytes_of(content));
		}
		auto installed = aStore.install_all(pieces, aStop);
		if (!installed) {
			return std::unexpected(installed.error());
		}
		auto commit = cycle_import_commit{ .Summary = core::summarize_import(opened->Round), .Payload = installed->front() };
		commit.Objects.assign(installed->begin() + 1, installed->end());
		const auto receipt = aStore.admit_import(aOperation, commit, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		auto record = aStore.load_import(core::cycle_import_id{ receipt->Allocated }, aStop);
		if (!record) {
			return std::unexpected(record.error());
		}
		if (!*record) {
			return failed(cycle_store_failure::corrupt);
		}
		return cycle_import_result{ .Import = std::move(*record), .Existing = !receipt->Changed, .Replayed = receipt->Replayed };
	}

	auto read_imported_round(review_cycle_store& aStore, const core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<cycle_imported_round>
	{
		auto record = aStore.load_import(aImport, aStop);
		if (!record) {
			return std::unexpected(record.error());
		}
		if (!*record) {
			return failed(cycle_store_failure::not_found);
		}
		const auto bytes = aStore.read_object((*record)->Payload, std::move(aStop));
		if (!bytes) {
			return std::unexpected(bytes.error());
		}
		auto content = core::decode_round_payload({ reinterpret_cast<const char*>(bytes->data()), bytes->size() });
		if (!content || content->Digest != (*record)->Summary.Digest) {
			return failed(cycle_store_failure::corrupt);
		}
		return cycle_imported_round{ .Record = std::move(**record), .Content = std::move(*content) };
	}
}
