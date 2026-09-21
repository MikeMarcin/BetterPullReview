#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_NAVIGATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_NAVIGATION_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/profile_window.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace bpr::core {
	/// \brief Opaque accepted body context; cannot be reused across windows or publications.
	struct markdown_context
	{
		bpr_window_context         Window;
		application_session_id     Session;
		github_identity_generation Identity;
		provider_snapshot_id       Snapshot;
		std::uint64_t              Revision                                          = 0;
		auto                       operator==(const markdown_context&) const -> bool = default;
		auto                       operator<=>(const markdown_context& aOther) const
		{
			return std::tie(Window.Profile, Window.Window, Window.Generation, Session, Identity, Snapshot, Revision)
				<=> std::tie(aOther.Window.Profile, aOther.Window.Window, aOther.Window.Generation, aOther.Session, aOther.Identity, aOther.Snapshot, aOther.Revision);
		}
	};

	/// \brief Untrusted destination from one explicit reading gesture. Empty Body identifies the description.
	struct markdown_link_intent
	{
		markdown_context Context;
		std::string      Body;
		std::string      Destination;
	};

	enum class markdown_link_result : std::uint8_t {
		opened,
		stale,
		unavailable,
		refused,
		failed,
	};

	enum class markdown_destination_kind : std::uint8_t {
		absolute_web,
		repository_path,
		pull_request_fragment,
	};

	/// \brief Admitted semantic destination; the host constructs provider-specific addresses.
	struct markdown_destination
	{
		markdown_destination_kind Kind;
		std::string               Target;
	};

	/// \brief Host-observed current body and exact repository revision, with no credential material.
	struct markdown_link_observation
	{
		std::optional<markdown_context> Context;
		bool                            BodyAvailable    = false;
		bool                            BrowserAvailable = false;
		std::string_view                Hostname;
		std::string_view                Owner;
		std::string_view                Repository;
		std::string_view                Revision;
		pull_request_number             Number;
	};

	/// \brief Admits bounded HTTP(S) destinations or repository-root paths bound to the accepted head.
	[[nodiscard]] auto admit_markdown_link(const markdown_link_intent&      aIntent,
										   const markdown_link_observation& aObservation)
		-> std::expected<markdown_destination, markdown_link_result>;
}
#endif
