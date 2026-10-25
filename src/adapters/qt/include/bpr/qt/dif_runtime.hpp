#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIF_RUNTIME_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIF_RUNTIME_HPP

#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QRect>
#include <QTimer>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::qt {
	enum class dif_clip_id : std::size_t { rest,
										   calm_idle,
										   greet,
										   blink,
										   compare_glance,
										   consider,
										   comment_ready,
										   attention,
										   acknowledge,
										   blocked };
	constexpr auto dif_index(dif_clip_id aClip) -> std::size_t { return static_cast<std::size_t>(aClip); }
	enum class dif_playback { still,
							  loop,
							  once };

	/// \brief The accepted review identity whose projections may propose a cue.
	struct dif_cue_context final
	{
		core::application_session_id ApplicationSession;
		core::pull_request_authority Authority;
		core::agent_revision_binding Binding;

		auto operator<=>(const dif_cue_context&) const = default;
	};

	/// \brief Bounded Qt-local admission for presentation-only contextual cues.
	/// \details This stores one current invocation, one draft operation, and one
	/// continuation. It owns no domain result, effect,
	/// timer, queue, provider, or history. Callers first present authoritative
	/// text/results, then offer the matching typed fact here.
	class dif_cue_router final
	{
	public:
		auto observe(dif_cue_context aContext) -> bool;
		void clear();
		void set_visible(bool aVisible);
		auto admit(
			const application::agent_invocation_projection_identity& aIdentity)
			-> bool;

		[[nodiscard]] auto reveal_findings(std::int64_t aNow)
			-> std::optional<dif_clip_id>;
		[[nodiscard]] auto start(
			const application::agent_invocation_projection_identity& aIdentity,
			std::int64_t                                             aNow) -> std::optional<dif_clip_id>;
		void deliver(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_finding_delivery&               aDelivery,
			bool                                                     aReplyPresented,
			bool                                                     aFindingsPresented);
		[[nodiscard]] auto finish(
			const application::agent_invocation_projection_identity& aIdentity,
			application::agent_run_outcome                           aOutcome,
			std::int64_t                                             aNow) -> std::optional<dif_clip_id>;

		[[nodiscard]] auto present_workshop(
			const core::agent_session& aSession,
			std::uint64_t              aActionGeneration,
			core::review_draft_id      aDraft,
			std::uint64_t              aRevision,
			std::string_view           aProposed,
			std::int64_t               aNow) -> std::optional<dif_clip_id>;
		void               begin_draft(const core::revisioned_review_draft_command& aCommand);
		[[nodiscard]] auto accept_draft(
			const core::revisioned_review_draft_command& aCommand,
			const core::revisioned_review_draft&         aAccepted,
			std::int64_t                                 aNow) -> std::optional<dif_clip_id>;
		void               cancel_draft(const core::revisioned_review_draft_command& aCommand);
		[[nodiscard]] auto present_continuation(
			const core::agent_session&            aSession,
			const core::agent_continuation_offer& aOffer,
			std::int64_t                          aNow) -> std::optional<dif_clip_id>;

		[[nodiscard]] auto active_invocation() const noexcept
			-> const std::optional<application::agent_invocation_projection_identity>&;

	private:
		struct workshop_key final
		{
			core::agent_session_id Session;
			std::uint64_t          ActionGeneration = 0U;
			core::review_draft_id  Draft;
			std::uint64_t          Revision = 0U;
			std::string            Proposed;
			auto                   operator<=>(const workshop_key&) const = default;
		};
		[[nodiscard]] auto propose(dif_clip_id aClip, std::int64_t aNow)
			-> std::optional<dif_clip_id>;
		[[nodiscard]] auto current(
			const application::agent_invocation_projection_identity& aIdentity) const
			-> bool;
		[[nodiscard]] auto in_context(const core::agent_session& aSession) const
			-> bool;

		std::optional<dif_cue_context>                                   Context;
		std::optional<application::agent_invocation_projection_identity> Active;
		std::optional<core::agent_session>                               LatestSession;
		std::optional<core::agent_session>                               LastContinuation;
		std::optional<workshop_key>                                      Workshop;
		std::optional<core::revisioned_review_draft_command>             DraftOperation;
		std::uint64_t                                                    HighestInvocationGeneration     = 0U;
		std::uint64_t                                                    HighestWorkshopActionGeneration = 0U;
		std::optional<std::int64_t>                                      LastSemanticStart;
		std::optional<std::int64_t>                                      LastComparisonStart;
		int                                                              LastPriority      = 0;
		bool                                                             Visible           = false;
		bool                                                             Started           = false;
		bool                                                             ReplyPresented    = false;
		bool                                                             FindingsPresented = false;
	};

	/// \brief Validated strip geometry and timing, independent of production asset names.
	struct dif_clip final
	{
		int          Frames{ 1 };
		int          Cell{ 256 };
		int          Gutter{ 0 };
		int          FrameMs{ 0 };
		dif_playback Playback{ dif_playback::still };
		std::size_t  Return{ 0 };
		QByteArray   Sha256; // SHA-256 of straight RGBA8 rows, excluding no pixels.
		int          Origin{ 0 }; // Equal transparent leading/trailing margins.
	};

	/// \brief Immutable, explicitly shared catalog; creation decodes one injected PNG at a time.
	/// \details Index zero is validated static rest. Sources are RGBA8 PNG without compressed
	/// metadata. The composition-owned loader supplies bounded bytes and does not throw.
	/// Retained/peak accounting includes a conservative opaque Qt/allocator allowance;
	/// backend-owned paint caches require separate native measurement.
	class dif_cache final
	{
	public:
		using descriptors  = std::array<dif_clip, 10>;
		using strip_loader = std::function<QByteArray(std::size_t)>;
		/// \brief Refuses malformed descriptors, digest/geometry mismatch, or memory excess.
		static auto create(const descriptors& aClips, const strip_loader& aLoad) -> std::shared_ptr<const dif_cache>;
		auto        clip(std::size_t aIndex) const -> const dif_clip&;
		auto        image(std::size_t aIndex) const -> const QImage&;
		auto        source(std::size_t aClip, int aFrame) const -> QRect;
		auto        pixel_bytes() const -> std::uint64_t;
		auto        retained_bytes() const -> std::uint64_t;
		auto        conversion_peak_bytes() const -> std::uint64_t;
		/// \brief Full-strip destination bytes allocated instead of reusing validated RGBA storage.
		auto premultiplication_copy_bytes() const -> std::uint64_t;

	private:
		descriptors            Clips;
		std::array<QImage, 10> Images;
		std::uint64_t          Pixels{ 0 };
		std::uint64_t          Peak{ 0 };
		std::uint64_t          PremultiplicationCopies{ 0 };
	};

	/// \brief Accepted generated descriptors in typed runtime order (rest is zero).
	auto production_dif_catalog() -> dif_cache::descriptors;

	/// \brief Composition-owned lazy catalog, shared across profiles and windows on the Qt lane.
	/// \details Failure is latched; no retries or duplicate diagnostics until process teardown.
	class dif_catalog_provider final
	{
	public:
		dif_catalog_provider();
		explicit dif_catalog_provider(dif_cache::strip_loader aLoader);
		auto acquire() -> std::shared_ptr<const dif_cache>;
		auto load_attempts() const -> std::size_t;
		auto decoded_count() const -> std::size_t;
		auto cache_identity() const -> const dif_cache*;
		auto failure_diagnostic() const -> QByteArray;

	private:
		dif_cache::strip_loader          Loader;
		std::shared_ptr<const dif_cache> Cache;
		std::size_t                      Attempts{ 0 };
	};

	/// \brief One sampled frame and a relative deadline; absence means no wakeup.
	struct dif_frame final
	{
		std::size_t        Clip{ 0 };
		int                Frame{ 0 };
		std::optional<int> NextMs;
	};

	/// \brief Pure monotonic elapsed-time playback; invalid time cancels to static rest.
	class dif_sequencer final
	{
	public:
		explicit dif_sequencer(std::shared_ptr<const dif_cache> aCache);
		auto start(std::size_t aClip, std::int64_t aNow) -> bool;
		auto sample(std::int64_t aNow) -> dif_frame;
		void reset();
		auto greeted() const -> bool { return Greeted; }
		auto completed_calm_loops() const -> int { return CalmLoops; }
		/// \brief Starts/resumes Home without replaying its lifetime greeting.
		auto home(std::int64_t aNow) -> bool;

	private:
		std::shared_ptr<const dif_cache> Cache;
		std::size_t                      Current{ 0 };
		std::int64_t                     Started{ 0 };
		std::int64_t                     Last{ 0 };
		bool                             Home{ false };
		bool                             Greeted{ false };
		int                              CalmLoops{ 0 };
	};

	/// \brief Presentation eligibility supplied by the owning window, in addition to Qt exposure.
	struct dif_eligibility final
	{
		bool Shown{ true };
		bool FullMotion{ false };
		bool Active{ false };
		bool Modal{ false };
		bool Suspended{ false };
	};

	/// \brief Bounded presentation counters for local performance evidence.
	struct dif_widget_statistics final
	{
		std::uint64_t TimerWakeups{ 0 };
		std::uint64_t Paints{ 0 };
		std::uint64_t PlaybackStarts{ 0 };
		int           CompletedCalmLoops{ 0 };
		bool          Greeted{ false };
	};

	/// \brief Decorative retained sprite with one single-shot timer and no per-frame image work.
	class dif_widget final : public QWidget
	{
	public:
		explicit dif_widget(std::shared_ptr<const dif_cache> aCache, QWidget* aParent = nullptr);
		~dif_widget() override;
		void set_eligibility(dif_eligibility aEligibility);
		/// \brief Select Home automatic playback; false cancels to static contextual baseline.
		void set_home(bool aHome);
		/// \brief Drops a contextual one-shot immediately after its exact context changes.
		void reset_contextual();
		auto play(dif_clip_id aClip) -> bool { return play(dif_index(aClip)); }
		auto current_frame() const -> dif_frame { return Frame; }
		auto cache_identity() const -> const dif_cache* { return Cache.get(); }
		auto statistics() const -> dif_widget_statistics;
		auto play(std::size_t aClip) -> bool;
		void shutdown();

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override;

	private:
		auto                             eligible() const -> bool;
		void                             cancel();
		void                             tick();
		std::shared_ptr<const dif_cache> Cache;
		dif_sequencer                    Sequencer;
		QTimer                           Timer;
		QElapsedTimer                    Clock;
		dif_frame                        Frame;
		dif_eligibility                  Eligibility;
		bool                             Stopped{ false };
		bool                             Home{ false };
		dif_widget_statistics            Statistics;
		void                             resume_home();
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIF_RUNTIME_HPP
