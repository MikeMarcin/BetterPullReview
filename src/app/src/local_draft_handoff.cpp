#include "local_draft_handoff.hpp"

#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/local_draft_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/core/hex.hpp"

#include <QDateTime>
#include <QRandomGenerator>

#include <array>

namespace bpr::app {
	namespace {
		auto text(const std::string_view aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		auto failed(const QString& aCode, const QString& aMessage) -> local_draft_handoff
		{
			return { .Ok = false, .Result = QJsonObject{ { "ok", false }, { "code", aCode }, { "message", aMessage } } };
		}

		auto new_id() -> core::local_draft_id
		{
			auto bytes = std::array<std::uint8_t, 16>{ };
			for (auto& byte : bytes) {
				byte = static_cast<std::uint8_t>(QRandomGenerator::system()->bounded(256U));
			}
			return { core::lower_hex(bytes) };
		}
	}

	auto open_local_draft_here(const std::filesystem::path& aRoot, const std::filesystem::path& aGitExecutable, const std::optional<std::filesystem::path>& aWindowsSystemRoot,
							   const std::string& aLocation, const std::optional<std::string>& aContext) -> local_draft_handoff
	{
		const auto isolation = aRoot / "local-draft-git-v1";
		if (aGitExecutable.empty() || !adapters::sqlite::create_storage_root_owner_private(isolation)) {
			return failed(QStringLiteral("capability_unavailable"), QStringLiteral("Installed Git was not found; a local draft reads the worktree with it."));
		}
		auto reader = adapters::git::make_installed_git_local_draft_reader({ .GitExecutable = aGitExecutable, .IsolationRoot = isolation, .WindowsSystemRoot = aWindowsSystemRoot });
		if (!reader) {
			return failed(QStringLiteral("capability_unavailable"), QStringLiteral("Installed Git could not be prepared for reading the worktree."));
		}
		auto store = adapters::sqlite::open_local_draft_store(aRoot);
		if (!store) {
			return failed(QStringLiteral("storage_unavailable"), QStringLiteral("This profile's local drafts could not be opened."));
		}
		auto source = adapters::workspace::make_local_workspace_source();
		auto opened = application::open_local_draft(*store, *source, **reader, aLocation, aContext, new_id(), QDateTime::currentSecsSinceEpoch(), { });
		if (!opened) {
			switch (opened.error().Failure) {
			case application::local_draft_open_failure::not_git:
				return failed(QStringLiteral("not_git"), QStringLiteral("This folder is not a Git worktree. Review it with bpr cycle open instead."));
			case application::local_draft_open_failure::repository:
				return failed(QStringLiteral("unsupported_repository"), QStringLiteral("This repository uses a format BetterPullReview does not read."));
			case application::local_draft_open_failure::workspace:
			case application::local_draft_open_failure::cancelled:
				return failed(QStringLiteral("unreadable"), QStringLiteral("The worktree could not be read."));
			}
		}
		auto data = QJsonObject{ { "outcome", text(application::name(opened->Outcome)) }, { "location", text(aLocation) } };
		if (opened->Recovery != application::local_draft_recovery::none) {
			data.insert("recovery", text(application::name(opened->Recovery)));
		}
		if (opened->Discovery.Selection) {
			data.insert("target_question", text(core::name(opened->Discovery.Selection->Question)));
		}
		if (opened->Draft) {
			const auto& draft = *opened->Draft;
			data.insert("draft", text(draft.Id.Value));
			data.insert("title", text(draft.Title));
			data.insert("branch", text(draft.Branch));
			data.insert("target", text(draft.Target.Target.Repository.Owner + "/" + draft.Target.Target.Repository.Name + " " + draft.Target.Target.Branch));
			data.insert("candidates", static_cast<qint64>(draft.Candidates.size()));
			data.insert("notes", static_cast<qint64>(draft.Notes.size()));
		}
		const auto ok     = opened->Draft.has_value() && opened->Outcome != application::local_draft_open_outcome::moved;
		auto       result = QJsonObject{ { "ok", ok }, { "data", data } };
		if (!ok) {
			result.insert("code", opened->Outcome == application::local_draft_open_outcome::moved ? QStringLiteral("rebind_needed") : opened->Outcome == application::local_draft_open_outcome::choose_target ? QStringLiteral("target_needed")
																																																			  : QStringLiteral("not_ready"));
			result.insert("message", opened->Outcome == application::local_draft_open_outcome::moved ? QStringLiteral("This worktree's draft was prepared at another location. Open it in the desktop to rebind it.") : opened->Outcome == application::local_draft_open_outcome::choose_target ? QStringLiteral("Choose the target in the desktop; it is not guessed.")
																																																																								: QStringLiteral("Nothing can be prepared yet; see recovery."));
		}
		return { .Ok = ok, .Result = std::move(result), .Draft = opened->Draft ? std::optional{ opened->Draft->Id } : std::nullopt };
	}
}
