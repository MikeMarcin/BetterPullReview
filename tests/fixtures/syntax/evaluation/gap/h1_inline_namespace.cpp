// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
namespace A::inline B {
enum class Mode { Fast, Slow };
}
auto mode = A::B::Mode::Fast;
