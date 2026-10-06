#ifndef BPR_PLATFORM_WINDOWS_STANDARD_HANDLES_HPP
#define BPR_PLATFORM_WINDOWS_STANDARD_HANDLES_HPP

// [[BPR-1115]]. The one Windows launch restriction the adapters that start
// programs through Qt's process class share: a child inherits its three
// standard handles and nothing else.
//
// Qt creates every child with handle inheritance on and no handle list, so the
// child inherits every inheritable handle this process holds at that instant
// and keeps it until it exits. A C runtime stream (`std::ifstream`) is
// inheritable by default. [[BPR-1104]] found a Git child holding another
// operation's pack spool that way, and Windows then refused the spool's
// exclusive open and its removal for as long as that Git ran. This is the
// Windows counterpart of the `CloseFileDescriptors` flag every Unix launch
// sets. A guarded Git launch and the agent supervisor build their own lists in
// their own sources.
//
// A launch whose list cannot be built is refused by process creation itself,
// so nothing is created: no program runs, not even one with no handles.
//
// [[BPR-1265]]. A launcher that keeps its child in a kill-on-close job names
// the job before the launch, and process creation puts the child in it. Opening
// the started child by its process id and assigning it afterwards raced the
// child: a Git command that finished first could no longer be assigned
// (`ERROR_ACCESS_DENIED`), and ran, however briefly, outside the job. With the
// job in the list, a child that cannot join it is never created.
//
// Header-only and free of Qt. An adapter passes the three fields of its own
// process-creation arguments; this decides nothing about what runs, with which
// arguments, environment or directory.

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>

	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <vector>

namespace bpr::platform::windows_standard_handles {
	/// \brief Creation flags that process creation refuses outright.
	///
	/// \details A new console and no console exclude each other, so
	/// `CreateProcessW` fails a call that asks for both with
	/// `ERROR_INVALID_PARAMETER` before it opens the program, and creates no
	/// process.
	inline constexpr DWORD refused_creation_flags = DETACHED_PROCESS | CREATE_NEW_CONSOLE;

	/// \brief Limits one child process to the three standard handles it is
	/// created with.
	///
	/// \details The object owns the extended startup block and the attribute
	/// list that `apply` points the call at, so it must outlive the process
	/// creation it changes. One object serves one launch.
	class launch_restriction final
	{
	public:
		launch_restriction() noexcept                                    = default;
		launch_restriction(const launch_restriction&)                    = delete;
		auto operator=(const launch_restriction&) -> launch_restriction& = delete;
		launch_restriction(launch_restriction&&)                         = delete;
		auto operator=(launch_restriction&&) -> launch_restriction&      = delete;
		~launch_restriction()
		{
			if (Attributes != nullptr) {
				::DeleteProcThreadAttributeList(Attributes);
			}
		}

		/// \brief Restricts one process creation to the standard handles it
		/// names.
		///
		/// \details On success \p aStartup points at this object's extended
		/// startup block, whose handle list names exactly the standard input,
		/// output and error handles of the original; \p aFlags carries
		/// `EXTENDED_STARTUPINFO_PRESENT` and \p aInherit is true. The child
		/// then inherits those handles and nothing else.
		///
		/// When a standard handle is null or invalid, or the list cannot be
		/// built, or the object has already been applied, \p aStartup is left
		/// as it was, \p aInherit is false and \p aFlags carries
		/// `refused_creation_flags`: process creation refuses the call and
		/// nothing starts.
		void apply(STARTUPINFOW*& aStartup, DWORD& aFlags, bool& aInherit) noexcept
		{
			const auto first = !Applied;
			Applied          = true;
			if (first && aStartup != nullptr && build(*aStartup)) {
				aStartup = &Startup.StartupInfo;
				aFlags |= EXTENDED_STARTUPINFO_PRESENT;
				aInherit   = true;
				Restricted = true;
				return;
			}
			aInherit = false;
			aFlags |= refused_creation_flags;
		}

		/// \brief Names the job the child is created in.
		///
		/// \details Called before `apply`. The handle must stay open until the
		/// process creation `apply` changes has returned. Process creation then
		/// adds the child to \p aJob as it creates it, or creates nothing. A null
		/// handle, or a call after `apply`, refuses the launch.
		void place_in_job(const HANDLE aJob) noexcept
		{
			JobRequested = true;
			if (!Applied && aJob != nullptr && aJob != INVALID_HANDLE_VALUE) {
				Job[0] = aJob;
			}
		}

		/// \brief Whether `apply` restricted a launch and put the child in the
		/// job `place_in_job` named.
		[[nodiscard]] auto placed_in_job() const noexcept -> bool
		{
			return Restricted && JobRequested && Job[0] != nullptr;
		}

		/// \brief Whether `apply` restricted a launch.
		///
		/// \details False before `apply` runs, and after a refusal. A launcher
		/// that finds a child started while this is false knows the child was
		/// created without the restriction, and must not let it run.
		[[nodiscard]] auto restricted() const noexcept -> bool
		{
			return Restricted;
		}

	private:
		[[nodiscard]] auto build(const STARTUPINFOW& aOriginal) noexcept -> bool
		{
			auto count = std::size_t{ 0U };
			for (const auto handle : { aOriginal.hStdInput, aOriginal.hStdOutput, aOriginal.hStdError }) {
				if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
					return false;
				}
				// A list that names one handle twice fails process creation
				// ([[BPR-0622]]), and standard output and error may share one.
				const auto named = Inherited.begin() + static_cast<std::ptrdiff_t>(count);
				if (std::find(Inherited.begin(), named, handle) == named) {
					Inherited[count] = handle;
					++count;
				}
			}
			if (JobRequested && Job[0] == nullptr) {
				return false;
			}
			const auto attributes = JobRequested ? DWORD{ 2U } : DWORD{ 1U };
			auto       size       = SIZE_T{ 0U };
			static_cast<void>(::InitializeProcThreadAttributeList(nullptr, attributes, 0U, &size));
			if (size == 0U) {
				return false;
			}
			try {
				AttributeBuffer.assign(size, std::byte{ 0U });
			} catch (...) {
				return false;
			}
			auto* const list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(AttributeBuffer.data());
			if (::InitializeProcThreadAttributeList(list, attributes, 0U, &size) == FALSE) {
				return false;
			}
			Attributes = list;
			// The list keeps a pointer to the handles, which live in this object.
			if (::UpdateProcThreadAttribute(Attributes, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, static_cast<void*>(Inherited.data()), count * sizeof(HANDLE), nullptr, nullptr) == FALSE) {
				return false;
			}
			// The list keeps a pointer to the job handle too, also in this object.
			if (JobRequested && ::UpdateProcThreadAttribute(Attributes, 0U, PROC_THREAD_ATTRIBUTE_JOB_LIST, static_cast<void*>(Job.data()), sizeof(HANDLE), nullptr, nullptr) == FALSE) {
				return false;
			}
			Startup                 = STARTUPINFOEXW{ };
			Startup.StartupInfo     = aOriginal;
			Startup.StartupInfo.cb  = sizeof(Startup);
			Startup.lpAttributeList = Attributes;
			return true;
		}

		std::vector<std::byte>       AttributeBuffer;
		LPPROC_THREAD_ATTRIBUTE_LIST Attributes = nullptr;
		std::array<HANDLE, 3U>       Inherited{ };
		std::array<HANDLE, 1U>       Job{ };
		STARTUPINFOEXW               Startup{ };
		bool                         Applied      = false;
		bool                         Restricted   = false;
		bool                         JobRequested = false;
	};
} // namespace bpr::platform::windows_standard_handles
#endif // defined(_WIN32)

#endif // BPR_PLATFORM_WINDOWS_STANDARD_HANDLES_HPP
