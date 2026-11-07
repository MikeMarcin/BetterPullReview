#ifndef BPR_APP_GITHUB_APPLICATION_GITHUB_PRESENTATION_PROJECTIONS_HPP
#define BPR_APP_GITHUB_APPLICATION_GITHUB_PRESENTATION_PROJECTIONS_HPP
#include "bpr/application/github_projection.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/core/github_model.hpp"

#include <cstddef>
#include <functional>
#include <utility>

// Fan-out of GitHub and public-preview presentation to the recording
// sink and the window, and the public-preview capability.
namespace bpr::app {
	// The session projection, with composition listening behind the window.
	// [[BPR-0536]] needs one fact the window has no use for: that an
	// authenticated `/user` observation was accepted, which is the first
	// moment the vault authority a refresh entry is keyed by exists. The
	// window is told first and unchanged, so every existing reader sees
	// exactly the sequence it saw before; composition only reads.
	class composition_github_projection final
		: public application::github_projection_port
	{
	public:
		using accepted_sink = std::function<void(
			const application::github_presentation_update&)>;

		composition_github_projection(
			application::github_projection_port& aPresented,
			accepted_sink                        aAccepted);

		void publish(
			const application::github_presentation_update& aUpdate) override;

	private:
		application::github_projection_port* Presented = nullptr;
		accepted_sink                        Accepted;
	};

	// Records what the signed-out presentation was told. The window is the
	// reader [[BPR-0304]] built and tests at the adapter boundary; what
	// composition needs is the same published record, so the public
	// self-test can state the mode and the typed refusal a reviewer would
	// see rather than infer them from a picture.
	class public_preview_projection_sink final
		: public application::public_preview_projection_port
	{
	public:
		void publish(
			const application::public_preview_presentation& aPresentation)
			override;

		[[nodiscard]] auto last() const noexcept
			-> const application::public_preview_presentation&;

		[[nodiscard]] auto published() const noexcept -> std::size_t;

	private:
		application::public_preview_presentation Last;
		std::size_t                              Published = 0U;
	};

	// The same fan-out the device projection above uses, for the same
	// reason: the host publishes to exactly one port, the self-test
	// contracts read the recording sink, and the window is a second reader
	// that must not reorder, delay, or veto what the first one sees. The
	// sink is told first and unconditionally.
	class composition_public_preview_projection final
		: public application::public_preview_projection_port
	{
	public:
		using presented_sink = std::function<void(
			const application::public_preview_presentation&)>;

		composition_public_preview_projection(
			application::public_preview_projection_port& aRecorded,
			presented_sink                               aPresented);

		void publish(
			const application::public_preview_presentation& aPresentation)
			override;

	private:
		application::public_preview_projection_port* Recorded = nullptr;
		presented_sink                               Presented;
	};

	/// \brief Describes whether this build offers the signed-out preview.
	struct public_preview_capability
	{
		bool Available = false;
	};

	// BPR-1664 extends the existing anonymous composition to Windows and
	// Linux under the same credential-free GitHub.com-only authority.
	[[nodiscard]] constexpr auto shipped_public_preview_capability() noexcept
		-> public_preview_capability
	{
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
		return { .Available = true };
#else
		return { .Available = false };
#endif
	}

	[[nodiscard]] auto initial_github_state() -> core::github_state;
}
#endif
