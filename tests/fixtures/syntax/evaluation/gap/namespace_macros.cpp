// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
#define BEGIN_NAMESPACE(name) namespace name {
#define END_NAMESPACE }
BEGIN_NAMESPACE(Engine)
enum class Phase { Load, Run };
END_NAMESPACE
QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE
auto phase = Engine::Phase::Run;
