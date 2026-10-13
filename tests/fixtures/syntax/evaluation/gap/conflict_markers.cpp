// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
namespace Merge {
<<<<<<< HEAD
enum class Side { Ours, Both };
=======
enum class Side { Theirs, Both };
>>>>>>> feature
}
auto side = Merge::Side::Both;
namespace Kept { enum class Mark { Done }; }
auto mark = Kept::Mark::Done;
