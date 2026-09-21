#ifndef BPR_SRC_APP_SRC_INSTALLED_RUNTIME_HPP
#define BPR_SRC_APP_SRC_INSTALLED_RUNTIME_HPP

namespace bpr::app {
	// Before QApplication can load a platform plugin. Development graphs keep
	// their configured Qt provider; distribution graphs have no fallback.
	[[nodiscard]] auto restrict_installed_runtime(int aArgc, char** aArgv) -> bool;
}

#endif
