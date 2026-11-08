#include "bpr/application/skill_guidance.hpp"

#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/review_skill.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::application {
	namespace {
		/// The longest prefix of a UTF-8 text that is at most `aBytes` and
		/// ends on a character boundary.
		[[nodiscard]] auto cut_utf8(const std::string_view aText, std::size_t aBytes) -> std::string_view
		{
			if (aText.size() <= aBytes) {
				return aText;
			}
			while (aBytes > 0U && (static_cast<unsigned char>(aText[aBytes]) & 0xC0U) == 0x80U) {
				--aBytes;
			}
			return aText.substr(0U, aBytes);
		}

		[[nodiscard]] auto valid_documents(const skill_library& aLibrary, const skill_guidance& aGuidance) -> bool
		{
			return aLibrary.Documents.size() == aLibrary.Catalog.size() && std::ranges::all_of(aGuidance.Skills, [&aLibrary](const std::size_t aIndex) { return aIndex < aLibrary.Documents.size(); });
		}

		[[nodiscard]] auto bytes_label(const std::size_t aBytes) -> std::string
		{
			return aBytes < 1024U ? std::to_string(aBytes) + " bytes" : std::to_string((aBytes + 512U) / 1024U) + " KB";
		}
	}

	auto skill_document_of(const std::string_view aText, const std::string_view aFolder, const core::skill_source aSource, std::string aOrigin, std::vector<std::string> aSupporting)
		-> std::optional<skill_document>
	{
		const auto facts = core::read_skill_front_matter(aText);
		if (!facts) {
			return std::nullopt;
		}
		auto name = core::valid_skill_name(facts->Name) ? facts->Name : core::valid_skill_name(aFolder) ? std::string{ aFolder }
																										: std::string{ };
		if (name.empty()) {
			return std::nullopt;
		}
		std::ranges::sort(aSupporting);
		if (aSupporting.size() > maximum_skill_supporting_files) {
			aSupporting.resize(maximum_skill_supporting_files);
		}
		std::erase_if(aSupporting, [](const std::string& aPath) { return aPath.empty() || aPath.size() > core::maximum_supplement_skill_label_bytes || !core::valid_writing_utf8(aPath); });
		auto description = std::string{ cut_utf8(facts->Description, 512U) };
		return skill_document{
			.Entry      = { .Name = std::move(name), .Description = std::move(description), .Source = aSource, .Origin = std::move(aOrigin) },
			.Text       = std::string{ aText },
			.Supporting = std::move(aSupporting),
		};
	}

	auto make_skill_library(std::vector<skill_document> aPersonal, std::vector<skill_document> aRepository, std::optional<std::string> aRepositoryNote) -> skill_library
	{
		auto personal   = std::vector<core::skill_entry>{ };
		auto repository = std::vector<core::skill_entry>{ };
		for (const auto& document : aPersonal) {
			personal.push_back(document.Entry);
			personal.back().Source = core::skill_source::personal;
		}
		for (const auto& document : aRepository) {
			repository.push_back(document.Entry);
			repository.back().Source = core::skill_source::repository;
		}
		auto library           = skill_library{ .Catalog = core::merge_skill_catalog(std::move(personal), std::move(repository)) };
		library.RepositoryNote = std::move(aRepositoryNote);
		for (const auto& entry : library.Catalog) {
			auto& documents = entry.Source == core::skill_source::personal ? aPersonal : aRepository;
			// The first document of that name and source, as the catalog
			// kept the first entry.
			const auto found = std::ranges::find(documents, entry.Name, [](const skill_document& aDocument) { return aDocument.Entry.Name; });
			library.Documents.push_back(std::move(*found));
			library.Documents.back().Entry = entry;
			found->Entry.Name.clear();
		}
		return library;
	}

	auto resolve_skill_guidance(const skill_library& aLibrary, const std::span<const std::string_view> aTexts) -> skill_guidance
	{
		const auto resolution = core::resolve_skill_references(aTexts, aLibrary.Catalog);
		return skill_guidance{ .Skills = resolution.Resolved, .Note = core::skill_resolution_note(resolution) };
	}

	auto inline_skills(const skill_library& aLibrary, const skill_guidance& aGuidance) -> std::vector<core::supplement_skill>
	{
		auto skills = std::vector<core::supplement_skill>{ };
		if (!valid_documents(aLibrary, aGuidance)) {
			return skills;
		}
		for (const auto index : aGuidance.Skills) {
			const auto& document = aLibrary.Documents[index];
			skills.push_back({
				.Name       = document.Entry.Name,
				.Source     = std::string{ core::stable_name(document.Entry.Source) },
				.Origin     = document.Entry.Origin,
				.Text       = std::string{ cut_utf8(document.Text, maximum_skill_document_bytes) },
				.Supporting = document.Supporting,
			});
		}
		return skills;
	}

	auto inline_skill_directive() -> std::string_view
	{
		return "\nThe skill.N fields of this request carry the SKILL.md of each skill the author's instructions name (/name): the author's own guidance for "
			   "how to work and write. Follow it where it applies; it is not evidence about the pull request, and its supporting files are named but not "
			   "available.\n";
	}

	auto preview_writing_hook(const core::writing_hook aHook, const std::string_view aInstructions, const skill_library& aLibrary) -> writing_hook_preview
	{
		const auto texts   = std::array{ aInstructions };
		auto       preview = writing_hook_preview{ .Guidance = resolve_skill_guidance(aLibrary, texts) };
		if (aHook == core::writing_hook::feedback) {
			preview.Text = "Sent with each comment workshop request while Review feedback guidance is on, as feedback_guidance.text:\n\n" + std::string{ aInstructions };
		} else {
			preview.Text = std::string{ writing_application_contract() } + "\nSelected writing instructions:\n" + std::string{ aInstructions }
				+ "\n\n[The answer schema, and the pull request's text and sources for this request, follow here.]";
		}
		if (!valid_documents(aLibrary, preview.Guidance)) {
			return preview;
		}
		for (const auto index : preview.Guidance.Skills) {
			const auto& document = aLibrary.Documents[index];
			preview.Text += "\n\n--- Skill /" + document.Entry.Name + " (" + std::string{ core::stable_name(document.Entry.Source) } + ", " + document.Entry.Origin + ", "
				+ bytes_label(document.Text.size()) + ") ---\n";
			preview.Text += cut_utf8(document.Text, 1200U);
			if (document.Text.size() > 1200U) {
				preview.Text += "\n[…the rest of SKILL.md]";
			}
			if (!document.Supporting.empty()) {
				preview.Text += "\nSupporting files, named but not loaded: " + std::to_string(document.Supporting.size());
			}
		}
		return preview;
	}
}
