#include "bpr/qt/motion_observer.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QObject>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	void expect(bool aCondition)
	{
		if (!aCondition) {
			std::cerr << "motion-observer-test:failed\n";
			std::exit(EXIT_FAILURE);
		}
	}

	struct source_state
	{
		bpr::core::motion_observation Value = bpr::core::motion_observation::unknown;
		std::function<void()>         Changed;
		bool                          Destroyed = false;
	};

	class fake_source final : public bpr::qt::motion_observation_source
	{
	public:
		explicit fake_source(std::shared_ptr<source_state> aState)
			: State{ std::move(aState) }
		{ }
		~fake_source() override { State->Destroyed = true; }
		[[nodiscard]] auto read() noexcept -> bpr::core::motion_observation override
		{
			return State->Value;
		}
		void set_change_handler(std::function<void()> aHandler) override
		{
			State->Changed = std::move(aHandler);
		}

	private:
		std::shared_ptr<source_state> State;
	};
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	using bpr::core::motion_observation;
	if (argc > 1 && std::string_view{ argv[1] } == "--native-probe") {
		const auto native = bpr::qt::make_native_motion_source();
		const auto result = native->read();
		if (result != motion_observation::full && result != motion_observation::reduced) {
			std::cerr << "motion-native-probe:unavailable\n";
			return 1;
		}
		std::cout << "motion-native-probe:observed\n";
		return 0;
	}

	const auto state  = std::make_shared<source_state>();
	auto       first  = std::vector<motion_observation>{ };
	auto       second = std::vector<motion_observation>{ };
	{
		auto       observer = bpr::qt::motion_observer{ std::make_unique<fake_source>(state) };
		auto       lifetime = QObject{ };
		const auto firstId  = observer.subscribe(lifetime, [&](motion_observation aValue) {
			first.push_back(aValue);
		});
		expect(firstId != 0U);
		expect((first == std::vector{ motion_observation::unknown }));
		expect(observer.current() == motion_observation::unknown);

		state->Value = motion_observation::full;
		state->Changed();
		QCoreApplication::processEvents();
		expect((first == std::vector{ motion_observation::unknown, motion_observation::full }));
		state->Changed();
		QCoreApplication::processEvents();
		expect(first.size() == 2U);

		const auto secondId = observer.subscribe(lifetime, [&](motion_observation aValue) {
			second.push_back(aValue);
		});
		expect(secondId != 0U && secondId != firstId);
		expect((second == std::vector{ motion_observation::full }));
		observer.unsubscribe(firstId);
		state->Value = motion_observation::reduced;
		state->Changed();
		QCoreApplication::processEvents();
		expect(first.size() == 2U);
		expect((second == std::vector{ motion_observation::full, motion_observation::reduced }));
		observer.unsubscribe(secondId);
		auto       fenceDeliveries = 0U;
		auto       fencedId        = std::uint64_t{ 0 };
		const auto fenceOwnerId    = observer.subscribe(lifetime, [&](motion_observation aValue) {
			if (aValue == motion_observation::failed) {
				observer.unsubscribe(fencedId);
			}
		});
		fencedId                   = observer.subscribe(lifetime, [&](motion_observation) {
			++fenceDeliveries;
		});
		expect(fenceOwnerId != 0U && fencedId != 0U && fenceDeliveries == 1U);

		auto       removed           = std::make_unique<QObject>();
		auto       removedDeliveries = 0U;
		const auto removedId         = observer.subscribe(*removed, [&](motion_observation) {
			++removedDeliveries;
		});
		expect(removedId != 0U);
		expect(removedDeliveries == 1U);
		removed.reset();
		state->Value = motion_observation::failed;
		state->Changed();
		QCoreApplication::processEvents();
		expect(observer.current() == motion_observation::failed);
		expect(removedDeliveries == 1U);
		expect(fenceDeliveries == 1U);
	}
	expect(state->Destroyed);
	expect(!state->Changed);

	const auto native = bpr::qt::make_native_motion_source();
	expect(native != nullptr);
	expect(native->read() == motion_observation::unsupported);
	std::cout << "motion-observer-tests:pass\n";
	return 0;
}
