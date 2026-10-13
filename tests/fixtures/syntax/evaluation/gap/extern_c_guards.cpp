// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
#ifdef __cplusplus
extern "C" {
#endif
enum Flags { FLAG_NONE = 0, FLAG_READ = 1 };
int api_call(int value);
#ifdef __cplusplus
}
#endif
namespace After { enum Tail { END }; }
auto tail = After::Tail::END;
auto flag = Flags::FLAG_READ;
