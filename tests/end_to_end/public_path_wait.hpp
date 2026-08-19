#pragma once

/// \file
/// \brief One polling wait for the end-to-end public paths, that says what it saw.
///
/// \details [[BPR-0344]] cost a week of elapsed investigation, three hosts'
/// worth of filesystem comparison, and a temporary instrumented build to
/// establish a fact the failing process already had in hand: the queue had zero
/// rows. The assertion that failed was an eight-term conjunction that printed one
/// string on timeout — the name of the question, never the answer. Nothing
/// distinguished "no row appeared" from "the row appeared with the wrong bucket",
/// and those have completely different causes.
///
/// So a predicate here is written as a recorded conjunction:
///
/// ```cpp
/// wait_until(terms, [&](wait_terms& aTerms) {
///     return aTerms.require("remove-enabled", action->isEnabled())
///         && aTerms.require("row-count-is-one", model->rowCount() == 1);
/// })
/// ```
///
/// `&&` short-circuits, so the last term recorded is the first false one and
/// everything before it was true. A newly written wait therefore reports its
/// failing conjunct because of how it is written, not because someone remembered
/// to add a dump — which is the difference between this and the three
/// independently hand-rolled diagnostics it replaces.
///
/// On timeout the predicate is evaluated once more against a fresh recorder, so
/// the report describes the state when waiting stopped rather than some earlier
/// poll.
///
/// ## What may be reported
///
/// Booleans, integers, and names drawn from fixed vocabularies go through
/// `note`. Anything identifier-shaped goes through `note_identifier`, which caps
/// its length and replaces every byte outside printable ASCII. There is
/// deliberately **no** free-text channel: repository content — file text, pull
/// request bodies, filesystem paths — has nowhere to go. `tests/AGENTS.md`
/// requires that fixtures never carry private repository content, and BPR-0344's
/// own evidence contains a real `/home/<user>/…` workspace path, so this holds by
/// construction rather than by the fixtures happening to be synthetic.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace bpr::e2e {
	/// \brief Records the terms of one waited-on condition and what they saw.
	class wait_terms final
	{
	public:
		/// The most terms one report will carry. A conjunction longer than this
		/// is a predicate that should have been split; the excess is reported as
		/// elided rather than dropped silently.
		static constexpr std::size_t maximum_terms = 24U;

		/// The most bytes one report will render. A 20-second poll must not
		/// become four thousand lines.
		static constexpr std::size_t maximum_bytes = 2048U;

		/// The most bytes one identifier may contribute.
		static constexpr std::size_t maximum_identifier_bytes = 96U;

		/// \brief Records one term of the conjunction and returns its value.
		///
		/// \details Returns `aValue` so that a predicate reads as an ordinary
		/// `&&` chain. Because `&&` short-circuits, the last recorded term of a
		/// failing predicate is the first false one.
		auto require(std::string_view aName, bool aValue) -> bool
		{
			append(aName, aValue ? "true" : "false");
			if (!aValue && FailingTerm.empty()) {
				// Sanitized here too. A term's name is caller-supplied, and this
				// copy is rendered separately from the one `append` bounded, so
				// leaving it raw would let a newline through the front of the
				// report -- which is how this line came to exist.
				FailingTerm = sanitized(aName);
			}
			return aValue;
		}

		/// \brief Records an observed boolean that is not itself a term.
		void note(std::string_view aName, bool aValue)
		{
			append(aName, aValue ? "true" : "false");
		}

		/// \brief Records an observed count.
		void note(std::string_view aName, std::uint64_t aValue)
		{
			append(aName, std::to_string(aValue));
		}

		/// \brief Records an observed count that may be negative.
		void note(std::string_view aName, std::int64_t aValue)
		{
			append(aName, std::to_string(aValue));
		}

		/// \brief Records a name from a fixed vocabulary, such as an enumerator.
		///
		/// \details For `core::name(...)` results and the like. Sanitized anyway:
		/// a vocabulary is only fixed until someone widens it.
		void note_name(std::string_view aName, std::string_view aValue)
		{
			append(aName, sanitized(aValue));
		}

		/// \brief Records an identifier, bounded and sanitized.
		///
		/// \details The one channel that accepts caller-shaped text, and the only
		/// reason it exists is that a failing queue row's ref is the single most
		/// useful thing to know. Repository *content* must not come through here.
		void note_identifier(std::string_view aName, std::string_view aValue)
		{
			append(aName, sanitized(aValue));
		}

		/// \brief The sanitized name of the first false term, empty if none was.
		[[nodiscard]] auto failing_term() const noexcept -> std::string_view
		{
			return FailingTerm;
		}

		/// \brief The rendered report: the failing term first, then every value.
		[[nodiscard]] auto render() const -> std::string
		{
			auto out = std::string{ };
			if (!FailingTerm.empty()) {
				out += "failed=";
				out += FailingTerm;
				out += ' ';
			}
			out += Rendered;
			if (Elided != 0U) {
				out += " elided=";
				out += std::to_string(Elided);
			}
			return out;
		}

	private:
		void append(std::string_view aName, std::string_view aValue)
		{
			if (Recorded >= maximum_terms) {
				++Elided;
				return;
			}
			const auto addition = aName.size() + aValue.size() + 2U;
			if (Rendered.size() + addition > maximum_bytes) {
				++Elided;
				return;
			}
			if (!Rendered.empty()) {
				Rendered += ' ';
			}
			Rendered += sanitized(aName);
			Rendered += '=';
			Rendered += aValue;
			++Recorded;
		}

		/// \brief Printable ASCII, bounded, with every other byte replaced.
		[[nodiscard]] static auto sanitized(std::string_view aValue) -> std::string
		{
			auto       out = std::string{ };
			const auto cap = aValue.size() > maximum_identifier_bytes
				? maximum_identifier_bytes
				: aValue.size();
			out.reserve(cap);
			for (std::size_t index = 0U; index < cap; ++index) {
				const auto byte = static_cast<unsigned char>(aValue[index]);
				// Space included so a two-word enumerator stays readable; '='
				// excluded so a value can never look like another term.
				out += (byte >= 0x20U && byte < 0x7FU && byte != '=')
					? static_cast<char>(byte)
					: '?';
			}
			if (aValue.size() > cap) {
				out += "...";
			}
			return out;
		}

		std::string Rendered;
		std::string FailingTerm;
		std::size_t Recorded = 0U;
		std::size_t Elided   = 0U;
	};
} // namespace bpr::e2e
