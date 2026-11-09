#include "github_application/agent_composition.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QTextStream>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/app/diagnostic_export_directory.hpp"
	#include <QByteArray>
	#include <QDateTime>
	#include <QFile>
	#include <QFileDevice>
	#include <QIODevice>
	#include <QIODeviceBase>
	#include <fstream>
#endif

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	auto save_review_skill_copy(const std::string& aText) -> std::optional<std::string>
	{
		const auto directory = resolve_diagnostic_export_directory(diagnostic_export_locations());
		if (!directory) {
			return std::nullopt;
		}
		const auto name = QStringLiteral("bpr-pr-review SKILL %1.md").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm.ss")));
		auto       file = QFile{ QDir{ QString::fromStdString(*directory) }.filePath(name) };
		if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
			return std::nullopt;
		}
		const auto written = file.write(aText.data(), static_cast<qint64>(aText.size()));
		file.close();
		if (written != static_cast<qint64>(aText.size()) || file.error() != QFileDevice::NoError) {
			static_cast<void>(file.remove());
			return std::nullopt;
		}
		return QDir::toNativeSeparators(file.fileName()).toStdString();
	}
#endif
	auto named_workspace_ownership_failure(const int aFailure) -> std::string
	{
		switch (static_cast<adapters::git::workspace_ownership_failure>(aFailure)) {
		case adapters::git::workspace_ownership_failure::repair_required: return "repair-required";
		case adapters::git::workspace_ownership_failure::capacity:        return "capacity";
		case adapters::git::workspace_ownership_failure::cancelled:       return "cancelled";
		case adapters::git::workspace_ownership_failure::unavailable:     break;
		}
		return "unavailable";
	}

	auto agent_workspace_refusal_text(const adapters::git::workspace_recovery_refusal& aRefusal) -> QString
	{
		using check        = adapters::git::workspace_refusal_check;
		const auto folder  = QDir::toNativeSeparators(QString::fromStdU16String(aRefusal.Folder.u16string()));
		const auto account = QString::fromUtf8(aRefusal.Account.data(), static_cast<qsizetype>(aRefusal.Account.size()));
		const auto lead    = QStringLiteral("Disposable agent workspaces are unavailable: ");
		switch (aRefusal.Check) {
		case check::none:
			break;
		case check::invalid_path:
			return lead + QStringLiteral("their folder %1 is not a usable path.").arg(folder);
		case check::neighbourhood_parent_shared:
			if (account.startsWith(QStringLiteral("owner "))) {
				return lead + QStringLiteral("%1 is owned by %2, not by you, so a workspace kept in it could be replaced. Take ownership of that folder, then restart.").arg(folder, account.mid(6));
			}
			return lead + (account.isEmpty() ? QStringLiteral("another account can change %1, so a workspace kept in it could be replaced. Remove other accounts' write access to that folder, or ask your administrator, then restart.").arg(folder) : QStringLiteral("%2 can change %1, so a workspace kept in it could be replaced. Remove that account's write access to the folder, or ask your administrator, then restart.").arg(folder, account));
		case check::neighbourhood_not_private:
			// [[BPR-1870]]. One of this user's own is narrowed before this
			// is reached; what is left is another account's, or a link.
			return lead + QStringLiteral("%1 is not private to you and could not be made so: it belongs to another account, or is a link. Rename it aside and restart; a new private folder is made in its place.").arg(folder);
		case check::neighbourhood_not_created:
			return lead + QStringLiteral("%1 could not be created. Check that the folder above it exists and you can write to it, then restart.").arg(folder);
		case check::data_folder_not_private:
			return lead + QStringLiteral("the data folder %1 is not private to you. Quit, then restart: startup makes the application's own data folder private.").arg(folder);
		case check::registry_not_private:
			return lead + QStringLiteral("their record folder %1 is not private to you and could not be made so: it belongs to another account, or is a link. Rename it aside and restart.").arg(folder);
		case check::registry_unexpected_entry:
			return lead + QStringLiteral("%1 is not a workspace record. Move it out of that folder, then restart.").arg(folder);
		case check::unrecorded_leftover:
			return lead + QStringLiteral("%1 is a workspace no record accounts for. It is kept, not deleted; move it out of that folder, then restart.").arg(folder);
		case check::record_not_private:
			return lead + QStringLiteral("the workspace record %1 is not private to you and could not be made so (another account's, a link, or a file with more than one name), or cannot be opened. Move it out of its folder, then restart.").arg(folder);
		case check::record_unreadable:
			return lead + QStringLiteral("the workspace that record %1 names could not be read back or cleaned up exactly, so it is kept. Move the record and its workspace out of their folders, then restart.").arg(folder);
		case check::limit_reached:
			return lead + QStringLiteral("%1 holds more entries than the limit. Move old workspaces out of it, then restart.").arg(folder);
		case check::lock_unavailable:
			return lead + QStringLiteral("%1 is locked by another process. Close other BetterPullReview windows, then restart.").arg(folder);
		}
		return { };
	}
#if defined(BPR_BUILD_TESTING) && defined(BPR_AGENT_INTEGRATIONS)
	composition_agent_process_launcher_probe::composition_agent_process_launcher_probe(
		adapters::agent::agent_process_launcher& aLauncher) noexcept
		: Launcher{ aLauncher }
	{
	}

	auto composition_agent_process_launcher_probe::run(
		const application::agent_launch_request& aRequest,
		const int                                aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink,
		const std::stop_token                    aStop)
		-> application::agent_run_result
	{
		Invocations.fetch_add(1U, std::memory_order_relaxed);
		return Launcher.run(aRequest, aTimeoutMilliseconds, aSink, aStop);
	}

	void composition_agent_process_launcher_probe::cancel() noexcept
	{
		Launcher.cancel();
	}

	auto composition_agent_process_launcher_probe::invocations() const noexcept -> std::size_t
	{
		return Invocations.load(std::memory_order_relaxed);
	}
#endif
#if defined(BPR_AGENT_INTEGRATIONS)
	auto freeze_mcp_snapshot(
		const std::filesystem::path&                          aRoot,
		const std::string&                                    aBinding,
		const std::vector<core::agent_finding_snapshot_file>& aFiles) -> bool
	{
		auto error = std::error_code{ };
		std::filesystem::remove_all(aRoot, error);
		std::filesystem::create_directories(aRoot / "files", error);
		if (error) {
			return false;
		}
		const auto hex = [](const auto& aIdentity) {
			const auto& bytes = aIdentity.bytes();
			return QByteArray::fromRawData(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())).toHex().toStdString();
		};
		auto binding = std::ofstream{ aRoot / "binding", std::ios::binary };
		binding << aBinding << '\n';
		auto alignment = std::ofstream{ aRoot / "alignment", std::ios::binary };
		for (const auto& file : aFiles) {
			alignment << "file\t" << file.Path << '\n'
					  << "comparison\t" << hex(file.Comparison) << '\n'
					  << "left_line_count\t" << file.LeftLineCount << '\n'
					  << "right_line_count\t" << file.RightLineCount << '\n';
			for (const auto& segment : file.Segments) {
				alignment << "segment\t" << hex(segment) << '\n';
			}
			for (const auto& group : file.Groups) {
				alignment << "group\t" << hex(group) << '\n';
			}
			for (const auto& move : file.Moves) {
				alignment << "move\t" << hex(move) << '\n';
			}
		}
		return binding.good() && alignment.good();
	}
#endif
	tour_session_projection::tour_session_projection(identity_sink aAdmit, start_sink aStart, run_sink aFinish, refuse_sink aRefuse, identity_sink aStream)
		: Admit{ std::move(aAdmit) }
		, Start{ std::move(aStart) }
		, Finish{ std::move(aFinish) }
		, Refuse{ std::move(aRefuse) }
		, Stream{ std::move(aStream) }
	{
	}

	void tour_session_projection::offer(std::span<const core::agent_profile>) { }

	void tour_session_projection::refuse(const std::string_view aReason) { Refuse(aReason); }

	void tour_session_projection::admit(const application::agent_invocation_projection_identity& aIdentity) { Admit(aIdentity); }

	void tour_session_projection::start(const application::agent_invocation_projection_identity& aIdentity, const application::prepared_workspace& aWorkspace) { Start(aIdentity, aWorkspace); }

	void tour_session_projection::stream(const application::agent_invocation_projection_identity& aIdentity, const application::agent_stream_event&)
	{
		if (Stream) {
			Stream(aIdentity);
		}
	}

	void tour_session_projection::finish(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult) { Finish(aIdentity, aResult); }

	void tour_session_projection::present_session_catalog(const application::agent_session_catalog_projection&) { }

	composition_agent_invocation_projection::composition_agent_invocation_projection(
		application::agent_invocation_projection_port& aPanel,
		finding_sink                                   aFindings,
		session_sink                                   aSessions,
		admit_sink                                     aAdmissions,
		run_sink                                       aRuns,
		workspace_sink                                 aWorkspaces,
		start_sink aStarts, std::function<void(std::string_view)> aRefused)
		: Panel{ aPanel }
		, Findings{ std::move(aFindings) }
		, Sessions{ std::move(aSessions) }
		, Admissions{ std::move(aAdmissions) }
		, Runs{ std::move(aRuns) }
		, Workspaces{ std::move(aWorkspaces) }
		, Starts{ std::move(aStarts) }
		, Refused{ std::move(aRefused) }
	{
	}

	void composition_agent_invocation_projection::offer(const std::span<const core::agent_profile> aProfiles)
	{
		// [[BPR-1753]], owner amendment 2026-10-23. The picker offers
		// the agents the reviewer added, not every profile the host knows.
		if (OfferFilter) {
			const auto offered = OfferFilter(aProfiles);
			Panel.offer(offered);
			return;
		}
		Panel.offer(aProfiles);
	}

	void composition_agent_invocation_projection::set_offer_filter(std::function<std::vector<core::agent_profile>(std::span<const core::agent_profile>)> aFilter) { OfferFilter = std::move(aFilter); }

	void composition_agent_invocation_projection::refuse(const std::string_view aReason)
	{
		Panel.refuse(aReason);
		if (Refused) {
			Refused(aReason);
		}
	}

	void composition_agent_invocation_projection::admit(
		const application::agent_invocation_projection_identity& aIdentity)

	{
		Panel.admit(aIdentity);
		// The one place an agent session identity is allocated. The chat
		// host learns the session its turn actually runs under from here
		// rather than keeping one of its own, so the delivery this run
		// produces is keyed to the session the chat is holding.
		if (Sessions) {
			Sessions(aIdentity.Session);
		}
		if (Admissions) {
			Admissions(aIdentity);
		}
	}

	void composition_agent_invocation_projection::start(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::prepared_workspace&                   aWorkspace)
	{
		Panel.start(aIdentity, aWorkspace);
		if (Workspaces) {
			Workspaces(aWorkspace);
		}
		if (Starts) {
			Starts(aIdentity);
		}
	}

	void composition_agent_invocation_projection::stream(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_stream_event&                   aEvent)
	{
		Panel.stream(aIdentity, aEvent);
	}

	void composition_agent_invocation_projection::finish(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_run_result&                     aResult)
	{
		Panel.finish(aIdentity, aResult);
		// The one place a turn is known to be over. A press taken out on
		// this turn is answered by now or is never going to be, and either
		// way the turn it was waiting for has stopped running.
		if (Runs) {
			Runs(aIdentity, aResult);
		}
	}

	void composition_agent_invocation_projection::deliver_findings(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_finding_delivery&               aDelivery)
	{
		Panel.deliver_findings(aIdentity, aDelivery);
		if (Findings) {
			Findings(aIdentity, aDelivery);
		}
	}

	void composition_agent_invocation_projection::restore_native_session(const core::agent_session& aSession)
	{
		if (NativeSession) {
			NativeSession(aSession);
		} else if (Sessions) {
			Sessions(aSession);
		}
	}

	void composition_agent_invocation_projection::present_session_catalog(
		const application::agent_session_catalog_projection& aProjection)

	{
		Panel.present_session_catalog(aProjection);
	}

	void composition_agent_invocation_projection::set_private_sink(std::function<void(const application::private_review_delta&)> aChanged, std::function<void(const application::private_run_error&)> aFailed)
	{
		PrivateChanged = std::move(aChanged);
		PrivateFailed  = std::move(aFailed);
	}

	void composition_agent_invocation_projection::private_changed(const application::agent_invocation_projection_identity&, const application::private_review_delta& aDelta)
	{
		if (PrivateChanged) {
			PrivateChanged(aDelta);
		}
	}

	void composition_agent_invocation_projection::private_refused(const application::agent_invocation_projection_identity&, const application::private_run_error& aError)
	{
		if (PrivateFailed) {
			PrivateFailed(aError);
		}
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	composition_agent_chat_projection::composition_agent_chat_projection(
		state_sink aState, outcome_sink aOutcome, clear_sink aClear)
		: State{ std::move(aState) }
		, Outcome{ std::move(aOutcome) }
		, Clear{ std::move(aClear) }
	{
	}

	void composition_agent_chat_projection::present(
		const application::agent_chat_projection& aProjection)
	{
		if (State) {
			State(aProjection);
		}
	}

	void composition_agent_chat_projection::present(const application::agent_chat_outcome& aOutcome)
	{
		if (Outcome) {
			Outcome(aOutcome);
		}
	}

	void composition_agent_chat_projection::clear()
	{
		if (Clear) {
			Clear();
		}
	}

	composition_agent_finding_projection::composition_agent_finding_projection(
		delivery_sink aDelivered, outcome_sink aAnswered, clear_sink aCleared)
		: Delivered{ std::move(aDelivered) }
		, Answered{ std::move(aAnswered) }
		, Cleared{ std::move(aCleared) }
	{
	}

	void composition_agent_finding_projection::present(const application::agent_finding_delivery& aDelivery)
	{
		if (Delivered) {
			Delivered(aDelivery);
		}
	}

	void composition_agent_finding_projection::present(const application::agent_finding_outcome& aOutcome)
	{
		if (Answered) {
			Answered(aOutcome);
		}
	}

	void composition_agent_finding_projection::clear()
	{
		if (Cleared) {
			Cleared();
		}
	}

	composition_comment_workshop_projection::composition_comment_workshop_projection(
		proposal_sink aProposed, refusal_sink aRefused)
		: Proposed{ std::move(aProposed) }
		, Refused{ std::move(aRefused) }
	{
	}

	void composition_comment_workshop_projection::present(
		const application::agent_comment_workshop_proposal& aProposal)
	{
		if (Proposed) {
			Proposed(aProposal);
		}
	}

	void composition_comment_workshop_projection::refuse(
		const application::agent_comment_workshop_refusal aRefusal)
	{
		if (Refused) {
			Refused(aRefusal);
		}
	}
#endif
}
