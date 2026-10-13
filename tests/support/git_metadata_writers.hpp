#ifndef BPR_TESTS_SUPPORT_GIT_METADATA_WRITERS_HPP
#define BPR_TESTS_SUPPORT_GIT_METADATA_WRITERS_HPP

/// \file
/// \brief [[BPR-1457]]. Test-only writers for the formats the Git metadata
/// readers parse, so a fuzz target or unit test can check serialize-then-parse
/// round trips (RFC-BPR-0083, Fuzzing). Nothing here is product code.

#include "bpr/core/git_config.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/sha1.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bpr::tests {
	/// \brief An index of `aIndex.Version` holding `aIndex.Entries`, with zeroed
	/// stat data, `link` and `sdir` extensions when the index says so, and a
	/// trailer that is the repository hash, or all zero when `aZeroTrailer`.
	[[nodiscard]] inline auto write_git_index(const core::git_index& aIndex, const core::git_hash_algorithm aAlgorithm, const bool aZeroTrailer) -> std::vector<std::uint8_t>
	{
		const auto put32 = [](std::vector<std::uint8_t>& aOut, const std::uint32_t aValue) {
			for (auto shift = 24; shift >= 0; shift -= 8) {
				aOut.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
			}
		};
		const auto put16 = [](std::vector<std::uint8_t>& aOut, const std::uint16_t aValue) {
			aOut.push_back(static_cast<std::uint8_t>(aValue >> 8U));
			aOut.push_back(static_cast<std::uint8_t>(aValue & 0xFFU));
		};
		const auto hashLength = aAlgorithm == core::git_hash_algorithm::sha256 ? 32U : 20U;
		auto       out        = std::vector<std::uint8_t>{ 'D', 'I', 'R', 'C' };
		put32(out, aIndex.Version);
		put32(out, static_cast<std::uint32_t>(aIndex.Entries.size()));
		auto previous = std::string{ };
		for (const auto& entry : aIndex.Entries) {
			const auto start = out.size();
			out.insert(out.end(), 24U, 0U);
			put32(out, entry.Mode);
			out.insert(out.end(), 12U, 0U);
			out.insert(out.end(), entry.Object.bytes().begin(), entry.Object.bytes().begin() + hashLength);
			const auto extended = entry.SkipWorktree || entry.IntentToAdd;
			auto       flags    = static_cast<std::uint16_t>((static_cast<unsigned>(entry.Stage) & 0x3U) << 12U);
			flags               = static_cast<std::uint16_t>(flags | std::min<std::size_t>(entry.Path.size(), 0x0FFFU));
			if (extended) {
				flags = static_cast<std::uint16_t>(flags | 0x4000U);
			}
			put16(out, flags);
			if (extended) {
				put16(out, static_cast<std::uint16_t>((entry.SkipWorktree ? 0x4000U : 0U) | (entry.IntentToAdd ? 0x2000U : 0U)));
			}
			if (aIndex.Version == 4U) {
				// Strip the previous path's suffix beyond the common prefix.
				auto common = std::size_t{ 0 };
				while (common < previous.size() && common < entry.Path.size() && previous[common] == entry.Path[common]) {
					++common;
				}
				auto strip = static_cast<std::uint64_t>(previous.size() - common);
				// Git's offset varint, most significant group first.
				auto groups = std::vector<std::uint8_t>{ static_cast<std::uint8_t>(strip & 0x7FU) };
				while ((strip >>= 7U) != 0U) {
					--strip;
					groups.push_back(static_cast<std::uint8_t>(0x80U | (strip & 0x7FU)));
				}
				out.insert(out.end(), groups.rbegin(), groups.rend());
				out.insert(out.end(), entry.Path.begin() + static_cast<std::ptrdiff_t>(common), entry.Path.end());
				out.push_back(0U);
			} else {
				out.insert(out.end(), entry.Path.begin(), entry.Path.end());
				const auto padded = (out.size() - start + 8U) & ~std::size_t{ 7U };
				out.resize(start + padded, 0U);
			}
			previous = entry.Path;
		}
		for (const auto* signature : { aIndex.SplitIndex ? "link" : nullptr, aIndex.SparseDirectories ? "sdir" : nullptr }) {
			if (signature != nullptr) {
				out.insert(out.end(), signature, signature + 4);
				put32(out, 0U);
			}
		}
		if (aZeroTrailer) {
			out.insert(out.end(), hashLength, 0U);
		} else if (aAlgorithm == core::git_hash_algorithm::sha256) {
			const auto trailer = core::sha256(out);
			out.insert(out.end(), trailer.begin(), trailer.end());
		} else {
			const auto trailer = core::sha1(out);
			out.insert(out.end(), trailer.begin(), trailer.end());
		}
		return out;
	}

	/// \brief A config file Git reads back as exactly `aEntries`: one header per
	/// change of section, every value quoted with Git's escapes.
	[[nodiscard]] inline auto write_git_config(const std::vector<core::git_config_entry>& aEntries) -> std::string
	{
		const auto quoted = [](const std::string& aText, const bool aValue) {
			auto out = std::string{ "\"" };
			for (const auto character : aText) {
				switch (character) {
				case '"':
					out += "\\\"";
					break;
				case '\\':
					out += "\\\\";
					break;
				case '\n':
					out += aValue ? "\\n" : "\n";
					break;
				case '\t':
					out += aValue ? "\\t" : "\t";
					break;
				case '\b':
					out += aValue ? "\\b" : "\b";
					break;
				default:
					out += character;
					break;
				}
			}
			return out + "\"";
		};
		auto text    = std::string{ };
		auto current = std::pair<std::string, std::optional<std::string>>{ };
		auto started = false;
		for (const auto& entry : aEntries) {
			const auto section = std::pair{ entry.Section, entry.Subsection };
			if (!entry.Section.empty() || entry.Subsection) {
				if (!started || section != current) {
					text += "[" + entry.Section;
					if (entry.Subsection) {
						text += " " + quoted(*entry.Subsection, false);
					}
					text += "]\n";
				}
				current = section;
				started = true;
			}
			text += "\t" + entry.Key;
			if (entry.Value) {
				text += " = " + quoted(*entry.Value, true);
			}
			text += "\n";
		}
		return text;
	}
}

#endif
