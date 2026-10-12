#ifndef BPR_SRC_APPLICATION_SRC_PRIVATE_REVIEW_OPERATIONS_HPP
#define BPR_SRC_APPLICATION_SRC_PRIVATE_REVIEW_OPERATIONS_HPP
#include "bpr/application/private_review_application_host.hpp"
#include <exception>
#include <utility>
namespace bpr::application::private_review_operations {
	struct refused final : std::exception
	{
		private_run_error Error;
		explicit refused(private_run_error aError)
			: Error(std::move(aError))
		{ }
		auto what() const noexcept -> const char* override { return "Private review operation refused"; }
	};
	template <typename T>
	auto take(private_store_result<T> aValue) -> T
	{
		if (!aValue) {
			throw refused{ { .Store = aValue.error() } };
		}
		return std::move(*aValue);
	}
	template <typename T>
	auto decide(std::expected<T, core::private_review_refusal> aValue) -> T
	{
		if (!aValue) {
			throw refused{ { .Policy = aValue.error() } };
		}
		return std::move(*aValue);
	}
	inline void require(bool aValue, core::private_review_refusal aReason = core::private_review_refusal::conflict)
	{
		if (!aValue) {
			throw refused{ { .Policy = aReason } };
		}
	}
	template <typename F>
	auto attempt(F aWork) -> private_run_result<decltype(aWork())>
	{
		try {
			return aWork();
		} catch (const refused& failure) {
			return std::unexpected(failure.Error);
		} catch (...) {
			return std::unexpected(private_run_error{ .Store = private_store_failure::unavailable });
		}
	}
	inline auto observe(private_review_store_port& aStore, std::span<const core::private_source_selection> aSources) -> std::vector<core::private_source_observation>
	{
		require(aSources.size() <= core::maximum_private_sources, core::private_review_refusal::bounds);
		std::vector<core::private_source_reference> refs;
		for (const auto& source : aSources) {
			refs.push_back(source.Source);
		}
		return take(aStore.load_sources(refs));
	}
	inline auto body(private_review_store_port& aStore, const core::private_source_reference& aSource) -> std::string
	{
		std::string                        result;
		std::optional<private_body_cursor> cursor{ private_body_cursor{ aSource, 0 } };
		while (cursor && aSource.BodyBytes > 0) {
			auto page = take(aStore.read_revision_body(*cursor));
			require(!page.Bytes.empty() && page.Bytes.size() <= maximum_private_page_bytes && page.Bytes.size() <= aSource.BodyBytes - result.size(), core::private_review_refusal::bounds);
			result += page.Bytes;
			if (page.Next) {
				require(page.Next->Source == aSource && page.Next->Offset == result.size());
			}
			cursor = page.Next;
		}
		require(result.size() == aSource.BodyBytes && core::private_digest(result) == aSource.Digest, core::private_review_refusal::digest_mismatch);
		return result;
	}
}
#endif
