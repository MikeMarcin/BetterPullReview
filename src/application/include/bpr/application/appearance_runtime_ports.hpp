#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_RUNTIME_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_RUNTIME_PORTS_HPP

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/syntax_classification.hpp"
#include "bpr/core/theme_exchange.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t maximum_appearance_outstanding_requests = 64U;
	inline constexpr std::size_t maximum_token_span_cache_entries        = 4'096U;
	inline constexpr std::size_t maximum_token_spans_per_blob            = 4'194'304U;
	inline constexpr std::size_t maximum_syntax_definition_bytes         = 128U;

	/// \brief Describes the appearance host limits record.
	///
	/// \details `OutstandingRequests` bounds concurrently submitted effect work.
	/// `TokenSpanCacheEntries` bounds how many classified blobs the host retains,
	/// and `TokenSpansPerBlob` bounds one classification, so neither a long review
	/// nor a pathological blob can make the cache unbounded.
	struct appearance_host_limits
	{
		std::size_t OutstandingRequests   = 8U;
		std::size_t TokenSpanCacheEntries = 64U;
		std::size_t TokenSpansPerBlob     = 262'144U;

		auto operator<=>(const appearance_host_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const appearance_host_limits& aLimits) noexcept -> bool
	{
		return aLimits.OutstandingRequests > 0U
			&& aLimits.OutstandingRequests <= maximum_appearance_outstanding_requests
			&& aLimits.TokenSpanCacheEntries > 0U
			&& aLimits.TokenSpanCacheEntries <= maximum_token_span_cache_entries
			&& aLimits.TokenSpansPerBlob > 0U
			&& aLimits.TokenSpansPerBlob <= maximum_token_spans_per_blob;
	}

	/// \brief Describes the appearance request context record.
	///
	/// \details `Request` is unique for the host lifetime and is the exactly-once
	/// terminal key: a second delivery of the same request finds no outstanding
	/// record. `Session` names the host that issued it, so a terminal from a
	/// foreign host cannot be mistaken for a local one. There is deliberately no
	/// generation here. Appearance staleness is per domain and is stamped on the
	/// outstanding record when a later gesture supersedes it, because a global
	/// generation would make a settings change retire an unrelated in-flight
	/// classification, which is exactly the reparse this host exists to avoid. A
	/// locally decided change carries a zero `Request`, because no effect produced
	/// it.
	struct appearance_request_context
	{
		core::request_id             Request;
		core::application_session_id Session;

		auto operator<=>(const appearance_request_context&) const = default;
	};

	/// \brief Returns the palette-table index of the supplied variant.
	[[nodiscard]] constexpr auto variant_index(
		const core::appearance_variant aVariant) noexcept -> std::size_t
	{
		return static_cast<std::size_t>(aVariant);
	}

	/// \brief Checks whether the supplied variant is inside the closed vocabulary.
	///
	/// \details Against the core's published count rather than a restated member
	/// list, for the same reason the mode check below is.
	[[nodiscard]] constexpr auto known(
		const core::appearance_variant aVariant) noexcept -> bool
	{
		return variant_index(aVariant) < core::appearance_variant_count;
	}

	/// \brief Describes one variant's stored colour customization.
	///
	/// \details `Stored` is the load-bearing half, and the reason this is a pair
	/// rather than a bare palette. A composed default and a stored palette are
	/// different facts, and a record that could not tell them apart is BPR-0216:
	/// the default palette is built under an empty observation, so on a dark
	/// desktop it differs from the built-in dark palette in every role and was
	/// read as forty-six deliberate overrides. An explicit palette reset persists
	/// `false`, preserving the absence of overrides across restart — per variant,
	/// so resetting Dark cannot forget that Light was customized.
	///
	/// `Palette` is a total palette over the core's closed role vocabulary rather
	/// than a sparse override set, because that is what a theme document is and
	/// the store round-trips it through `core::import_theme`, which refuses a
	/// document that fails to assign every role. Which of those roles count as
	/// customization is decided by comparing against the built-in profile of this
	/// variant, and the built-in profiles are presentation's.
	struct appearance_palette_customisation
	{
		core::appearance_palette Palette{ };
		bool                     Stored = false;

		auto operator<=>(const appearance_palette_customisation&) const = default;
	};

	/// \brief One stored customization per resolved variant, indexed by variant.
	///
	/// \details RFC-BPR-0034. A palette is one total set of colours for exactly
	/// one variant, so a record holding a single palette could describe only one
	/// theme: tuning Dark silently forgot Light. There is deliberately no
	/// "current" or "last committed" slot beside this table — that would be a
	/// second authority on the same fact, and the earlier single-palette record
	/// needed `PaletteVariant` precisely because it had no place to say which
	/// theme it meant. Index with `variant_index`.
	using appearance_palette_customisations
		= std::array<appearance_palette_customisation, core::appearance_variant_count>;

	/// \brief Returns one variant's customization from a bare table.
	///
	/// \details The published presentation invalidation carries the table rather
	/// than a whole record, so a consumer reading it needs the same guarded
	/// accessor a record reader gets. A variant outside the closed vocabulary
	/// cannot index the table, so it answers the first slot rather than reading
	/// past the end; every caller that can act on such a value refuses it first,
	/// which keeps the accessor itself total.
	[[nodiscard]] constexpr auto customisation_of(
		const appearance_palette_customisations& aPalettes,
		const core::appearance_variant           aVariant) noexcept
		-> const appearance_palette_customisation&
	{
		return aPalettes[known(aVariant) ? variant_index(aVariant) : 0U];
	}

	/// \brief The four built-in profile palettes, one per variant.
	///
	/// \details [[BPR-1479]]. Supplied by the composition, because the built-in
	/// colours belong to presentation and the host invents none. The host reads
	/// them for one thing: filling the roles an older theme document omits
	/// from the built-in palette of that document's base before the import is
	/// checked and previewed. Each must be total and assign every role. Index
	/// with `variant_index`.
	using appearance_builtin_palettes
		= std::array<core::appearance_palette, core::appearance_variant_count>;

	/// \brief Describes the durable appearance settings record.
	///
	/// \details This is the whole durable appearance state: the selected mode, one
	/// colour customization per variant, both fonts, and Dif preferences. A fresh
	/// installation has no durable record, the mode defaults to `system`, and no
	/// variant carries a customization. The host chooses no colour and no family:
	/// the typography a host starts from is supplied by its composition, the
	/// built-in colours belong to presentation, and every other value the record
	/// holds was either loaded or applied by a gesture.
	struct appearance_settings
	{
		core::appearance_mode             Mode = core::appearance_mode::system;
		appearance_palette_customisations Palettes;
		core::typography_settings         Typography;
		core::dif_preferences             Dif;

		auto operator<=>(const appearance_settings&) const = default;
	};

	/// \brief Returns one variant's stored customization.
	[[nodiscard]] constexpr auto customisation_of(
		const appearance_settings&     aSettings,
		const core::appearance_variant aVariant) noexcept
		-> const appearance_palette_customisation&
	{
		return customisation_of(aSettings.Palettes, aVariant);
	}

	/// \brief Returns one variant's stored customization for modification.
	[[nodiscard]] constexpr auto customisation_of(
		appearance_settings&           aSettings,
		const core::appearance_variant aVariant) noexcept
		-> appearance_palette_customisation&
	{
		return aSettings.Palettes[known(aVariant) ? variant_index(aVariant) : 0U];
	}

	/// \brief Checks whether the supplied mode is inside the closed vocabulary.
	///
	/// \details A durable record or a caller can carry a value that was never a
	/// declared mode. The check is against the core's published count rather than
	/// a restated member list, so it cannot drift from the vocabulary.
	[[nodiscard]] constexpr auto known(const core::appearance_mode aMode) noexcept
		-> bool
	{
		return static_cast<std::size_t>(aMode) < core::appearance_mode_count;
	}

	enum class appearance_settings_failure : std::uint8_t {
		unavailable,
		cancelled,
	};

	/// \brief Describes the appearance settings error record.
	struct appearance_settings_error
	{
		appearance_settings_failure Failure = appearance_settings_failure::unavailable;

		auto operator<=>(const appearance_settings_error&) const = default;
	};

	template <typename Value>
	using appearance_settings_result = std::expected<Value, appearance_settings_error>;

	/// \brief Provides the appearance settings port contract.
	///
	/// \details The host calls this on the effect executor, never on the decision
	/// lane, and marshals the answer back as a typed terminal. An absent durable
	/// record is not a failure and is not a defaulted record either: `load` reports
	/// no value, and the host keeps the settings its composition started from. That
	/// distinction is what lets a fresh installation be told apart from one that
	/// deliberately stored the defaults.
	class appearance_settings_port
	{
	public:
		virtual ~appearance_settings_port() = default;

		[[nodiscard]] virtual auto load(std::stop_token aStop)
			-> appearance_settings_result<std::optional<appearance_settings>> = 0;
		[[nodiscard]] virtual auto store(
			const appearance_settings& aSettings,
			std::stop_token            aStop)
			-> appearance_settings_result<void> = 0;
	};

	enum class theme_document_failure : std::uint8_t {
		unavailable,
		cancelled,
	};

	/// \brief Describes a bounded local theme-document operation failure.
	struct theme_document_error
	{
		theme_document_failure Failure = theme_document_failure::unavailable;

		auto operator<=>(const theme_document_error&) const = default;
	};

	template <typename Value>
	using theme_document_result = std::expected<Value, theme_document_error>;

	/// \brief Exchanges local theme documents for the appearance host.
	///
	/// \details The composition owns the filesystem effect. Qt selects a path but
	/// never opens it. `MaximumBytes` is inclusive of the one overflow byte the
	/// core parser needs in order to return its typed `document_too_large`
	/// rejection rather than accepting a truncated document.
	class theme_document_port
	{
	public:
		virtual ~theme_document_port() = default;

		[[nodiscard]] virtual auto read(
			std::string_view aPath,
			std::size_t      aMaximumBytes,
			std::stop_token  aStop) -> theme_document_result<std::string> = 0;

		/// \brief Atomically replaces one bounded theme document on the effect executor.
		[[nodiscard]] virtual auto write(
			std::string_view, std::string_view, std::stop_token)
			-> theme_document_result<void>
		{
			return std::unexpected{ theme_document_error{ } };
		}
	};

	/// \brief Describes one syntax definition and the revision that produced it.
	///
	/// \details `Name` is the stable definition identifier and `Version` the
	/// revision of that definition's rules. Both belong to the cache key because a
	/// definition update changes the classification of unchanged bytes, and nothing
	/// else about the blob would say so.
	struct syntax_definition_version
	{
		std::string   Name;
		std::uint32_t Version = 0U;

		auto operator<=>(const syntax_definition_version&) const = default;
	};

	/// \brief Identifies one cached classification.
	///
	/// \details The key is the blob identity and the definition version, and
	/// nothing else. It deliberately carries no mode, palette, or typography: a
	/// theme or font change alters how spans are drawn, never which spans the
	/// bytes produce, so a colour that entered this key would force a reparse that
	/// no rule requires.
	struct token_span_cache_key
	{
		core::git_blob_oid        Blob;
		syntax_definition_version Definition;

		auto operator<=>(const token_span_cache_key&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const token_span_cache_key& aKey) noexcept
		-> bool
	{
		return aKey.Blob.valid() && !aKey.Definition.Name.empty()
			&& aKey.Definition.Name.size() <= maximum_syntax_definition_bytes;
	}

	using token_span      = core::token_span;
	using token_span_list = core::token_span_list;
	// A retained classification is immutable and shared. Presentation may hold it
	// for its projection lifetime; it cannot mutate it or become its authority.
	using shared_token_spans = std::shared_ptr<const token_span_list>;

	/// \brief Describes the syntax classification request record.
	///
	/// \details `MaximumSpans` is the bound the host will accept. A port that
	/// cannot answer within it must report `limit_exceeded` rather than truncate,
	/// because a truncated span list would silently unclassify the tail of a blob.
	struct syntax_classification_request
	{
		appearance_request_context Context;
		token_span_cache_key       Key;
		std::size_t                MaximumSpans = 0U;

		auto operator<=>(const syntax_classification_request&) const = default;
	};

	using syntax_classification_status = core::syntax_classification_status;

	/// \brief Describes the syntax classification record.
	struct syntax_classification
	{
		appearance_request_context   Context;
		token_span_cache_key         Key;
		syntax_classification_status Status = syntax_classification_status::unavailable;
		token_span_list              Spans;

		auto operator<=>(const syntax_classification&) const = default;
	};

	/// \brief Provides the syntax classification port contract.
	///
	/// \details The host calls this on the effect executor, never on the decision
	/// lane. An implementation classifies the complete blob the key names, honours
	/// the supplied stop token, and repeats the request context and key in its
	/// answer. The host reads no blob bytes and holds no lexical rule; it decides
	/// only whether the port has to run at all.
	class syntax_classification_port
	{
	public:
		virtual ~syntax_classification_port() = default;

		[[nodiscard]] virtual auto classify(
			const syntax_classification_request& aRequest,
			std::stop_token                      aStop) -> syntax_classification = 0;
	};

	struct appearance_epoch_tag;
	using appearance_epoch = core::strong_id<appearance_epoch_tag>;

	/// \brief Describes the bounded presentation invalidation record.
	///
	/// \details An engaged member is a presentation domain that must be rebuilt and
	/// carries the value to rebuild it from; an absent member is a domain
	/// presentation may keep. `Epoch` advances once per adopted appearance change
	/// so presentation can drop derived state it stamped with an earlier epoch.
	///
	/// The record names presentation work only. It carries no review state, no file
	/// identity, and no token span, because an appearance change cannot alter
	/// review semantics and must not be able to express one.
	///
	/// `Palettes` is the whole per-variant table rather than the one variant that
	/// moved. A commit changes one variant and a durable restore changes all four,
	/// so a member naming a single variant would have to be absent exactly when
	/// the most changed. Presentation also needs the other variants' customizations
	/// whether or not they moved: it draws a tile per theme and captions how many
	/// colours each one customizes.
	struct appearance_presentation_invalidation
	{
		appearance_epoch                                 Epoch;
		std::optional<core::appearance_resolution>       Resolution;
		std::optional<appearance_palette_customisations> Palettes;
		std::optional<core::typography_settings>         Typography;
		std::optional<core::dif_resolution>              Dif;

		auto operator<=>(const appearance_presentation_invalidation&) const = default;
	};

	/// \brief Checks whether the supplied invalidation names no changed domain.
	[[nodiscard]] inline auto empty(
		const appearance_presentation_invalidation& aInvalidation) noexcept -> bool
	{
		return !aInvalidation.Resolution.has_value()
			&& !aInvalidation.Palettes.has_value()
			&& !aInvalidation.Typography.has_value() && !aInvalidation.Dif.has_value();
	}

	/// \brief Describes one published classification.
	///
	/// \details `CacheHit` reports whether the host answered from its retained
	/// cache. It is the published half of the same fact the host's counters
	/// report, so a consumer can tell a reparse from a reuse without instrumenting
	/// the classifier.
	struct token_span_publication
	{
		token_span_cache_key Key;
		shared_token_spans   Spans;
		bool                 CacheHit = false;
	};

	/// \brief Preserves the variant alongside one core contrast finding.
	struct appearance_contrast_rejection
	{
		core::appearance_variant Variant = core::appearance_variant::light;
		core::contrast_finding   Finding;

		auto operator<=>(const appearance_contrast_rejection&) const = default;
	};

	/// \brief Publishes one imported, validated total palette for preview only.
	struct theme_preview_publication
	{
		core::appearance_palette Palette{ };

		auto operator<=>(const theme_preview_publication&) const = default;
	};

	/// \brief Acknowledges one host-accepted palette commit intent.
	///
	/// \details This is distinct from presentation invalidation. A late durable
	/// restore can invalidate the palette without accepting a preview, while a
	/// commit equal to the already-stored palette accepts the preview without
	/// invalidating pixels or writing the same record again.
	///
	/// `Variant` names the theme the acknowledged palette belongs to, because a
	/// commit changes exactly one of them. Every host publication sets it from the
	/// variant the accepted intent was scoped to; the declared default exists only
	/// so the record can be aggregate-initialized.
	struct palette_adoption_publication
	{
		core::appearance_palette Palette{ };
		core::appearance_variant Variant = core::appearance_variant::light;

		auto operator<=>(const palette_adoption_publication&) const = default;
	};

	/// \brief Describes the appearance projection record.
	///
	/// \details Every presentation domain is engaged only when its published
	/// meaning changed, so a bounded change never forces presentation to rebuild
	/// the whole appearance. A palette-adoption acknowledgement may stand alone:
	/// it terminates an equal commit without inventing a pixel change.
	struct appearance_projection
	{
		appearance_request_context                          Context;
		std::optional<appearance_presentation_invalidation> Presentation;
		std::optional<token_span_publication>               Spans;
		std::optional<theme_preview_publication>            ThemePreview;
		std::optional<palette_adoption_publication>         PaletteAdoption;
		bool                                                ThemeExported = false;
	};

	/// \brief Checks whether the supplied projection carries no changed domain.
	[[nodiscard]] inline auto empty(
		const appearance_projection& aProjection) noexcept -> bool
	{
		return !aProjection.Presentation.has_value()
			&& !aProjection.Spans.has_value()
			&& !aProjection.ThemePreview.has_value()
			&& !aProjection.PaletteAdoption.has_value() && !aProjection.ThemeExported;
	}

	enum class appearance_host_failure : std::uint8_t {
		settings_unavailable,
		settings_rejected,
		theme_document_unavailable,
		theme_import_rejected,
		theme_export_unavailable,
		palette_contrast_rejected,
		unknown_appearance_mode,
		invalid_dif_preferences,
		classification_unavailable,
		classification_limit,
		invalid_classification_key,
		host_failure,
	};

	/// \brief Describes the appearance failure publication record.
	///
	/// \details `Rejections` is nonempty exactly for `settings_rejected` and
	/// repeats the core's own typed triples in the core's own order. The host
	/// neither summarises them nor substitutes a legal value for an illegal one:
	/// a clamped value would tell a settings surface that it was accepted.
	struct appearance_failure_publication
	{
		appearance_request_context                 Context;
		appearance_host_failure                    Failure = appearance_host_failure::host_failure;
		std::vector<core::typography_rejection>    Rejections;
		std::vector<core::theme_import_rejection>  ThemeRejections;
		std::vector<appearance_contrast_rejection> ContrastRejections;

		auto operator<=>(const appearance_failure_publication&) const = default;
	};

	enum class appearance_terminal_rejection : std::uint8_t {
		none,
		unknown_request,
		inexact_terminal,
		superseded,
		cancelled,
	};

	/// \brief Provides the appearance projection port contract.
	///
	/// \details Both operations run on the decision lane inside the host's
	/// exception boundary. A stale, superseded, cancelled, duplicate, or foreign
	/// terminal reaches neither operation.
	class appearance_projection_port
	{
	public:
		virtual ~appearance_projection_port() = default;

		virtual void publish(const appearance_projection& aProjection)    = 0;
		virtual void fail(const appearance_failure_publication& aFailure) = 0;
		/// \brief Reports only the typed rejection category for host diagnostics.
		virtual void rejected(appearance_terminal_rejection) { }
	};

	/// \brief Asks for one appearance mode.
	struct select_appearance_mode_intent
	{
		core::appearance_mode Mode = core::appearance_mode::system;

		auto operator<=>(const select_appearance_mode_intent&) const = default;
	};

	/// \brief Offers one complete typography record.
	///
	/// \details Complete rather than per-field on purpose. The core validates a
	/// record whole and refuses it whole, so an intent carrying one field would
	/// have to be merged with retained state somewhere before it could be
	/// judged — and whoever held that state would become a second authority on
	/// what the current typography is.
	struct apply_typography_intent
	{
		core::typography_settings Typography;

		auto operator<=>(const apply_typography_intent&) const = default;
	};

	/// \brief [[BPR-1336]]. Asks for one zoom step of the comparison's code.
	///
	/// \details One gesture rather than a record: the host steps from the
	/// record it holds, so repeated gestures cannot be computed from a stale
	/// copy and collapse into one.
	struct step_code_zoom_intent
	{
		core::code_zoom_step Step = core::code_zoom_step::actual_size;

		auto operator<=>(const step_code_zoom_intent&) const = default;
	};

	/// \brief Offers one total semantic palette for an immediate local preview.
	struct preview_appearance_palette_intent
	{
		core::appearance_palette Palette{ };

		auto operator<=>(const preview_appearance_palette_intent&) const = default;
	};

	/// \brief Commits one total semantic palette through the application host.
	struct commit_appearance_palette_intent
	{
		core::appearance_palette Palette{ };

		auto operator<=>(const commit_appearance_palette_intent&) const = default;
	};

	/// \brief Restores presentation to the last palette adopted by the host.
	struct cancel_appearance_palette_preview_intent
	{
		auto operator<=>(const cancel_appearance_palette_preview_intent&) const
			= default;
	};

	/// \brief Requests a bounded host-owned read of one local theme document.
	struct import_appearance_theme_intent
	{
		std::string Path;

		auto operator<=>(const import_appearance_theme_intent&) const = default;
	};

	/// \brief Identifies an independently resettable durable appearance group.
	enum class appearance_reset_scope : std::uint8_t { dif,
													   mode,
													   interface_font,
													   code_font,
													   palette };

	struct reset_appearance_intent
	{
		appearance_reset_scope Scope = appearance_reset_scope::mode;
	};

	struct export_appearance_theme_intent
	{
		std::string Path;
	};

	/// \brief Every gesture an appearance surface can express.
	///
	/// \details A closed set, so a surface cannot invent a way to change
	/// appearance that the host has no rule for. Preview is explicitly distinct
	/// from commit: the former stays in Qt presentation while the latter passes
	/// through host validation and persistence.
	/// \brief Offers profile-owned decorative preferences for host validation.
	struct apply_dif_preferences_intent
	{
		core::dif_preferences Preferences;
	};

	using appearance_intent = std::variant<apply_dif_preferences_intent, select_appearance_mode_intent,
										   apply_typography_intent,
										   step_code_zoom_intent,
										   preview_appearance_palette_intent,
										   commit_appearance_palette_intent,
										   cancel_appearance_palette_preview_intent,
										   import_appearance_theme_intent, reset_appearance_intent, export_appearance_theme_intent>;

	using appearance_intent_dispatch = std::function<void(appearance_intent)>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_APPEARANCE_RUNTIME_PORTS_HPP
