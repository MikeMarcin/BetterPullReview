#ifndef BPR_SRC_APPLICATION_SRC_HEADER_SOURCE_INDEX_HPP
#define BPR_SRC_APPLICATION_SRC_HEADER_SOURCE_INDEX_HPP

#include "bpr/core/header_source_navigation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief Host-owned index of core-classified paths, with one node per reversed directory component.
	/// \details Nodes summarize distinct stable identities in each family. Looking up the deepest
	/// reachable suffix never walks candidates, even when every file has the same basename.
	class header_source_index
	{
	public:
		/// \brief Adds one already-deduplicated accepted path to its family and suffix groups.
		void insert(const core::file_identity aFile, const std::string_view aPath)
		{
			const auto parts = core::classify_header_source_path(aPath);
			if (!parts) {
				return;
			}
			const auto [stem, inserted] = Stems.try_emplace(std::string{ parts->Stem }, Nodes.size());
			if (inserted) {
				Nodes.emplace_back();
			}
			insert_directory(stem->second, *parts, aFile);
		}

		/// \brief Returns the core decision and counts each visited directory lookup.
		[[nodiscard]] auto resolve(const std::string_view aPath, std::uint64_t& aLookups) const -> core::header_source_resolution
		{
			const auto parts = core::classify_header_source_path(aPath);
			if (!parts) {
				return { .Status = core::header_source_status::unsupported_file };
			}
			const auto stem = Stems.find(parts->Stem);
			if (stem == Stems.end()) {
				return { core::header_source_status::no_match, std::nullopt, parts->Family };
			}
			const auto family    = parts->Family == core::header_source_family::source ? 0U : 1U;
			auto       directory = parts->Directory;
			auto       position  = stem->second;
			auto       closest   = Nodes[position].Candidates[family];
			while (!directory.empty()) {
				++aLookups;
				const auto component = core::take_header_source_directory_component(directory);
				const auto found     = Nodes[position].Children.find(component);
				if (found == Nodes[position].Children.end()) {
					return core::resolve_header_source_match(parts->Family, { }, closest);
				}
				position               = found->second;
				const auto& candidates = Nodes[position].Candidates[family];
				if (candidates.Unique || candidates.Ambiguous) {
					closest = candidates;
				}
			}
			return core::resolve_header_source_match(parts->Family, Nodes[position].Exact[family], closest);
		}

	private:
		struct directory_node
		{
			std::map<std::string, std::size_t, std::less<>> Children;
			std::array<core::header_source_candidates, 2U>  Candidates;
			std::array<core::header_source_candidates, 2U>  Exact;
		};

		void insert_directory(std::size_t aPosition, const core::header_source_path& aParts, const core::file_identity aFile)
		{
			const auto family = aParts.Family == core::header_source_family::header ? 0U : 1U;
			Nodes[aPosition].Candidates[family].include(aFile);
			auto directory = aParts.Directory;
			while (!directory.empty()) {
				const auto component         = core::take_header_source_directory_component(directory);
				const auto [child, inserted] = Nodes[aPosition].Children.try_emplace(std::string{ component }, Nodes.size());
				const auto next              = child->second;
				if (inserted) {
					Nodes.emplace_back();
				}
				aPosition = next;
				Nodes[aPosition].Candidates[family].include(aFile);
			}
			Nodes[aPosition].Exact[family].include(aFile);
		}

		std::map<std::string, std::size_t, std::less<>> Stems;
		std::vector<directory_node>                     Nodes;
	};
}

#endif
