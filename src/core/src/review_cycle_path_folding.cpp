#include "bpr/core/review_cycle_workspace.hpp"

#include "bpr/core/utf8.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Collision keys for paths another platform would merge.
/// \details [[BPR-1434]]. Canonical decomposition and case folding come from
/// utf8proc through `bpr/core/utf8.hpp`, for every script, replacing about
/// 1,400 lines of pasted Latin, Greek and Cyrillic tables and a hand-written
/// decoder that read lead bytes F5-FF as three-byte sequences.
namespace bpr::core {
	namespace {
		struct keys
		{
			/// Canonically decomposed.
			std::string Normalized;
			/// Case-folded without decomposition.
			std::string Lowered;
			/// Decomposed and case-folded.
			std::string Folded;
		};

		/// Invalid UTF-8 bytes are kept as themselves in every key. A key
		/// holding one is itself invalid UTF-8, so it never equals the key of
		/// a valid path. Each maximal well-formed run is decomposed as a whole,
		/// so its combining marks take canonical order.
		auto collision_keys(const std::string_view aPath) -> keys
		{
			auto result = keys{ };
			for (std::size_t offset = 0; offset < aPath.size();) {
				const auto start = offset;
				while (offset < aPath.size()) {
					auto       next  = offset;
					const auto value = utf8::decode(aPath, next);
					if (!value) {
						break;
					}
					static_cast<void>(utf8::append(result.Lowered, utf8::simple_fold(*value)));
					offset = next;
				}
				if (offset > start) {
					// A run that decoded scalar by scalar is well formed as a whole.
					const auto decomposed = utf8::decompose(aPath.substr(start, offset - start));
					for (const auto scalar : decomposed.value_or(std::u32string{ })) {
						static_cast<void>(utf8::append(result.Normalized, scalar));
						static_cast<void>(utf8::append(result.Folded, utf8::simple_fold(scalar)));
					}
					continue;
				}
				result.Normalized.push_back(aPath[offset]);
				result.Lowered.push_back(aPath[offset]);
				result.Folded.push_back(aPath[offset]);
				++offset;
			}
			return result;
		}
	}

	auto workspace_collision_key(const std::string_view aPath) -> std::string
	{
		return collision_keys(aPath).Folded;
	}

	auto find_path_collisions(const std::span<const std::string> aPaths) -> std::vector<workspace_path_collision>
	{
		auto derived = std::vector<keys>{ };
		derived.reserve(aPaths.size());
		auto byFolded = std::map<std::string_view, std::vector<std::size_t>>{ };
		auto result   = std::vector<workspace_path_collision>{ };
		for (const auto& path : aPaths) {
			derived.push_back(collision_keys(path));
		}
		for (std::size_t index = 0; index < aPaths.size(); ++index) {
			auto& earlier = byFolded[derived[index].Folded];
			for (const auto first : earlier) {
				if (aPaths[first] == aPaths[index]) {
					continue;
				}
				// Case alone when lowercasing the spellings as written already
				// merges them; any composition difference makes it normalization.
				const auto caseOnly = derived[first].Lowered == derived[index].Lowered && derived[first].Normalized != derived[index].Normalized;
				result.push_back({ .First = first, .Second = index, .Kind = caseOnly ? workspace_collision_kind::case_fold : workspace_collision_kind::unicode_normalization });
			}
			earlier.push_back(index);
		}
		return result;
	}
}
