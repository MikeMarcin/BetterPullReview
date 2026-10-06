#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/application/tour_assist.hpp"
#include "bpr/core/sha256.hpp"
#include "review_tour_codec.hpp"
#include <algorithm>
namespace bpr::adapters::sqlite {
	namespace {
		void edit_codec(detail::tour_codec& aCodec, application::tour_service_request& aRequest)
		{
			auto& c = aRequest.Command;
			aCodec.enumeration(aRequest.Operation);
			aCodec.enumeration(aRequest.Origin);
			aCodec.id(c.Draft);
			aCodec.owner(c.Owner);
			aCodec.number(c.ExpectedRevision);
			aCodec.enumeration(c.Kind);
			aCodec.id(c.Stop);
			aCodec.text(c.Text);
			aCodec.stop(c.Value);
			aCodec.number(c.Position);
			aCodec.number(c.SplitByte);
			aCodec.optional(c.Baseline, [&](auto& aValue) { aCodec.baseline(aValue); });
			aCodec.list(c.Selection, core::maximum_tour_stops, [&](auto& aValue) { aCodec.id(aValue); });
			aCodec.enumeration(c.CompletionOrigin);
			aCodec.number(c.Attention.EditorVisible);
			aCodec.optional(c.Attention.Source, [&](auto& aValue) { aCodec.source(aValue); });
			aCodec.number(c.Attention.SourceUnavailable);
			const bool assisting = aRequest.Operation == application::tour_service_operation::assist_complete || aRequest.Operation == application::tour_service_operation::outline_use;
			if (assisting) {
				auto& a = aRequest.Assist;
				aCodec.number(a.Id);
				aCodec.text(a.ResultJson, 32768);
				aCodec.list(a.Indices, 24, [&](auto& aIndex) { aCodec.number(aIndex); });
				aCodec.number(a.Position);
				// The caller supplies the answer only. The receiver reads the clock and the hook itself, and
				// the command is built from the stored ticket: nothing else may ride along.
				if (a.Now != 0 || a.CurrentHookRevision != 0 || c.Kind != core::tour_command_kind::problem || !c.Text.empty() || c.Stop.value() != 0 || !(c.Value == core::tour_stop{ }) || c.Position != 0 || c.SplitByte != 0 || c.Baseline || !c.Selection.empty() || c.ExpectedRevision != 0) {
					aCodec.Good = false;
				}
			}
			// Observation, arbitrary restore and imported/provider state cannot enter via IPC.
			if (aRequest.Operation == application::tour_service_operation::import_selected || aRequest.Operation > application::tour_service_operation::outline_use || aRequest.Origin > application::tour_presentation_origin::command_client || c.Kind > core::tour_command_kind::visit || c.Kind == core::tour_command_kind::observe_head || c.Kind == core::tour_command_kind::restore || c.Kind == core::tour_command_kind::insert_outline || c.Restore || !c.Outline.empty()) {
				aCodec.Good = false;
			}
		}
		auto flag(detail::tour_codec& aCodec, bool& aValue)
		{
			auto value = static_cast<std::uint8_t>(aValue);
			aCodec.byte(value);
			if (value > 1U) {
				aCodec.Good = false;
			}
			aValue = value != 0U;
		}
		/// [[BPR-1168]]. A cycle command: a registry name and that entry's options only. The caller's
		/// own choices (output form, profile, data root, schema) stay with the caller.
		void cycle_request_codec(detail::tour_codec& aCodec, application::cycle_owner_request& aRequest)
		{
			aCodec.enumeration(aRequest.Command);
			aCodec.text(aRequest.Operation, core::maximum_cycle_operation_id_bytes);
			aCodec.number(aRequest.Step);
			aCodec.list(aRequest.Positional, 1U, [&](auto& aValue) { aCodec.text(aValue, 8192U); });
			aCodec.list(aRequest.Options, 24U, [&](auto& aValue) {
				aCodec.text(aValue.first, 32U);
				aCodec.text(aValue.second, 8192U);
			});
			aCodec.text(aRequest.Directory, 8192U);
			aCodec.optional(aRequest.Text, [&](auto& aValue) { aCodec.text(aValue, application::maximum_cycle_owner_text_bytes); });
			aCodec.optional(aRequest.Document, [&](auto& aValue) { aCodec.text(aValue, application::maximum_cycle_owner_document_bytes); });
			flag(aCodec, aRequest.Check);
			flag(aCodec, aRequest.Interactive);
			if (!aCodec.Good) {
				return;
			}
			if (aRequest.Command > core::last_cycle_command || !core::valid_cycle_operation_id(aRequest.Operation)) {
				aCodec.Good = false;
				return;
			}
			const auto allowed = core::cycle_command_options(aRequest.Command);
			for (const auto& [option, value] : aRequest.Options) {
				const auto repeated = std::ranges::count(aRequest.Options, option, &std::pair<std::string, std::string>::first) != 1;
				if (repeated || std::ranges::find(allowed, std::string_view{ option }) == allowed.end() || (core::cycle_option_is_flag(option) && value != "true")) {
					aCodec.Good = false;
					return;
				}
			}
			for (const auto& text : aRequest.Positional) {
				if (!core::valid_writing_utf8(text)) {
					aCodec.Good = false;
				}
			}
			if (!core::valid_writing_utf8(aRequest.Directory) || (aRequest.Text && !core::valid_writing_utf8(*aRequest.Text))) {
				aCodec.Good = false;
			}
		}
		void cycle_response_codec(detail::tour_codec& aCodec, application::cycle_owner_response& aResponse)
		{
			constexpr std::size_t bulk = application::maximum_tour_channel_bytes;
			aCodec.text(aResponse.Operation, core::maximum_cycle_operation_id_bytes);
			aCodec.number(aResponse.Step);
			flag(aCodec, aResponse.Ok);
			aCodec.text(aResponse.Code, 128U);
			aCodec.text(aResponse.Message, 16384U);
			aCodec.text(aResponse.Next, 16384U);
			aCodec.text(aResponse.Detail, 65536U);
			aCodec.text(aResponse.Data, bulk);
			aCodec.text(aResponse.Human, bulk);
			flag(aCodec, aResponse.Complete);
			aCodec.text(aResponse.Output, bulk);
			if (aCodec.Good && aResponse.Ok == !aResponse.Code.empty()) {
				aCodec.Good = false;
			}
		}
		void request_codec(detail::tour_codec& aCodec, application::tour_owner_request& aRequest)
		{
			aCodec.enumeration(aRequest.Operation);
			aCodec.text(aRequest.Draft, 64);
			aCodec.text(aRequest.Edit.RequestId, 64);
			const bool profileWide = aRequest.Operation == application::tour_owner_operation::hooks_show || aRequest.Operation == application::tour_owner_operation::cycle;
			if (aRequest.Operation > application::tour_owner_operation::source || aRequest.Draft.size() != (profileWide ? 0U : 64U)) {
				aCodec.Good = false;
			}
			if (aRequest.Operation == application::tour_owner_operation::cycle) {
				if (!aRequest.Edit.RequestId.empty()) {
					aCodec.Good = false;
				}
				cycle_request_codec(aCodec, aRequest.Cycle);
				return;
			}
			if (aRequest.Operation == application::tour_owner_operation::source) {
				// [[BPR-1227]]. A read names its pinned destination and nothing else: no request ID,
				// since it is no mutation and leaves no record.
				auto& s = aRequest.Source;
				aCodec.oid(s.Commit);
				aCodec.optional(s.Blob, [&](auto& aBlob) { aCodec.oid(aBlob); });
				aCodec.text(s.Path, core::maximum_tour_path_bytes);
				aCodec.enumeration(s.Side);
				aCodec.number(s.FirstLine);
				aCodec.number(s.LastLine);
				if (!aRequest.Edit.RequestId.empty() || (aCodec.Good && !core::valid(s))) {
					aCodec.Good = false;
				}
				return;
			}
			if (aRequest.Operation == application::tour_owner_operation::mutate) {
				edit_codec(aCodec, aRequest.Edit);
			}
			if (aRequest.Operation == application::tour_owner_operation::assist_issue) {
				auto& q = aRequest.Assist;
				aCodec.enumeration(q.Hook);
				aCodec.id(q.Stop);
				aCodec.number(q.HasRange);
				aCodec.number(q.HasQuote);
				aCodec.number(q.Begin);
				aCodec.number(q.End);
				aCodec.text(q.Quote, 8192);
				aCodec.text(q.Session, application::maximum_assist_session_bytes);
				if (q.Hook > core::writing_hook::comment || (q.Hook == core::writing_hook::comment) != (q.Stop.value() != 0) || (q.HasRange && q.HasQuote) || (!q.HasRange && (q.Begin || q.End)) || (!q.HasQuote && !q.Quote.empty()) || !core::valid_writing_utf8(q.Quote) || !core::valid_writing_utf8(q.Session)) {
					aCodec.Good = false;
				}
			}
		}
		void response_codec(detail::tour_codec& aCodec, application::tour_owner_response& aResponse)
		{
			aCodec.optional(aResponse.Target, [&](auto& aTarget) { aCodec.owner(aTarget.Owner); aCodec.id(aTarget.Draft); aCodec.id(aTarget.HostProfile); aCodec.text(aTarget.Hostname,253); aCodec.text(aTarget.Repository.Owner,100); aCodec.text(aTarget.Repository.Name,100); aCodec.id(aTarget.Number); aCodec.oid(aTarget.Base); aCodec.oid(aTarget.Head); });
			aCodec.optional(aResponse.Draft, [&](auto& aValue) { aCodec.tour(aValue); });
			aCodec.text(aResponse.UndoLabel, 128);
			aCodec.text(aResponse.RedoLabel, 128);
			aCodec.enumeration(aResponse.Resume.Section);
			aCodec.id(aResponse.Resume.Stop);
			aCodec.optional(aResponse.Receipt, [&](auto& aReceipt) { aCodec.text(aReceipt.Id,64); for(auto& aValue:aReceipt.Digest){aCodec.byte(aValue);} aCodec.byte(aReceipt.Status); aCodec.enumeration(aReceipt.Refusal); aCodec.number(aReceipt.Revision); });
			aCodec.text(aResponse.Error, 128);
			aCodec.text(aResponse.Json, 65536);
			aCodec.optional(aResponse.Cycle, [&](auto& aValue) { cycle_response_codec(aCodec, aValue); });
			aCodec.optional(aResponse.Source, [&](auto& aValue) {
				aCodec.source(aValue.Source);
				aCodec.text(aValue.Text);
			});
			if (aResponse.Draft && core::validate(*aResponse.Draft) != core::tour_refusal::none) {
				aCodec.Good = false;
			}
			if (aCodec.Good && aResponse.Source && (!core::valid_writing_utf8(aResponse.Source->Text) || !core::valid(aResponse.Source->Source))) {
				aCodec.Good = false;
			}
		}
		template <typename T, typename F>
		auto encode(T aValue, F aFunction) -> application::tour_channel_bytes
		{
			detail::tour_codec codec;
			aFunction(codec, aValue);
			return codec.Good && codec.Bytes.size() <= application::maximum_tour_channel_bytes ? std::move(codec.Bytes) : application::tour_channel_bytes{ };
		}
		template <typename T, typename F>
		auto decode(std::span<const std::uint8_t> aBytes, F aFunction) -> std::optional<T>
		{
			if (aBytes.empty() || aBytes.size() > application::maximum_tour_channel_bytes) {
				return std::nullopt;
			}
			detail::tour_codec codec{ .Input = aBytes, .Reading = true };
			T                  aValue;
			aFunction(codec, aValue);
			return codec.Good && codec.Input.empty() ? std::optional{ std::move(aValue) } : std::nullopt;
		}
	}
	auto encode_tour_request(application::tour_owner_request aRequest) -> application::tour_channel_bytes { return encode(std::move(aRequest), request_codec); }
	auto decode_tour_request(std::span<const std::uint8_t> aBytes) -> std::optional<application::tour_owner_request>
	{
		auto result = decode<application::tour_owner_request>(aBytes, request_codec);
		if (result) {
			result->Edit.Digest = tour_request_digest(result->Edit);
		}
		return result;
	}
	auto encode_tour_response(application::tour_owner_response aResponse) -> application::tour_channel_bytes { return encode(std::move(aResponse), response_codec); }
	auto decode_tour_response(std::span<const std::uint8_t> aBytes) -> std::optional<application::tour_owner_response> { return decode<application::tour_owner_response>(aBytes, response_codec); }
	auto tour_request_digest(const application::tour_service_request& aRequest) -> std::array<std::uint8_t, 32> { return core::sha256(encode(aRequest, edit_codec)); }
}
