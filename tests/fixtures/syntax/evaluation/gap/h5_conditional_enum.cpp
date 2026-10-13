// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
#if defined(MODERN)
enum class State : int {
#else
enum State {
#endif
	Idle,
	Busy
};
struct Pair { int first, second; };
void use(int left, int right);
int a, b, c;
