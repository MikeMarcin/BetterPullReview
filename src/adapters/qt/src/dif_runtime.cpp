#include "bpr/qt/dif_runtime.hpp"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDebug>
#include <QEvent>
#include <QImageReader>
#include <QPainter>
#include <QResource>

#include "../assets/dif/generated/dif-sprite-catalog.hpp"

#include <algorithm>
#include <limits>

static void initialize_dif_resources()
{
#if !defined(BPR_EXTERNAL_QT_RESOURCES)
	Q_INIT_RESOURCE(bpr_dif);
#endif
	// [[BPR-0974]]. Otherwise the application registered its resource bundle,
	// strips included, before it read any resource.
}

namespace bpr::qt {
	namespace {
		constexpr std::uint64_t mib = 1024 * 1024;
		// Conservative allowance for opaque Qt image data and allocator bookkeeping.
		constexpr std::uint64_t overhead = 16 * 1024;
		auto                    valid_clip(const dif_clip& aClip) -> bool
		{
			return aClip.Frames > 0 && aClip.Frames <= 128 && aClip.Cell > 0 && aClip.Cell <= 1024
				&& aClip.Origin >= 0 && aClip.Origin <= aClip.Cell
				&& aClip.Gutter >= 0 && aClip.Gutter <= 32 && aClip.Return < 10 && aClip.Sha256.size() == 32
				&& ((aClip.Playback == dif_playback::still && aClip.Frames == 1 && aClip.FrameMs == 0)
					|| ((aClip.Playback == dif_playback::loop || aClip.Playback == dif_playback::once)
						&& aClip.FrameMs >= 84 && aClip.FrameMs <= 60'000));
		}
		// Reject metadata with independent decompression costs before the PNG reader.
		auto bounded_png(const QByteArray& aBytes) -> bool
		{
			if (aBytes.size() < 33 || aBytes.left(8) != QByteArray::fromHex("89504e470d0a1a0a")
				|| static_cast<unsigned char>(aBytes[24]) != 8 || static_cast<unsigned char>(aBytes[25]) != 6) {
				return false;
			}
			qsizetype offset = 8;
			while (offset <= aBytes.size() - 12) {
				std::uint32_t length = 0;
				for (qsizetype byte = 0; byte < 4; ++byte) {
					length = (length << 8) | static_cast<unsigned char>(aBytes[offset + byte]);
				}
				if (length > static_cast<std::uint64_t>(aBytes.size() - offset - 12)) {
					return false;
				}
				const auto kind = aBytes.sliced(offset + 4, 4);
				if (kind != "IHDR" && kind != "IDAT" && kind != "IEND" && kind != "sRGB"
					&& kind != "gAMA" && kind != "cHRM" && kind != "pHYs") {
					return false;
				}
				offset += static_cast<qsizetype>(length) + 12;
				if (kind == "IEND") {
					return length == 0 && offset == aBytes.size();
				}
			}
			return false;
		}

		auto transparent_gutters(const QImage& aImage, const dif_clip& aClip) -> bool
		{
			for (int x = 0; x < aImage.width(); ++x) {
				if (x >= aClip.Origin && x < aImage.width() - aClip.Origin
					&& (x - aClip.Origin) % (aClip.Cell + aClip.Gutter) < aClip.Cell) {
					continue;
				}
				for (int y = 0; y < aImage.height(); ++y) {
					if (qAlpha(aImage.pixel(x, y)) != 0) {
						return false;
					}
				}
			}
			return true;
		}

		auto same_command(
			const core::revisioned_review_draft_command& aLeft,
			const core::revisioned_review_draft_command& aRight) -> bool
		{
			return aLeft.Command == aRight.Command
				&& aLeft.ExpectedRevision == aRight.ExpectedRevision
				&& aLeft.ChangeReplyTarget == aRight.ChangeReplyTarget
				&& aLeft.ReplyToThread == aRight.ReplyToThread
				&& aLeft.Scope == aRight.Scope;
		}

		auto cue_priority(const dif_clip_id aClip) -> int
		{
			switch (aClip) {
			case dif_clip_id::blocked:        return 4;
			case dif_clip_id::attention:      return 3;
			case dif_clip_id::comment_ready:
			case dif_clip_id::acknowledge:    return 2;
			case dif_clip_id::consider:
			case dif_clip_id::compare_glance: return 1;
			default:                          return 0;
			}
		}

	}

	auto dif_cue_router::observe(dif_cue_context aContext) -> bool
	{
		if (Context == std::optional{ aContext }) {
			return false;
		}
		clear();
		Context = std::move(aContext);
		return true;
	}

	void dif_cue_router::clear()
	{
		Context.reset();
		Active.reset();
		LatestSession.reset();
		LastContinuation.reset();
		Workshop.reset();
		DraftOperation.reset();
		HighestInvocationGeneration     = 0U;
		HighestWorkshopActionGeneration = 0U;
		LastSemanticStart.reset();
		LastComparisonStart.reset();
		LastPriority      = 0;
		Started           = false;
		ReplyPresented    = false;
		FindingsPresented = false;
	}

	void dif_cue_router::set_visible(const bool aVisible)
	{
		Visible = aVisible;
	}

	auto dif_cue_router::in_context(const core::agent_session& aSession) const
		-> bool
	{
		return Context.has_value()
			&& aSession.Binding.Host == Context->Authority.Host
			&& aSession.Binding.Repository == Context->Authority.Repository
			&& aSession.Binding.PullRequest == Context->Authority.PullRequest;
	}

	auto dif_cue_router::current(
		const application::agent_invocation_projection_identity& aIdentity) const
		-> bool
	{
		return Context.has_value() && Active == std::optional{ aIdentity }
		&& aIdentity.ApplicationSession == Context->ApplicationSession
			&& aIdentity.Authority == Context->Authority
			&& in_context(aIdentity.Session)
			&& aIdentity.Session.Binding == Context->Binding;
	}

	auto dif_cue_router::propose(const dif_clip_id aClip, const std::int64_t aNow)
		-> std::optional<dif_clip_id>
	{
		if (!Visible || aNow < 0
			|| (LastSemanticStart.has_value() && aNow < *LastSemanticStart)) {
			return std::nullopt;
		}
		if (aClip == dif_clip_id::compare_glance
			&& LastComparisonStart.has_value()
			&& aNow - *LastComparisonStart < 5'000) {
			return std::nullopt;
		}
		const auto priority = cue_priority(aClip);
		if (LastSemanticStart.has_value() && aNow - *LastSemanticStart < 2'000) {
			const auto interrupts = (aClip == dif_clip_id::blocked
									 || aClip == dif_clip_id::attention)
				&& priority > LastPriority;
			if (!interrupts) {
				return std::nullopt;
			}
		}
		LastSemanticStart = aNow;
		LastPriority      = priority;
		if (aClip == dif_clip_id::compare_glance) {
			LastComparisonStart = aNow;
		}
		return aClip;
	}

	auto dif_cue_router::reveal_findings(const std::int64_t aNow)
		-> std::optional<dif_clip_id>
	{
		return Context.has_value()
			? propose(dif_clip_id::compare_glance, aNow)
			: std::nullopt;
	}

	auto dif_cue_router::admit(
		const application::agent_invocation_projection_identity& aIdentity)
		-> bool
	{
		if (!Context.has_value()
			|| aIdentity.ApplicationSession != Context->ApplicationSession
			|| aIdentity.Authority != Context->Authority
			|| !in_context(aIdentity.Session)
			|| aIdentity.Session.Binding != Context->Binding
			|| aIdentity.InvocationGeneration <= HighestInvocationGeneration) {
			return false;
		}
		HighestInvocationGeneration = aIdentity.InvocationGeneration;
		Active                      = aIdentity;
		if (LatestSession.has_value() && *LatestSession != aIdentity.Session) {
			Workshop.reset();
			DraftOperation.reset();
		}
		LatestSession     = aIdentity.Session;
		Started           = false;
		ReplyPresented    = false;
		FindingsPresented = false;
		return true;
	}

	auto dif_cue_router::start(
		const application::agent_invocation_projection_identity& aIdentity,
		const std::int64_t                                       aNow) -> std::optional<dif_clip_id>
	{
		if (!current(aIdentity) || Started) {
			return std::nullopt;
		}
		Started = true;
		return propose(dif_clip_id::consider, aNow);
	}

	void dif_cue_router::deliver(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_finding_delivery&               aDelivery,
		const bool                                               aReplyPresented,
		const bool                                               aFindingsPresented)
	{
		if (!current(aIdentity) || aDelivery.Session != aIdentity.Session.Id
			|| aDelivery.Binding != aIdentity.Session.Binding
			|| aDelivery.Findings.empty()) {
			return;
		}
		ReplyPresented |= aReplyPresented;
		FindingsPresented |= aFindingsPresented;
	}

	auto dif_cue_router::finish(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_run_outcome                     aOutcome,
		const std::int64_t                                       aNow) -> std::optional<dif_clip_id>
	{
		if (!current(aIdentity)) {
			return std::nullopt;
		}
		Active.reset();
		Started   = false;
		auto clip = std::optional<dif_clip_id>{ };
		if (aOutcome != application::agent_run_outcome::success) {
			clip = dif_clip_id::blocked;
		} else if (FindingsPresented) {
			clip = dif_clip_id::attention;
		} else if (ReplyPresented) {
			clip = dif_clip_id::comment_ready;
		}
		ReplyPresented    = false;
		FindingsPresented = false;
		return clip.has_value() ? propose(*clip, aNow) : std::nullopt;
	}

	auto dif_cue_router::present_workshop(
		const core::agent_session&  aSession,
		const std::uint64_t         aActionGeneration,
		const core::review_draft_id aDraft,
		const std::uint64_t         aRevision,
		const std::string_view      aProposed,
		const std::int64_t          aNow) -> std::optional<dif_clip_id>
	{
		const auto key = workshop_key{
			aSession.Id,
			aActionGeneration,
			aDraft,
			aRevision,
			std::string{ aProposed },
		};
		if (!Context.has_value() || !LatestSession.has_value()
			|| *LatestSession != aSession || !in_context(aSession)
			|| aActionGeneration == 0U
			|| aActionGeneration <= HighestWorkshopActionGeneration
			|| aDraft.value() == 0U || aRevision == 0U || aProposed.empty()) {
			return std::nullopt;
		}
		HighestWorkshopActionGeneration = aActionGeneration;
		Workshop                        = key;
		DraftOperation.reset();
		return propose(dif_clip_id::comment_ready, aNow);
	}

	void dif_cue_router::begin_draft(
		const core::revisioned_review_draft_command& aCommand)
	{
		DraftOperation.reset();
		if (Workshop.has_value() && aCommand.Command.Kind == core::review_draft_command_kind::edit
			&& !aCommand.ChangeReplyTarget
			&& aCommand.Command.Id == Workshop->Draft
			&& aCommand.ExpectedRevision == Workshop->Revision
			&& aCommand.Command.Body == Workshop->Proposed) {
			DraftOperation = aCommand;
		}
	}

	auto dif_cue_router::accept_draft(
		const core::revisioned_review_draft_command& aCommand,
		const core::revisioned_review_draft&         aAccepted,
		const std::int64_t                           aNow) -> std::optional<dif_clip_id>
	{
		if (!DraftOperation.has_value() || !same_command(*DraftOperation, aCommand)
			|| aAccepted.Scope != aCommand.Scope
			|| aAccepted.Draft.Id != aCommand.Command.Id
			|| aAccepted.Draft.Body != aCommand.Command.Body
			|| aAccepted.Revision < aCommand.ExpectedRevision
			|| aAccepted.Revision - aCommand.ExpectedRevision > 1U) {
			return std::nullopt;
		}
		DraftOperation.reset();
		return propose(dif_clip_id::acknowledge, aNow);
	}

	void dif_cue_router::cancel_draft(
		const core::revisioned_review_draft_command& aCommand)
	{
		if (DraftOperation.has_value() && same_command(*DraftOperation, aCommand)) {
			DraftOperation.reset();
		}
	}

	auto dif_cue_router::present_continuation(
		const core::agent_session&            aSession,
		const core::agent_continuation_offer& aOffer,
		const std::int64_t                    aNow) -> std::optional<dif_clip_id>
	{
		if (!aOffer.Required || !in_context(aSession)
			|| aOffer.ContinueAgainst != aSession.Binding
			|| LastContinuation == std::optional{ aSession }) {
			return std::nullopt;
		}
		LastContinuation = aSession;
		return propose(dif_clip_id::blocked, aNow);
	}

	auto dif_cue_router::active_invocation() const noexcept
		-> const std::optional<application::agent_invocation_projection_identity>&
	{
		return Active;
	}
	auto dif_cache::create(const descriptors& aClips, const strip_loader& aLoad) -> std::shared_ptr<const dif_cache>
	{
		if (!aLoad || !std::ranges::all_of(aClips, valid_clip) || aClips[0].Playback != dif_playback::still) {
			return { };
		}
		auto cache   = std::make_shared<dif_cache>();
		cache->Clips = aClips;
		for (auto& clip : cache->Clips) {
			clip.Sha256 = QByteArray{ clip.Sha256.constData(), clip.Sha256.size() };
		}
		std::uint64_t fullDecodedPixels = 0;
		for (std::size_t index = 0; index < aClips.size(); ++index) {
			const auto& clip = aClips[index];
			if (aClips[clip.Return].Playback == dif_playback::once) {
				return { };
			}
			auto bytes = aLoad(index);
			if (!bounded_png(bytes) || bytes.size() > 8 * static_cast<qsizetype>(mib)) {
				return { };
			}
			QBuffer buffer{ &bytes };
			buffer.open(QIODevice::ReadOnly);
			QImageReader reader{ &buffer, "PNG" };
			const QSize  size{ 2 * clip.Origin + clip.Frames * clip.Cell + (clip.Frames - 1) * clip.Gutter, clip.Cell };
			const auto   pixels = static_cast<std::uint64_t>(size.width()) * static_cast<std::uint64_t>(size.height()) * 4;
			// Keep admission and conservative peak accounting in full encoded
			// strip bytes, independently of the smaller retained representation.
			const auto peak = fullDecodedPixels + pixels * 2 + static_cast<std::uint64_t>(bytes.size()) + sizeof(dif_cache) + overhead;
			if (reader.size() != size || fullDecodedPixels + pixels > 24 * mib || peak >= 40 * mib) {
				return { };
			}
			auto decoded = reader.read();
			if (decoded.isNull() || decoded.size() != size || decoded.depth() > 32 || !transparent_gutters(decoded, clip)) {
				return { };
			}
			decoded.convertTo(QImage::Format_RGBA8888);
			if (QCryptographicHash::hash(QByteArrayView{ reinterpret_cast<const char*>(decoded.constBits()), decoded.sizeInBytes() }, QCryptographicHash::Sha256) != clip.Sha256) {
				return { };
			}
			const auto* straightPixels = decoded.constBits();
			// Validation is complete; let Qt consume the unique 32-bit strip
			// instead of keeping it alive beside a second full-size image.
			auto retained = std::move(decoded).convertToFormat(QImage::Format_ARGB32_Premultiplied);
			if (retained.isNull()) {
				return { };
			}
			if (retained.constBits() != straightPixels) {
				cache->PremultiplicationCopies += static_cast<std::uint64_t>(retained.sizeInBytes());
			}
			// An allocating Qt fallback can leave the unused source alive.
			decoded = QImage{ };
			if (clip.Origin != 0) {
				// Both end margins passed full pixel/digest validation above.
				// They are outside every frame; retain an owning interior copy.
				retained = retained.copy(clip.Origin, 0, retained.width() - 2 * clip.Origin, retained.height());
				if (retained.isNull()) {
					return { };
				}
			}
			cache->Images[index] = std::move(retained);
			fullDecodedPixels += pixels;
			cache->Pixels += static_cast<std::uint64_t>(cache->Images[index].sizeInBytes());
			cache->Peak = std::max(cache->Peak, peak);
		}
		if (cache->retained_bytes() > 32 * mib) {
			return { };
		}
		return cache;
	}
	auto dif_cache::clip(std::size_t aIndex) const -> const dif_clip& { return Clips.at(aIndex); }
	auto dif_cache::image(std::size_t aIndex) const -> const QImage& { return Images.at(aIndex); }
	auto dif_cache::source(std::size_t aClip, int aFrame) const -> QRect
	{
		if (aClip >= Clips.size() || aFrame < 0 || aFrame >= Clips[aClip].Frames) {
			return { };
		}
		const auto& clip = Clips[aClip];
		return { aFrame * (clip.Cell + clip.Gutter), 0, clip.Cell, clip.Cell };
	}
	auto dif_cache::pixel_bytes() const -> std::uint64_t { return Pixels; }
	auto dif_cache::retained_bytes() const -> std::uint64_t { return Pixels + sizeof(dif_cache) + overhead; }
	auto dif_cache::conversion_peak_bytes() const -> std::uint64_t { return Peak; }
	auto dif_cache::premultiplication_copy_bytes() const -> std::uint64_t { return PremultiplicationCopies; }

	namespace {
		constexpr std::array<std::string_view, 10> production_ids{ "rest", "calm_idle", "greet", "blink", "compare_glance", "consider", "comment_ready", "attention", "acknowledge", "blocked" };
		constexpr std::array<std::size_t, 10>      production_order{ 2, 0, 1, 3, 4, 5, 6, 7, 8, 9 };
		auto                                       load_production_strip(std::size_t aIndex) -> QByteArray
		{
			if (aIndex == 0) {
				initialize_dif_resources();
			}
			const auto      path = adapters::qt::dif::kClips.at(production_order.at(aIndex)).resource;
			const QResource resource{ QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())) };
			if (!resource.isValid() || resource.compressionAlgorithm() != QResource::NoCompression || resource.size() > 8 * static_cast<qint64>(mib)) {
				return { };
			}
			return QByteArray::fromRawData(reinterpret_cast<const char*>(resource.data()), resource.size());
		}
	}
	auto production_dif_catalog() -> dif_cache::descriptors
	{
		static_assert(adapters::qt::dif::kSchemaVersion == 2);
		static_assert(adapters::qt::dif::kManifestSha256 == "c9f3f38e303caea3bb2c1b28edf947a717c2c1e5f484945a1d6814d8aee7f40d");
		static_assert([] {
			for (std::size_t index = 0; index < production_order.size(); ++index) {
				const auto& clip = adapters::qt::dif::kClips[production_order[index]];
				if (clip.id != production_ids[index] || clip.cell_width != 256 || clip.cell_height != 256
					|| clip.content_origin_x != 256 || clip.strip_height != 256
					|| clip.strip_width != static_cast<int>(clip.frame_count + 2) * 256
					|| clip.decoded_bytes != static_cast<std::size_t>(clip.strip_width) * 256 * 4
					|| clip.return_target != (index == 0 ? "rest" : "calm_idle")) {
					return false;
				}
			}
			return true;
		}());
		dif_cache::descriptors result;
		for (std::size_t index = 0; index < result.size(); ++index) {
			const auto& source = adapters::qt::dif::kClips[production_order[index]];
			const auto  kind   = source.kind == adapters::qt::dif::clip_kind::static_frame ? dif_playback::still
				: source.kind == adapters::qt::dif::clip_kind::loop                        ? dif_playback::loop
																						   : dif_playback::once;
			result[index]      = { static_cast<int>(source.frame_count), source.cell_width, 0, source.frame_duration_ms, kind,
								   source.return_target == "rest" ? 0U : 1U,
								   QByteArray::fromHex(QByteArray{ source.rgba_sha256.data(), static_cast<qsizetype>(source.rgba_sha256.size()) }), source.content_origin_x };
		}
		return result;
	}
	dif_catalog_provider::dif_catalog_provider()
		: dif_catalog_provider{ load_production_strip }
	{ }
	dif_catalog_provider::dif_catalog_provider(dif_cache::strip_loader aLoader)
		: Loader{ std::move(aLoader) }
	{ }
	auto dif_catalog_provider::acquire() -> std::shared_ptr<const dif_cache>
	{
		if (Attempts == 0) {
			++Attempts;
			Cache = dif_cache::create(production_dif_catalog(), Loader);
			if (!Cache) {
				qWarning("dif-catalog-unavailable");
			}
		}
		return Cache;
	}
	auto dif_catalog_provider::load_attempts() const -> std::size_t { return Attempts; }
	auto dif_catalog_provider::decoded_count() const -> std::size_t { return Cache ? 10 : 0; }
	auto dif_catalog_provider::cache_identity() const -> const dif_cache* { return Cache.get(); }
	auto dif_catalog_provider::failure_diagnostic() const -> QByteArray { return Attempts && !Cache ? QByteArray{ "dif-catalog-unavailable" } : QByteArray{ }; }

	dif_sequencer::dif_sequencer(std::shared_ptr<const dif_cache> aCache)
		: Cache{ std::move(aCache) }
	{ }
	void dif_sequencer::reset()
	{
		Home      = false;
		CalmLoops = 0;
		Current   = 0;
		Started   = 0;
		Last      = 0;
	}
	auto dif_sequencer::start(std::size_t aClip, std::int64_t aNow) -> bool
	{
		if (!Cache || aClip >= 10 || aNow < 0 || aNow < Last) {
			reset();
			return false;
		}
		CalmLoops = 0;
		Current   = aClip;
		Started   = aNow;
		Last      = aNow;
		return true;
	}
	auto dif_sequencer::home(std::int64_t aNow) -> bool
	{
		if (!start(dif_index(Greeted ? dif_clip_id::calm_idle : dif_clip_id::greet), aNow)) {
			return false;
		}
		Home    = true;
		Greeted = true;
		return true;
	}
	auto dif_sequencer::sample(std::int64_t aNow) -> dif_frame
	{
		if (!Cache || aNow < 0 || aNow < Last) {
			reset();
			return { };
		}
		Last             = aNow;
		const auto& clip = Cache->clip(Current);
		if (clip.Playback == dif_playback::still) {
			return { Current, 0, { } };
		}
		const auto elapsed  = aNow - Started;
		const auto duration = static_cast<std::int64_t>(clip.Frames) * clip.FrameMs;
		if (Home && Current == dif_index(dif_clip_id::calm_idle)) {
			CalmLoops = static_cast<int>(std::min<std::int64_t>(4, elapsed / duration));
		}
		if (Home && Current == dif_index(dif_clip_id::calm_idle) && elapsed >= 4 * duration) {
			CalmLoops = 0;
			// A delayed wake never replays an expired blink or accumulates fidget debt.
			const auto& blink = Cache->clip(dif_index(dif_clip_id::blink));
			Current           = elapsed - 4 * duration < static_cast<std::int64_t>(blink.Frames) * blink.FrameMs
				? dif_index(dif_clip_id::blink)
				: dif_index(dif_clip_id::calm_idle);
			Started           = aNow;
			return { Current, 0, Cache->clip(Current).FrameMs };
		}
		if (clip.Playback == dif_playback::once && elapsed >= duration) {
			Current             = clip.Return;
			Started             = aNow;
			const auto& neutral = Cache->clip(Current);
			return { Current, 0, neutral.FrameMs ? std::optional<int>{ neutral.FrameMs } : std::nullopt };
		}
		return { Current, static_cast<int>((elapsed / clip.FrameMs) % clip.Frames), clip.FrameMs - static_cast<int>(elapsed % clip.FrameMs) };
	}

	dif_widget::dif_widget(std::shared_ptr<const dif_cache> aCache, QWidget* aParent)
		: QWidget{ aParent }
		, Cache{ std::move(aCache) }
		, Sequencer{ Cache }
	{
		setFocusPolicy(Qt::NoFocus);
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAccessibleName(QStringLiteral("Dif, BetterPullReview mascot"));
		Timer.setParent(this);
		Timer.setSingleShot(true);
		Timer.setTimerType(Qt::PreciseTimer);
		Clock.start();
		connect(&Timer, &QTimer::timeout, this, [this] { ++Statistics.TimerWakeups; tick(); });
		qApp->installEventFilter(this);
	}
	dif_widget::~dif_widget() { shutdown(); }
	void dif_widget::shutdown()
	{
		Stopped = true;
		Timer.stop();
		disconnect(&Timer, nullptr, this, nullptr);
		qApp->removeEventFilter(this);
	}
	auto dif_widget::eligible() const -> bool
	{
		return Cache && !Stopped && isVisible() && Eligibility.Shown && Eligibility.FullMotion
			&& Eligibility.Active && !Eligibility.Modal && !Eligibility.Suspended
			&& !window()->isMinimized() && !QApplication::activeModalWidget();
	}
	void dif_widget::cancel()
	{
		Timer.stop();
		Sequencer.reset();
		if (Frame.Clip != 0 || Frame.Frame != 0) {
			Frame = { };
			update();
		}
	}
	auto dif_widget::statistics() const -> dif_widget_statistics
	{
		auto result               = Statistics;
		result.Greeted            = Sequencer.greeted();
		result.CompletedCalmLoops = Sequencer.completed_calm_loops();
		return result;
	}
	void dif_widget::set_home(bool aHome)
	{
		if (Home != aHome) {
			Home = aHome;
			cancel();
		}
		resume_home();
	}
	void dif_widget::reset_contextual()
	{
		if (!Home) {
			cancel();
		}
	}
	void dif_widget::resume_home()
	{
		if (Home && eligible() && !Timer.isActive() && Sequencer.home(Clock.elapsed())) {
			++Statistics.PlaybackStarts;
			tick();
		}
	}
	void dif_widget::set_eligibility(dif_eligibility aEligibility)
	{
		const auto changedVisibility = Eligibility.Shown != aEligibility.Shown;
		Eligibility                  = aEligibility;
		if (!eligible()) {
			cancel();
		}
		resume_home();
		if (changedVisibility) {
			update();
		}
	}
	auto dif_widget::play(std::size_t aClip) -> bool
	{
		if (!eligible()) {
			cancel();
			return false;
		}
		if (!Sequencer.start(aClip, Clock.elapsed())) {
			cancel();
			return false;
		}
		++Statistics.PlaybackStarts;
		tick();
		return true;
	}
	void dif_widget::tick()
	{
		if (!eligible()) {
			cancel();
			return;
		}
		const auto next = Sequencer.sample(Clock.elapsed());
		if (next.Clip != Frame.Clip || next.Frame != Frame.Frame) {
			Frame = next;
			update();
		}
		Timer.stop();
		if (next.NextMs) {
			Timer.start(std::max(84, *next.NextMs));
		}
	}
	void dif_widget::paintEvent(QPaintEvent*)
	{
		if (!Cache || !Eligibility.Shown || Stopped) {
			return;
		}
		++Statistics.Paints;
		QPainter painter{ this };
		painter.drawImage(rect(), Cache->image(Frame.Clip), Cache->source(Frame.Clip, Frame.Frame));
	}
	auto dif_widget::event(QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::Hide || aEvent->type() == QEvent::WindowDeactivate
			|| aEvent->type() == QEvent::WindowBlocked) {
			cancel();
		}
		const auto handled = QWidget::event(aEvent);
		if (aEvent->type() == QEvent::Show) {
			resume_home();
		}
		return handled;
	}
	auto dif_widget::eventFilter(QObject* aObject, QEvent* aEvent) -> bool
	{
		if (aObject == window() && aEvent->type() == QEvent::WindowStateChange) {
			Eligibility.Active = window()->isActiveWindow();
			if (eligible()) {
				resume_home();
			} else {
				cancel();
			}
		} else if ((aObject == window() && (aEvent->type() == QEvent::WindowDeactivate || aEvent->type() == QEvent::WindowBlocked || aEvent->type() == QEvent::Hide))
				   || aEvent->type() == QEvent::ApplicationDeactivate) {
			Eligibility.Active = false;
			cancel();
		}
		if ((aObject == window() && aEvent->type() == QEvent::WindowActivate) || aEvent->type() == QEvent::ApplicationActivate) {
			Eligibility.Active = window()->isActiveWindow();
			resume_home();
		}
		return QWidget::eventFilter(aObject, aEvent);
	}
}
