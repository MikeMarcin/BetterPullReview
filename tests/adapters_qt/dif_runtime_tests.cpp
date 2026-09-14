#include "bpr/qt/dif_runtime.hpp"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QEvent>
#include <QEventLoop>
#include <QPointer>

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
	using namespace bpr::qt;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
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
				clip.Sha256 = QCryptographicHash::hash(Bytes[index], QCryptographicHash::Sha256);
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
	void test_catalog_and_playback()
	{
		fixture    input;
		const auto cache = input.load();
		expect(cache && input.Loads == 10, "ten strips decode exactly once");
		std::uint64_t pixelBytes = 0;
		for (std::size_t index = 0; index < 10; ++index) {
			const auto& clip = cache->clip(index);
			pixelBytes += static_cast<std::uint64_t>(cache->image(index).sizeInBytes());
			expect(cache->image(index).format() == QImage::Format_ARGB32_Premultiplied, "one premultiplied representation");
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
		input.Clips[1].Sha256 = QCryptographicHash::hash(input.Bytes[1], QCryptographicHash::Sha256);
		expect(!input.load(), "opaque gutter refused despite valid digest and dimensions");
		input.Bytes[1].append('x');
		expect(!input.load(), "digest mismatch refused");
		std::cout << "fixture pixels=" << cache->pixel_bytes() << " accounted-retained=" << cache->retained_bytes() << " conversion-peak=" << cache->conversion_peak_bytes() << '\n';
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
	test_catalog_and_playback();
	test_widget_lifecycle();
	std::cout << "dif-runtime-tests:pass\n";
}
