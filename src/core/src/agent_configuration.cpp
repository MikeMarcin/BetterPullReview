#include "bpr/core/agent_configuration.hpp"

#include "bpr/core/hex.hpp"
#include "bpr/core/json_preflight.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/core/review_skill.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <ranges>
#include <set>
#include <utility>

namespace bpr::core::detail {
	/// The file as glaze reads and writes it. Optional members are left out
	/// when absent, so the bytes hold only what an item has.
	struct wire_agent
	{
		std::string                PortableId;
		std::string                Name;
		std::string                Protocol;
		std::optional<std::string> Preset;
		std::optional<std::string> ProgramName;
		std::vector<std::string>   Arguments;
		std::vector<std::string>   CheckArguments;
		std::string                Model;
		std::string                Effort;
		bool                       StrictRunChecks = true;
		bool                       ScratchStarts   = true;
		bool                       BuildsAndTests  = false;
		std::string                McpServers;
		std::string                Instructions;
	};
	struct wire_prompt
	{
		std::string                Purpose;
		std::string                Origin;
		std::optional<std::string> WrittenAgainst;
		std::optional<std::string> WrittenAgainstSha256;
		std::string                Text;
	};
	/// [[BPR-2198]]. Identity and Repositories: hosts, logins and
	/// repository names, never a sign-in.
	struct wire_profile_account
	{
		std::string Host;
		std::string Login;
	};
	struct wire_profile_identity
	{
		std::string                       ProfileName;
		std::vector<wire_profile_account> Accounts;
	};
	struct wire_profile_repository
	{
		std::optional<std::string> Host;
		std::string                Repository;
		bool                       Saved    = false;
		bool                       Followed = false;
	};
	/// [[BPR-2283]]. The Theme section: the base by name and each role's
	/// colour, keyed by the role's stable identifier.
	struct wire_theme
	{
		std::string                        Base;
		std::map<std::string, std::string> Colors;
	};
	struct wire_file
	{
		std::string                Format;
		std::uint32_t              Version = 0U;
		std::optional<std::string> Label;
		std::string                ExportedBy;
		std::vector<wire_agent>    Agents;
		std::vector<wire_prompt>   Prompts;
		/// Each section after Prompts is left out when it is not exported.
		std::optional<glz::raw_json>                        Workflows;
		std::optional<wire_profile_identity>                Identity;
		std::optional<std::vector<wire_profile_repository>> Repositories;
		/// RFC-BPR-0125 AC24.
		std::optional<glz::raw_json> PromptPack;
		/// [[BPR-2283]].
		std::optional<wire_theme> Theme;
	};
	/// Only the head, read first so a newer or foreign file is named as such.
	struct wire_head
	{
		std::string   Format;
		std::uint32_t Version = 0U;
	};
} // namespace bpr::core::detail

template <>
struct glz::meta<bpr::core::detail::wire_agent>
{
	using value_type            = bpr::core::detail::wire_agent;
	static constexpr auto value = object("id", &value_type::PortableId, "name", &value_type::Name, "protocol", &value_type::Protocol, "preset", &value_type::Preset, "program_name", &value_type::ProgramName,
										 "arguments", &value_type::Arguments, "check_arguments", &value_type::CheckArguments, "model", &value_type::Model, "effort", &value_type::Effort,
										 "strict_run_checks", &value_type::StrictRunChecks, "scratch_starts", &value_type::ScratchStarts, "builds_and_tests", &value_type::BuildsAndTests,
										 "mcp_servers", &value_type::McpServers, "instructions", &value_type::Instructions);
};
template <>
struct glz::meta<bpr::core::detail::wire_prompt>
{
	using value_type            = bpr::core::detail::wire_prompt;
	static constexpr auto value = object("purpose", &value_type::Purpose, "origin", &value_type::Origin, "written_against", &value_type::WrittenAgainst, "written_against_sha256",
										 &value_type::WrittenAgainstSha256, "text", &value_type::Text);
};
template <>
struct glz::meta<bpr::core::detail::wire_profile_account>
{
	using value_type            = bpr::core::detail::wire_profile_account;
	static constexpr auto value = object("host", &value_type::Host, "login", &value_type::Login);
};
template <>
struct glz::meta<bpr::core::detail::wire_profile_identity>
{
	using value_type            = bpr::core::detail::wire_profile_identity;
	static constexpr auto value = object("profile_name", &value_type::ProfileName, "accounts", &value_type::Accounts);
};
template <>
struct glz::meta<bpr::core::detail::wire_profile_repository>
{
	using value_type            = bpr::core::detail::wire_profile_repository;
	static constexpr auto value = object("host", &value_type::Host, "repository", &value_type::Repository, "saved", &value_type::Saved, "followed", &value_type::Followed);
};
template <>
struct glz::meta<bpr::core::detail::wire_theme>
{
	using value_type            = bpr::core::detail::wire_theme;
	static constexpr auto value = object("base", &value_type::Base, "colors", &value_type::Colors);
};
template <>
struct glz::meta<bpr::core::detail::wire_file>
{
	using value_type            = bpr::core::detail::wire_file;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version, "label", &value_type::Label, "exported_by", &value_type::ExportedBy, "agents", &value_type::Agents,
										 "prompts", &value_type::Prompts, "workflows", &value_type::Workflows, "identity", &value_type::Identity, "repositories", &value_type::Repositories, "prompt_pack", &value_type::PromptPack,
										 "theme", &value_type::Theme);
};
template <>
struct glz::meta<bpr::core::detail::wire_head>
{
	using value_type            = bpr::core::detail::wire_head;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version);
};

namespace bpr::core {
	namespace {
		constexpr auto strict_options = glz::opts{ .error_on_unknown_keys = true };
		constexpr auto head_options   = glz::opts{ .error_on_unknown_keys = false };
		constexpr auto write_options  = glz::opts{ .prettify = true };

		[[nodiscard]] auto plain(const std::string_view aText) noexcept -> bool
		{
			return utf8::valid(aText) && std::ranges::none_of(aText, [](const char aByte) { return static_cast<unsigned char>(aByte) < 0x20U || aByte == 0x7F; });
		}

		[[nodiscard]] auto protocol_of(const std::string_view aName) noexcept -> std::optional<agent_protocol>
		{
			for (std::size_t index = 0U; index < agent_protocol_count; ++index) {
				if (stable_name(static_cast<agent_protocol>(index)) == aName) {
					return static_cast<agent_protocol>(index);
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto preset_of(const std::string_view aName) noexcept -> std::optional<setup_agent>
		{
			for (std::size_t index = 0U; index < setup_agent_count; ++index) {
				if (stable_name(static_cast<setup_agent>(index)) == aName) {
					return static_cast<setup_agent>(index);
				}
			}
			return std::nullopt;
		}

		/// A program this agent must be given on the recipient's computer:
		/// every agent but Claude Code's and Codex's own.
		[[nodiscard]] auto needs_program(const portable_agent& aAgent) noexcept -> bool
		{
			return !aAgent.Preset || *aAgent.Preset == setup_agent::plain_text_harness || aAgent.Protocol == agent_protocol::external_terminal;
		}

		[[nodiscard]] auto file_name(const std::string_view aPath) -> std::string
		{
			const auto slash = aPath.find_last_of("/\\");
			return std::string{ slash == std::string_view::npos ? aPath : aPath.substr(slash + 1U) };
		}

		[[nodiscard]] auto valid_portable_id(const std::string_view aId) noexcept -> bool
		{
			return !aId.empty() && aId.size() <= maximum_portable_id_bytes
				&& std::ranges::all_of(aId, [](const char aByte) { return (aByte >= 'a' && aByte <= 'z') || (aByte >= '0' && aByte <= '9') || aByte == '-'; });
		}

		[[nodiscard]] auto valid_version_text(const std::string_view aText) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= maximum_prompt_version_bytes && plain(aText);
		}

		[[nodiscard]] auto digest_of_hex(const std::string_view aHex) -> std::optional<std::array<std::uint8_t, 32>>
		{
			if (aHex.size() != 64U) {
				return std::nullopt;
			}
			auto       digest = std::array<std::uint8_t, 32>{ };
			const auto nibble = [](const char aByte) -> int {
				return aByte >= '0' && aByte <= '9' ? aByte - '0' : aByte >= 'a' && aByte <= 'f' ? aByte - 'a' + 10
																								 : -1;
			};
			for (std::size_t index = 0U; index < digest.size(); ++index) {
				const auto high = nibble(aHex[index * 2U]);
				const auto low  = nibble(aHex[(index * 2U) + 1U]);
				if (high < 0 || low < 0) {
					return std::nullopt;
				}
				digest[index] = static_cast<std::uint8_t>((high << 4) | low);
			}
			return digest;
		}

		[[nodiscard]] auto valid_prompt_text(const prompt_kind aKind, const std::string_view aText) -> bool
		{
			if (aText.empty()) {
				return false;
			}
			if (aKind == prompt_kind::review_skill) {
				return inspect_review_skill(aText).has_value();
			}
			return aText.size() <= maximum_writing_instruction_bytes && valid_writing_utf8(aText);
		}

		[[nodiscard]] auto valid_host(const std::string_view aHost) noexcept -> bool
		{
			return !aHost.empty() && aHost.size() <= maximum_host_bytes
				&& std::ranges::all_of(aHost, [](const char aByte) { return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '-' || aByte == '.' || aByte == ':'; });
		}

		[[nodiscard]] auto valid_login(const std::string_view aLogin) noexcept -> bool
		{
			return !aLogin.empty() && aLogin.size() <= maximum_login_bytes
				&& std::ranges::all_of(aLogin, [](const char aByte) { return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '-' || aByte == '_'; });
		}

		/// "owner/name": one slash, neither part empty, GitHub's characters.
		[[nodiscard]] auto valid_repository_name(const std::string_view aName) noexcept -> bool
		{
			const auto slash = aName.find('/');
			if (slash == std::string_view::npos) {
				return false;
			}
			const auto owner = aName.substr(0U, slash);
			const auto name  = aName.substr(slash + 1U);
			if (owner.starts_with('.') || name == "." || name == "..") {
				return false;
			}
			return aName.size() <= maximum_repository_name_bytes && slash != std::string_view::npos && slash != 0U && slash + 1U < aName.size()
				&& aName.find('/', slash + 1U) == std::string_view::npos
				&& std::ranges::all_of(aName, [](const char aByte) { return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '-' || aByte == '_' || aByte == '.' || aByte == '/'; });
		}

		[[nodiscard]] auto same_host(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) {
					   const auto lower = [](const char aByte) { return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte; };
					   return lower(aA) == lower(aB);
				   });
		}

		[[nodiscard]] auto same_account(const portable_account& aLeft, const portable_account& aRight) noexcept -> bool
		{
			return same_host(aLeft.Host, aRight.Host) && same_host(aLeft.Login, aRight.Login);
		}

		/// The account signed in here on `aHost`, if any.
		[[nodiscard]] auto signed_in_on(const profile_facts& aHere, const std::string_view aHost) -> const portable_account*
		{
			const auto found = std::ranges::find_if(aHere.SignedIn, [aHost](const portable_account& aAccount) { return same_host(aAccount.Host, aHost); });
			return found == aHere.SignedIn.end() ? nullptr : &*found;
		}

		/// The entry an imported agent's own values make, with a stand-in
		/// program where it needs one, for the profile's validators.
		[[nodiscard]] auto inspect_portable(const portable_agent& aAgent) -> std::optional<agent_entry_refusal>
		{
			auto entry            = imported_entry(aAgent, agent_import_choice{ .Program = needs_program(aAgent) ? "/bound-on-import" : "" }, nullptr);
			entry.StrictRunChecks = true;
			if (!aAgent.ProgramName.empty() && (aAgent.ProgramName.size() > 255U || !plain(aAgent.ProgramName) || aAgent.ProgramName.find_first_of("/\\") != std::string::npos)) {
				return agent_entry_refusal::invalid_program;
			}
			return inspect_agent_entry(entry);
		}

		/// [[BPR-2198]]. Identity starts unchosen; each repository not here
		/// yet starts chosen unless it can only be saved and no account is
		/// signed in on its host ("Sign in to use").
		void plan_profile_sections(const decoded_configuration& aFile, const profile_facts& aHere, configuration_import_plan& aPlan)
		{
			if (aFile.Identity) {
				auto identity    = identity_import_plan{ };
				identity.Renames = aFile.Identity->ProfileName != aHere.ProfileName;
				for (const auto& account : aFile.Identity->Accounts) {
					const auto signedIn = std::ranges::any_of(aHere.SignedIn, [&account](const portable_account& aKnown) { return same_account(aKnown, account); });
					identity.Accounts.push_back(signedIn ? account_standing::signed_in : account_standing::sign_in_to_use);
				}
				const auto remembered = std::ranges::all_of(aFile.Identity->Accounts, [&aHere](const portable_account& aAccount) {
					return same_host(aAccount.Host, "github.com") || std::ranges::any_of(aHere.Remembered, [&aAccount](const portable_account& aKnown) { return same_host(aKnown.Host, aAccount.Host); });
				});
				identity.Standing     = !identity.Renames && remembered ? import_standing::unchanged : import_standing::updated;
				identity.Default      = false;
				aPlan.Identity        = identity;
			}
			for (const auto& repository : aFile.Repositories) {
				auto&      plan     = aPlan.Repositories.emplace_back();
				const auto saved    = !repository.Saved || std::ranges::any_of(aHere.Repositories, [&repository](const portable_repository& aKnown) {
					return aKnown.Saved && aKnown.Repository == repository.Repository && same_host(aKnown.Host, repository.Host);
				});
				const auto followed = !repository.Followed || std::ranges::any_of(aHere.Repositories, [&repository](const portable_repository& aKnown) {
					return aKnown.Followed && aKnown.Repository == repository.Repository;
				});
				if (saved && followed) {
					plan.Standing = import_standing::unchanged;
					continue;
				}
				plan.Standing    = import_standing::added;
				plan.SignInToUse = !saved && signed_in_on(aHere, repository.Host) == nullptr;
				// Following needs no account; saving alone does.
				plan.Default = !plan.SignInToUse || !followed;
			}
		}
	} // namespace

	auto stable_name(const profile_settings_section aSection) noexcept -> std::string_view
	{
		switch (aSection) {
		case profile_settings_section::agents:       return "agents";
		case profile_settings_section::prompts:      return "prompts";
		case profile_settings_section::workflows:    return "workflows";
		case profile_settings_section::identity:     return "identity";
		case profile_settings_section::repositories: return "repositories";
		case profile_settings_section::theme:        return "theme";
		}
		return "agents";
	}

	auto exported_by_default(const profile_settings_section aSection) noexcept -> bool { return aSection != profile_settings_section::identity; }

	auto portable_identity_of(const profile_facts& aFacts) -> portable_identity
	{
		auto identity        = portable_identity{ };
		identity.ProfileName = aFacts.ProfileName;
		const auto add       = [&identity](const portable_account& aAccount) {
			if (!aAccount.Host.empty() && !aAccount.Login.empty()
				&& std::ranges::none_of(identity.Accounts, [&aAccount](const portable_account& aKnown) { return same_account(aKnown, aAccount); })) {
				identity.Accounts.push_back(aAccount);
			}
		};
		std::ranges::for_each(aFacts.SignedIn, add);
		std::ranges::for_each(aFacts.Remembered, add);
		std::ranges::sort(identity.Accounts);
		return identity;
	}

	auto portable_repositories_of(const profile_facts& aFacts) -> std::vector<portable_repository>
	{
		// A saved repository and a follow of the same name stay two items: one
		// names its host, the other follows it on any host.
		auto repositories = std::vector<portable_repository>{ };
		for (const auto& repository : aFacts.Repositories) {
			const auto known = std::ranges::find_if(repositories, [&repository](const portable_repository& aKnown) {
				return same_host(aKnown.Host, repository.Host) && aKnown.Repository == repository.Repository;
			});
			if (known == repositories.end()) {
				repositories.push_back(repository);
			} else {
				known->Saved    = known->Saved || repository.Saved;
				known->Followed = known->Followed || repository.Followed;
			}
		}
		std::ranges::sort(repositories);
		return repositories;
	}

	auto stable_name(const portable_prompt_origin aOrigin) noexcept -> std::string_view
	{
		return aOrigin == portable_prompt_origin::built_in ? "built-in" : "edited";
	}

	auto stable_name(const configuration_file_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case configuration_file_refusal::too_large:           return "too_large";
		case configuration_file_refusal::not_text:            return "not_text";
		case configuration_file_refusal::malformed:           return "malformed";
		case configuration_file_refusal::not_a_configuration: return "not_a_configuration";
		case configuration_file_refusal::unsupported_version: return "unsupported_version";
		case configuration_file_refusal::too_many_items:      return "too_many_items";
		}
		return "malformed";
	}

	auto describe(const configuration_file_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case configuration_file_refusal::too_large:           return "The file is larger than 1 MiB.";
		case configuration_file_refusal::not_text:            return "The file is not UTF-8 text.";
		case configuration_file_refusal::malformed:           return "The file is not Profile Settings this version can read: its JSON is malformed or has fields it does not know.";
		case configuration_file_refusal::not_a_configuration: return "The file is not a BetterPullReview Profile Settings file.";
		case configuration_file_refusal::unsupported_version: return "The file was written by a newer version of BetterPullReview.";
		case configuration_file_refusal::too_many_items:      return "The file lists more items than a Profile Settings file may hold.";
		}
		return "The file cannot be read.";
	}

	auto stable_name(const configuration_item_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case configuration_item_problem::unsupported_protocol: return "unsupported_protocol";
		case configuration_item_problem::unsupported_purpose:  return "unsupported_purpose";
		case configuration_item_problem::duplicate:            return "duplicate";
		case configuration_item_problem::invalid_agent:        return "invalid_agent";
		case configuration_item_problem::invalid_prompt:       return "invalid_prompt";
		}
		return "invalid_agent";
	}

	auto describe(const configuration_item_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case configuration_item_problem::unsupported_protocol: return "Uses an agent kind this version does not support.";
		case configuration_item_problem::unsupported_purpose:  return "A prompt this version does not have.";
		case configuration_item_problem::duplicate:            return "Listed twice in the file; only the first is used.";
		case configuration_item_problem::invalid_agent:        return "Has a setting this version refuses.";
		case configuration_item_problem::invalid_prompt:       return "Its text is empty, too long, not UTF-8, or not a review skill.";
		}
		return "Cannot be applied.";
	}

	auto stable_name(const agent_field aField) noexcept -> std::string_view
	{
		switch (aField) {
		case agent_field::name:              return "name";
		case agent_field::protocol:          return "protocol";
		case agent_field::program:           return "program";
		case agent_field::arguments:         return "arguments";
		case agent_field::check_arguments:   return "check_arguments";
		case agent_field::model:             return "model";
		case agent_field::effort:            return "effort";
		case agent_field::strict_run_checks: return "strict_run_checks";
		case agent_field::scratch_starts:    return "scratch_starts";
		case agent_field::builds_and_tests:  return "builds_and_tests";
		case agent_field::mcp_servers:       return "mcp_servers";
		case agent_field::instructions:      return "instructions";
		}
		return "name";
	}

	auto stable_name(const configuration_apply_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case configuration_apply_refusal::unusable_item:    return "unusable_item";
		case configuration_apply_refusal::duplicate_choice: return "duplicate_choice";
		case configuration_apply_refusal::unbound_program:  return "unbound_program";
		case configuration_apply_refusal::unknown_target:   return "unknown_target";
		case configuration_apply_refusal::refused_list:     return "refused_list";
		}
		return "unusable_item";
	}

	auto stable_name(const import_standing aStanding) noexcept -> std::string_view
	{
		switch (aStanding) {
		case import_standing::added:     return "added";
		case import_standing::updated:   return "updated";
		case import_standing::unchanged: return "unchanged";
		case import_standing::refused:   return "refused";
		}
		return "refused";
	}

	auto portable_ids(const std::span<const std::string> aNames) -> std::vector<std::string>
	{
		auto ids  = std::vector<std::string>{ };
		auto seen = std::set<std::string>{ };
		for (const auto& name : aNames) {
			auto base = std::string{ };
			for (const auto byte : name) {
				const auto lower = static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte - 'A' + 'a' : byte);
				if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) {
					base.push_back(lower);
				} else if (!base.empty() && base.back() != '-') {
					base.push_back('-');
				}
			}
			while (!base.empty() && base.back() == '-') {
				base.pop_back();
			}
			base.resize(std::min(base.size(), maximum_portable_id_bytes - 4U));
			if (base.empty()) {
				base = "agent";
			}
			auto id = base;
			for (auto suffix = 2U; seen.contains(id); ++suffix) {
				id = base + "-" + std::to_string(suffix);
			}
			seen.insert(id);
			ids.push_back(std::move(id));
		}
		return ids;
	}

	auto portable_agent_of(const agent_entry& aEntry, std::string aPortableId) -> portable_agent
	{
		auto agent            = portable_agent{ };
		agent.PortableId      = std::move(aPortableId);
		agent.Name            = aEntry.Name;
		agent.Protocol        = aEntry.Protocol;
		agent.Preset          = aEntry.Preset;
		agent.Arguments       = aEntry.Arguments;
		agent.CheckArguments  = aEntry.CheckArguments;
		agent.Model           = aEntry.Model;
		agent.Effort          = aEntry.Effort;
		agent.StrictRunChecks = aEntry.StrictRunChecks;
		agent.ScratchStarts   = aEntry.ScratchStarts;
		agent.BuildsAndTests  = aEntry.BuildsAndTests;
		agent.McpServers      = aEntry.McpServers;
		agent.Instructions    = aEntry.Instructions;
		agent.ProgramName     = needs_program(agent) ? file_name(aEntry.Program) : std::string{ };
		return agent;
	}

	auto portable_prompt_of(const local_prompt& aPrompt, const bool aBuiltIn) -> portable_prompt
	{
		auto prompt    = portable_prompt{ };
		prompt.Purpose = aPrompt.Purpose;
		if (aBuiltIn || aPrompt.Override.empty()) {
			prompt.Origin               = portable_prompt_origin::built_in;
			prompt.Text                 = aPrompt.BuiltIn;
			prompt.WrittenAgainst       = aPrompt.BuiltInVersion;
			prompt.WrittenAgainstDigest = aPrompt.BuiltInDigest;
			return prompt;
		}
		prompt.Origin         = portable_prompt_origin::edited;
		prompt.Text           = aPrompt.Override;
		prompt.WrittenAgainst = aPrompt.WrittenAgainst;
		if (aPrompt.WrittenAgainst == aPrompt.BuiltInVersion) {
			prompt.WrittenAgainstDigest = aPrompt.BuiltInDigest;
		}
		return prompt;
	}

	auto encode_agent_configuration(const agent_configuration& aConfiguration) -> std::optional<std::string>
	{
		auto file       = detail::wire_file{ };
		file.Format     = std::string{ agent_configuration_format };
		file.Version    = agent_configuration_version;
		file.ExportedBy = aConfiguration.ExportedBy;
		// RFC-BPR-0125 AC24. The pack is left out when it holds no prompt.
		if (!aConfiguration.PromptMacros.empty()) {
			auto pack = encode_prompt_pack(aConfiguration.PromptPack, aConfiguration.PromptMacros);
			if (!pack) {
				return std::nullopt;
			}
			// Indented to sit as a member of the file, one level in.
			auto indented = std::string{ };
			for (const auto byte : *pack) {
				indented.push_back(byte);
				if (byte == '\n') {
					indented.append("   ");
				}
			}
			file.PromptPack = glz::raw_json{ std::move(indented) };
		}
		if (!aConfiguration.Label.empty()) {
			file.Label = aConfiguration.Label;
		}
		for (const auto& agent : aConfiguration.Agents) {
			auto wire            = detail::wire_agent{ };
			wire.PortableId      = agent.PortableId;
			wire.Name            = agent.Name;
			wire.Protocol        = std::string{ stable_name(agent.Protocol) };
			wire.Preset          = agent.Preset ? std::optional{ std::string{ stable_name(*agent.Preset) } } : std::nullopt;
			wire.ProgramName     = agent.ProgramName.empty() ? std::nullopt : std::optional{ agent.ProgramName };
			wire.Arguments       = agent.Arguments;
			wire.CheckArguments  = agent.CheckArguments;
			wire.Model           = agent.Model;
			wire.Effort          = agent.Effort;
			wire.StrictRunChecks = agent.StrictRunChecks;
			wire.ScratchStarts   = agent.ScratchStarts;
			wire.BuildsAndTests  = agent.BuildsAndTests;
			wire.McpServers      = std::string{ stable_name(agent.McpServers) };
			wire.Instructions    = std::string{ stable_name(agent.Instructions) };
			file.Agents.push_back(std::move(wire));
		}
		for (const auto& prompt : aConfiguration.Prompts) {
			auto wire                 = detail::wire_prompt{ };
			wire.Purpose              = prompt.Purpose;
			wire.Origin               = std::string{ stable_name(prompt.Origin) };
			wire.WrittenAgainst       = prompt.WrittenAgainst;
			wire.WrittenAgainstSha256 = prompt.WrittenAgainstDigest ? std::optional{ lower_hex(std::span<const std::uint8_t>{ *prompt.WrittenAgainstDigest }) } : std::nullopt;
			wire.Text                 = prompt.Text;
			file.Prompts.push_back(std::move(wire));
		}
		if (!aConfiguration.Workflows.empty()) {
			auto       workflows = encode_workflows(aConfiguration.Workflows);
			const auto carried   = [&aConfiguration](const workflow_action& aAction) {
				return aAction.Binding != workflow_binding::agent || std::ranges::find(aConfiguration.Agents, aAction.Agent, &portable_agent::PortableId) != aConfiguration.Agents.end();
			};
			const auto agentsCarried = std::ranges::all_of(aConfiguration.Workflows, [&carried](const workflow_configuration& aWorkflow) { return std::ranges::all_of(aWorkflow.Actions, carried); });
			if (!workflows || !agentsCarried) {
				return std::nullopt;
			}
			file.Workflows = glz::raw_json{ std::move(*workflows) };
		}
		if (aConfiguration.Identity) {
			auto identity        = detail::wire_profile_identity{ };
			identity.ProfileName = aConfiguration.Identity->ProfileName;
			for (const auto& account : aConfiguration.Identity->Accounts) {
				identity.Accounts.push_back({ .Host = account.Host, .Login = account.Login });
			}
			file.Identity = std::move(identity);
		}
		if (!aConfiguration.Repositories.empty()) {
			auto repositories = std::vector<detail::wire_profile_repository>{ };
			for (const auto& repository : aConfiguration.Repositories) {
				repositories.push_back({ .Host = repository.Host.empty() ? std::nullopt : std::optional{ repository.Host }, .Repository = repository.Repository, .Saved = repository.Saved, .Followed = repository.Followed });
			}
			file.Repositories = std::move(repositories);
		}
		if (aConfiguration.Theme) {
			auto theme = detail::wire_theme{ .Base = std::string{ stable_name(aConfiguration.Theme->Base) }, .Colors = { } };
			for (auto& entry : theme_entries(aConfiguration.Theme->Palette)) {
				theme.Colors.emplace(std::move(entry.Role), std::move(entry.Color));
			}
			file.Theme = std::move(theme);
		}
		auto bytes = std::string{ };
		if (glz::write<write_options>(file, bytes) || bytes.size() + 1U > maximum_agent_configuration_bytes) {
			return std::nullopt;
		}
		bytes.push_back('\n');
		return bytes;
	}

	auto decode_agent_configuration(const std::string_view aBytes, const std::span<const local_prompt> aRecognized) -> std::expected<decoded_configuration, configuration_file_refusal>
	{
		if (aBytes.size() > maximum_agent_configuration_bytes) {
			return std::unexpected(configuration_file_refusal::too_large);
		}
		if (!utf8::valid(aBytes)) {
			return std::unexpected(configuration_file_refusal::not_text);
		}
		// RFC-BPR-0125 AC24: a prompt's condition patterns sit eleven deep.
		// [[BPR-2283]]: the Theme's colours are one object of a member a role.
		const auto limits = json_preflight_limits{ .Depth = 12U, .ObjectMembers = appearance_role_count + 32U, .ArrayElements = maximum_configuration_repositories, .Values = 50'000U, .StringBytes = maximum_review_skill_bytes * 6U };
		if (const auto report = json_preflight(aBytes, limits); report.Result != json_preflight_result::valid) {
			if (report.Result == json_preflight_result::structural_limit) {
				return std::unexpected(configuration_file_refusal::too_many_items);
			}
			// [[BPR-2283]]. Text that is not JSON and never names the format
			// (a `.bprtheme` document, a note) is not Profile Settings at all.
			return std::unexpected(aBytes.find(agent_configuration_format) == std::string_view::npos ? configuration_file_refusal::not_a_configuration : configuration_file_refusal::malformed);
		}
		auto head = detail::wire_head{ };
		if (glz::read<head_options>(head, aBytes)) {
			return std::unexpected(configuration_file_refusal::malformed);
		}
		if (head.Format != agent_configuration_format) {
			return std::unexpected(configuration_file_refusal::not_a_configuration);
		}
		// [[BPR-2283]] (owner, 2026-11-19: "don't keep legacy support"). A
		// file written before version 3 is not one this version reads.
		if (head.Version > agent_configuration_version) {
			return std::unexpected(configuration_file_refusal::unsupported_version);
		}
		if (head.Version != agent_configuration_version) {
			return std::unexpected(configuration_file_refusal::not_a_configuration);
		}
		auto file = detail::wire_file{ };
		if (glz::read<strict_options>(file, aBytes)) {
			return std::unexpected(configuration_file_refusal::malformed);
		}
		if (file.Agents.size() > maximum_configuration_agents || file.Prompts.size() > maximum_configuration_prompts
			|| (file.Identity && file.Identity->Accounts.size() > maximum_configuration_accounts)
			|| (file.Repositories && file.Repositories->size() > maximum_configuration_repositories)) {
			return std::unexpected(configuration_file_refusal::too_many_items);
		}
		auto decoded    = decoded_configuration{ };
		decoded.Version = head.Version;
		// RFC-BPR-0125 AC24. A prompt pack this version cannot read makes the
		// file malformed.
		if (file.PromptPack) {
			auto pack = decode_prompt_pack(file.PromptPack->str);
			if (!pack) {
				return std::unexpected(configuration_file_refusal::malformed);
			}
			decoded.PromptPack = std::move(*pack);
		}
		decoded.Label      = file.Label.value_or(std::string{ });
		decoded.ExportedBy = file.ExportedBy;
		if (decoded.Label.size() > maximum_agent_configuration_label_bytes || !plain(decoded.Label) || decoded.ExportedBy.size() > maximum_agent_configuration_label_bytes
			|| !plain(decoded.ExportedBy)) {
			return std::unexpected(configuration_file_refusal::malformed);
		}
		auto ids = std::set<std::string>{ };
		for (auto& wire : file.Agents) {
			auto item                  = decoded_agent{ };
			item.Agent.PortableId      = wire.PortableId;
			item.Agent.Name            = wire.Name;
			item.Agent.ProgramName     = wire.ProgramName.value_or(std::string{ });
			item.Agent.Arguments       = std::move(wire.Arguments);
			item.Agent.CheckArguments  = std::move(wire.CheckArguments);
			item.Agent.Model           = wire.Model;
			item.Agent.Effort          = wire.Effort;
			item.Agent.StrictRunChecks = wire.StrictRunChecks;
			item.Agent.ScratchStarts   = wire.ScratchStarts;
			item.Agent.BuildsAndTests  = wire.BuildsAndTests;
			// [[BPR-2139]], [[BPR-2140]]. A value this version does not
			// know makes the file malformed, as any other unknown value does.
			const auto mcp          = parse_agent_mcp_servers(wire.McpServers);
			const auto instructions = parse_agent_instruction_delivery(wire.Instructions);
			if (!mcp || !instructions) {
				return std::unexpected(configuration_file_refusal::malformed);
			}
			item.Agent.McpServers   = *mcp;
			item.Agent.Instructions = *instructions;
			const auto protocol     = protocol_of(wire.Protocol);
			const auto preset       = wire.Preset ? preset_of(*wire.Preset) : std::nullopt;
			if (!protocol || (wire.Preset && !preset)) {
				item.Problem     = configuration_item_problem::unsupported_protocol;
				item.Unsupported = !protocol ? wire.Protocol : *wire.Preset;
			} else {
				item.Agent.Protocol = *protocol;
				item.Agent.Preset   = preset;
				if (!valid_portable_id(item.Agent.PortableId)) {
					item.Problem = configuration_item_problem::invalid_agent;
				} else if (const auto refusal = inspect_portable(item.Agent)) {
					item.Problem = configuration_item_problem::invalid_agent;
					item.Refusal = refusal;
				}
			}
			if (!ids.insert(wire.PortableId).second && !item.Problem) {
				item.Problem = configuration_item_problem::duplicate;
			}
			decoded.Agents.push_back(std::move(item));
		}
		auto purposes = std::set<std::size_t>{ };
		for (auto& wire : file.Prompts) {
			auto item           = decoded_prompt{ };
			item.Prompt.Purpose = wire.Purpose;
			item.Prompt.Text    = std::move(wire.Text);
			const auto local    = std::ranges::find(aRecognized, wire.Purpose, &local_prompt::Purpose);
			if (local == aRecognized.end()) {
				item.Problem = configuration_item_problem::unsupported_purpose;
				decoded.Prompts.push_back(std::move(item));
				continue;
			}
			item.Local        = static_cast<std::size_t>(local - aRecognized.begin());
			const auto origin = wire.Origin == stable_name(portable_prompt_origin::built_in) ? std::optional{ portable_prompt_origin::built_in }
				: wire.Origin == stable_name(portable_prompt_origin::edited)                 ? std::optional{ portable_prompt_origin::edited }
																							 : std::nullopt;
			const auto digest = wire.WrittenAgainstSha256 ? digest_of_hex(*wire.WrittenAgainstSha256) : std::nullopt;
			if (!origin || (wire.WrittenAgainst && !valid_version_text(*wire.WrittenAgainst)) || (wire.WrittenAgainstSha256 && !digest)
				|| !valid_prompt_text(local->Kind, item.Prompt.Text)) {
				item.Problem = configuration_item_problem::invalid_prompt;
			} else {
				item.Prompt.Origin               = *origin;
				item.Prompt.WrittenAgainst       = wire.WrittenAgainst;
				item.Prompt.WrittenAgainstDigest = digest;
			}
			if (!purposes.insert(*item.Local).second && !item.Problem) {
				item.Problem = configuration_item_problem::duplicate;
			}
			decoded.Prompts.push_back(std::move(item));
		}
		if (file.Workflows) {
			auto agents = std::vector<std::string>{ };
			std::ranges::transform(decoded.Agents, std::back_inserter(agents), [](const decoded_agent& aAgent) { return aAgent.Agent.PortableId; });
			auto workflows = decode_workflows(file.Workflows->str, agents);
			if (!workflows) {
				return std::unexpected(configuration_file_refusal::malformed);
			}
			decoded.Workflows = std::move(*workflows);
		}
		// [[BPR-2198]]. A host, login or name this version would not accept
		// makes the file malformed: these sections are plain names, and one
		// that is not is not a file to trust in part.
		if (file.Identity) {
			auto identity = portable_identity{ .ProfileName = file.Identity->ProfileName, .Accounts = { } };
			if (identity.ProfileName.empty() || identity.ProfileName.size() > maximum_bpr_profile_label_bytes || !plain(identity.ProfileName)) {
				return std::unexpected(configuration_file_refusal::malformed);
			}
			for (const auto& account : file.Identity->Accounts) {
				if (!valid_host(account.Host) || !valid_login(account.Login)) {
					return std::unexpected(configuration_file_refusal::malformed);
				}
				identity.Accounts.push_back({ .Host = account.Host, .Login = account.Login });
			}
			decoded.Identity = std::move(identity);
		}
		for (const auto& wire : file.Repositories.value_or(std::vector<detail::wire_profile_repository>{ })) {
			const auto host = wire.Host.value_or(std::string{ });
			if ((!host.empty() && !valid_host(host)) || !valid_repository_name(wire.Repository) || (!wire.Saved && !wire.Followed) || (wire.Saved && host.empty())) {
				return std::unexpected(configuration_file_refusal::malformed);
			}
			decoded.Repositories.push_back({ .Host = host, .Repository = wire.Repository, .Saved = wire.Saved, .Followed = wire.Followed });
		}
		// [[BPR-2283]]. A theme the theme reader refuses is listed refused.
		if (file.Theme) {
			auto entries = std::vector<theme_entry>{ };
			for (const auto& [role, color] : file.Theme->Colors) {
				entries.push_back({ .Role = role, .Color = color });
			}
			auto read  = import_theme_entries(file.Theme->Base, entries);
			auto theme = decoded_theme{ .Theme = std::nullopt, .Rejections = std::move(read.Rejections) };
			if (theme.Rejections.empty() && read.Palette && read.Base) {
				theme.Theme = portable_theme{ .Base = *read.Base, .Palette = *read.Palette };
			}
			decoded.Theme = std::move(theme);
		}
		return decoded;
	}

	auto plan_theme_import(const decoded_theme& aFile, const std::optional<appearance_palette>& aReadable, const appearance_palette& aHere) -> theme_import_plan
	{
		if (!aFile.Theme || !aReadable) {
			return { .Standing = import_standing::refused, .Default = false };
		}
		if (*aReadable == aHere) {
			return { .Standing = import_standing::unchanged, .Default = false };
		}
		return { .Standing = import_standing::updated, .Default = true };
	}

	auto encode_theme_settings(const portable_theme& aTheme, std::string aExportedBy) -> std::optional<std::string>
	{
		auto file       = agent_configuration{ };
		file.ExportedBy = std::move(aExportedBy);
		file.Theme      = aTheme;
		return encode_agent_configuration(file);
	}

	auto colliding_agent(const std::span<const agent_entry> aAgents, const portable_agent& aAgent) noexcept -> const agent_entry*
	{
		const auto named = std::ranges::find(aAgents, aAgent.Name, &agent_entry::Name);
		if (named != aAgents.end()) {
			return &*named;
		}
		// Settings offers a popular agent only until it is added: an imported
		// Claude Code or Codex meets the one already there.
		if (aAgent.Preset && *aAgent.Preset != setup_agent::plain_text_harness) {
			return find_preset_entry(aAgents, *aAgent.Preset);
		}
		return nullptr;
	}

	auto compatible_agent(const agent_entry& aLocal, const portable_agent& aAgent) noexcept -> bool { return aLocal.Protocol == aAgent.Protocol; }

	auto keeps_agent_identity(const agent_entry& aBefore, const agent_entry& aAfter) noexcept -> bool
	{
		return aBefore.Protocol == aAfter.Protocol && aBefore.Program == aAfter.Program && aBefore.Arguments == aAfter.Arguments
			&& aBefore.StrictRunChecks == aAfter.StrictRunChecks;
	}

	auto agent_differences(const agent_entry& aBefore, const agent_entry& aAfter) -> std::vector<agent_field>
	{
		auto       fields = std::vector<agent_field>{ };
		const auto differ = [&fields](const bool aDiffers, const agent_field aField) {
			if (aDiffers) {
				fields.push_back(aField);
			}
		};
		differ(aBefore.Name != aAfter.Name, agent_field::name);
		differ(aBefore.Protocol != aAfter.Protocol || aBefore.Preset != aAfter.Preset, agent_field::protocol);
		differ(aBefore.Program != aAfter.Program, agent_field::program);
		differ(aBefore.Arguments != aAfter.Arguments, agent_field::arguments);
		differ(aBefore.CheckArguments != aAfter.CheckArguments, agent_field::check_arguments);
		differ(aBefore.Model != aAfter.Model, agent_field::model);
		differ(aBefore.Effort != aAfter.Effort, agent_field::effort);
		differ(aBefore.StrictRunChecks != aAfter.StrictRunChecks, agent_field::strict_run_checks);
		differ(aBefore.ScratchStarts != aAfter.ScratchStarts, agent_field::scratch_starts);
		differ(aBefore.BuildsAndTests != aAfter.BuildsAndTests, agent_field::builds_and_tests);
		differ(aBefore.McpServers != aAfter.McpServers, agent_field::mcp_servers);
		differ(aBefore.Instructions != aAfter.Instructions, agent_field::instructions);
		return fields;
	}

	auto imported_entry(const portable_agent& aAgent, const agent_import_choice& aChoice, const agent_entry* const aTarget) -> agent_entry
	{
		auto entry           = agent_entry{ };
		entry.Id             = aTarget != nullptr ? aTarget->Id : 0U;
		entry.Name           = aChoice.Name.empty() ? aAgent.Name : aChoice.Name;
		entry.Protocol       = aAgent.Protocol;
		entry.Preset         = aAgent.Preset;
		entry.Program        = aChoice.Program.empty() && aTarget != nullptr ? aTarget->Program : aChoice.Program;
		entry.Arguments      = aAgent.Arguments;
		entry.CheckArguments = aAgent.CheckArguments;
		entry.Model          = aAgent.Model;
		entry.Effort         = aAgent.Effort;
		// RFC-BPR-0107: imported settings never silently relax a check, and a
		// popular agent stays strict.
		entry.StrictRunChecks = aAgent.Preset.has_value() || aAgent.StrictRunChecks || !aChoice.AcceptRelaxedChecks;
		// X17: builds stay off unless accepted; a requested Off is applied,
		// otherwise the recipient's own starting state.
		entry.BuildsAndTests = aAgent.BuildsAndTests && aChoice.AcceptBuilds;
		entry.ScratchStarts  = aAgent.ScratchStarts && (aTarget == nullptr || aTarget->ScratchStarts);
		// [[BPR-2139]], [[BPR-2140]]. Shown in the preview like any other
		// setting and applied as the file says.
		entry.McpServers   = aAgent.McpServers;
		entry.Instructions = aAgent.Instructions;
		return entry;
	}

	auto apply_agent_configuration(const std::span<const agent_entry> aAgents, const std::span<const local_prompt> aLocal, const decoded_configuration& aFile,
								   const std::span<const agent_import_choice> aAgentChoices, const std::span<const std::size_t> aPrompts)
		-> std::expected<agent_configuration_result, configuration_apply_error>
	{
		auto result    = agent_configuration_result{ };
		result.Agents  = { aAgents.begin(), aAgents.end() };
		result.Prompts = { aLocal.begin(), aLocal.end() };
		auto chosen    = std::set<std::size_t>{ };
		for (const auto& choice : aAgentChoices) {
			const auto fail = [&choice](const configuration_apply_refusal aRefusal, std::optional<agent_entry_refusal> aEntry = std::nullopt) {
				return std::unexpected(configuration_apply_error{ .Refusal = aRefusal, .Item = choice.Item, .Entry = aEntry });
			};
			if (choice.Item >= aFile.Agents.size() || aFile.Agents[choice.Item].Problem) {
				return fail(configuration_apply_refusal::unusable_item);
			}
			if (!chosen.insert(choice.Item).second) {
				return fail(configuration_apply_refusal::duplicate_choice);
			}
			const auto& agent  = aFile.Agents[choice.Item].Agent;
			const auto  target = std::ranges::find(result.Agents, choice.Target, &agent_entry::Id);
			switch (choice.Action) {
			case agent_import_action::leave: break;
			case agent_import_action::use_existing:
				if (target == result.Agents.end() || !compatible_agent(*target, agent)) {
					return fail(configuration_apply_refusal::unknown_target);
				}
				++result.AgentsApplied;
				break;
			case agent_import_action::add: {
				if (needs_program(agent) && choice.Program.empty()) {
					return fail(configuration_apply_refusal::unbound_program);
				}
				auto entry = imported_entry(agent, choice, nullptr);
				if (const auto refusal = inspect_agent_entry(entry); refusal == agent_entry_refusal::invalid_program || refusal == agent_entry_refusal::no_program) {
					return fail(configuration_apply_refusal::unbound_program, refusal);
				}
				auto added = add_agent_entry(std::move(result.Agents), std::move(entry));
				if (!added) {
					return fail(configuration_apply_refusal::refused_list, added.error());
				}
				result.Agents = std::move(*added);
				++result.AgentsApplied;
				break;
			}
			case agent_import_action::replace: {
				if (target == result.Agents.end()) {
					return fail(configuration_apply_refusal::unknown_target);
				}
				auto entry = imported_entry(agent, choice, &*target);
				if (keeps_agent_identity(*target, entry)) {
					entry.Identity = target->Identity;
				} else {
					result.Detached.push_back(target->Id);
				}
				auto updated = update_agent_entry(std::move(result.Agents), std::move(entry));
				if (!updated) {
					return fail(updated.error() == agent_entry_refusal::invalid_program || updated.error() == agent_entry_refusal::no_program ? configuration_apply_refusal::unbound_program
																																			  : configuration_apply_refusal::refused_list,
								updated.error());
				}
				result.Agents = std::move(*updated);
				result.Replaced.push_back(choice.Target);
				++result.AgentsApplied;
				break;
			}
			}
		}
		if (const auto refusal = inspect_agent_entries(result.Agents)) {
			return std::unexpected(configuration_apply_error{ .Refusal = configuration_apply_refusal::refused_list, .Entry = refusal });
		}
		auto used = std::set<std::size_t>{ };
		for (const auto index : aPrompts) {
			if (index >= aFile.Prompts.size() || aFile.Prompts[index].Problem || !aFile.Prompts[index].Local) {
				return std::unexpected(configuration_apply_error{ .Refusal = configuration_apply_refusal::unusable_item, .Item = index });
			}
			if (!used.insert(index).second) {
				return std::unexpected(configuration_apply_error{ .Refusal = configuration_apply_refusal::duplicate_choice, .Item = index });
			}
			const auto& prompt = aFile.Prompts[index].Prompt;
			auto&       local  = result.Prompts[*aFile.Prompts[index].Local];
			// The shipped text needs no override: Restore default still works,
			// and a later default reaches it.
			if (prompt.Text == local.BuiltIn) {
				local.Override.clear();
				local.WrittenAgainst.reset();
			} else {
				local.Override       = prompt.Text;
				local.WrittenAgainst = prompt.WrittenAgainst;
			}
			++result.PromptsApplied;
		}
		return result;
	}

	auto plan_configuration_import(const decoded_configuration& aFile, const std::span<const agent_entry> aAgents, const std::span<const local_prompt> aPrompts,
								   const std::span<const local_workflow> aWorkflows, const profile_facts& aHere) -> configuration_import_plan
	{
		auto plan = configuration_import_plan{ };
		for (std::size_t index = 0U; index < aFile.Agents.size(); ++index) {
			const auto& item  = aFile.Agents[index];
			auto&       agent = plan.Agents.emplace_back();
			if (item.Problem) {
				continue;
			}
			for (const auto& entry : aAgents) {
				if (compatible_agent(entry, item.Agent)) {
					agent.StandIns.push_back(entry.Id);
				}
			}
			const auto named = std::ranges::find(aAgents, item.Agent.Name, &agent_entry::Name);
			if (named == aAgents.end()) {
				// A popular agent is run as found; any other needs its program.
				agent.Standing     = import_standing::added;
				agent.NeedsProgram = !item.Agent.Preset || *item.Agent.Preset == setup_agent::plain_text_harness || item.Agent.Protocol == agent_protocol::external_terminal;
				agent.Default      = { .Item = index, .Action = agent_import_action::add };
				continue;
			}
			agent.Meets = named->Id;
			// Unchanged: replacing it, with its own relaxed checks and builds,
			// would change nothing.
			const auto same  = agent_import_choice{ .Item = index, .Action = agent_import_action::replace, .Target = named->Id, .AcceptRelaxedChecks = !named->StrictRunChecks, .AcceptBuilds = named->BuildsAndTests };
			const auto after = imported_entry(item.Agent, same, &*named);
			if (compatible_agent(*named, item.Agent) && agent_differences(*named, after).empty()) {
				agent.Standing = import_standing::unchanged;
				agent.Default  = { .Item = index, .Action = agent_import_action::leave };
			} else {
				// An entry of the same name but another kind cannot take the
				// file's settings: it starts left out and Update is not offered.
				agent.Standing = import_standing::updated;
				agent.Default  = compatible_agent(*named, item.Agent) ? agent_import_choice{ .Item = index, .Action = agent_import_action::replace, .Target = named->Id }
																	  : agent_import_choice{ .Item = index, .Action = agent_import_action::leave };
			}
		}
		for (const auto& item : aFile.Prompts) {
			auto& prompt = plan.Prompts.emplace_back();
			if (item.Problem || !item.Local || *item.Local >= aPrompts.size()) {
				continue;
			}
			const auto& local = aPrompts[*item.Local];
			const auto& mine  = local.Override.empty() ? local.BuiltIn : local.Override;
			prompt.Standing   = item.Prompt.Text == mine ? import_standing::unchanged : import_standing::updated;
			prompt.Default    = prompt.Standing == import_standing::updated;
		}
		for (const auto& item : aFile.Workflows) {
			auto& workflow = plan.Workflows.emplace_back();
			if (item.Problem) {
				continue;
			}
			const auto local = std::ranges::find_if(aWorkflows, [&item](const local_workflow& aLocal) { return aLocal.Workflow.PortableId == item.Workflow.PortableId; });
			if (local == aWorkflows.end()) {
				workflow.Standing = import_standing::added;
				workflow.Default  = workflow_import_action::add;
				continue;
			}
			workflow.Local       = static_cast<std::size_t>(local - aWorkflows.begin());
			workflow.Differences = workflow_differences(local->Workflow, item.Workflow);
			workflow.Standing    = workflow.Differences.empty() ? import_standing::unchanged : import_standing::updated;
			workflow.Default     = workflow.Differences.empty() ? workflow_import_action::leave : workflow_import_action::replace;
		}
		plan_profile_sections(aFile, aHere, plan);
		return plan;
	}

	auto profile_changes(const decoded_configuration& aFile, const configuration_import_plan& aPlan, const bool aIdentity, const std::span<const std::size_t> aRepositories,
						 const profile_facts& aHere) -> profile_settings_changes
	{
		auto changes = profile_settings_changes{ };
		if (aIdentity && aFile.Identity && aPlan.Identity && aPlan.Identity->Standing != import_standing::refused) {
			if (aPlan.Identity->Renames) {
				changes.Rename = aFile.Identity->ProfileName;
			}
			// github.com needs no server remembered; an enterprise server
			// this profile does not know yet is remembered with the login to
			// sign in as. Signing in stays the reviewer's.
			for (const auto& account : aFile.Identity->Accounts) {
				const auto known = std::ranges::any_of(aHere.Remembered, [&account](const portable_account& aKnown) { return same_host(aKnown.Host, account.Host); });
				if (!same_host(account.Host, "github.com") && !known) {
					changes.RememberServers.push_back(account);
				}
			}
		}
		for (const auto index : aRepositories) {
			if (index >= aFile.Repositories.size() || index >= aPlan.Repositories.size() || aPlan.Repositories[index].Standing != import_standing::added) {
				continue;
			}
			const auto& repository = aFile.Repositories[index];
			const auto  has        = [&aHere, &repository](const bool aSaved) {
				return std::ranges::any_of(aHere.Repositories, [&](const portable_repository& aKnown) {
					return aKnown.Repository == repository.Repository && (aSaved ? aKnown.Saved && same_host(aKnown.Host, repository.Host) : aKnown.Followed);
				});
			};
			if (repository.Saved && !has(true)) {
				if (const auto* const account = signed_in_on(aHere, repository.Host)) {
					changes.Save.push_back({ .Account = *account, .Repository = repository.Repository });
				}
			}
			if (repository.Followed && !has(false)) {
				changes.Follow.push_back(repository.Repository);
			}
		}
		return changes;
	}
} // namespace bpr::core
