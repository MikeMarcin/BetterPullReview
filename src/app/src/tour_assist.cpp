#include "tour_assist.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
namespace bpr::app {
	namespace {
		auto refuse(std::string aCode, std::string aMessage, int aExit = 2) -> std::unexpected<tour_cli_error> { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		auto text(std::string_view aValue) -> QString { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); }
		auto id_text(std::uint64_t aValue) -> QString { return QString::number(aValue); }
		auto compact(const QJsonObject& aObject) -> std::string { return QJsonDocument{ aObject }.toJson(QJsonDocument::Compact).toStdString(); }
		auto bounded_source(std::string aId, std::string_view aText, std::size_t aLimit, std::size_t aRemaining) -> std::optional<application::writing_source>
		{
			auto size = std::min({ aText.size(), aLimit, aRemaining });
			while (size && !core::writing_boundary(aText, size)) {
				--size;
			}
			if (!size || !core::valid_writing_utf8(aText.substr(0, size))) {
				return std::nullopt;
			}
			return application::writing_source{ std::move(aId), std::string{ aText.substr(0, size) } };
		}
		void put_digest(std::vector<std::uint8_t>& aInput, std::string_view aText)
		{
			const auto size = static_cast<std::uint64_t>(aText.size());
			for (unsigned i = 0; i < 8; ++i) {
				aInput.push_back(static_cast<std::uint8_t>(size >> (i * 8)));
			}
			aInput.insert(aInput.end(), aText.begin(), aText.end());
		}
		auto error_for(std::string_view aReason) -> std::unexpected<tour_cli_error>
		{
			if (aReason == "stale") {
				return refuse("stale", "The draft changed while the request was prepared. Read it again.", 4);
			}
			if (aReason == "busy") {
				return refuse("owner_busy", "The draft is loading or closing. Nothing was issued; retry shortly.", 4);
			}
			if (aReason == "read_only") {
				return refuse("read_only", "This draft is read-only, so nothing can be rewritten.", 4);
			}
			if (aReason == "no_such_field") {
				return refuse("unknown_stop", "No such field: the stop is not in this draft.", 2);
			}
			return refuse("invalid_range", "The requested range is not valid for that field.", 2);
		}
		auto describe_range(const application::tour_assist_ticket& aTicket) -> QString
		{
			return aTicket.Scope == core::writing_scope::selection ? "selection" : aTicket.Scope == core::writing_scope::empty_field ? "empty_field"
																																	 : "field";
		}
	}
	auto tour_hook_name(core::writing_hook aHook) -> std::string_view
	{
		switch (aHook) {
		case core::writing_hook::problem:  return "problem";
		case core::writing_hook::solution: return "solution";
		case core::writing_hook::outline:  return "outline";
		case core::writing_hook::comment:  return "comment";
		}
		return "problem";
	}
	auto parse_tour_hook(std::string_view aName) -> std::optional<core::writing_hook>
	{
		for (const auto hook : { core::writing_hook::problem, core::writing_hook::solution, core::writing_hook::outline, core::writing_hook::comment }) {
			if (aName == tour_hook_name(hook)) {
				return hook;
			}
		}
		return std::nullopt;
	}
	auto tour_assist_issue(application::review_tour_host& aHost, const core::writing_hook_settings& aSettings, const tour_assist_query& aQuery, std::vector<application::writing_source> aExtra, std::int64_t aNow) -> tour_cli_result<tour_assist_issued>
	{
		const auto shown = aHost.presentation();
		if (!shown.Draft || shown.Loading) {
			return refuse("owner_busy", "The draft is loading. Nothing was issued; retry shortly.", 4);
		}
		const auto& draft   = *shown.Draft;
		const bool  outline = aQuery.Hook == core::writing_hook::outline;
		std::string raw;
		std::size_t begin = 0, end = 0;
		if (!outline) {
			const std::string* field = nullptr;
			if (aQuery.Hook == core::writing_hook::problem) {
				field = &draft.Problem;
			} else if (aQuery.Hook == core::writing_hook::solution) {
				field = &draft.Solution;
			} else if (const auto stop = std::ranges::find(draft.Stops, aQuery.Stop, &core::tour_stop::Id); stop != draft.Stops.end()) {
				field = &stop->Body;
			}
			if (!field) {
				return error_for("no_such_field");
			}
			raw = *field;
			if (aQuery.Range && aQuery.Quote) {
				return refuse("invalid_range", "Use either --range-bytes or --quote, not both.");
			}
			if (aQuery.Quote) {
				const auto found = core::writing_quote_range(raw, *aQuery.Quote);
				if (!found) {
					return found.error() == core::writing_refusal::ambiguous ? refuse("ambiguous_quote", "That passage occurs more than once. Quote more of it, or give --range-bytes.")
						: found.error() == core::writing_refusal::not_found  ? refuse("quote_not_found", "That passage is not in the current text.")
																			 : refuse("invalid_range", "That quote is empty, malformed or starts inside a line ending.");
				}
				begin = found->first;
				end   = found->second;
			} else if (aQuery.Range) {
				if (aQuery.Range->first >= aQuery.Range->second || aQuery.Range->second > raw.size() || !core::writing_boundary(raw, static_cast<std::size_t>(aQuery.Range->first)) || !core::writing_boundary(raw, static_cast<std::size_t>(aQuery.Range->second))) {
					return refuse("invalid_range", "The byte range must be non-empty, inside the field and on character boundaries (not inside a CRLF pair).");
				}
				begin = static_cast<std::size_t>(aQuery.Range->first);
				end   = static_cast<std::size_t>(aQuery.Range->second);
			} else {
				begin = 0;
				end   = raw.size();
			}
		}
		const bool blank = !outline && core::writing_blank(raw);
		const auto scope = outline ? core::writing_scope::field : blank ? core::writing_scope::empty_field
			: begin == 0 && end == raw.size()                           ? core::writing_scope::field
																		: core::writing_scope::selection;
		if (blank) {
			begin = 0;
			end   = raw.size();
		}
		// Context: the least that lets the agent write for this target. Everything is a bounded snapshot.
		std::vector<application::writing_source> sources;
		std::size_t                              total = 0;
		const auto                               add   = [&](std::string aId, std::string_view aText, std::size_t aLimit, std::optional<core::tour_source_range> aRange = std::nullopt) {
			if (aText.empty() || sources.size() == application::maximum_assist_sources) {
				return;
			}
			if (auto source = bounded_source(std::move(aId), aText, aLimit, 8192 - total)) {
				source->Source = std::move(aRange);
				total += source->Text.size();
				sources.push_back(std::move(*source));
			}
		};
		std::vector<std::string> omitted{ "pr-description", "author-comments" };
		for (auto& extra : aExtra) {
			add(extra.Id, extra.Text, 4096, extra.Source);
		}
		if (aQuery.Hook != core::writing_hook::problem) {
			add("private-problem", draft.Problem, 1024);
		}
		if (aQuery.Hook != core::writing_hook::solution) {
			add("private-solution", draft.Solution, 1024);
		}
		if (scope == core::writing_scope::selection) {
			add("field-before", std::string_view{ raw }.substr(0, begin), 512);
			add("field-after", std::string_view{ raw }.substr(end), 512);
		}
		if (aQuery.Hook == core::writing_hook::comment && std::ranges::find(sources, "stop-source", &application::writing_source::Id) == sources.end()) {
			omitted.emplace_back("stop-source");
		}
		if (scope == core::writing_scope::empty_field && !outline && (sources.empty() || (aQuery.Hook == core::writing_hook::comment && std::ranges::find(sources, "stop-source", &application::writing_source::Id) == sources.end()))) {
			return refuse("insufficient_context", "Not enough approved context to draft this. Add the author's problem or solution first; a stop's source needs a session that can read it.", 2);
		}
		if (outline && std::ranges::none_of(sources, [](const auto& aSource) { return aSource.Source.has_value(); })) {
			return refuse("insufficient_context", "An outline needs at least one source destination. Add the code ranges to outline with --source PATH:LINES.", 2);
		}
		std::vector<std::uint8_t> keyed;
		put_digest(keyed, tour_hook_name(aQuery.Hook));
		put_digest(keyed, std::to_string(aSettings.Revision));
		put_digest(keyed, application::effective_writing_hook(aSettings, aQuery.Hook));
		for (const auto& source : sources) {
			put_digest(keyed, source.Id);
			put_digest(keyed, source.Text);
		}
		application::tour_assist_ticket ticket{ .Hook = aQuery.Hook, .Stop = outline || aQuery.Hook != core::writing_hook::comment ? core::tour_stop_id{ } : aQuery.Stop, .Scope = scope, .Begin = begin, .End = end, .DraftRevision = draft.Revision, .HookRevision = aSettings.Revision, .Sources = sources, .Session = aQuery.Session };
		ticket.RawDigest     = core::writing_digest(raw);
		ticket.ContextDigest = core::writing_digest(std::string_view{ reinterpret_cast<const char*>(keyed.data()), keyed.size() });
		auto issued          = aHost.assist_issue(std::move(ticket), aNow);
		if (!issued) {
			return error_for(issued.error());
		}
		core::writing_request request;
		request.Id                  = issued->Id;
		request.Target.Hook         = aQuery.Hook;
		request.Target.HookRevision = aSettings.Revision;
		request.Scope               = scope;
		request.Selection.Begin     = begin;
		request.Selection.End       = end;
		request.Raw                 = raw;
		const auto prompt           = application::build_writing_prompt(aSettings, request, sources);
		if (!prompt) {
			aHost.assist_cancel();
			return refuse("context_too_large", "This request exceeds the writing context limit. Select a smaller passage or shorten the hook instructions.", 2);
		}
		QJsonArray sourceList;
		for (const auto& source : sources) {
			QJsonObject item{ { "id", text(source.Id) }, { "text", text(source.Text) } };
			if (source.Source) {
				item.insert("path", text(source.Source->Path));
				item.insert("first_line", id_text(static_cast<std::uint64_t>(source.Source->First.Line) + 1U));
				item.insert("last_line", id_text(static_cast<std::uint64_t>(source.Source->Last.Line) + 1U));
			}
			sourceList.append(item);
		}
		QJsonArray omittedList;
		for (const auto& name : omitted) {
			omittedList.append(text(name));
		}
		const auto        selected = std::string_view{ raw }.substr(begin, end - begin);
		const QJsonObject packet{ { "schema", "bpr.tour.assist-context.v1" },
								  { "ticket", id_text(issued->Id) },
								  { "hook", QJsonObject{ { "name", text(tour_hook_name(aQuery.Hook)) }, { "version", text(application::writing_hook_version) }, { "revision", id_text(aSettings.Revision) }, { "instructions", text(application::effective_writing_hook(aSettings, aQuery.Hook)) } } },
								  { "contract", text(application::writing_application_contract()) },
								  { "scope", describe_range(*issued) },
								  { "field", outline ? QString{ } : aQuery.Hook == core::writing_hook::comment ? "stop:" + id_text(aQuery.Stop.value())
																											   : text(tour_hook_name(aQuery.Hook)) },
								  { "selection", QJsonObject{ { "begin", id_text(begin) }, { "end", id_text(end) }, { "text", text(selected) } } },
								  { "sources", sourceList },
								  { "omitted", omittedList },
								  { "draft_revision", id_text(draft.Revision) },
								  { "expires_unix", id_text(static_cast<std::uint64_t>(issued->Expires)) },
								  { "prompt", text(*prompt) },
								  { "return", "Return exactly the JSON object the prompt describes to `bpr tour assist complete --ticket TICKET --input -`. Never name a target, range, hook, approval or ready state in it." } };
		return tour_assist_issued{ std::move(*issued), compact(packet) };
	}
	auto tour_assist_outcome_json(const application::tour_service_result& aResult, const application::review_tour_host& aHost) -> std::string
	{
		QJsonArray notes;
		for (const auto& note : aResult.Notes) {
			QJsonArray references;
			for (const auto& reference : note.SourceRefs) {
				references.append(text(reference));
			}
			notes.append(QJsonObject{ { "kind", text(note.Kind) }, { "text", text(note.Text) }, { "source_refs", references } });
		}
		QJsonObject outcome{ { "applied", aResult.Status == application::tour_service_status::accepted && aResult.Proposal == 0 }, { "reason", text(aResult.Reason) }, { "detail", text(aResult.Detail) }, { "notes", notes } };
		if (aResult.Proposal != 0) {
			QJsonArray entries;
			if (const auto proposal = aHost.assist_proposal(aResult.Proposal)) {
				for (std::size_t index = 0; index < proposal->Entries.size(); ++index) {
					const auto& entry = proposal->Entries[index];
					QJsonObject item{ { "index", id_text(index) }, { "title", text(entry.Title) }, { "reason", text(entry.Suggestion.Reason) }, { "question", text(entry.Suggestion.Question) } };
					if (entry.Source) {
						item.insert("path", text(entry.Source->Path));
						item.insert("first_line", id_text(static_cast<std::uint64_t>(entry.Source->First.Line) + 1U));
						item.insert("last_line", id_text(static_cast<std::uint64_t>(entry.Source->Last.Line) + 1U));
					}
					entries.append(item);
				}
			}
			outcome.insert("proposal", QJsonObject{ { "id", id_text(aResult.Proposal) }, { "entries", entries } });
		}
		return compact(outcome);
	}
	auto tour_hooks_json(const core::writing_hook_settings& aSettings) -> std::string
	{
		QJsonArray hooks;
		for (std::size_t index = 0; index < core::writing_hook_count; ++index) {
			const auto hook = static_cast<core::writing_hook>(index);
			hooks.append(QJsonObject{ { "name", text(tour_hook_name(hook)) }, { "customized", !aSettings.Overrides[index].empty() }, { "instructions", text(application::effective_writing_hook(aSettings, hook)) } });
		}
		return compact(QJsonObject{ { "version", text(application::writing_hook_version) }, { "revision", id_text(aSettings.Revision) }, { "hooks", hooks } });
	}
	auto load_tour_hook_settings(const std::filesystem::path& aRoot) -> tour_cli_result<core::writing_hook_settings>
	{
		auto store = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRoot });
		if (!store) {
			return refuse("storage_unavailable", "The profile's hook settings are unavailable.", 7);
		}
		auto loaded = (*store)->load_preferences({ });
		if (!loaded) {
			return refuse("storage_unavailable", "The profile's hook settings are unavailable.", 7);
		}
		return loaded->WritingHooks;
	}
	auto store_tour_hook_override(const std::filesystem::path& aRoot, core::writing_hook aHook, std::string aInstructions) -> tour_cli_result<core::writing_hook_settings>
	{
		auto store = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRoot });
		if (!store) {
			return refuse("storage_unavailable", "The profile's hook settings are unavailable.", 7);
		}
		auto loaded = (*store)->load_preferences({ });
		if (!loaded) {
			return refuse("storage_unavailable", "The profile's hook settings are unavailable.", 7);
		}
		auto candidate                                                    = *loaded;
		candidate.WritingHooks.Overrides[static_cast<std::size_t>(aHook)] = std::move(aInstructions);
		++candidate.WritingHooks.Revision;
		if (!core::valid(candidate.WritingHooks)) {
			return refuse("invalid_input", "The instructions are too long (8192 bytes) or not valid UTF-8.", 2);
		}
		if (!(*store)->store_preferences(candidate, { })) {
			return refuse("storage_unavailable", "The hook change was not saved.", 7);
		}
		return candidate.WritingHooks;
	}
}
