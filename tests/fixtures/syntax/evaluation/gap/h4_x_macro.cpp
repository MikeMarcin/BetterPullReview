// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
#define COLOUR_LIST(X) X(RED) X(GREEN)
#define ENTRY(name) name,
enum Colour { COLOUR_LIST(ENTRY) };
auto colour = Colour::RED;
