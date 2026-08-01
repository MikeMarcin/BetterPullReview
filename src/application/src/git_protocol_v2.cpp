#include "bpr/application/git_protocol_v2.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <new>
#include <string_view>

namespace bpr::application
{
namespace
{
enum class packet_kind : std::uint8_t
{
    data,
    flush,
    delimiter,
    response_end,
};

struct packet_view
{
    packet_kind Kind = packet_kind::data;
    std::string_view Payload;
};

struct decoded_packets
{
    std::array<packet_view, maximum_git_protocol_v2_control_packets> Packets{};
    std::size_t Count = 0U;
};

[[nodiscard]] constexpr auto protocol_error(
    const git_protocol_v2_failure failure) noexcept -> git_protocol_v2_error
{
    return {.Failure = failure};
}

[[nodiscard]] constexpr auto hexadecimal_nibble(
    const char character) noexcept -> std::optional<std::uint8_t>
{
    if(character >= '0' && character <= '9')
    {
        return static_cast<std::uint8_t>(character - '0');
    }
    if(character >= 'a' && character <= 'f')
    {
        return static_cast<std::uint8_t>(character - 'a' + 10);
    }
    if(character >= 'A' && character <= 'F')
    {
        return static_cast<std::uint8_t>(character - 'A' + 10);
    }
    return std::nullopt;
}

[[nodiscard]] constexpr auto decode_packet_length(
    const std::span<const std::byte, 4U> header) noexcept
    -> std::optional<std::uint16_t>
{
    std::uint16_t length = 0U;
    for(const auto byte : header)
    {
        const auto nibble =
            hexadecimal_nibble(static_cast<char>(std::to_integer<unsigned char>(
                byte)));
        if(!nibble.has_value())
        {
            return std::nullopt;
        }
        length = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(length << 4U) | *nibble);
    }
    return length;
}

[[nodiscard]] auto decode_control_packets(
    const std::span<const std::byte> response)
    -> git_protocol_v2_result<decoded_packets>
{
    if(
        response.empty()
        || response.size() > maximum_git_smart_control_response_bytes)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::response_limit));
    }

    decoded_packets decoded;
    std::size_t cursor = 0U;
    while(cursor < response.size())
    {
        if(
            decoded.Count >= decoded.Packets.size()
            || response.size() - cursor < 4U)
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }

        const auto header =
            std::span<const std::byte, 4U>{response.data() + cursor, 4U};
        const auto encoded_length = decode_packet_length(header);
        if(!encoded_length.has_value())
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        cursor += 4U;

        packet_view packet;
        switch(*encoded_length)
        {
        case 0U:
            packet.Kind = packet_kind::flush;
            break;
        case 1U:
            packet.Kind = packet_kind::delimiter;
            break;
        case 2U:
            packet.Kind = packet_kind::response_end;
            break;
        case 3U:
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        default:
        {
            if(
                *encoded_length > maximum_git_protocol_v2_packet_bytes
                || *encoded_length < 4U)
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::malformed_packet));
            }
            const auto payload_size =
                static_cast<std::size_t>(*encoded_length) - 4U;
            if(payload_size > response.size() - cursor)
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::malformed_packet));
            }
            packet.Kind = packet_kind::data;
            packet.Payload = {
                reinterpret_cast<const char*>(response.data() + cursor),
                payload_size,
            };
            cursor += payload_size;
            break;
        }
        }
        decoded.Packets[decoded.Count++] = packet;
    }
    return decoded;
}

[[nodiscard]] constexpr auto text_line_content(
    std::string_view line) noexcept -> std::optional<std::string_view>
{
    if(
        line.empty() || line.find('\0') != std::string_view::npos
        || line.find('\r') != std::string_view::npos)
    {
        return std::nullopt;
    }
    if(line.ends_with('\n'))
    {
        line.remove_suffix(1U);
    }
    if(line.empty() || line.find('\n') != std::string_view::npos)
    {
        return std::nullopt;
    }
    return line;
}

[[nodiscard]] constexpr auto valid_capability_key_character(
    const char character) noexcept -> bool
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z')
        || (character >= '0' && character <= '9') || character == '-'
        || character == '_';
}

[[nodiscard]] constexpr auto valid_capability_value_character(
    const char character) noexcept -> bool
{
    if(
        (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z')
        || (character >= '0' && character <= '9') || character == ' ')
    {
        return true;
    }
    constexpr std::string_view punctuation =
        "-_.,?/{}[]()<>!@#$%^&*+=:;";
    return punctuation.find(character) != std::string_view::npos;
}

struct parsed_capability
{
    std::string_view Key;
    std::optional<std::string_view> Value;
};

[[nodiscard]] auto parse_capability_line(const std::string_view line) noexcept
    -> std::optional<parsed_capability>
{
    const auto content = text_line_content(line);
    if(!content.has_value())
    {
        return std::nullopt;
    }

    const auto separator = content->find('=');
    const auto key = content->substr(0U, separator);
    if(
        key.empty()
        || !std::all_of(
            key.begin(),
            key.end(),
            valid_capability_key_character))
    {
        return std::nullopt;
    }
    if(separator == std::string_view::npos)
    {
        return parsed_capability{.Key = key};
    }

    const auto value = content->substr(separator + 1U);
    if(
        value.empty()
        || !std::all_of(
            value.begin(),
            value.end(),
            valid_capability_value_character))
    {
        return std::nullopt;
    }
    return parsed_capability{.Key = key, .Value = value};
}

[[nodiscard]] constexpr auto object_format_name(
    const core::git_hash_algorithm format) noexcept -> std::string_view
{
    switch(format)
    {
    case core::git_hash_algorithm::sha1:
        return "sha1";
    case core::git_hash_algorithm::sha256:
        return "sha256";
    case core::git_hash_algorithm::invalid:
        break;
    }
    return {};
}

[[nodiscard]] auto append_bytes(
    std::vector<std::byte>& destination,
    const std::string_view value) -> bool
{
    if(
        destination.size() > maximum_git_smart_request_bytes
        || value.size()
            > maximum_git_smart_request_bytes - destination.size())
    {
        return false;
    }
    destination.insert(
        destination.end(),
        reinterpret_cast<const std::byte*>(value.data()),
        reinterpret_cast<const std::byte*>(value.data() + value.size()));
    return true;
}

[[nodiscard]] auto append_packet(
    std::vector<std::byte>& destination,
    const std::string_view payload) -> bool
{
    const auto total_size = payload.size() + 4U;
    if(
        payload.empty()
        || total_size > maximum_git_protocol_v2_packet_bytes
        || total_size > std::numeric_limits<std::uint16_t>::max())
    {
        return false;
    }

    constexpr std::string_view digits = "0123456789abcdef";
    std::array<char, 4U> header{};
    auto remaining = total_size;
    for(auto index = header.size(); index-- > 0U;)
    {
        header[index] = digits[remaining & 0x0fU];
        remaining >>= 4U;
    }
    return append_bytes(destination, {header.data(), header.size()})
        && append_bytes(destination, payload);
}

[[nodiscard]] auto append_marker(
    std::vector<std::byte>& destination,
    const std::string_view marker) -> bool
{
    return marker.size() == 4U && append_bytes(destination, marker);
}

[[nodiscard]] auto append_request_prelude(
    std::vector<std::byte>& request,
    const std::string_view command,
    const core::git_hash_algorithm format) -> bool
{
    std::array<char, 32U> object_format_line{};
    constexpr std::string_view prefix = "object-format=";
    auto output =
        std::copy(prefix.begin(), prefix.end(), object_format_line.begin());
    const auto format_name = object_format_name(format);
    output = std::copy(format_name.begin(), format_name.end(), output);
    *output++ = '\n';

    return append_packet(request, command)
        && append_packet(
            request,
            {object_format_line.data(),
             static_cast<std::size_t>(
                 output - object_format_line.begin())})
        && append_marker(request, "0001");
}

[[nodiscard]] auto pull_request_ref(
    const core::pull_request_number number) noexcept
    -> std::optional<std::array<char, 64U>>
{
    if(!number.valid())
    {
        return std::nullopt;
    }

    std::array<char, 64U> result{};
    constexpr std::string_view prefix = "refs/pull/";
    constexpr std::string_view suffix = "/head";
    std::copy(prefix.begin(), prefix.end(), result.begin());
    auto* begin = result.data() + prefix.size();
    auto* end = result.data() + result.size() - suffix.size();
    const auto rendered = std::to_chars(begin, end, number.value());
    if(rendered.ec != std::errc{})
    {
        return std::nullopt;
    }
    std::copy(suffix.begin(), suffix.end(), rendered.ptr);
    return result;
}

[[nodiscard]] auto pull_request_ref_view(
    const std::array<char, 64U>& value) noexcept -> std::string_view
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}

[[nodiscard]] constexpr auto expected_object_format(
    const git_smart_request_context& context) noexcept
    -> core::git_hash_algorithm
{
    return context.Revision.Base.algorithm();
}

[[nodiscard]] auto valid_haves(
    const git_smart_request_context& context,
    const std::span<const git_protocol_v2_have> haves,
    const core::git_hash_algorithm format) noexcept -> bool
{
    return haves.size() <= maximum_git_protocol_v2_haves
        && std::all_of(
            haves.begin(),
            haves.end(),
            [&](const git_protocol_v2_have& have)
            {
                return have.Cache == context.Authority.Cache
                    && have.Commit.valid()
                    && have.Commit.algorithm() == format
                    && have.CatalogVisible && have.ObjectVerified;
            });
}

[[nodiscard]] auto exact_lowercase_oid(
    const std::string_view text,
    const core::git_hash_algorithm format) noexcept
    -> std::optional<core::git_commit_oid>
{
    const auto expected_length =
        format == core::git_hash_algorithm::sha1
        ? 40U
        : format == core::git_hash_algorithm::sha256 ? 64U : 0U;
    if(text.size() != expected_length)
    {
        return std::nullopt;
    }
    if(!std::all_of(
           text.begin(),
           text.end(),
           [](const char character)
           {
               return (character >= '0' && character <= '9')
                   || (character >= 'a' && character <= 'f');
           }))
    {
        return std::nullopt;
    }
    return core::parse_git_commit_oid(text);
}

[[nodiscard]] auto render_oid_line(
    const std::string_view prefix,
    const core::git_commit_oid& oid,
    std::vector<std::byte>& request) -> bool
{
    const auto canonical_value = core::canonical_hex(oid);
    const auto canonical = canonical_value.view();
    std::array<char, 80U> line{};
    if(prefix.size() + canonical.size() + 1U > line.size())
    {
        return false;
    }
    auto output = std::copy(prefix.begin(), prefix.end(), line.begin());
    output = std::copy(canonical.begin(), canonical.end(), output);
    *output++ = '\n';
    return append_packet(
        request,
        {line.data(), static_cast<std::size_t>(output - line.begin())});
}

[[nodiscard]] auto make_error(
    const git_pack_stream_failure failure) noexcept -> git_protocol_v2_error
{
    return {
        .Failure = git_protocol_v2_failure::pack_sink,
        .PackFailure = failure,
    };
}

class abort_once_pack_sink final : public git_pack_stream_sink
{
public:
    explicit abort_once_pack_sink(git_pack_stream_sink& target) noexcept
        : Target{target}
    {
    }

    ~abort_once_pack_sink() override
    {
        if(!Released)
        {
            abort();
        }
    }

    [[nodiscard]] auto append(
        const std::span<const std::byte> bytes,
        const std::stop_token stop) -> git_pack_stream_result<void> override
    {
        return Target.append(bytes, stop);
    }

    [[nodiscard]] auto finish(const std::stop_token stop)
        -> git_pack_stream_result<completed_git_pack_receipt> override
    {
        return Target.finish(stop);
    }

    auto abort() noexcept -> void override
    {
        if(!Aborted)
        {
            Target.abort();
            Aborted = true;
        }
    }

    auto release() noexcept -> void
    {
        Released = true;
    }

private:
    git_pack_stream_sink& Target;
    bool Aborted = false;
    bool Released = false;
};
} // namespace

auto valid(const git_protocol_v2_fetch_limits& limits) noexcept -> bool
{
    return limits.MaximumWireBytes != 0U
        && limits.MaximumWireBytes
            <= maximum_git_smart_fetch_response_bytes
        && limits.MaximumPackBytes != 0U
        && limits.MaximumPackBytes <= maximum_git_protocol_v2_pack_bytes
        && limits.MaximumProgressBytes
            <= maximum_git_protocol_v2_progress_bytes
        && limits.MaximumPackets != 0U
        && limits.MaximumPackets <= maximum_git_protocol_v2_fetch_packets;
}

namespace detail
{
auto valid(const git_protocol_v2_capabilities& capabilities) noexcept -> bool
{
    return capabilities.LsRefs && capabilities.Fetch
        && !object_format_name(capabilities.ObjectFormat).empty();
}

auto parse_git_protocol_v2_advertisement(
    const std::span<const std::byte> response,
    const core::git_hash_algorithm expected_format)
    -> git_protocol_v2_result<git_protocol_v2_capabilities>
{
    if(object_format_name(expected_format).empty())
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::invalid_request));
    }

    const auto decoded = decode_control_packets(response);
    if(!decoded.has_value())
    {
        return std::unexpected(decoded.error());
    }
    if(
        decoded->Count < 2U
        || decoded->Packets[0U].Kind != packet_kind::data
        || text_line_content(decoded->Packets[0U].Payload)
            != std::optional<std::string_view>{"version 2"})
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::unsupported_version));
    }

    git_protocol_v2_capabilities capabilities;
    std::array<std::string_view, maximum_git_protocol_v2_control_packets>
        seen_keys{};
    std::size_t seen_count = 0U;
    bool terminal = false;
    for(std::size_t index = 1U; index < decoded->Count; ++index)
    {
        const auto& packet = decoded->Packets[index];
        if(packet.Kind == packet_kind::flush)
        {
            if(index + 1U != decoded->Count)
            {
                return std::unexpected(
                    protocol_error(
                        git_protocol_v2_failure::unexpected_section));
            }
            terminal = true;
            break;
        }
        if(packet.Kind != packet_kind::data)
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::unexpected_section));
        }

        const auto parsed = parse_capability_line(packet.Payload);
        if(!parsed.has_value())
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        if(
            std::find(
                seen_keys.begin(),
                seen_keys.begin() + static_cast<std::ptrdiff_t>(seen_count),
                parsed->Key)
            != seen_keys.begin()
                + static_cast<std::ptrdiff_t>(seen_count))
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        seen_keys[seen_count++] = parsed->Key;

        if(parsed->Key == "ls-refs")
        {
            capabilities.LsRefs = true;
        }
        else if(parsed->Key == "fetch")
        {
            capabilities.Fetch = true;
        }
        else if(parsed->Key == "object-format")
        {
            if(
                !parsed->Value.has_value()
                || (*parsed->Value != "sha1" && *parsed->Value != "sha256"))
            {
                return std::unexpected(
                    protocol_error(
                        git_protocol_v2_failure::object_format_mismatch));
            }
            capabilities.ObjectFormat = *parsed->Value == "sha1"
                ? core::git_hash_algorithm::sha1
                : core::git_hash_algorithm::sha256;
        }
    }

    if(!terminal)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::incomplete_response));
    }
    if(
        !capabilities.LsRefs || !capabilities.Fetch
        || capabilities.ObjectFormat == core::git_hash_algorithm::invalid)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::missing_capability));
    }
    if(capabilities.ObjectFormat != expected_format)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::object_format_mismatch));
    }
    return capabilities;
}

auto make_git_protocol_v2_ls_refs_request(
    const git_smart_request_context& context,
    const git_protocol_v2_capabilities& capabilities)
    -> git_protocol_v2_result<std::vector<std::byte>>
{
    if(
        !valid(context) || !valid(capabilities)
        || capabilities.ObjectFormat != expected_object_format(context))
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::invalid_request));
    }

    const auto pr_ref_buffer = pull_request_ref(context.Authority.Number);
    if(!pr_ref_buffer.has_value())
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::invalid_request));
    }

    try
    {
        std::vector<std::byte> request;
        request.reserve(256U);

        std::array<char, maximum_git_base_branch_ref_bytes + 16U> base_line{};
        constexpr std::string_view ref_prefix = "ref-prefix ";
        auto base_end =
            std::copy(ref_prefix.begin(), ref_prefix.end(), base_line.begin());
        base_end = std::copy(
            context.BaseRef.view().begin(),
            context.BaseRef.view().end(),
            base_end);
        *base_end++ = '\n';

        std::array<char, 80U> pr_line{};
        auto pr_end =
            std::copy(ref_prefix.begin(), ref_prefix.end(), pr_line.begin());
        const auto pr_ref = pull_request_ref_view(*pr_ref_buffer);
        pr_end = std::copy(pr_ref.begin(), pr_ref.end(), pr_end);
        *pr_end++ = '\n';

        if(
            !append_request_prelude(
                request,
                "command=ls-refs\n",
                capabilities.ObjectFormat)
            || !append_packet(
                request,
                {base_line.data(),
                 static_cast<std::size_t>(base_end - base_line.begin())})
            || !append_packet(
                request,
                {pr_line.data(),
                 static_cast<std::size_t>(pr_end - pr_line.begin())})
            || !append_marker(request, "0000"))
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::request_limit));
        }
        return request;
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::allocation));
    }
}

auto parse_git_protocol_v2_ls_refs(
    const std::span<const std::byte> response,
    const git_smart_request_context& context)
    -> git_protocol_v2_result<git_protocol_v2_refs>
{
    if(!valid(context))
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::invalid_request));
    }
    const auto pr_ref_buffer = pull_request_ref(context.Authority.Number);
    if(!pr_ref_buffer.has_value())
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::invalid_request));
    }
    const auto pr_ref = pull_request_ref_view(*pr_ref_buffer);
    const auto decoded = decode_control_packets(response);
    if(!decoded.has_value())
    {
        return std::unexpected(decoded.error());
    }

    git_protocol_v2_refs refs;
    bool base_seen = false;
    bool head_seen = false;
    bool terminal = false;
    for(std::size_t index = 0U; index < decoded->Count; ++index)
    {
        const auto& packet = decoded->Packets[index];
        if(packet.Kind == packet_kind::flush)
        {
            if(index + 1U != decoded->Count)
            {
                return std::unexpected(
                    protocol_error(
                        git_protocol_v2_failure::unexpected_section));
            }
            terminal = true;
            break;
        }
        if(packet.Kind != packet_kind::data)
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }

        const auto line = text_line_content(packet.Payload);
        if(!line.has_value())
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        const auto separator = line->find(' ');
        if(
            separator == std::string_view::npos
            || line->find(' ', separator + 1U) != std::string_view::npos)
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        const auto oid_text = line->substr(0U, separator);
        const auto ref_name = line->substr(separator + 1U);
        const auto oid =
            exact_lowercase_oid(oid_text, expected_object_format(context));
        if(!oid.has_value())
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::oid_mismatch));
        }

        if(ref_name == context.BaseRef.view())
        {
            if(base_seen)
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::duplicate_ref));
            }
            if(*oid != context.Revision.Base)
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::oid_mismatch));
            }
            refs.Base = *oid;
            base_seen = true;
        }
        else if(ref_name == pr_ref)
        {
            if(head_seen)
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::duplicate_ref));
            }
            if(*oid != context.Revision.Head)
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::oid_mismatch));
            }
            refs.Head = *oid;
            head_seen = true;
        }
        else
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::unexpected_ref));
        }
    }
    if(!terminal || !base_seen || !head_seen)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::incomplete_response));
    }
    return refs;
}

auto make_git_protocol_v2_fetch_request(
    const git_smart_request_context& context,
    const git_protocol_v2_capabilities& capabilities,
    const std::span<const git_protocol_v2_have> haves)
    -> git_protocol_v2_result<std::vector<std::byte>>
{
    if(
        !valid(context) || !valid(capabilities)
        || capabilities.ObjectFormat != expected_object_format(context)
        || !valid_haves(context, haves, capabilities.ObjectFormat))
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::invalid_request));
    }

    std::array<core::git_commit_oid, maximum_git_protocol_v2_haves>
        deduplicated_haves{};
    std::size_t have_count = 0U;
    for(const auto& have : haves)
    {
        const auto end =
            deduplicated_haves.begin()
            + static_cast<std::ptrdiff_t>(have_count);
        if(
            std::find(
                deduplicated_haves.begin(),
                end,
                have.Commit)
            == end)
        {
            deduplicated_haves[have_count++] = have.Commit;
        }
    }

    try
    {
        std::vector<std::byte> request;
        request.reserve(512U + have_count * 72U);

        if(
            !append_request_prelude(
                request,
                "command=fetch\n",
                capabilities.ObjectFormat)
            || !render_oid_line("want ", context.Revision.Base, request)
            || (context.Revision.Head != context.Revision.Base
                && !render_oid_line(
                    "want ",
                    context.Revision.Head,
                    request)))
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::request_limit));
        }
        for(std::size_t index = 0U; index < have_count; ++index)
        {
            if(!render_oid_line("have ", deduplicated_haves[index], request))
            {
                return std::unexpected(
                    protocol_error(git_protocol_v2_failure::request_limit));
            }
        }
        if(
            !append_packet(request, "done\n")
            || !append_marker(request, "0000"))
        {
            return std::unexpected(
                protocol_error(git_protocol_v2_failure::request_limit));
        }
        return request;
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected(
            protocol_error(git_protocol_v2_failure::allocation));
    }
}

git_protocol_v2_fetch_parser::git_protocol_v2_fetch_parser(
    git_pack_stream_sink& sink,
    const git_protocol_v2_fetch_limits limits,
    const core::git_hash_algorithm expected_format,
    const std::stop_token stop) noexcept
    : Sink{&sink},
      Limits{limits},
      ExpectedFormat{expected_format},
      Stop{stop}
{
    if(
        !valid(limits)
        || object_format_name(expected_format).empty())
    {
        static_cast<void>(
            fail(protocol_error(git_protocol_v2_failure::invalid_request)));
    }
}

git_protocol_v2_fetch_parser::~git_protocol_v2_fetch_parser()
{
    if(State != response_state::finished)
    {
        abort_sink();
    }
}

auto git_protocol_v2_fetch_parser::fail(
    const git_protocol_v2_error error) noexcept -> bool
{
    if(!Error.has_value())
    {
        Error = error;
    }
    State = response_state::failed;
    abort_sink();
    return false;
}

auto git_protocol_v2_fetch_parser::begin_packet() noexcept -> bool
{
    std::array<std::byte, 4U> header{};
    std::transform(
        Header.begin(),
        Header.end(),
        header.begin(),
        [](const char character)
        {
            return static_cast<std::byte>(
                static_cast<unsigned char>(character));
        });
    const auto length = decode_packet_length(header);
    if(!length.has_value())
    {
        return fail(protocol_error(git_protocol_v2_failure::malformed_packet));
    }
    if(Packets == Limits.MaximumPackets)
    {
        return fail(protocol_error(git_protocol_v2_failure::response_limit));
    }
    ++Packets;

    if(*length <= 3U)
    {
        if(*length == 3U)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        if(*length != 0U)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::unexpected_section));
        }
        if(
            State != response_state::sideband || !SawPackData
            || HeaderBytes != Header.size())
        {
            return fail(
                protocol_error(git_protocol_v2_failure::incomplete_response));
        }
        State = response_state::terminal;
        reset_packet();
        return true;
    }
    if(
        *length > maximum_git_protocol_v2_packet_bytes
        || *length < 4U)
    {
        return fail(protocol_error(git_protocol_v2_failure::malformed_packet));
    }
    if(State == response_state::terminal)
    {
        return fail(
            protocol_error(git_protocol_v2_failure::unexpected_section));
    }

    PayloadBytes = static_cast<std::uint32_t>(*length - 4U);
    PayloadConsumed = 0U;
    if(State == response_state::packfile_header)
    {
        constexpr auto section_name_bytes = std::string_view{"packfile"}.size();
        if(
            PayloadBytes < section_name_bytes
            || PayloadBytes > SectionHeader.size())
        {
            return fail(
                protocol_error(git_protocol_v2_failure::unexpected_section));
        }
    }
    else if(State == response_state::sideband && PayloadBytes < 1U)
    {
        return fail(protocol_error(git_protocol_v2_failure::malformed_packet));
    }
    return true;
}

auto git_protocol_v2_fetch_parser::consume_payload(
    const std::span<const std::byte> bytes) noexcept -> bool
{
    if(State == response_state::packfile_header)
    {
        if(
            SectionHeaderBytes > SectionHeader.size()
            || bytes.size() > SectionHeader.size() - SectionHeaderBytes)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        for(const auto byte : bytes)
        {
            if(SectionHeaderBytes == SectionHeader.size())
            {
                return fail(
                    protocol_error(
                        git_protocol_v2_failure::malformed_packet));
            }
            SectionHeader[SectionHeaderBytes] = static_cast<char>(
                std::to_integer<unsigned char>(byte));
            ++SectionHeaderBytes;
        }
        PayloadConsumed += static_cast<std::uint32_t>(bytes.size());
        if(PayloadConsumed == PayloadBytes)
        {
            const auto section = text_line_content(
                {SectionHeader.data(), SectionHeaderBytes});
            if(!section.has_value() || *section != "packfile")
            {
                return fail(
                    protocol_error(
                        git_protocol_v2_failure::unexpected_section));
            }
            State = response_state::sideband;
            reset_packet();
        }
        return true;
    }

    auto remaining = bytes;
    if(!ChannelRead)
    {
        Channel = std::to_integer<std::uint8_t>(remaining.front());
        ChannelRead = true;
        remaining = remaining.subspan(1U);
        ++PayloadConsumed;
        if(Channel == 3U)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::sideband_error));
        }
        if(Channel != 1U && Channel != 2U)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
    }

    if(!remaining.empty())
    {
        if(Channel == 1U)
        {
            if(
                remaining.size() > Limits.MaximumPackBytes - PackBytes)
            {
                return fail(
                    protocol_error(git_protocol_v2_failure::pack_limit));
            }
            try
            {
                const auto appended = Sink->append(remaining, Stop);
                if(!appended.has_value())
                {
                    return fail(make_error(appended.error()));
                }
            }
            catch(...)
            {
                return fail(make_error(git_pack_stream_failure::storage));
            }
            PackBytes += remaining.size();
            SawPackData = true;
        }
        else
        {
            if(
                remaining.size()
                > Limits.MaximumProgressBytes - ProgressBytes)
            {
                return fail(
                    protocol_error(git_protocol_v2_failure::progress_limit));
            }
            ProgressBytes += remaining.size();
        }
        PayloadConsumed += static_cast<std::uint32_t>(remaining.size());
    }

    if(PayloadConsumed == PayloadBytes)
    {
        reset_packet();
    }
    return true;
}

auto git_protocol_v2_fetch_parser::consume(
    const std::span<const std::byte> bytes) noexcept -> bool
{
    if(
        State == response_state::failed
        || State == response_state::finished)
    {
        return false;
    }
    if(Stop.stop_requested())
    {
        return fail(make_error(git_pack_stream_failure::cancelled));
    }
    if(bytes.size() > Limits.MaximumWireBytes - WireBytes)
    {
        return fail(protocol_error(git_protocol_v2_failure::response_limit));
    }
    WireBytes += bytes.size();

    std::size_t cursor = 0U;
    while(cursor < bytes.size())
    {
        if(State == response_state::terminal)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::unexpected_section));
        }
        if(HeaderBytes < Header.size())
        {
            const auto count =
                std::min(Header.size() - HeaderBytes, bytes.size() - cursor);
            std::transform(
                bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                bytes.begin()
                    + static_cast<std::ptrdiff_t>(cursor + count),
                Header.begin() + static_cast<std::ptrdiff_t>(HeaderBytes),
                [](const std::byte byte)
                {
                    return static_cast<char>(
                        std::to_integer<unsigned char>(byte));
                });
            HeaderBytes += count;
            cursor += count;
            if(HeaderBytes != Header.size())
            {
                continue;
            }
            if(!begin_packet())
            {
                return false;
            }
            if(State == response_state::terminal)
            {
                continue;
            }
            if(cursor == bytes.size())
            {
                continue;
            }
        }

        const auto payload_remaining =
            static_cast<std::size_t>(PayloadBytes - PayloadConsumed);
        const auto count =
            std::min(payload_remaining, bytes.size() - cursor);
        if(count == 0U)
        {
            return fail(
                protocol_error(git_protocol_v2_failure::malformed_packet));
        }
        if(!consume_payload(bytes.subspan(cursor, count)))
        {
            return false;
        }
        cursor += count;
    }
    return true;
}

auto git_protocol_v2_fetch_parser::finish()
    -> git_protocol_v2_result<git_protocol_v2_fetch_result>
{
    if(Error.has_value())
    {
        return std::unexpected(*Error);
    }
    if(
        State != response_state::terminal || HeaderBytes != 0U
        || PayloadBytes != 0U || !SawPackData)
    {
        const auto error =
            protocol_error(git_protocol_v2_failure::incomplete_response);
        static_cast<void>(fail(error));
        return std::unexpected(error);
    }

    git_pack_stream_result<completed_git_pack_receipt> receipt =
        std::unexpected(git_pack_stream_failure::storage);
    try
    {
        receipt = Sink->finish(Stop);
    }
    catch(...)
    {
        const auto error = make_error(git_pack_stream_failure::storage);
        static_cast<void>(fail(error));
        return std::unexpected(error);
    }
    if(!receipt.has_value())
    {
        const auto error = make_error(receipt.error());
        static_cast<void>(fail(error));
        return std::unexpected(error);
    }
    if(
        !agrees(*receipt)
        || receipt->Spool.Bytes != PackBytes
        || receipt->ExpectedPack.Algorithm != ExpectedFormat)
    {
        const auto error = make_error(git_pack_stream_failure::invalid_state);
        static_cast<void>(fail(error));
        return std::unexpected(error);
    }

    State = response_state::finished;
    return git_protocol_v2_fetch_result{
        .Pack = *receipt,
        .WireBytes = WireBytes,
        .PackBytes = PackBytes,
        .ProgressBytes = ProgressBytes,
        .Packets = Packets,
    };
}

auto git_protocol_v2_fetch_parser::abort() noexcept -> void
{
    if(State != response_state::finished)
    {
        static_cast<void>(
            fail(protocol_error(git_protocol_v2_failure::incomplete_response)));
    }
}

auto git_protocol_v2_fetch_parser::error() const noexcept
    -> std::optional<git_protocol_v2_error>
{
    return Error;
}

auto git_protocol_v2_fetch_parser::wire_bytes() const noexcept
    -> std::uint64_t
{
    return WireBytes;
}

auto git_protocol_v2_fetch_parser::pack_bytes() const noexcept
    -> std::uint64_t
{
    return PackBytes;
}

auto git_protocol_v2_fetch_parser::progress_bytes() const noexcept
    -> std::uint64_t
{
    return ProgressBytes;
}

auto git_protocol_v2_fetch_parser::packets() const noexcept -> std::uint64_t
{
    return Packets;
}

auto git_protocol_v2_fetch_parser::reset_packet() noexcept -> void
{
    Header.fill('\0');
    HeaderBytes = 0U;
    PayloadBytes = 0U;
    PayloadConsumed = 0U;
    Channel = 0U;
    ChannelRead = false;
}

auto git_protocol_v2_fetch_parser::abort_sink() noexcept -> void
{
    if(!SinkAborted)
    {
        Sink->abort();
        SinkAborted = true;
    }
}

static auto discover_git_protocol_v2(
    git_smart_transport_port& transport,
    const git_smart_request_context& context,
    const std::stop_token stop)
    -> git_protocol_v2_client_result<git_protocol_v2_capabilities>
{
    if(!valid(context))
    {
        return std::unexpected(
            git_protocol_v2_client_error{
                protocol_error(git_protocol_v2_failure::invalid_request)});
    }
    const auto response =
        transport.discover(git_smart_discovery_request{context}, stop);
    if(!response.has_value())
    {
        return std::unexpected(
            git_protocol_v2_client_error{response.error()});
    }
    auto parsed = parse_git_protocol_v2_advertisement(
        response->Body,
        expected_object_format(context));
    if(!parsed.has_value())
    {
        return std::unexpected(
            git_protocol_v2_client_error{parsed.error()});
    }
    git_protocol_v2_client_result<git_protocol_v2_capabilities> result;
    *result = std::move(*parsed);
    return result;
}

static auto list_git_protocol_v2_refs(
    git_smart_transport_port& transport,
    const git_smart_request_context& context,
    const git_protocol_v2_capabilities& capabilities,
    const std::stop_token stop)
    -> git_protocol_v2_client_result<git_protocol_v2_refs>
{
    const auto body =
        make_git_protocol_v2_ls_refs_request(context, capabilities);
    if(!body.has_value())
    {
        return std::unexpected(
            git_protocol_v2_client_error{body.error()});
    }
    const auto response = transport.ls_refs(
        git_smart_ls_refs_request{
            .Context = context,
            .Body = *body,
        },
        stop);
    if(!response.has_value())
    {
        return std::unexpected(
            git_protocol_v2_client_error{response.error()});
    }
    auto parsed = parse_git_protocol_v2_ls_refs(
        response->Body,
        context);
    if(!parsed.has_value())
    {
        return std::unexpected(
            git_protocol_v2_client_error{parsed.error()});
    }
    git_protocol_v2_client_result<git_protocol_v2_refs> result;
    *result = std::move(*parsed);
    return result;
}

static auto fetch_git_protocol_v2(
    git_smart_transport_port& transport,
    const git_smart_request_context& context,
    const git_protocol_v2_capabilities& capabilities,
    const std::span<const git_protocol_v2_have> haves,
    git_pack_stream_sink& sink,
    const git_protocol_v2_fetch_limits limits,
    const std::stop_token stop)
    -> git_protocol_v2_client_result<git_protocol_v2_fetch_result>
{
    const auto body =
        make_git_protocol_v2_fetch_request(context, capabilities, haves);
    if(!body.has_value())
    {
        sink.abort();
        return std::unexpected(
            git_protocol_v2_client_error{body.error()});
    }
    if(!valid(limits))
    {
        sink.abort();
        return std::unexpected(git_protocol_v2_client_error{
            protocol_error(git_protocol_v2_failure::invalid_request)});
    }

    git_protocol_v2_fetch_parser parser{
        sink,
        limits,
        expected_object_format(context),
        stop,
    };
    const auto transported = transport.fetch(
        git_smart_fetch_request{
            .Context = context,
            .Body = *body,
            .MaximumResponseBytes = limits.MaximumWireBytes,
        },
        parser,
        stop);
    if(!transported.has_value())
    {
        if(
            transported.error().Failure
                == git_smart_transport_failure::consumer_rejected
            && parser.error().has_value())
        {
            return std::unexpected(
                git_protocol_v2_client_error{*parser.error()});
        }
        parser.abort();
        return std::unexpected(
            git_protocol_v2_client_error{transported.error()});
    }

    auto parsed = parser.finish();
    if(!parsed.has_value())
    {
        return std::unexpected(
            git_protocol_v2_client_error{parsed.error()});
    }
    git_protocol_v2_client_result<git_protocol_v2_fetch_result> result;
    *result = std::move(*parsed);
    return result;
}

} // namespace detail

auto acquire_git_protocol_v2_pack(
    git_smart_transport_port& transport,
    const git_smart_request_context& context,
    const std::span<const git_protocol_v2_have> haves,
    git_pack_stream_sink& sink,
    const git_protocol_v2_fetch_limits limits,
    const std::stop_token stop)
    -> git_protocol_v2_client_result<git_protocol_v2_acquisition_result>
{
    abort_once_pack_sink guarded_sink{sink};
    const auto cancelled = [] {
        return std::unexpected(git_protocol_v2_client_error{
            git_smart_transport_error{
                .Failure = git_smart_transport_failure::cancelled,
            }});
    };

    if(
        !valid(context) || !valid(limits)
        || !valid_haves(
            context,
            haves,
            expected_object_format(context)))
    {
        return std::unexpected(git_protocol_v2_client_error{
            protocol_error(git_protocol_v2_failure::invalid_request)});
    }
    if(stop.stop_requested())
    {
        return cancelled();
    }
    const auto capabilities =
        detail::discover_git_protocol_v2(transport, context, stop);
    if(!capabilities.has_value())
    {
        return std::unexpected(capabilities.error());
    }

    if(stop.stop_requested())
    {
        return cancelled();
    }
    const auto refs = detail::list_git_protocol_v2_refs(
        transport,
        context,
        *capabilities,
        stop);
    if(!refs.has_value())
    {
        return std::unexpected(refs.error());
    }

    if(stop.stop_requested())
    {
        return cancelled();
    }
    const auto fetched = detail::fetch_git_protocol_v2(
        transport,
        context,
        *capabilities,
        haves,
        guarded_sink,
        limits,
        stop);
    if(!fetched.has_value())
    {
        return std::unexpected(fetched.error());
    }

    guarded_sink.release();
    return git_protocol_v2_acquisition_result{
        .AdvertisedRefs = *refs,
        .Fetch = *fetched,
    };
}
} // namespace bpr::application
