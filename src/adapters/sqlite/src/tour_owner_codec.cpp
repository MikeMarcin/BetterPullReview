#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/core/sha256.hpp"
#include "review_tour_codec.hpp"
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
			// Observation, arbitrary restore and imported/provider state cannot enter via IPC.
			if (aRequest.Operation > application::tour_service_operation::redo || aRequest.Origin > application::tour_presentation_origin::command_client || c.Kind > core::tour_command_kind::visit || c.Kind == core::tour_command_kind::observe_head || c.Kind == core::tour_command_kind::restore || c.Kind == core::tour_command_kind::insert_outline || c.Restore || !c.Outline.empty()) {
				aCodec.Good = false;
			}
		}
		void request_codec(detail::tour_codec& aCodec, application::tour_owner_request& aRequest)
		{
			aCodec.enumeration(aRequest.Operation);
			aCodec.text(aRequest.Draft, 64);
			aCodec.text(aRequest.Edit.RequestId, 64);
			if (aRequest.Operation > application::tour_owner_operation::request_status || aRequest.Draft.size() != 64) {
				aCodec.Good = false;
			}
			if (aRequest.Operation == application::tour_owner_operation::mutate) {
				edit_codec(aCodec, aRequest.Edit);
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
			if (aResponse.Draft && core::validate(*aResponse.Draft) != core::tour_refusal::none) {
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
