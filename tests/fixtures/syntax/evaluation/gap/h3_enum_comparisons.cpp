// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
constexpr int X = 1;
constexpr int Y = 4;
enum Limits { Low = X < 3, High = Y > 2, Last };
auto limit = Limits::High;
