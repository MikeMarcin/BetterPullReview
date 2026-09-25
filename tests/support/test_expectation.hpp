#ifndef BPR_TESTS_SUPPORT_TEST_EXPECTATION_HPP
#define BPR_TESTS_SUPPORT_TEST_EXPECTATION_HPP

/// \file
/// \brief A failed expectation that unwinds, so a failing course still removes
/// its scratch roots.
///
/// \details [[BPR-1121]], after [[BPR-1118]]. A test's `expect` used to print
/// its `FAILED:` line and call `std::exit`, which runs no destructor of a local
/// object. Every failing run then left its roots in the temporary directory:
/// scratch directories, and whole application data roots held in a
/// `QTemporaryDir`.
///
/// Here a failure throws `expectation_failed` instead. `main` is a function
/// try block whose handler returns `failing_exit()`, so by the time it runs,
/// every owner in `main`'s body has been destroyed:
///
/// \code
/// auto main() -> int
/// try {
/// 	test_something();
/// 	std::cout << "something-tests:pass\n";
/// 	return EXIT_SUCCESS;
/// } catch (...) {
/// 	return bpr::tests::failing_exit();
/// }
/// \endcode
///
/// Unwinding is not always safe where `std::exit` was, so a failure still
/// ends the process with `std::exit`, exactly as before, when:
/// - it happens on a thread other than the one that runs `main`: the
///   exception could not cross that thread's boundary, and a worker that
///   caught it could let the course run on;
/// - it happens in a child the course forked: the child's thread is the
///   parent's, so a throw would unwind into the parent's frames and run the
///   parent's destructors, including the one that removes the shared root;
/// - an exception is already unwinding;
/// - a scope has declared `exit_on_failure`: a destructor, a Qt callback, or a
///   window in which a helper thread would block the unwinding.
///
/// Two backstops keep a failure a failure. An expectation whose exception the
/// code under test catches is still reported when the process exits, which
/// then exits with failure. A process still running `unwind_bound` after a
/// failure is ended rather than left to hang. What still leaves a
/// root is what runs no code in the process: a kill, a CTest timeout, an abort
/// or a crash, and a failure inside an `exit_on_failure` scope.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

#if !defined(_WIN32)
	#include <unistd.h>
#endif

namespace bpr::tests {
	/// \brief What a failed expectation throws, once its `FAILED:` line is out.
	/// \details Not a `std::exception`, so a handler in the code under test
	/// that catches those does not take it for a failure of its own.
	struct expectation_failed final
	{
	};

	/// \brief How long the process may run on after a failure before it is ended.
	inline constexpr auto unwind_bound = std::chrono::seconds{ 20 };

	namespace detail {
		/// \brief Where the course runs, and what its failures have done so far.
		/// \details One per program, made before `main` on the thread that runs it.
		class failure_state final
		{
		public:
			failure_state()                                        = default;
			failure_state(const failure_state&)                    = delete;
			auto operator=(const failure_state&) -> failure_state& = delete;

			/// \brief Reports, at exit, a failure that the code under test caught.
			/// \details The run's exit code is replaced by the failing one, because
			/// the course carried on after the failure and may have reported a pass.
			~failure_state()
			{
				if (Failed.load() && !Reported.load()) {
					std::cerr << "FAILED: an expectation failed and its exception was caught before main\n";
					std::fflush(nullptr);
					std::_Exit(EXIT_FAILURE);
				}
			}

			/// \brief Whether a failure here may unwind rather than exit.
			[[nodiscard]] auto may_unwind() const noexcept -> bool
			{
				return std::this_thread::get_id() == Thread
#if !defined(_WIN32)
					&& ::getpid() == Process
#endif
					&& ExitScopes.load() == 0U && std::uncaught_exceptions() == 0;
			}

			/// \brief Starts, once, the thread that ends the process if it is still
			/// running `unwind_bound` after a failure: an unwinding that hangs, or a
			/// course that runs on because the code under test caught the failure.
			void arm_unwind_bound() noexcept
			{
				if (Armed.exchange(true)) {
					return;
				}
				try {
					std::thread{ [] {
						std::this_thread::sleep_for(unwind_bound);
						std::cerr << "FAILED: the process was still running " << unwind_bound.count() << " s after an expectation failed\n";
						std::_Exit(EXIT_FAILURE);
					} }.detach();
				} catch (...) {
					// Without the thread the failure still unwinds; only the bound is lost.
				}
			}

			std::thread::id Thread = std::this_thread::get_id();
#if !defined(_WIN32)
			pid_t Process = ::getpid();
#endif
			std::atomic<unsigned> ExitScopes{ 0U };
			std::atomic<bool>     Failed{ false };
			std::atomic<bool>     Reported{ false };
			std::atomic<bool>     Armed{ false };
		};

		inline failure_state State;
	} // namespace detail

	/// \brief Ends a failure whose `FAILED:` line the caller has already printed.
	/// \details Throws `expectation_failed` where unwinding is safe; otherwise
	/// ends the process with `std::exit(EXIT_FAILURE)`, as before.
	[[noreturn]] inline void fail()
	{
		auto& state = detail::State;
		state.Failed.store(true);
		if (!state.may_unwind()) {
			state.Reported.store(true);
			std::exit(EXIT_FAILURE);
		}
		state.arm_unwind_bound();
		throw expectation_failed{ };
	}

	/// \brief Prints `FAILED: <message>` and ends the failure.
	[[noreturn]] inline void fail(const std::string_view aMessage)
	{
		std::cerr << "FAILED: " << aMessage << '\n';
		fail();
	}

	/// \brief Fails with `aMessage` unless `aCondition` holds.
	inline void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			fail(aMessage);
		}
	}

	/// \brief While it lives on the thread that runs `main`, a failure ends the
	/// process with `std::exit` instead of unwinding.
	/// \details For a destructor, a Qt callback, and a window in which unwinding
	/// would destroy a joinable thread or join one that is waiting on the course.
	class exit_on_failure final
	{
	public:
		exit_on_failure() noexcept { ++detail::State.ExitScopes; }
		~exit_on_failure() { end(); }
		exit_on_failure(const exit_on_failure&)                    = delete;
		auto operator=(const exit_on_failure&) -> exit_on_failure& = delete;

		/// \brief Ends the scope before its owner goes, once the hazard has passed.
		void end() noexcept
		{
			if (Active) {
				Active = false;
				--detail::State.ExitScopes;
			}
		}

	private:
		bool Active = true;
	};

	/// \brief Removes a path the course made, with everything under it, unless it
	/// is released.
	/// \details No destructor runs for an object whose constructor throws. So a
	/// scratch root's owner holds one of these from the moment the root exists
	/// until its own construction is complete, and then releases it. A root that
	/// a course makes without an owner holds one for its whole life.
	class scratch_guard final
	{
	public:
		explicit scratch_guard(std::filesystem::path aPath) noexcept
			: Path{ std::move(aPath) }
		{
		}
		~scratch_guard()
		{
			if (!Path.empty()) {
				std::error_code ignored;
				std::filesystem::remove_all(Path, ignored);
			}
		}
		scratch_guard(const scratch_guard&)                    = delete;
		auto operator=(const scratch_guard&) -> scratch_guard& = delete;

		/// \brief Leaves the path to its owner, which removes it from now on.
		void release() noexcept { Path.clear(); }

	private:
		std::filesystem::path Path;
	};

	/// \brief The exit code for the exception that reached `main`'s handler.
	/// \details Call it only from that handler. An expectation's line is already
	/// out; any other exception is named here, since before it would have ended in
	/// `std::terminate`.
	[[nodiscard]] inline auto failing_exit() -> int
	{
		detail::State.Reported.store(true);
		try {
			throw;
		} catch (const expectation_failed&) {
			// Its FAILED line is already out.
		} catch (const std::exception& aError) {
			std::cerr << "FAILED: unexpected exception: " << aError.what() << '\n';
		} catch (...) {
			std::cerr << "FAILED: unexpected exception\n";
		}
		return EXIT_FAILURE;
	}

#if !defined(_WIN32)
	/// \brief Runs a forked child's work and ends the child there, however the
	/// work ends.
	/// \details A forked child's stack is a copy of its parent's. An exception
	/// that escaped the work would unwind into the parent's frames and run the
	/// parent's destructors in the child, including the one that removes the
	/// shared root. A failed expectation in a child already exits (see `fail`);
	/// this ends any other exception at the child's boundary, with its
	/// `FAILED:` line. The child ends with `_exit`, as the courses' children
	/// always did, so it flushes none of the parent's buffered output again.
	/// \param aWork Returns the child's exit code, or nothing for success.
	template <typename TWork>
	[[noreturn]] void end_forked_child(TWork&& aWork) noexcept
	{
		auto code = EXIT_SUCCESS;
		try {
			if constexpr (std::is_void_v<std::invoke_result_t<TWork>>) {
				std::forward<TWork>(aWork)();
			} else {
				code = std::forward<TWork>(aWork)();
			}
		} catch (...) {
			code = failing_exit();
		}
		::_exit(code);
	}
#endif
} // namespace bpr::tests

#endif // BPR_TESTS_SUPPORT_TEST_EXPECTATION_HPP
