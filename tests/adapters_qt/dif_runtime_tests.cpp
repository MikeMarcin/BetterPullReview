#include "bpr/qt/dif_runtime.hpp"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPointer>
#include <QResource>
#include <QSysInfo>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <thread>

#ifdef __APPLE__
	#include <mach/mach.h>
	#include <malloc/malloc.h>
#endif

namespace {
	using namespace bpr::qt;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	auto rgba_digest(QImage aImage) -> QByteArray
	{
		aImage.convertTo(QImage::Format_RGBA8888);
		return QCryptographicHash::hash(QByteArrayView{ reinterpret_cast<const char*>(aImage.constBits()), aImage.sizeInBytes() }, QCryptographicHash::Sha256);
	}
	auto png_bytes(const QImage& aImage) -> QByteArray
	{
		QByteArray bytes;
		QBuffer    buffer{ &bytes };
		buffer.open(QIODevice::WriteOnly);
		expect(aImage.save(&buffer, "PNG"), "padded fixture PNG encodes");
		return bytes;
	}
	auto render_frame(const QImage& aImage, const QRect& aSource, int aExtent) -> QImage
	{
		QImage result{ aExtent, aExtent, QImage::Format_ARGB32_Premultiplied };
		result.fill(QColor{ 37, 53, 71 });
		{
			QPainter painter{ &result };
			// Match dif_widget::paintEvent's ordinary drawImage settings.
			painter.drawImage(result.rect(), aImage, aSource);
		}
		return result;
	}
	struct fixture final
	{
		dif_cache::descriptors     Clips;
		std::array<QByteArray, 10> Bytes;
		int                        Loads{ 0 };
		fixture()
		{
			for (std::size_t index = 0; index < Clips.size(); ++index) {
				auto& clip    = Clips[index];
				clip.Frames   = index == 0 ? 1 : static_cast<int>(index + 1);
				clip.Cell     = 8;
				clip.Gutter   = 2;
				clip.FrameMs  = index == 0 ? 0 : 84 + static_cast<int>(index) * 10;
				clip.Playback = index == 0 ? dif_playback::still : index == 1 ? dif_playback::loop
																			  : dif_playback::once;
				clip.Return   = index % 2;
				QImage image{ clip.Frames * clip.Cell + (clip.Frames - 1) * clip.Gutter, clip.Cell, QImage::Format_ARGB32 };
				image.fill(Qt::transparent);
				for (int frame = 0; frame < clip.Frames; ++frame) {
					for (int x = 0; x < clip.Cell; ++x) {
						for (int y = 0; y < clip.Cell; ++y) {
							image.setPixelColor(frame * (clip.Cell + clip.Gutter) + x, y, QColor{ frame * 20, static_cast<int>(index) * 20, 80, 128 });
						}
					}
				}
				QBuffer buffer{ &Bytes[index] };
				buffer.open(QIODevice::WriteOnly);
				expect(image.save(&buffer, "PNG"), "fixture PNG encodes");
				clip.Sha256 = rgba_digest(image);
			}
		}
		auto load() -> std::shared_ptr<const dif_cache>
		{
			return dif_cache::create(Clips, [this](std::size_t aIndex) { ++Loads; return Bytes[aIndex]; });
		}
	};
	class paint_counter final : public QObject
	{
	public:
		int  Paints{ 0 };
		bool eventFilter(QObject*, QEvent* aEvent) override
		{
			if (aEvent->type() == QEvent::Paint) {
				++Paints;
			}
			return false;
		}
	};

	void wait_events(int aMs)
	{
		QEventLoop loop;
		QTimer::singleShot(aMs, &loop, &QEventLoop::quit);
		loop.exec();
	}

	struct process_memory final
	{
		std::uint64_t MallocInUse{ 0U };
		std::uint64_t Resident{ 0U };
		std::uint64_t PhysicalFootprint{ 0U };
	};

	[[nodiscard]] auto sample_process_memory() -> process_memory
	{
#ifdef __APPLE__
		malloc_statistics_t mallocStatistics{ };
		malloc_zone_statistics(malloc_default_zone(), &mallocStatistics);
		task_vm_info_data_t taskInfo{ };
		auto                count = mach_msg_type_number_t{ TASK_VM_INFO_COUNT };
		expect(task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&taskInfo), &count) == KERN_SUCCESS, "Mach task memory sample");
		return {
			static_cast<std::uint64_t>(mallocStatistics.size_in_use),
			static_cast<std::uint64_t>(taskInfo.resident_size),
			static_cast<std::uint64_t>(taskInfo.phys_footprint),
		};
#else
		return { };
#endif
	}

	void retain_max(std::atomic<std::uint64_t>& aMaximum, const std::uint64_t aValue)
	{
		auto observed = aMaximum.load(std::memory_order_relaxed);
		while (observed < aValue && !aMaximum.compare_exchange_weak(observed, aValue, std::memory_order_relaxed)) {
		}
	}

	class process_memory_peak final
	{
	public:
		explicit process_memory_peak(const process_memory& aInitial)
			: Malloc{ aInitial.MallocInUse }
			, Resident{ aInitial.Resident }
			, PhysicalFootprint{ aInitial.PhysicalFootprint }
			, Sampler{ [this] {
				while (!Stop.load(std::memory_order_relaxed)) {
					const auto sample = sample_process_memory();
					retain_max(Malloc, sample.MallocInUse);
					retain_max(Resident, sample.Resident);
					retain_max(PhysicalFootprint, sample.PhysicalFootprint);
					std::this_thread::yield();
				}
			} }
		{
		}
		~process_memory_peak() { stop(); }
		void stop()
		{
			if (!Stop.exchange(true, std::memory_order_relaxed) && Sampler.joinable()) {
				Sampler.join();
			}
		}
		[[nodiscard]] auto malloc_in_use() const -> std::uint64_t { return Malloc.load(std::memory_order_relaxed); }
		[[nodiscard]] auto resident() const -> std::uint64_t { return Resident.load(std::memory_order_relaxed); }
		[[nodiscard]] auto physical_footprint() const -> std::uint64_t { return PhysicalFootprint.load(std::memory_order_relaxed); }

	private:
		std::atomic<bool>          Stop{ false };
		std::atomic<std::uint64_t> Malloc;
		std::atomic<std::uint64_t> Resident;
		std::atomic<std::uint64_t> PhysicalFootprint;
		std::thread                Sampler;
	};

#ifdef __APPLE__
	// Report helpers for the macOS-only native memory probe; defined exactly where it is.
	[[nodiscard]] auto positive_delta(const std::uint64_t aAfter, const std::uint64_t aBefore) -> std::uint64_t
	{
		return aAfter >= aBefore ? aAfter - aBefore : 0U;
	}

	[[nodiscard]] auto memory_json(const process_memory& aSample) -> QJsonObject
	{
		return {
			{ "malloc_default_zone_in_use_bytes", static_cast<qint64>(aSample.MallocInUse) },
			{ "resident_bytes", static_cast<qint64>(aSample.Resident) },
			{ "physical_footprint_bytes", static_cast<qint64>(aSample.PhysicalFootprint) },
		};
	}

	void write_native_memory_report(const QJsonObject& aReport)
	{
		const auto path = qEnvironmentVariable("BPR_DIF_NATIVE_MEMORY_REPORT_PATH");
		expect(!path.isEmpty(), "native memory report path provided");
		QFile report{ path };
		expect(QDir{ }.mkpath(QFileInfo{ report }.absolutePath()) && report.open(QIODevice::WriteOnly | QIODevice::Truncate), "native memory report opens");
		const auto bytes = QJsonDocument{ aReport }.toJson(QJsonDocument::Indented);
		expect(report.write(bytes) == bytes.size() && report.flush(), "native memory report written");
	}
#endif

	[[nodiscard]] auto cue_authority(const std::uint64_t aPullRequest = 3U)
		-> bpr::core::pull_request_authority
	{
		return {
			.Host        = bpr::core::git_host_id{ 1U },
			.Repository  = bpr::core::repository_id{ 2U },
			.PullRequest = bpr::core::pull_request_id{ aPullRequest },
		};
	}

	[[nodiscard]] auto cue_identity(
		const std::uint64_t aGeneration,
		const std::uint64_t aPullRequest = 3U,
		const std::uint64_t aRevision    = 1U)
		-> bpr::application::agent_invocation_projection_identity
	{
		const auto authority = cue_authority(aPullRequest);
		return {
			.ApplicationSession = bpr::core::application_session_id{ 4U },
			.Authority          = authority,
			.Session            = {
				.Id      = bpr::core::agent_session_id{ aGeneration + 10U },
				.Profile = bpr::core::agent_profile_id{ 5U },
				.Binding = {
					.Host        = authority.Host,
					.Repository  = authority.Repository,
					.PullRequest = authority.PullRequest,
					.LeftEpoch   = bpr::core::epoch_ordinal{
						static_cast<std::int64_t>(aRevision) },
					.RightEpoch = bpr::core::epoch_ordinal{ static_cast<std::int64_t>(aRevision) },
				},
			},
			.InvocationGeneration = aGeneration,
		};
	}

	[[nodiscard]] auto cue_delivery(
		const bpr::application::agent_invocation_projection_identity& aIdentity)
		-> bpr::application::agent_finding_delivery
	{
		auto result = bpr::application::agent_finding_delivery{
			.Session = aIdentity.Session.Id,
			.Binding = aIdentity.Session.Binding,
		};
		result.Findings.push_back(bpr::application::agent_delivered_finding{ });
		return result;
	}

	[[nodiscard]] auto admit_and_start(
		dif_cue_router&
																	  aRouter,
		const bpr::application::agent_invocation_projection_identity& aIdentity,
		const std::int64_t                                            aNow) -> std::optional<dif_clip_id>
	{
		static_cast<void>(aRouter.admit(aIdentity));
		return aRouter.start(aIdentity, aNow);
	}

	void test_contextual_cue_router_uses_accepted_identity_and_bounded_latches()
	{
		auto router = dif_cue_router{ };
		router.observe({
			.ApplicationSession = bpr::core::application_session_id{ 4U },
			.Authority          = cue_authority(),
			.Binding            = cue_identity(0U).Session.Binding,
		});
		router.set_visible(true);
		expect(router.reveal_findings(0) == dif_clip_id::compare_glance
				   && !router.reveal_findings(4'999)
				   && router.reveal_findings(5'000) == dif_clip_id::compare_glance,
			   "an explicit findings reveal emits one glance per five seconds");

		const auto first = cue_identity(1U);
		expect(admit_and_start(router, first, 7'000) == dif_clip_id::consider
				   && !admit_and_start(router, first, 7'001),
			   "only a fresh admitted invocation start emits consider");
		router.deliver(first, cue_delivery(first), true, false);
		expect(!router.finish(first, bpr::application::agent_run_outcome::success, 7'100),
			   "a ready cue inside two seconds is dropped after its reply is presented");
		expect(!router.finish(first, bpr::application::agent_run_outcome::success, 9'100)
				   && !admit_and_start(router, first, 9'101),
			   "terminal replay and a replayed start stay spent");

		const auto findings = cue_identity(2U);
		expect(admit_and_start(router, findings, 10'000) == dif_clip_id::consider,
			   "a newer invocation replaces the terminal latch");
		router.deliver(findings, cue_delivery(findings), true, true);
		expect(router.finish(findings, bpr::application::agent_run_outcome::success, 10'100)
				   == dif_clip_id::attention,
			   "a presented triage batch coalesces over ready");

		const auto failed = cue_identity(3U);
		expect(admit_and_start(router, failed, 12'100) == dif_clip_id::consider
				   && router.finish(failed, bpr::application::agent_run_outcome::nonzero_exit, 12'200)
					   == dif_clip_id::blocked,
			   "a matching presented failure preempts a lower-priority cue");
		const auto empty = cue_identity(4U);
		expect(admit_and_start(router, empty, 14'200) == dif_clip_id::consider
				   && !router.finish(empty, bpr::application::agent_run_outcome::success, 14'300),
			   "process success without accepted presented content emits no success cue");

		const auto hidden = cue_identity(5U);
		router.set_visible(false);
		expect(!admit_and_start(router, hidden, 16'300), "hidden invocation starts no animation");
		router.deliver(hidden, cue_delivery(hidden), true, true);
		expect(!router.finish(hidden, bpr::application::agent_run_outcome::success, 16'400),
			   "hidden terminal content is consumed without animation");
		router.set_visible(true);
		expect(!router.finish(hidden, bpr::application::agent_run_outcome::success, 18'400),
			   "showing the surface never replays a hidden result");

		const auto superseded = cue_identity(6U);
		const auto current    = cue_identity(7U);
		expect(admit_and_start(router, superseded, 18'500) == dif_clip_id::consider,
			   "a fresh visible invocation starts");
		expect(!admit_and_start(router, current, 18'600),
			   "the replacement is admitted even when its same-priority cue is cooled down");
		expect(!router.finish(superseded, bpr::application::agent_run_outcome::nonzero_exit, 18'700)
				   && router.finish(current, bpr::application::agent_run_outcome::nonzero_exit, 18'700)
					   == dif_clip_id::blocked,
			   "a superseded invocation cannot publish a stale terminal");

		router.observe({
			.ApplicationSession = bpr::core::application_session_id{ 4U },
			.Authority          = cue_authority(8U),
			.Binding            = cue_identity(0U, 8U).Session.Binding,
		});
		expect(!admit_and_start(router, cue_identity(8U), 20'700),
			   "a prior pull request cannot enter the replacement context");

		const auto workshop = cue_identity(9U, 8U);
		expect(router.admit(workshop)
				   && router.present_workshop(workshop.Session, 1U, bpr::core::review_draft_id{ 21U }, 4U, "accepted revision", 0)
					   == dif_clip_id::comment_ready
				   && !router.present_workshop(workshop.Session, 1U, bpr::core::review_draft_id{ 21U }, 4U, "accepted revision", 2'000),
			   "one accepted workshop proposal emits ready and duplicate presentation does not");
		const auto command = bpr::core::revisioned_review_draft_command{
			.Command = {
				.Id   = bpr::core::review_draft_id{ 21U },
				.Kind = bpr::core::review_draft_command_kind::edit,
				.Body = "accepted revision",
			},
			.ExpectedRevision = 4U,
			.Scope            = bpr::core::review_draft_scope_id{ 22U },
		};
		auto accepted = bpr::core::revisioned_review_draft{
			.Draft = {
				.Id   = command.Command.Id,
				.Body = command.Command.Body,
			},
			.Revision = 5U,
			.Scope    = command.Scope,
		};
		router.begin_draft(command);
		auto staleAccepted     = accepted;
		staleAccepted.Revision = 7U;
		expect(!router.accept_draft(command, staleAccepted, 2'100),
			   "an uncorrelated accepted revision cannot acknowledge the draft operation");
		expect(router.accept_draft(command, accepted, 2'100) == dif_clip_id::acknowledge
				   && !router.accept_draft(command, accepted, 4'100),
			   "only the exact accepted draft operation acknowledges once");

		auto continuation = cue_identity(1U, 8U).Session;
		auto offer        = bpr::core::agent_continuation_offer{
			.Required        = true,
			.ContinueAgainst = continuation.Binding,
		};
		expect(router.present_continuation(continuation, offer, 4'200)
					   == dif_clip_id::blocked
				   && !router.present_continuation(continuation, offer, 6'200),
			   "a real continuation offer blocks once under its bound session identity");
	}

	void test_contextual_cue_router_rejects_stale_and_conflicting_facts()
	{
		const auto first   = cue_identity(1U);
		const auto context = dif_cue_context{
			.ApplicationSession = first.ApplicationSession,
			.Authority          = first.Authority,
			.Binding            = first.Session.Binding,
		};
		const auto configured = [&context] {
			auto router = dif_cue_router{ };
			router.observe(context);
			router.set_visible(true);
			return router;
		};

		{
			auto router                    = configured();
			auto wrongWindow               = first;
			wrongWindow.ApplicationSession = bpr::core::application_session_id{ 99U };
			auto wrongReview               = cue_identity(1U, 4U);
			auto wrongRevision             = cue_identity(1U, 3U, 2U);
			expect(!admit_and_start(router, wrongWindow, 0) && !admit_and_start(router, wrongReview, 0)
					   && !admit_and_start(router, wrongRevision, 0)
					   && !router.active_invocation().has_value(),
				   "window, pull-request, and revision mismatches admit no cue or latch");
		}

		{
			auto router = configured();
			expect(router.admit(first)
					   && router.finish(first, bpr::application::agent_run_outcome::workspace_unavailable, 0)
						   == dif_clip_id::blocked,
				   "an admitted workspace failure blocks without a consider start");
		}

		{
			auto router = configured();
			expect(admit_and_start(router, first, 0) == dif_clip_id::consider,
				   "the exact current invocation remains admitted beside rejected identities");
			auto foreign    = cue_delivery(first);
			foreign.Session = bpr::core::agent_session_id{ 99U };
			router.deliver(first, foreign, true, true);
			expect(!router.finish(first, bpr::application::agent_run_outcome::success, 1),
				   "a mismatched delivery cannot fabricate reply or finding success");
		}

		{
			auto router = configured();
			expect(admit_and_start(router, first, 0) == dif_clip_id::consider,
				   "the duplicate-delivery course starts from one exact invocation");
			const auto delivery = cue_delivery(first);
			router.deliver(first, delivery, true, true);
			router.deliver(first, delivery, true, true);
			expect(router.finish(first, bpr::application::agent_run_outcome::success, 1)
						   == dif_clip_id::attention
					   && !router.finish(first, bpr::application::agent_run_outcome::success, 2),
				   "duplicate delivery coalesces to one highest-priority terminal and cannot replay");
		}

		{
			auto router = configured();
			expect(admit_and_start(router, first, 0) == dif_clip_id::consider,
				   "the supersession course starts its first invocation");
			auto replacement            = cue_identity(2U);
			replacement.Session.Profile = bpr::core::agent_profile_id{ 6U };
			expect(!admit_and_start(router, replacement, 1)
					   && router.active_invocation() == std::optional{ replacement },
				   "a new profile invocation supersedes immediately even while its cue is cooled down");
			router.deliver(first, cue_delivery(first), true, true);
			expect(!router.finish(first, bpr::application::agent_run_outcome::cancelled, 2),
				   "the superseded profile's interruption cannot block the replacement");
			router.deliver(replacement, cue_delivery(replacement), true, false);
			expect(!router.finish(replacement, bpr::application::agent_run_outcome::success, 2),
				   "the replacement reply stays authoritative while its cooled-down ready cue is dropped");
		}

		{
			auto router = configured();
			expect(admit_and_start(router, first, 0) == dif_clip_id::consider,
				   "the context-switch course starts its old invocation");
			auto nextWindow               = cue_identity(1U);
			nextWindow.ApplicationSession = bpr::core::application_session_id{ 5U };
			router.observe({
				.ApplicationSession = nextWindow.ApplicationSession,
				.Authority          = nextWindow.Authority,
				.Binding            = nextWindow.Session.Binding,
			});
			expect(!router.finish(first, bpr::application::agent_run_outcome::cancelled, 1)
					   && admit_and_start(router, nextWindow, 1) == dif_clip_id::consider,
				   "a window switch clears the old terminal and starts with a fresh bounded history");

			auto nextReview = cue_identity(1U, 4U);
			router.observe({
				.ApplicationSession = nextReview.ApplicationSession,
				.Authority          = nextReview.Authority,
				.Binding            = nextReview.Session.Binding,
			});
			expect(!router.finish(nextWindow, bpr::application::agent_run_outcome::success, 2)
					   && admit_and_start(router, nextReview, 2) == dif_clip_id::consider,
				   "a pull-request switch admits neither the old result nor its cue");

			auto nextRevision = cue_identity(1U, 4U, 2U);
			router.observe({
				.ApplicationSession = nextRevision.ApplicationSession,
				.Authority          = nextRevision.Authority,
				.Binding            = nextRevision.Session.Binding,
			});
			expect(!router.finish(nextReview, bpr::application::agent_run_outcome::success, 3)
					   && admit_and_start(router, nextRevision, 3) == dif_clip_id::consider,
				   "a same-review revision switch rejects the old binding without borrowing its cooldown");
		}

		{
			auto router = configured();
			expect(router.reveal_findings(0) == dif_clip_id::compare_glance,
				   "comparison cooldown begins only after an accepted visible reveal");
			expect(!admit_and_start(router, first, 1)
					   && router.active_invocation() == std::optional{ first },
				   "cooldown suppresses only playback and never delays invocation state");
			router.deliver(first, cue_delivery(first), true, true);
			expect(router.finish(first, bpr::application::agent_run_outcome::success, 2)
					   == dif_clip_id::attention,
				   "terminal coalescing preempts the lower-priority running cue synchronously");
			const auto interrupted = cue_identity(2U);
			expect(!admit_and_start(router, interrupted, 3)
					   && router.finish(interrupted, bpr::application::agent_run_outcome::cancelled, 4)
						   == dif_clip_id::blocked
					   && !router.reveal_findings(5),
				   "interruption preempts attention while lower cues are dropped rather than queued");
		}

		{
			auto router  = dif_cue_router{ };
			auto revised = cue_identity(1U, 3U, 2U);
			router.observe({
				.ApplicationSession = revised.ApplicationSession,
				.Authority          = revised.Authority,
				.Binding            = revised.Session.Binding,
			});
			router.set_visible(true);
			auto oldSession = cue_identity(1U, 3U, 1U).Session;
			auto offer      = bpr::core::agent_continuation_offer{
				.Required        = true,
				.ContinueAgainst = oldSession.Binding,
			};
			expect(router.present_continuation(oldSession, offer, 0)
					   == dif_clip_id::blocked,
				   "an explicitly presented continuation stays bound to its named old revision");
			offer.ContinueAgainst = revised.Session.Binding;
			expect(!router.present_continuation(oldSession, offer, 2'000),
				   "a continuation whose label and session binding disagree emits no cue");
		}

		{
			auto       router   = configured();
			const auto workshop = cue_identity(1U);
			expect(router.admit(workshop)
					   && router.present_workshop(workshop.Session, 1U, bpr::core::review_draft_id{ 7U }, 3U, "proposed revision", 0)
						   == dif_clip_id::comment_ready,
				   "the draft-conflict course starts from a presented proposal");
			const auto command = bpr::core::revisioned_review_draft_command{
				.Command = {
					.Id   = bpr::core::review_draft_id{ 7U },
					.Kind = bpr::core::review_draft_command_kind::edit,
					.Body = "proposed revision",
				},
				.ExpectedRevision = 3U,
				.Scope            = bpr::core::review_draft_scope_id{ 8U },
			};
			router.begin_draft(command);
			router.cancel_draft(command);
			expect(!router.accept_draft(
					   command,
					   {
						   .Draft = {
							   .Id   = command.Command.Id,
							   .Body = command.Command.Body,
						   },
						   .Revision = 4U,
						   .Scope    = command.Scope,
					   },
					   2'000),
				   "a local conflict cancels its acknowledgement without consuming the visible proposal");
		}

		{
			auto       router   = configured();
			const auto original = cue_identity(1U);
			const auto next     = cue_identity(2U);
			const auto command  = bpr::core::revisioned_review_draft_command{
				.Command = {
					.Id   = bpr::core::review_draft_id{ 8U },
					.Kind = bpr::core::review_draft_command_kind::edit,
					.Body = "proposal from the old session",
				},
				.ExpectedRevision = 3U,
				.Scope            = bpr::core::review_draft_scope_id{ 9U },
			};
			expect(router.admit(original)
					   && router.present_workshop(
							  original.Session,
							  1U,
							  command.Command.Id,
							  command.ExpectedRevision,
							  command.Command.Body,
							  0)
						   == dif_clip_id::comment_ready
					   && router.admit(next),
				   "a newer admitted session replaces the workshop presentation identity");
			router.begin_draft(command);
			expect(!router.accept_draft(
					   command,
					   {
						   .Draft = {
							   .Id   = command.Command.Id,
							   .Body = command.Command.Body,
						   },
						   .Revision = 4U,
						   .Scope    = command.Scope,
					   },
					   2'000),
				   "accepting a still-visible proposal from the prior session cannot acknowledge it");

			auto between = configured();
			expect(between.admit(original)
					   && between.present_workshop(
							  original.Session,
							  1U,
							  command.Command.Id,
							  command.ExpectedRevision,
							  command.Command.Body,
							  0)
						   == dif_clip_id::comment_ready,
				   "the between-dispatch course starts from the original session proposal");
			between.begin_draft(command);
			expect(between.admit(next)
					   && !between.accept_draft(
						   command,
						   {
							   .Draft = {
								   .Id   = command.Command.Id,
								   .Body = command.Command.Body,
							   },
							   .Revision = 4U,
							   .Scope    = command.Scope,
						   },
						   2'000),
				   "a session replacement between begin and accepted result cancels acknowledgement");
		}

		{
			auto       router       = configured();
			const auto operationOne = cue_identity(1U);
			const auto operationTwo = cue_identity(2U);
			const auto draft        = bpr::core::review_draft_id{ 9U };
			const auto other        = bpr::core::review_draft_id{ 10U };
			expect(router.admit(operationOne)
					   && router.present_workshop(operationOne.Session, 1U, draft, 4U, "first", 0)
						   == dif_clip_id::comment_ready
					   && router.admit(operationTwo)
					   && router.present_workshop(operationTwo.Session, 2U, other, 4U, "second", 2'000)
						   == dif_clip_id::comment_ready
					   && !router.present_workshop(operationOne.Session, 1U, draft, 4U, "first", 4'000)
					   && !router.present_workshop(operationTwo.Session, 2U, other, 4U, "changed replay", 6'000),
				   "an obsolete cross-draft operation and conflicting replay cannot replace the latest proposal");
			const auto operationThree = cue_identity(3U);
			expect(router.admit(operationThree)
					   && router.present_workshop(operationThree.Session, 3U, draft, 4U, "fresh same revision", 8'000)
						   == dif_clip_id::comment_ready,
				   "a distinct fresh workshop operation remains eligible at the same draft revision");
			const auto ordinary = bpr::core::revisioned_review_draft_command{
				.Command = {
					.Id   = draft,
					.Kind = bpr::core::review_draft_command_kind::edit,
					.Body = "ordinary body edit",
				},
				.ExpectedRevision = 4U,
				.Scope            = bpr::core::review_draft_scope_id{ 10U },
			};
			router.begin_draft(ordinary);
			expect(!router.accept_draft(
					   ordinary,
					   {
						   .Draft = {
							   .Id   = ordinary.Command.Id,
							   .Body = ordinary.Command.Body,
						   },
						   .Revision = 6U,
						   .Scope    = ordinary.Scope,
					   },
					   6'100),
				   "an ordinary edit cannot acknowledge a retained or rejected workshop proposal");
			const auto acceptedWorkshop = bpr::core::revisioned_review_draft_command{
				.Command = {
					.Id   = draft,
					.Kind = bpr::core::review_draft_command_kind::edit,
					.Body = "fresh same revision",
				},
				.ExpectedRevision = 4U,
				.Scope            = ordinary.Scope,
			};
			router.begin_draft(acceptedWorkshop);
			expect(router.accept_draft(
					   acceptedWorkshop,
					   {
						   .Draft = {
							   .Id   = draft,
							   .Body = acceptedWorkshop.Command.Body,
						   },
						   .Revision = 5U,
						   .Scope    = acceptedWorkshop.Scope,
					   },
					   10'000)
					   == dif_clip_id::acknowledge,
				   "explicitly accepting the fresh same-revision proposal acknowledges its operation");
		}
	}
	void test_catalog_and_playback()
	{
		fixture    input;
		const auto cache = input.load();
		expect(cache && input.Loads == 10, "ten strips decode exactly once");
		expect(cache->premultiplication_copy_bytes() == 0U, "validated Dif strips reuse decoded storage for premultiplication");
		std::uint64_t pixelBytes = 0;
		for (std::size_t index = 0; index < 10; ++index) {
			const auto& clip = cache->clip(index);
			pixelBytes += static_cast<std::uint64_t>(cache->image(index).sizeInBytes());
			expect(cache->image(index).format() == QImage::Format_ARGB32_Premultiplied, "one premultiplied representation");
			const auto straight = QImage::fromData(input.Bytes[index], "PNG");
			expect(cache->image(index) == straight.convertToFormat(QImage::Format_ARGB32_Premultiplied), "storage reuse preserves every straight-alpha fixture pixel and gutter");
			dif_sequencer player{ cache };
			expect(player.start(index, 100), "all ten indices start");
			for (int frame = 0; frame < clip.Frames; ++frame) {
				const auto sampled = player.sample(100 + static_cast<std::int64_t>(frame) * clip.FrameMs);
				expect(sampled.Clip == index && sampled.Frame == frame, "every declared frame at its rate");
				const auto rect = cache->source(index, frame);
				expect(rect == QRect{ frame * (clip.Cell + clip.Gutter), 0, clip.Cell, clip.Cell }, "source excludes gutters");
				expect(std::abs(cache->image(index).pixelColor(rect.topLeft()).red() - frame * 20) <= 1, "source selects distinct fixture pixels");
			}
			const auto returned = player.sample(100 + static_cast<std::int64_t>(clip.Frames) * clip.FrameMs);
			expect(returned.Clip == (clip.Playback == dif_playback::once ? clip.Return : index) && returned.Frame == 0, "loop seam and declared one-shot returns");
		}
		expect(cache->pixel_bytes() == pixelBytes && cache->retained_bytes() > pixelBytes && cache->retained_bytes() < 32 * 1024 * 1024, "accounted retained memory bounded");
		expect(cache->conversion_peak_bytes() < 40 * 1024 * 1024, "conversion peak bounded");
		dif_sequencer player{ cache };
		expect(player.start(1, 0), "loop starts");
		auto late = player.sample(std::numeric_limits<std::int64_t>::max());
		expect(late.Frame == (std::numeric_limits<std::int64_t>::max() / cache->clip(1).FrameMs) % cache->clip(1).Frames, "huge elapsed uses bounded modulo");
		expect(player.sample(1).Clip == 0, "backward time cancels");
		expect(!player.start(10, 0) && !player.start(1, -1), "invalid index and negative time refused");
		expect(player.start(9, 0) && player.sample(1'000'000).Clip == 1, "late one-shot returns once without catch-up");
		expect(cache->source(10, 0).isEmpty() && cache->source(1, -1).isEmpty() && cache->source(1, 2).isEmpty(), "invalid frame rectangles refused");
		input.Clips[1].FrameMs = 1;
		expect(!input.load(), "excessive rate refused");
		input.Clips[1].FrameMs = 94;
		input.Clips[1].Frames  = std::numeric_limits<int>::max();
		expect(!input.load(), "overflow geometry refused before decode");
		input.Clips[1].Frames = 2;
		input.Clips[1].Gutter = 3;
		expect(!input.load(), "mismatched strip geometry refused");
		input.Clips[1].Gutter = 2;
		auto badGutter        = QImage::fromData(input.Bytes[1], "PNG");
		badGutter.setPixelColor(8, 0, Qt::red);
		input.Bytes[1].clear();
		QBuffer badBuffer{ &input.Bytes[1] };
		badBuffer.open(QIODevice::WriteOnly);
		expect(badGutter.save(&badBuffer, "PNG"), "malformed gutter fixture encodes");
		input.Clips[1].Sha256 = rgba_digest(badGutter);
		expect(!input.load(), "opaque gutter refused despite valid digest and dimensions");
		input.Bytes[1].append('x');
		expect(!input.load(), "digest mismatch refused");
		std::cout << "fixture pixels=" << cache->pixel_bytes() << " accounted-retained=" << cache->retained_bytes() << " conversion-peak=" << cache->conversion_peak_bytes() << '\n';
	}
	void test_retained_end_margins_and_source_admission()
	{
		fixture                input;
		std::array<QImage, 10> paddedSources;
		for (std::size_t index = 0; index < input.Clips.size(); ++index) {
			auto& clip  = input.Clips[index];
			clip.Cell   = 7;
			clip.Origin = 3;
			QImage padded{ 2 * clip.Origin + clip.Frames * clip.Cell + (clip.Frames - 1) * clip.Gutter, clip.Cell, QImage::Format_ARGB32 };
			padded.fill(Qt::transparent);
			{
				QPainter painter{ &padded };
				for (int frame = 0; frame < clip.Frames; ++frame) {
					painter.fillRect(clip.Origin + frame * (clip.Cell + clip.Gutter), 0, clip.Cell, clip.Cell, QColor{ frame * 20, static_cast<int>(index) * 20, 80, 128 });
				}
			}
			for (int frame = 0; frame < clip.Frames; ++frame) {
				const auto firstColumn = clip.Origin + frame * (clip.Cell + clip.Gutter);
				for (int row = 0; row < clip.Cell; ++row) {
					padded.setPixelColor(firstColumn, row, QColor{ 255, 17, 41, 255 });
					padded.setPixelColor(firstColumn + clip.Cell - 1, row, QColor{ 13, 211, 239, 127 });
				}
			}
			input.Bytes[index]   = png_bytes(padded);
			clip.Sha256          = rgba_digest(padded);
			paddedSources[index] = std::move(padded);
		}
		const auto cache = input.load();
		expect(cache && input.Loads == 10, "padded source strips decode once");
		expect(cache->image(0).width() == input.Clips[0].Cell, "validated transparent end cells consume no retained pixel storage");
		for (std::size_t index = 0; index < input.Clips.size(); ++index) {
			const auto& clip      = input.Clips[index];
			const auto  reference = paddedSources[index].convertToFormat(QImage::Format_ARGB32_Premultiplied);
			expect(cache->clip(index).Origin == clip.Origin && cache->clip(index).Sha256 == clip.Sha256, "encoded origin and digest remain unchanged");
			expect(cache->image(index) == reference.copy(clip.Origin, 0, reference.width() - 2 * clip.Origin, clip.Cell), "every retained cell and inner gutter equals the validated interior");
			for (int frame = 0; frame < clip.Frames; ++frame) {
				const QRect encodedSource{ clip.Origin + frame * (clip.Cell + clip.Gutter), 0, clip.Cell, clip.Cell };
				expect(cache->source(index, frame) == encodedSource.translated(-clip.Origin, 0), "frame rectangles address retained interior coordinates");
				for (const int extent : { 96, 120, 192 }) {
					expect(render_frame(cache->image(index), cache->source(index, frame), extent) == render_frame(reference, encodedSource, extent), "opaque and translucent frame edges render exactly as the padded source at every scale");
				}
			}
		}
		for (const int column : { 0, paddedSources[0].width() - 1 }) {
			auto bad = paddedSources[0].copy();
			bad.setPixelColor(column, 0, Qt::red);
			const auto bytes = png_bytes(bad);
			auto       clips = input.Clips;
			clips[0].Sha256  = rgba_digest(bad);
			expect(!dif_cache::create(clips, [&](std::size_t aIndex) { return aIndex == 0 ? bytes : input.Bytes[aIndex]; }), "both omitted end cells are still validated despite a matching digest");
		}

		QImage wide{ 3 * 512, 512, QImage::Format_ARGB32 };
		wide.fill(Qt::transparent);
		{
			QPainter painter{ &wide };
			painter.fillRect(512, 0, 512, 512, Qt::red);
		}
		const auto             bytes  = png_bytes(wide);
		const auto             digest = rgba_digest(wide);
		dif_cache::descriptors clips;
		for (auto& clip : clips) {
			clip.Frames = 1;
			clip.Cell   = 512;
			clip.Origin = 512;
			clip.Sha256 = digest;
		}
		int loads = 0;
		expect(!dif_cache::create(clips, [&](std::size_t) { ++loads; return bytes; }) && loads == 9, "full decoded source above24MiB refuses even when retained interiors would fit");

		fixture transient;
		auto&   large = transient.Clips[1];
		large.Frames  = 3;
		large.Cell    = 1024;
		large.Origin  = 1024;
		large.Gutter  = 0;
		QImage largest{ 5 * 1024, 1024, QImage::Format_ARGB32 };
		largest.fill(Qt::transparent);
		{
			QPainter painter{ &largest };
			painter.fillRect(1024, 0, 3 * 1024, 1024, Qt::red);
		}
		large.Sha256       = rgba_digest(largest);
		transient.Bytes[1] = png_bytes(largest);
		expect(!transient.load() && transient.Loads == 2, "two full source strips preserve the40MiB transient refusal before decoding");
	}
	void test_production_home()
	{
		dif_catalog_provider provider;
		expect(provider.load_attempts() == 0 && provider.decoded_count() == 0, "production provider is lazy");
		const auto cache = provider.acquire();
		expect(cache && provider.acquire() == cache && provider.cache_identity() == cache.get(), "compiled resources decode once and share identity");
		expect(provider.load_attempts() == 1 && provider.decoded_count() == 10 && provider.failure_diagnostic().isEmpty(), "one production decode lifetime");
		expect(cache->premultiplication_copy_bytes() == 0U, "all production Dif strips reuse validated decoded storage");
		const auto productionLoader = [](std::size_t aIndex) {
			const std::array names{ "rest", "calm-idle", "greet", "blink", "compare-glance", "consider", "comment-ready", "attention", "acknowledge", "blocked" };
			const QResource  resource{ QStringLiteral(":/bpr/dif/dif-") + QString::fromLatin1(names.at(aIndex)) + QStringLiteral(".png") };
			return QByteArray{ reinterpret_cast<const char*>(resource.data()), resource.size() };
		};
		std::uint64_t fullSourceBytes = 0;
		for (std::size_t index = 0; index < 10; ++index) {
			const auto& clip      = cache->clip(index);
			const auto  source    = QImage::fromData(productionLoader(index), "PNG");
			const auto  reference = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
			fullSourceBytes += static_cast<std::uint64_t>(source.sizeInBytes());
			expect(cache->source(index, 0).x() == 0 && cache->source(index, clip.Frames - 1).right() == cache->image(index).width() - 1, "retained production rectangles omit only both full-cell end gutters");
			expect(cache->image(index) == reference.copy(clip.Origin, 0, reference.width() - 2 * clip.Origin, clip.Cell), "every production interior pixel equals the validated padded source");
			for (int frame = 0; frame < clip.Frames; ++frame) {
				const QRect encodedSource{ clip.Origin + frame * clip.Cell, 0, clip.Cell, clip.Cell };
				for (const int extent : { 96, 120, 192 }) {
					expect(render_frame(cache->image(index), cache->source(index, frame), extent) == render_frame(reference, encodedSource, extent), "every production frame renders identically to its padded source at every scale");
				}
			}
		}
		expect(fullSourceBytes == 23'855'104 && cache->pixel_bytes() == 18'612'224 && fullSourceBytes - cache->pixel_bytes() == 5'242'880, "all twenty source end cells are validated without retaining their transparent pixels");
		expect(cache->retained_bytes() < 32 * 1024 * 1024 && cache->conversion_peak_bytes() < 40 * 1024 * 1024, "production retained and full-source transient ceilings remain unchanged");
		auto malformed         = production_dif_catalog();
		malformed[0].Sha256[0] = malformed[0].Sha256[0] == 'x' ? 'y' : 'x';
		expect(!dif_cache::create(malformed, productionLoader), "decoded RGBA digest mismatch refuses production catalog");
		malformed           = production_dif_catalog();
		malformed[0].Origin = 0;
		expect(!dif_cache::create(malformed, productionLoader), "missing production end cells refuse dimensions");
		for (const int x : { 0, 767 }) {
			auto image = QImage::fromData(productionLoader(0), "PNG");
			image.setPixelColor(x, 0, Qt::red);
			QByteArray bytes;
			QBuffer    buffer{ &bytes };
			buffer.open(QIODevice::WriteOnly);
			expect(image.save(&buffer, "PNG"), "corrupt production end cell encodes");
			malformed           = production_dif_catalog();
			malformed[0].Sha256 = rgba_digest(image);
			expect(!dif_cache::create(malformed, [&bytes, &productionLoader](std::size_t aIndex) { return aIndex == 0 ? bytes : productionLoader(aIndex); }), "both production gutters are validated independently of digest");
		}
		dif_sequencer home{ cache };
		expect(home.home(0) && home.sample(0).Clip == 2 && home.greeted(), "first eligible Home greets");
		expect(home.sample(1000).Clip == 1, "greet returns to calm");
		expect(home.sample(3999).Frame == 11 && home.completed_calm_loops() == 0, "full first calm cycle");
		expect(home.sample(10000).Clip == 1 && home.completed_calm_loops() == 3, "three complete breaths do not blink");
		expect(home.sample(13000).Clip == 3 && home.sample(13599).Frame == 5, "fourth boundary starts six-frame blink");
		expect(home.sample(13600).Clip == 1 && home.completed_calm_loops() == 0, "blink returns fresh calm");
		home.reset();
		expect(home.home(20000) && home.sample(20000).Clip == 1, "resume never repeats greeting");
		expect(home.sample(1000000).Clip == 1 && home.completed_calm_loops() == 0, "long delay drops expired blink debt");
		expect(home.sample(-1).Clip == 0 && !home.sample(0).NextMs, "invalid time rests without timer");
		int                  refusedLoads = 0;
		dif_catalog_provider refused{ [&refusedLoads](std::size_t) { ++refusedLoads; return QByteArray{ }; } };
		expect(!refused.acquire() && !refused.acquire() && refusedLoads == 1 && refused.load_attempts() == 1 && refused.failure_diagnostic() == "dif-catalog-unavailable", "failed catalog is latched with one bounded diagnostic");
		dif_widget widget{ cache };
		widget.show();
		QApplication::processEvents();
		widget.set_home(true);
		widget.set_eligibility({ true, true, true, false, false });
		expect(widget.current_frame().Clip == 2, "Home widget starts greet automatically");
		widget.set_home(true);
		expect(widget.statistics().PlaybackStarts == 1, "idempotent Home update does not restart");
		widget.set_eligibility({ true, false, true, false, false });
		expect(widget.current_frame().Clip == 0 && !widget.findChild<QTimer*>()->isActive(), "reduced Home rests");
		widget.set_eligibility({ true, true, true, false, false });
		expect(widget.current_frame().Clip == 1 && widget.statistics().Greeted, "full Home resumes calm without greet");
		QEvent deactivate{ QEvent::WindowDeactivate };
		QApplication::sendEvent(&widget, &deactivate);
		expect(widget.current_frame().Clip == 0 && !widget.findChild<QTimer*>()->isActive(), "Home deactivation cancels immediately");
		wait_events(100);
		expect(!widget.findChild<QTimer*>()->isActive(), "deactivated Home does not restart itself");
		widget.activateWindow();
		QApplication::processEvents();
		QEvent activate{ QEvent::WindowActivate };
		QApplication::sendEvent(&widget, &activate);
		expect(widget.current_frame().Clip == 1 && widget.findChild<QTimer*>()->isActive(), "activation resumes calm without repeated greeting");
		QEvent stateChange{ QEvent::WindowStateChange };
		QApplication::sendEvent(&widget, &stateChange);
		expect(widget.current_frame().Clip == 1 && widget.findChild<QTimer*>()->isActive(), "an active non-minimizing window-state change preserves playback");
		widget.shutdown();
		std::cout << "production pixels=" << cache->pixel_bytes() << " accounted-retained=" << cache->retained_bytes() << " conversion-peak=" << cache->conversion_peak_bytes() << '\n';
	}
	void test_native_memory_probe()
	{
#ifdef __APPLE__
		expect(QApplication::platformName() == QStringLiteral("cocoa"), "native memory probe uses Cocoa");
		QApplication::processEvents();
		const auto           beforeCache = sample_process_memory();
		process_memory_peak  conversionPeak{ beforeCache };
		dif_catalog_provider provider;
		const auto           cache           = provider.acquire();
		const auto           afterConversion = sample_process_memory();
		conversionPeak.stop();
		const auto afterCache = sample_process_memory();
		expect(cache && provider.load_attempts() == 1U && provider.decoded_count() == 10U, "native memory probe decodes one production catalog");

		const auto observedRetainedDelta       = positive_delta(afterCache.MallocInUse, beforeCache.MallocInUse);
		const auto observedConversionHighWater = positive_delta(std::max({ conversionPeak.malloc_in_use(), afterConversion.MallocInUse, afterCache.MallocInUse }), beforeCache.MallocInUse);
		expect(observedRetainedDelta >= cache->pixel_bytes(), "observed retained default-zone delta includes decoded pixels");
		expect(observedRetainedDelta < 32U * 1024U * 1024U, "observed retained default-zone delta stays below art CPU ceiling");
		expect(observedConversionHighWater >= observedRetainedDelta && observedConversionHighWater < 40U * 1024U * 1024U, "observed conversion default-zone high-water stays below transient ceiling");

		std::array<qint64, 10> imageKeys{ };
		for (std::size_t index = 0; index < imageKeys.size(); ++index) {
			imageKeys[index] = cache->image(index).cacheKey();
		}
		paint_counter       paints;
		process_memory_peak nativeWindowPeak{ afterCache };
		dif_widget          widget{ cache };
		widget.resize(96, 96);
		widget.installEventFilter(&paints);
		widget.show();
		widget.set_home(true);
		widget.set_eligibility({ true, false, true, false, false });
		widget.update();
		wait_events(250);
		nativeWindowPeak.stop();
		const auto afterWindow = sample_process_memory();
		expect(paints.Paints > 0, "native window paints production Dif");
		for (std::size_t index = 0; index < imageKeys.size(); ++index) {
			expect(cache->image(index).cacheKey() == imageKeys[index], "native paint retains CPU image identity without detach");
		}

		const auto peakWindowMalloc    = std::max(nativeWindowPeak.malloc_in_use(), afterWindow.MallocInUse);
		const auto peakWindowResident  = std::max(nativeWindowPeak.resident(), afterWindow.Resident);
		const auto peakWindowFootprint = std::max(nativeWindowPeak.physical_footprint(), afterWindow.PhysicalFootprint);
		write_native_memory_report({
			{ "schema", "bpr.dif-native-memory.v1" },
			{ "qt_platform", QApplication::platformName() },
			{ "environment", QJsonObject{
								 { "operating_system", QSysInfo::prettyProductName() },
								 { "architecture", QSysInfo::currentCpuArchitecture() },
								 { "qt_version", QString::fromLatin1(qVersion()) },
							 } },
			{ "measurement_scope", "macOS default malloc zone plus whole-process Mach TASK_VM_INFO" },
			{ "cache", QJsonObject{
						   { "decoded_pixel_bytes", static_cast<qint64>(cache->pixel_bytes()) },
						   { "observed_retained_default_zone_delta_bytes", static_cast<qint64>(observedRetainedDelta) },
						   { "observed_non_pixel_default_zone_delta_bytes", static_cast<qint64>(observedRetainedDelta - cache->pixel_bytes()) },
						   { "observed_conversion_default_zone_high_water_bytes", static_cast<qint64>(observedConversionHighWater) },
						   { "retained_ceiling_bytes", 32 * 1024 * 1024 },
						   { "conversion_peak_ceiling_bytes", 40 * 1024 * 1024 },
						   { "decoded_clips", static_cast<qint64>(provider.decoded_count()) },
						   { "load_attempts", static_cast<qint64>(provider.load_attempts()) },
						   { "production_accounted_retained_bytes", static_cast<qint64>(cache->retained_bytes()) },
						   { "production_accounted_conversion_peak_bytes", static_cast<qint64>(cache->conversion_peak_bytes()) },
					   } },
			{ "before_cache", memory_json(beforeCache) },
			{ "after_conversion_before_sampler_stop", memory_json(afterConversion) },
			{ "after_cache", memory_json(afterCache) },
			{ "native_window", QJsonObject{
								   { "paint_events", paints.Paints },
								   { "cpu_images_retained_without_detach", true },
								   { "observed_peak_malloc_delta_bytes", static_cast<qint64>(positive_delta(peakWindowMalloc, afterCache.MallocInUse)) },
								   { "observed_peak_resident_delta_bytes", static_cast<qint64>(positive_delta(peakWindowResident, afterCache.Resident)) },
								   { "observed_peak_physical_footprint_delta_bytes", static_cast<qint64>(positive_delta(peakWindowFootprint, afterCache.PhysicalFootprint)) },
								   { "retained_malloc_delta_bytes", static_cast<qint64>(positive_delta(afterWindow.MallocInUse, afterCache.MallocInUse)) },
								   { "retained_resident_delta_bytes", static_cast<qint64>(positive_delta(afterWindow.Resident, afterCache.Resident)) },
								   { "retained_physical_footprint_delta_bytes", static_cast<qint64>(positive_delta(afterWindow.PhysicalFootprint, afterCache.PhysicalFootprint)) },
							   } },
			{ "after_native_window", memory_json(afterWindow) },
			{ "qualification", "Retained and conversion values are sampled default-zone deltas in an otherwise idle process; they do not cover non-default allocators or prove a sub-sample maximum. The production accounting fields are arithmetic admission estimates, not allocator measurements. Native-window deltas come from one static 96-pixel widget and combine widget, raster backing-store, and Cocoa/backend costs." },
		});
		widget.shutdown();
		std::cout << "dif-native-memory-probe:pass\n";
#else
		expect(false, "native memory probe is available on macOS");
#endif
	}
	void test_widget_lifecycle()
	{
		fixture    input;
		const auto cache = input.load();
		dif_widget first{ cache };
		dif_widget second{ cache };
		first.resize(64, 64);
		second.resize(64, 64);
		first.show();
		second.show();
		QApplication::processEvents();
		first.set_eligibility({ true, true, true, false, false });
		expect(first.play(1), "eligible animation starts");
		auto* timer = first.findChild<QTimer*>();
		expect(timer && first.findChildren<QTimer*>().size() == 1 && timer->isSingleShot() && timer->isActive(), "exactly one single-shot timer");
		first.reset_contextual();
		expect(first.current_frame().Clip == dif_index(dif_clip_id::rest)
				   && !timer->isActive(),
			   "an exact context change cancels its old one-shot immediately");
		expect(first.play(1), "a new exact context can start fresh playback");
		expect(input.Loads == 10, "two widgets share one decoded cache");
		const std::array gates{
			dif_eligibility{ false, true, true, false, false },
			dif_eligibility{ true, false, true, false, false },
			dif_eligibility{ true, true, false, false, false },
			dif_eligibility{ true, true, true, true, false },
			dif_eligibility{ true, true, true, false, true }
		};
		for (const auto gate : gates) {
			first.set_eligibility(gate);
			expect(!timer->isActive() && !first.play(2), "each suppression cancels and refuses cues");
			first.set_eligibility({ true, true, true, false, false });
			expect(!timer->isActive(), "resume is neutral without replay");
			expect(first.play(1), "fresh cue starts after resume");
		}
		first.hide();
		expect(!timer->isActive(), "hide cancels immediately");
		first.show();
		first.set_eligibility({ true, false, true, false, false });
		paint_counter paints;
		first.installEventFilter(&paints);
		const auto before = first.grab().toImage();
		QApplication::processEvents();
		const auto staticPaints = paints.Paints;
		wait_events(350);
		expect(paints.Paints == staticPaints, "static interval schedules no repaints");
		expect(first.grab().toImage() == before && !timer->isActive(), "reduced remains static over frame boundaries");
		first.set_eligibility({ true, true, true, false, false });
		expect(first.play(1), "restart before deactivation");
		const auto imageKey = cache->image(1).cacheKey();
		int        wakeups  = 0;
		QObject::connect(timer, &QTimer::timeout, &first, [&wakeups] { ++wakeups; });
		wait_events(400);
		expect(wakeups > 0 && wakeups <= 5, "timer follows declared rate within twelve wakeups per second");
		expect(cache->image(1).cacheKey() == imageKey && input.Loads == 10, "ticks neither decode nor detach retained pixels");
		QEvent deactivate{ QEvent::WindowDeactivate };
		QApplication::sendEvent(&first, &deactivate);
		expect(!timer->isActive() && !first.play(1), "deactivation fences future cue admission");
		first.set_eligibility({ true, true, true, false, false });
		expect(first.play(1), "restart before shutdown");
		first.shutdown();
		expect(!timer->isActive() && !first.play(1), "shutdown refuses late work");
		auto* temporary = new dif_widget{ cache };
		temporary->show();
		temporary->set_eligibility({ true, true, true, false, false });
		expect(temporary->play(1), "destroy fixture starts");
		QPointer<QTimer> lifetime = temporary->findChild<QTimer*>();
		delete temporary;
		wait_events(150);
		expect(!lifetime, "destruction releases timer before late deadline");
	}
}
int main(int argc, char** argv)
{
	QApplication application{ argc, argv };
	if (argc == 2 && std::string_view{ argv[1] } == "--native-memory-probe") {
		test_native_memory_probe();
		return EXIT_SUCCESS;
	}
	test_catalog_and_playback();
	test_retained_end_margins_and_source_admission();
	test_contextual_cue_router_uses_accepted_identity_and_bounded_latches();
	test_contextual_cue_router_rejects_stale_and_conflicting_facts();
	test_widget_lifecycle();
	test_production_home();
	std::cout << "dif-runtime-tests:pass\n";
}
