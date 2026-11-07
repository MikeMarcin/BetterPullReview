#include "bpr/core/skill_references.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto name_character(const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
		}

		[[nodiscard]] auto word_character(const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '_'
				|| static_cast<unsigned char>(aCharacter) >= 0x80U;
		}

		/// A slash may open a reference here: the text's start, whitespace,
		/// or an opening bracket or quote.
		[[nodiscard]] auto opens_reference(const std::string_view aText, const std::size_t aSlash) noexcept -> bool
		{
			if (aSlash == 0U) {
				return true;
			}
			const auto before = aText[aSlash - 1U];
			return before == ' ' || before == '\t' || before == '\n' || before == '\r' || before == '(' || before == '[' || before == '"' || before == '\'';
		}

		/// The name may end here: the text's end, or a character that is not
		/// part of a word, a path or a file name.
		[[nodiscard]] auto closes_reference(const std::string_view aText, const std::size_t aEnd) noexcept -> bool
		{
			if (aEnd >= aText.size()) {
				return true;
			}
			const auto after = aText[aEnd];
			if (word_character(after) || after == '/' || after == '-' || after == ':' || after == '@') {
				return false;
			}
			if (after == '.') {
				return aEnd + 1U >= aText.size() || !word_character(aText[aEnd + 1U]);
			}
			return true;
		}

		[[nodiscard]] auto find_entry(const std::span<const skill_entry> aCatalog, const std::string_view aName, const skill_source aSource) -> std::optional<std::size_t>
		{
			for (std::size_t index = 0U; index < aCatalog.size(); ++index) {
				if (aCatalog[index].Source == aSource && aCatalog[index].Name == aName) {
					return index;
				}
			}
			return std::nullopt;
		}

		void keep_source(std::vector<skill_entry>& aEntries, const skill_source aSource)
		{
			std::erase_if(aEntries, [aSource](const skill_entry& aEntry) { return aEntry.Source != aSource || !valid_skill_name(aEntry.Name); });
			std::ranges::stable_sort(aEntries, { }, &skill_entry::Name);
			const auto duplicates = std::ranges::unique(aEntries, { }, &skill_entry::Name);
			aEntries.erase(duplicates.begin(), duplicates.end());
			if (aEntries.size() > maximum_catalog_skills) {
				aEntries.resize(maximum_catalog_skills);
			}
		}
	}

	auto stable_name(const skill_source aSource) noexcept -> std::string_view
	{
		switch (aSource) {
		case skill_source::personal:
			return "personal";
		case skill_source::repository:
			return "repository";
		}
		return { };
	}

	auto valid_skill_name(const std::string_view aName) noexcept -> bool
	{
		return !aName.empty() && aName.size() <= maximum_skill_name_bytes && aName.front() != '-' && std::ranges::all_of(aName, name_character);
	}

	auto find_skill_references(const std::string_view aText) -> std::vector<skill_reference>
	{
		auto found = std::vector<skill_reference>{ };
		for (auto slash = aText.find('/'); slash != std::string_view::npos; slash = aText.find('/', slash + 1U)) {
			if (!opens_reference(aText, slash)) {
				continue;
			}
			auto       start     = slash + 1U;
			auto       qualifier = std::optional<skill_source>{ };
			const auto rest      = aText.substr(start);
			if (rest.starts_with(repository_skill_qualifier)) {
				qualifier = skill_source::repository;
				start += repository_skill_qualifier.size();
			} else if (rest.starts_with(personal_skill_qualifier)) {
				qualifier = skill_source::personal;
				start += personal_skill_qualifier.size();
			}
			auto end = start;
			while (end < aText.size() && name_character(aText[end])) {
				++end;
			}
			const auto name = aText.substr(start, end - start);
			if (!valid_skill_name(name) || !closes_reference(aText, end)) {
				continue;
			}
			found.push_back({ .Begin = slash, .End = end, .Name = std::string{ name }, .Qualifier = qualifier });
		}
		return found;
	}

	auto resolve_skill_references(const std::span<const std::string_view> aTexts, const std::span<const skill_entry> aCatalog) -> skill_resolution
	{
		auto resolution = skill_resolution{ };
		for (const auto text : aTexts) {
			for (const auto& reference : find_skill_references(text)) {
				auto entry = std::optional<std::size_t>{ };
				if (reference.Qualifier) {
					entry = find_entry(aCatalog, reference.Name, *reference.Qualifier);
				} else {
					entry = find_entry(aCatalog, reference.Name, skill_source::personal);
					if (!entry) {
						entry = find_entry(aCatalog, reference.Name, skill_source::repository);
					}
				}
				if (!entry) {
					if (std::ranges::find(resolution.Unknown, reference.Name) == resolution.Unknown.end()) {
						resolution.Unknown.push_back(reference.Name);
					}
					continue;
				}
				if (std::ranges::find(resolution.Resolved, *entry) != resolution.Resolved.end()) {
					continue;
				}
				if (resolution.Resolved.size() == maximum_referenced_skills) {
					++resolution.Dropped;
					continue;
				}
				resolution.Resolved.push_back(*entry);
			}
		}
		return resolution;
	}

	auto skill_resolution_note(const skill_resolution& aResolution) -> std::optional<std::string>
	{
		auto note = std::string{ };
		if (!aResolution.Unknown.empty()) {
			for (std::size_t index = 0U; index < aResolution.Unknown.size(); ++index) {
				note += (index == 0U ? "/" : index + 1U == aResolution.Unknown.size() ? " and /"
																					  : ", /")
					+ aResolution.Unknown[index];
			}
			note += aResolution.Unknown.size() == 1U ? " matched no skill, so it was sent as text." : " matched no skill, so they were sent as text.";
		}
		if (aResolution.Dropped != 0U) {
			note += std::string{ note.empty() ? "" : " " } + "Only the first " + std::to_string(maximum_referenced_skills) + " skills were brought in; "
				+ std::to_string(aResolution.Dropped) + (aResolution.Dropped == 1U ? " more was" : " more were") + " left as text.";
		}
		return note.empty() ? std::nullopt : std::optional{ std::move(note) };
	}

	auto skill_completion_at(const std::string_view aText, const std::size_t aCursor) -> std::optional<skill_completion_context>
	{
		if (aCursor == 0U || aCursor > aText.size()) {
			return std::nullopt;
		}
		auto begin = aCursor;
		while (begin > 0U && (name_character(aText[begin - 1U]) || aText[begin - 1U] == ':')) {
			--begin;
		}
		if (begin == 0U || aText[begin - 1U] != '/' || !opens_reference(aText, begin - 1U)) {
			return std::nullopt;
		}
		auto prefix = aText.substr(begin, aCursor - begin);
		if (const auto colon = prefix.find(':'); colon != std::string_view::npos) {
			const auto qualifier = prefix.substr(0U, colon + 1U);
			if ((qualifier != repository_skill_qualifier && qualifier != personal_skill_qualifier) || prefix.find(':', colon + 1U) != std::string_view::npos) {
				return std::nullopt;
			}
		}
		if (prefix.size() > maximum_skill_name_bytes + repository_skill_qualifier.size()) {
			return std::nullopt;
		}
		return skill_completion_context{ .Begin = begin - 1U, .Prefix = std::string{ prefix } };
	}

	auto complete_skill_references(const std::string_view aPrefix, const std::span<const skill_entry> aCatalog) -> std::vector<skill_completion>
	{
		auto bare      = aPrefix;
		auto qualifier = std::optional<skill_source>{ };
		if (bare.starts_with(repository_skill_qualifier)) {
			qualifier = skill_source::repository;
			bare.remove_prefix(repository_skill_qualifier.size());
		} else if (bare.starts_with(personal_skill_qualifier)) {
			qualifier = skill_source::personal;
			bare.remove_prefix(personal_skill_qualifier.size());
		}
		auto rows = std::vector<skill_completion>{ };
		for (std::size_t index = 0U; index < aCatalog.size(); ++index) {
			const auto& entry = aCatalog[index];
			if (!entry.Name.starts_with(bare) || (qualifier && entry.Source != *qualifier)) {
				continue;
			}
			const bool shadowed = entry.Source == skill_source::repository && find_entry(aCatalog, entry.Name, skill_source::personal).has_value();
			auto       insert   = entry.Name;
			if (shadowed || qualifier) {
				insert.insert(0U, entry.Source == skill_source::repository ? repository_skill_qualifier : personal_skill_qualifier);
			}
			rows.push_back({ .Insert = std::move(insert), .Entry = index, .Shadowed = shadowed });
		}
		std::ranges::stable_sort(rows, [&aCatalog](const skill_completion& aLeft, const skill_completion& aRight) {
			const auto& left  = aCatalog[aLeft.Entry];
			const auto& right = aCatalog[aRight.Entry];
			if (left.Name != right.Name) {
				return left.Name < right.Name;
			}
			return left.Source == skill_source::personal && right.Source != skill_source::personal;
		});
		return rows;
	}

	auto merge_skill_catalog(std::vector<skill_entry> aPersonal, std::vector<skill_entry> aRepository) -> std::vector<skill_entry>
	{
		keep_source(aPersonal, skill_source::personal);
		keep_source(aRepository, skill_source::repository);
		aPersonal.insert(aPersonal.end(), std::make_move_iterator(aRepository.begin()), std::make_move_iterator(aRepository.end()));
		return aPersonal;
	}
}
