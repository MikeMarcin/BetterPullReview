// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
enum class Reason { ID, DNA };
void brace() {
	int Reason{ 0 };
	auto hidden = Reason::ID;
}
void assigned() {
	auto Reason = 0;
	auto hidden = Reason::ID;
}
auto visible = Reason::DNA;
