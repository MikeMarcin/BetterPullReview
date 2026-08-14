#!/usr/bin/env python3
"""Attended RFC-BPR-0016 device-flow capture course for macOS.

Transient codes and credentials remain process-local. The command prints only
fixed progress markers and does not persist raw HTTP messages. Durable fixture
projection remains owned by capture_rfc0016_device_contracts.py.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import importlib.util
import json
import os
import re
import secrets
import ssl
import stat
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Mapping, Protocol, Sequence


DEVICE_URL = "https://github.com/login/device/code"
TOKEN_URL = "https://github.com/login/oauth/access_token"
VERIFY_URL = "https://github.com/login/device"
DEVICE_GRANT = "urn:ietf:params:oauth:grant-type:device_code"
MAX_RESPONSE_BYTES = 64 * 1024
MAX_PROBE_RESPONSE_BYTES = 2 * 1024 * 1024
REQUEST_TIMEOUT_SECONDS = 20.0
TOTAL_COURSE_SECONDS = 20 * 60
EXPIRY_CAPTURE_GRACE_SECONDS = 30
FIXED_ENVIRONMENT = {"LANG": "C", "PATH": "/usr/bin:/bin"}
ALLOWED_URLS = frozenset({DEVICE_URL, TOKEN_URL})
COURSES = frozenset(
    {"access-denied", "device-flow-disabled", "expired-token", "slow-down", "token-success"}
)
TERMINALS = frozenset({"access_denied", "expired_token", "token_success"})
PUBLIC_CLIENT_ID = re.compile(r"^[A-Za-z0-9.]{16,64}$")
USER_CODE = re.compile(r"^[A-Z0-9]{4}-[A-Z0-9]{4}$")
APPROVED_PUBLIC_CLIENT_ID = "Iv23lil08fH3933w9MkW"
APPROVED_APPLICATION_ID = 4581304
AUTHORITY_AMENDMENT_ID = "rfc0016-private-owner-only-2026-08-13"
SLOW_DOWN_REPLACEMENT_AUTHORITY_ID = (
    "rfc0016-bpr0276-slow-down-replacement-once-2026-08-13"
)
SLOW_DOWN_RECOVERY_AUTHORITY_ID = (
    "rfc0016-bpr0276-slow-down-parser-repair-recovery-once-2026-08-13"
)


def _account_home() -> Path:
    if sys.platform == "darwin":
        import pwd

        return Path(pwd.getpwuid(os.getuid()).pw_dir)
    return Path.home()


_SLOW_DOWN_STATE_ROOT = (
    _account_home()
    / "Library"
    / "Application Support"
    / "BetterPullReview"
    / "rfc0016-bpr0276"
)
SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH = (
    _SLOW_DOWN_STATE_ROOT / "slow-down-replacement-authority.json"
)
SLOW_DOWN_SPENT_MARKER_PATH = _SLOW_DOWN_STATE_ROOT / "slow-down-reservation-spent.json"
SLOW_DOWN_RECOVERY_AUTHORITY_PATH = (
    _SLOW_DOWN_STATE_ROOT / "slow-down-parser-repair-recovery-authority.json"
)
SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH = (
    _SLOW_DOWN_STATE_ROOT / "slow-down-parser-repair-recovery-spent.json"
)
ERROR_FIELDS = frozenset({"error", "error_description", "error_uri"})
SLOW_DOWN_FIELDS = ERROR_FIELDS | {"interval"}
AMBIENT_NETWORK_ENVIRONMENT = (
    "ALL_PROXY",
    "HTTPS_PROXY",
    "HTTP_PROXY",
    "SSL_CERT_DIR",
    "SSL_CERT_FILE",
    "all_proxy",
    "https_proxy",
    "http_proxy",
)
API_VERSION = "2022-11-28"
GIT_UPLOAD_PACK_BODY = (
    b"0014command=ls-refs\n"
    b"0015agent=git/2.43.0\n"
    b"0001"
    b"0014ref-prefix HEAD\n"
    b"0000"
)


class LiveCaptureError(RuntimeError):
    """A fixed, non-sensitive attended-course failure."""


def _sha256(payload: bytes) -> str:
    return f"sha256:{hashlib.sha256(payload).hexdigest()}"


def _runner_sha256() -> str:
    return _sha256(Path(__file__).read_bytes())


LOADED_RUNNER_SHA256 = _runner_sha256()


def _execution_timestamp() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )


def _timestamp(value: object, diagnostic: str) -> datetime:
    if not isinstance(value, str) or not re.fullmatch(
        r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", value
    ):
        raise LiveCaptureError(diagnostic)
    try:
        return datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=timezone.utc
        )
    except ValueError as error:
        raise LiveCaptureError(diagnostic) from error


def _require_sha256(value: object, diagnostic: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", value):
        raise LiveCaptureError(diagnostic)
    return value


@dataclass(frozen=True)
class WireReply:
    status: int
    media_type: str
    final_url: str
    body: bytes


class Wire(Protocol):
    def post_form(
        self,
        url: str,
        fields: Mapping[str, str],
        *,
        timeout_seconds: float,
    ) -> WireReply: ...


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        raise LiveCaptureError("redirect_refused")


def _strict_opener() -> urllib.request.OpenerDirector:
    if any(os.environ.get(name) for name in AMBIENT_NETWORK_ENVIRONMENT):
        raise LiveCaptureError("ambient_network_configuration_refused")
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = True
    context.verify_mode = ssl.CERT_REQUIRED
    context.load_default_certs()
    return urllib.request.build_opener(
        urllib.request.ProxyHandler({}),
        NoRedirect(),
        urllib.request.HTTPSHandler(context=context),
    )


class HttpsWire:
    """Strict GitHub HTTPS form transport with bounded response bodies."""

    def __init__(self) -> None:
        self._opener = _strict_opener()

    def post_form(
        self,
        url: str,
        fields: Mapping[str, str],
        *,
        timeout_seconds: float = REQUEST_TIMEOUT_SECONDS,
    ) -> WireReply:
        if url not in ALLOWED_URLS:
            raise LiveCaptureError("authority_refused")
        parsed = urllib.parse.urlsplit(url)
        if (
            parsed.scheme != "https"
            or parsed.hostname != "github.com"
            or parsed.username is not None
            or parsed.password is not None
            or parsed.port is not None
            or parsed.query
            or parsed.fragment
        ):
            raise LiveCaptureError("authority_refused")
        if not 0.0 < timeout_seconds <= REQUEST_TIMEOUT_SECONDS:
            raise LiveCaptureError("request_timeout_invalid")
        data = urllib.parse.urlencode(fields).encode("ascii")
        request = urllib.request.Request(
            url,
            data=data,
            method="POST",
            headers={
                "Accept": "application/json",
                "Content-Type": "application/x-www-form-urlencoded",
            },
        )
        try:
            with self._opener.open(request, timeout=timeout_seconds) as reply:
                body = reply.read(MAX_RESPONSE_BYTES + 1)
                status = reply.status
                final_url = reply.geturl()
                media_type = reply.headers.get_content_type()
        except urllib.error.HTTPError as error:
            body = error.read(MAX_RESPONSE_BYTES + 1)
            status = error.code
            final_url = error.geturl()
            media_type = error.headers.get_content_type()
        except LiveCaptureError:
            raise
        except (OSError, urllib.error.URLError) as error:
            raise LiveCaptureError("transport_failed") from error
        if len(body) > MAX_RESPONSE_BYTES:
            raise LiveCaptureError("response_too_large")
        if status not in {200, 400}:
            raise LiveCaptureError("response_status_refused")
        if final_url != url:
            raise LiveCaptureError("response_final_url_refused")
        if media_type != "application/json":
            raise LiveCaptureError("response_media_type_refused")
        return WireReply(status, media_type, final_url, body)


@dataclass(frozen=True)
class ProbeRequest:
    operation: str
    method: str
    url: str
    body: bytes
    content_type: str | None
    accept: str


class ReadOnlyProbeWire:
    """Exact read-only GitHub route transport for the designated fixture."""

    def __init__(self) -> None:
        self._opener = _strict_opener()

    def perform(
        self,
        request_contract: ProbeRequest,
        access_token: str,
    ) -> WireReply:
        allowed = {request.operation: request for request in _probe_requests()}
        if allowed.get(request_contract.operation) != request_contract:
            raise LiveCaptureError("probe_route_refused")
        if not access_token or len(access_token.encode("utf-8")) > 4096:
            raise LiveCaptureError("probe_credential_invalid")
        parsed = urllib.parse.urlsplit(request_contract.url)
        if (
            parsed.scheme != "https"
            or parsed.hostname not in {"api.github.com", "github.com"}
            or parsed.username is not None
            or parsed.password is not None
            or parsed.port is not None
            or parsed.fragment
        ):
            raise LiveCaptureError("probe_route_refused")
        authorization = (
            "Basic "
            + base64.b64encode(f"x-access-token:{access_token}".encode("utf-8")).decode("ascii")
            if parsed.hostname == "github.com"
            else f"Bearer {access_token}"
        )
        headers = {
            "Accept": request_contract.accept,
            "Authorization": authorization,
            "User-Agent": "BetterPullReview-RFC0016-Capture",
        }
        if parsed.hostname == "api.github.com":
            headers["X-GitHub-Api-Version"] = API_VERSION
        if request_contract.operation.startswith("contents_"):
            headers["Git-Protocol"] = "version=2"
        if request_contract.content_type is not None:
            headers["Content-Type"] = request_contract.content_type
        request = urllib.request.Request(
            request_contract.url,
            data=request_contract.body if request_contract.method == "POST" else None,
            method=request_contract.method,
            headers=headers,
        )
        try:
            with self._opener.open(request, timeout=REQUEST_TIMEOUT_SECONDS) as reply:
                body = reply.read(MAX_PROBE_RESPONSE_BYTES + 1)
                status = reply.status
                final_url = reply.geturl()
                media_type = reply.headers.get_content_type()
        except LiveCaptureError:
            raise
        except (OSError, urllib.error.URLError, urllib.error.HTTPError) as error:
            raise LiveCaptureError("probe_transport_failed") from error
        if len(body) > MAX_PROBE_RESPONSE_BYTES:
            raise LiveCaptureError("probe_response_too_large")
        if status != 200 or final_url != request_contract.url:
            raise LiveCaptureError("probe_response_metadata_refused")
        expected_media = (
            "application/x-git-upload-pack-advertisement"
            if request_contract.operation == "contents_info_refs"
            else "application/x-git-upload-pack-result"
            if request_contract.operation == "contents_upload_pack"
            else "application/json"
        )
        if media_type != expected_media:
            raise LiveCaptureError("probe_response_metadata_refused")
        return WireReply(status, media_type, final_url, body)


def _probe_requests() -> tuple[ProbeRequest, ...]:
    offline = _offline_module()
    if GIT_UPLOAD_PACK_BODY != offline.GIT_UPLOAD_PACK_BODY:
        raise LiveCaptureError("probe_body_contract_mismatch")
    graphql_body = json.dumps(
        {"query": offline.GRAPHQL_DOCUMENT, "variables": offline.GRAPHQL_VARIABLES},
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    api_accept = "application/vnd.github+json"
    return (
        ProbeRequest(
            "metadata",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "contents_info_refs",
            "GET",
            "https://github.com/MikeMarcin/BPR-Test.git/info/refs?service=git-upload-pack",
            b"",
            None,
            "application/x-git-upload-pack-advertisement",
        ),
        ProbeRequest(
            "contents_upload_pack",
            "POST",
            "https://github.com/MikeMarcin/BPR-Test.git/git-upload-pack",
            GIT_UPLOAD_PACK_BODY,
            "application/x-git-upload-pack-request",
            "application/x-git-upload-pack-result",
        ),
        ProbeRequest(
            "checks",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/commits/HEAD/check-runs",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "pull_request",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "pull_request_files",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/files?per_page=100&page=1",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "pull_request_commits",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/commits?per_page=100&page=1",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "pull_request_reviews",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=1",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "pull_request_issue_comments",
            "GET",
            "https://api.github.com/repos/MikeMarcin/BPR-Test/"
            "issues/1/comments?per_page=100&page=1",
            b"",
            None,
            api_accept,
        ),
        ProbeRequest(
            "review_threads",
            "POST",
            "https://api.github.com/graphql",
            graphql_body,
            "application/json",
            api_accept,
        ),
    )


def _closed_json(payload: bytes) -> dict[str, object]:
    if len(payload) > MAX_RESPONSE_BYTES:
        raise LiveCaptureError("response_too_large")
    try:
        def exact_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
            value: dict[str, object] = {}
            for key, member in pairs:
                if key in value:
                    raise LiveCaptureError("response_duplicate_member")
                value[key] = member
            return value

        value = json.loads(payload.decode("utf-8"), object_pairs_hook=exact_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise LiveCaptureError("response_invalid") from error
    if not isinstance(value, dict):
        raise LiveCaptureError("response_invalid")
    return value


def _probe_json(payload: bytes) -> object:
    if len(payload) > MAX_PROBE_RESPONSE_BYTES:
        raise LiveCaptureError("probe_response_too_large")
    try:
        return json.loads(
            payload.decode("utf-8"),
            object_pairs_hook=lambda pairs: _unique_object(
                pairs,
                "probe_response_duplicate_member",
            ),
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise LiveCaptureError("probe_response_invalid") from error


def _unique_object(
    pairs: list[tuple[str, object]],
    diagnostic: str,
) -> dict[str, object]:
    value: dict[str, object] = {}
    for key, member in pairs:
        if key in value:
            raise LiveCaptureError(diagnostic)
        value[key] = member
    return value


def _exact_probe_object(
    value: object,
    keys: set[str],
) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != keys:
        raise LiveCaptureError("probe_graphql_invalid")
    return value


def _validate_graphql_page_info(value: object, *, has_nodes: bool) -> None:
    page_info = _exact_probe_object(value, {"endCursor", "hasNextPage"})
    end_cursor = page_info["endCursor"]
    if (
        page_info["hasNextPage"] is not False
        or (end_cursor is not None and not isinstance(end_cursor, str))
        or (has_nodes and not isinstance(end_cursor, str))
        or (not has_nodes and end_cursor is not None)
    ):
        raise LiveCaptureError("probe_graphql_invalid")


def _validate_graphql_connection(
    value: object,
    validate_node: Callable[[object], None],
) -> None:
    connection = _exact_probe_object(value, {"nodes", "pageInfo", "totalCount"})
    nodes = connection["nodes"]
    total_count = connection["totalCount"]
    if (
        not isinstance(nodes, list)
        or len(nodes) > 100
        or isinstance(total_count, bool)
        or not isinstance(total_count, int)
        or total_count != len(nodes)
    ):
        raise LiveCaptureError("probe_graphql_invalid")
    for node in nodes:
        validate_node(node)
    _validate_graphql_page_info(connection["pageInfo"], has_nodes=bool(nodes))


def _validate_graphql_file_node(value: object) -> None:
    node = _exact_probe_object(value, {"path", "viewerViewedState"})
    if (
        not isinstance(node["path"], str)
        or not node["path"]
        or node["viewerViewedState"] not in {"DISMISSED", "UNVIEWED", "VIEWED"}
    ):
        raise LiveCaptureError("probe_graphql_invalid")


def _validate_graphql_comment_node(value: object) -> None:
    node = _exact_probe_object(
        value,
        {
            "author",
            "bodyText",
            "id",
            "line",
            "originalLine",
            "outdated",
            "path",
            "replyTo",
        },
    )
    if not isinstance(node["id"], str) or not isinstance(node["bodyText"], str):
        raise LiveCaptureError("probe_graphql_invalid")
    if node["path"] is not None and not isinstance(node["path"], str):
        raise LiveCaptureError("probe_graphql_invalid")
    for field in ("line", "originalLine"):
        if node[field] is not None and (
            isinstance(node[field], bool) or not isinstance(node[field], int)
        ):
            raise LiveCaptureError("probe_graphql_invalid")
    if not isinstance(node["outdated"], bool):
        raise LiveCaptureError("probe_graphql_invalid")
    author = node["author"]
    if author is not None:
        author_record = _exact_probe_object(author, {"id", "login"})
        if not all(isinstance(author_record[field], str) for field in ("id", "login")):
            raise LiveCaptureError("probe_graphql_invalid")
    reply_to = node["replyTo"]
    if reply_to is not None:
        reply_record = _exact_probe_object(reply_to, {"id"})
        if not isinstance(reply_record["id"], str):
            raise LiveCaptureError("probe_graphql_invalid")


def _validate_graphql_thread_node(value: object) -> None:
    node = _exact_probe_object(
        value,
        {
            "comments",
            "id",
            "isOutdated",
            "isResolved",
            "line",
            "originalLine",
            "path",
            "subjectType",
            "viewerCanResolve",
            "viewerCanUnresolve",
        },
    )
    if (
        not isinstance(node["id"], str)
        or not isinstance(node["path"], str)
        or node["subjectType"] not in {"FILE", "LINE"}
    ):
        raise LiveCaptureError("probe_graphql_invalid")
    for field in ("isOutdated", "isResolved", "viewerCanResolve", "viewerCanUnresolve"):
        if not isinstance(node[field], bool):
            raise LiveCaptureError("probe_graphql_invalid")
    for field in ("line", "originalLine"):
        if node[field] is not None and (
            isinstance(node[field], bool) or not isinstance(node[field], int)
        ):
            raise LiveCaptureError("probe_graphql_invalid")
    _validate_graphql_connection(node["comments"], _validate_graphql_comment_node)


def _validate_graphql_review_reply(document: object) -> None:
    root = _exact_probe_object(document, {"data"})
    data = _exact_probe_object(root["data"], {"repository"})
    repository = _exact_probe_object(data["repository"], {"id", "pullRequest"})
    if not isinstance(repository["id"], str):
        raise LiveCaptureError("probe_graphql_invalid")
    pull_request = _exact_probe_object(
        repository["pullRequest"], {"files", "id", "reviewThreads"}
    )
    if not isinstance(pull_request["id"], str):
        raise LiveCaptureError("probe_graphql_invalid")
    _validate_graphql_connection(pull_request["files"], _validate_graphql_file_node)
    _validate_graphql_connection(
        pull_request["reviewThreads"], _validate_graphql_thread_node
    )


def _validate_probe_reply(request: ProbeRequest, reply: WireReply) -> None:
    if request.operation == "contents_info_refs":
        if not reply.body.startswith(b"001e# service=git-upload-pack\n"):
            raise LiveCaptureError("probe_git_advertisement_invalid")
        return
    if request.operation == "contents_upload_pack":
        if len(reply.body) < 4 or not all(
            character in b"0123456789abcdefABCDEF" for character in reply.body[:4]
        ):
            raise LiveCaptureError("probe_git_result_invalid")
        return
    document = _probe_json(reply.body)
    if request.operation == "metadata":
        if (
            not isinstance(document, dict)
            or document.get("id") != 1328277631
            or document.get("full_name") != "MikeMarcin/BPR-Test"
        ):
            raise LiveCaptureError("probe_repository_identity_invalid")
    elif request.operation == "pull_request":
        if not isinstance(document, dict) or document.get("number") != 1:
            raise LiveCaptureError("probe_pull_request_identity_invalid")
    elif request.operation == "checks":
        if not isinstance(document, dict) or not isinstance(document.get("check_runs"), list):
            raise LiveCaptureError("probe_checks_invalid")
    elif request.operation == "review_threads":
        _validate_graphql_review_reply(document)
    elif not isinstance(document, list):
        raise LiveCaptureError("probe_rest_collection_invalid")


def run_read_only_probes(access_token: str) -> list[dict[str, object]]:
    wire = ReadOnlyProbeWire()
    for request in _probe_requests():
        reply = wire.perform(request, access_token)
        _validate_probe_reply(request, reply)
    return _offline_module().expected_route_probes()


def _required_text(value: object, name: str, maximum: int) -> str:
    if not isinstance(value, str) or not value or len(value.encode()) > maximum:
        raise LiveCaptureError(f"{name}_invalid")
    return value


def _required_int(value: object, name: str, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 < value <= maximum:
        raise LiveCaptureError(f"{name}_invalid")
    return value


@dataclass
class DeviceGrant:
    device_code: str
    user_code: str
    expires_in: int
    interval: int
    reply: WireReply


@dataclass(frozen=True)
class TokenPoll:
    outcome: str
    reply: WireReply
    interval: int | None = None


def validate_client_id(client_id: str) -> str:
    if client_id != APPROVED_PUBLIC_CLIENT_ID or not PUBLIC_CLIENT_ID.fullmatch(client_id):
        raise LiveCaptureError("client_id_invalid")
    return client_id


def request_device_grant(
    wire: Wire,
    client_id: str,
    *,
    timeout_seconds: float = REQUEST_TIMEOUT_SECONDS,
) -> DeviceGrant:
    validate_client_id(client_id)
    reply = wire.post_form(
        DEVICE_URL,
        {"client_id": client_id},
        timeout_seconds=timeout_seconds,
    )
    if reply.status != 200:
        raise LiveCaptureError("device_response_status_invalid")
    value = _closed_json(reply.body)
    expected = {"device_code", "user_code", "verification_uri", "expires_in", "interval"}
    if set(value) != expected or value.get("verification_uri") != VERIFY_URL:
        raise LiveCaptureError("device_response_invalid")
    user_code = _required_text(value["user_code"], "user_code", 64)
    if not USER_CODE.fullmatch(user_code):
        raise LiveCaptureError("user_code_invalid")
    return DeviceGrant(
        device_code=_required_text(value["device_code"], "device_code", 512),
        user_code=user_code,
        expires_in=_required_int(value["expires_in"], "expires_in", 3600),
        interval=_required_int(value["interval"], "interval", 300),
        reply=reply,
    )


def request_device_flow_disabled(
    wire: Wire,
    client_id: str,
    *,
    timeout_seconds: float = REQUEST_TIMEOUT_SECONDS,
) -> WireReply:
    validate_client_id(client_id)
    reply = wire.post_form(
        DEVICE_URL,
        {"client_id": client_id},
        timeout_seconds=timeout_seconds,
    )
    value = _closed_json(reply.body)
    if reply.status not in {200, 400}:
        raise LiveCaptureError("device_flow_disabled_status_invalid")
    if set(value) != ERROR_FIELDS or value.get("error") != "device_flow_disabled":
        raise LiveCaptureError("device_flow_disabled_not_observed")
    _required_text(value["error_description"], "error_description", 512)
    _required_text(value["error_uri"], "error_uri", 512)
    return reply


def poll_token(
    wire: Wire,
    client_id: str,
    device_code: str,
    *,
    timeout_seconds: float = REQUEST_TIMEOUT_SECONDS,
) -> TokenPoll:
    validate_client_id(client_id)
    reply = wire.post_form(
        TOKEN_URL,
        {
            "client_id": client_id,
            "device_code": device_code,
            "grant_type": DEVICE_GRANT,
        },
        timeout_seconds=timeout_seconds,
    )
    value = _closed_json(reply.body)
    if "error" in value:
        if reply.status not in {200, 400}:
            raise LiveCaptureError("token_error_status_invalid")
        error = _required_text(value["error"], "error", 64)
        if error not in {"authorization_pending", "slow_down", *TERMINALS}:
            raise LiveCaptureError("token_error_unknown")
        expected_fields = SLOW_DOWN_FIELDS if error == "slow_down" else ERROR_FIELDS
        if set(value) != expected_fields:
            raise LiveCaptureError("token_error_schema_invalid")
        _required_text(value["error_description"], "error_description", 512)
        _required_text(value["error_uri"], "error_uri", 512)
        interval = (
            _required_int(value["interval"], "slow_down_interval", 300)
            if error == "slow_down"
            else None
        )
        return TokenPoll(error, reply, interval)
    expected = {
        "access_token",
        "expires_in",
        "refresh_token",
        "refresh_token_expires_in",
        "scope",
        "token_type",
    }
    if reply.status != 200:
        raise LiveCaptureError("token_response_status_invalid")
    if set(value) != expected or value.get("scope") != "" or value.get("token_type") != "bearer":
        raise LiveCaptureError("token_response_invalid")
    _required_text(value["access_token"], "access_token", 4096)
    _required_text(value["refresh_token"], "refresh_token", 4096)
    return TokenPoll("token_success", reply)


def _access_token(reply: WireReply) -> str:
    value = _closed_json(reply.body)
    if set(value) != {
        "access_token",
        "expires_in",
        "refresh_token",
        "refresh_token_expires_in",
        "scope",
        "token_type",
    }:
        raise LiveCaptureError("token_response_invalid")
    return _required_text(value["access_token"], "access_token", 4096)


def show_device_code(user_code: str) -> None:
    subprocess.run(
        ["/usr/bin/open", VERIFY_URL],
        check=True,
        env=FIXED_ENVIRONMENT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    script = (
        'display dialog "Enter this code at GitHub:\\n\\n'
        + user_code.replace("\\", "\\\\").replace('"', '\\"')
        + '" with title "BetterPullReview device authorization" '
        'buttons {"Continue"} default button 1'
    )
    subprocess.run(
        ["/usr/bin/osascript", "-"],
        input=script,
        text=True,
        check=True,
        env=FIXED_ENVIRONMENT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


@dataclass
class CourseEvidence:
    device_reply: WireReply | None
    outcomes: dict[str, WireReply]
    executed_at_utc: str
    capture_runner_sha256: str
    route_probes: list[dict[str, object]] | None = None
    slow_down_original_reservation_sha256: str | None = None
    slow_down_recovery_reservation_sha256: str | None = None
    device_flow_transition: bytes | None = None


@dataclass(frozen=True)
class SlowDownReservation:
    original_marker_sha256: str
    recovery_marker_sha256: str
    capture_runner_sha256: str


def _bounded_sleep(
    duration: float,
    deadline: float,
    *,
    sleep: Callable[[float], None],
    monotonic: Callable[[], float],
) -> None:
    if duration < 0.0 or monotonic() + duration > deadline:
        raise LiveCaptureError("course_deadline_refused")
    sleep(duration)
    if monotonic() > deadline:
        raise LiveCaptureError("course_deadline_refused")


def _poll_before_deadline(
    wire: Wire,
    client_id: str,
    device_code: str,
    deadline: float,
    *,
    monotonic: Callable[[], float],
) -> TokenPoll:
    remaining = deadline - monotonic()
    if remaining <= 0.0:
        raise LiveCaptureError("course_deadline_refused")
    result = poll_token(
        wire,
        client_id,
        device_code,
        timeout_seconds=min(REQUEST_TIMEOUT_SECONDS, remaining),
    )
    if monotonic() > deadline:
        raise LiveCaptureError("course_deadline_refused")
    return result


def _remaining_timeout(deadline: float, monotonic: Callable[[], float]) -> float:
    remaining = deadline - monotonic()
    if remaining <= 0.0:
        raise LiveCaptureError("course_deadline_refused")
    return min(REQUEST_TIMEOUT_SECONDS, remaining)


def run_authorization_course(
    wire: Wire,
    client_id: str,
    course: str,
    *,
    present: Callable[[str], None] = show_device_code,
    probe: Callable[[str], list[dict[str, object]]] | None = None,
    slow_down_reservation: SlowDownReservation | None = None,
    sleep: Callable[[float], None] = time.sleep,
    monotonic: Callable[[], float] = time.monotonic,
) -> CourseEvidence:
    """Run one closed attended course without sharing transient grants."""
    validate_client_id(client_id)
    if course not in COURSES:
        raise LiveCaptureError("course_invalid")
    executed_at_utc = _execution_timestamp()
    _timestamp(executed_at_utc, "course_timestamp_invalid")
    capture_runner_sha256 = LOADED_RUNNER_SHA256
    _require_sha256(capture_runner_sha256, "capture_runner_digest_invalid")
    if course == "slow-down":
        if (
            slow_down_reservation is None
            or slow_down_reservation.capture_runner_sha256
            != capture_runner_sha256
        ):
            raise LiveCaptureError("slow_down_reservation_required")
        _require_sha256(
            slow_down_reservation.original_marker_sha256,
            "slow_down_reservation_invalid",
        )
        _require_sha256(
            slow_down_reservation.recovery_marker_sha256,
            "slow_down_reservation_invalid",
        )
    elif slow_down_reservation is not None:
        raise LiveCaptureError("slow_down_reservation_unexpected")
    course_started = monotonic()
    total_deadline = course_started + float(TOTAL_COURSE_SECONDS)
    if course == "device-flow-disabled":
        reply = request_device_flow_disabled(
            wire,
            client_id,
            timeout_seconds=_remaining_timeout(total_deadline, monotonic),
        )
        if monotonic() > total_deadline:
            raise LiveCaptureError("course_deadline_refused")
        return CourseEvidence(
            None,
            {"device_flow_disabled": reply},
            executed_at_utc,
            capture_runner_sha256,
        )

    grant = request_device_grant(
        wire,
        client_id,
        timeout_seconds=_remaining_timeout(total_deadline, monotonic),
    )
    if monotonic() > total_deadline:
        raise LiveCaptureError("course_deadline_refused")
    grant_started = monotonic()
    normal_deadline = min(
        grant_started + float(grant.expires_in),
        total_deadline,
    )
    outcomes: dict[str, WireReply] = {}

    if course == "expired-token":
        # GitHub may evaluate expiry on a coarser service-side clock than this
        # process. Wait a fixed margin and still make exactly one post-expiry
        # request; a non-expired reply fails closed instead of polling again.
        expiry_probe_at = (
            grant_started
            + float(grant.expires_in)
            + float(EXPIRY_CAPTURE_GRACE_SECONDS)
        )
        _bounded_sleep(
            expiry_probe_at - monotonic(),
            total_deadline,
            sleep=sleep,
            monotonic=monotonic,
        )
        poll = _poll_before_deadline(
            wire,
            client_id,
            grant.device_code,
            total_deadline,
            monotonic=monotonic,
        )
        if poll.outcome != "expired_token":
            raise LiveCaptureError("expired_token_not_observed")
        return CourseEvidence(
            grant.reply,
            {poll.outcome: poll.reply},
            executed_at_utc,
            capture_runner_sha256,
        )

    _bounded_sleep(
        float(grant.interval),
        normal_deadline,
        sleep=sleep,
        monotonic=monotonic,
    )
    poll = _poll_before_deadline(
        wire,
        client_id,
        grant.device_code,
        normal_deadline,
        monotonic=monotonic,
    )
    if poll.outcome != "authorization_pending":
        raise LiveCaptureError("pending_not_observed")
    outcomes[poll.outcome] = poll.reply

    if course == "slow-down":
        # The sole RFC-authorized capture-only early poll is isolated here.
        poll = _poll_before_deadline(
            wire,
            client_id,
            grant.device_code,
            normal_deadline,
            monotonic=monotonic,
        )
        if poll.outcome != "slow_down":
            raise LiveCaptureError("slow_down_not_observed")
        if poll.interval != grant.interval + 5:
            raise LiveCaptureError("slow_down_interval_progression_invalid")
        outcomes[poll.outcome] = poll.reply
        return CourseEvidence(
            grant.reply,
            outcomes,
            executed_at_utc,
            capture_runner_sha256,
            slow_down_original_reservation_sha256=(
                slow_down_reservation.original_marker_sha256
            ),
            slow_down_recovery_reservation_sha256=(
                slow_down_reservation.recovery_marker_sha256
            ),
        )

    present(grant.user_code)
    if monotonic() > normal_deadline:
        raise LiveCaptureError("course_deadline_refused")

    interval = grant.interval
    expected = "access_denied" if course == "access-denied" else "token_success"
    while True:
        _bounded_sleep(
            float(interval),
            normal_deadline,
            sleep=sleep,
            monotonic=monotonic,
        )
        poll = _poll_before_deadline(
            wire,
            client_id,
            grant.device_code,
            normal_deadline,
            monotonic=monotonic,
        )
        if poll.outcome == "authorization_pending":
            outcomes.setdefault(poll.outcome, poll.reply)
            continue
        if poll.outcome == "slow_down":
            if poll.interval != interval + 5:
                raise LiveCaptureError("slow_down_interval_progression_invalid")
            interval = poll.interval
            continue
        if poll.outcome != expected:
            raise LiveCaptureError("unexpected_terminal")
        outcomes[poll.outcome] = poll.reply
        probes = (
            probe(_access_token(poll.reply))
            if poll.outcome == "token_success" and probe
            else None
        )
        return CourseEvidence(
            grant.reply,
            outcomes,
            executed_at_utc,
            capture_runner_sha256,
            route_probes=probes,
        )


def _offline_module():
    path = Path(__file__).with_name("capture_rfc0016_device_contracts.py")
    spec = importlib.util.spec_from_file_location("rfc0016_offline_capture", path)
    if spec is None or spec.loader is None:
        raise LiveCaptureError("offline_validator_unavailable")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _write_private(directory_fd: int, name: str, payload: bytes) -> None:
    if not name or "/" in name or name in {".", ".."}:
        raise LiveCaptureError("output_name_invalid")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(name, flags, 0o600, dir_fd=directory_fd)
    try:
        with os.fdopen(descriptor, "wb", closefd=False) as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
    finally:
        os.close(descriptor)


def _read_private_canonical_json(path: Path, diagnostic: str) -> tuple[dict[str, object], bytes]:
    flags = os.O_RDONLY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise LiveCaptureError(diagnostic) from error
    try:
        metadata = os.fstat(descriptor)
        if (
            not stat.S_ISREG(metadata.st_mode)
            or metadata.st_uid != os.getuid()
            or metadata.st_mode & 0o077
            or metadata.st_size > MAX_RESPONSE_BYTES
        ):
            raise LiveCaptureError(diagnostic)
        chunks: list[bytes] = []
        remaining = MAX_RESPONSE_BYTES + 1
        while remaining:
            chunk = os.read(descriptor, min(remaining, 16 * 1024))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        payload = b"".join(chunks)
        if len(payload) > MAX_RESPONSE_BYTES:
            raise LiveCaptureError(diagnostic)
    finally:
        os.close(descriptor)
    document = _closed_json(payload)
    if _offline_module().canonical_json(document) != payload:
        raise LiveCaptureError(diagnostic)
    return document, payload


def _validate_original_slow_down_attempt(
    authority: object,
    authority_payload: bytes,
    marker: object,
) -> tuple[str, datetime]:
    diagnostic = "slow_down_original_attempt_invalid"
    if not isinstance(authority, dict) or set(authority) != {
        "authority_amendment_id",
        "authority_id",
        "authority_subject_sha256",
        "authorized_at_utc",
        "course",
        "previous_early_poll_consumed",
        "replacement_early_poll_limit",
        "schema_name",
        "schema_version",
        "state",
    }:
        raise LiveCaptureError(diagnostic)
    offline = _offline_module()
    if (
        authority["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or authority["authority_id"] != SLOW_DOWN_REPLACEMENT_AUTHORITY_ID
        or authority["authority_subject_sha256"] != offline.AUTHORITY_SUBJECT_SHA256
        or authority["course"] != "slow-down"
        or authority["previous_early_poll_consumed"] is not True
        or type(authority["replacement_early_poll_limit"]) is not int
        or authority["replacement_early_poll_limit"] != 1
        or authority["schema_name"]
        != "bpr.rfc0016.slow_down_replacement_authority"
        or type(authority["schema_version"]) is not int
        or authority["schema_version"] != 1
        or authority["state"] != "replacement_authorized"
    ):
        raise LiveCaptureError(diagnostic)
    authorized_at = _timestamp(authority["authorized_at_utc"], diagnostic)
    if not isinstance(marker, dict) or set(marker) != {
        "authority_amendment_id",
        "authority_id",
        "authority_record_sha256",
        "authority_subject_sha256",
        "capture_runner_sha256",
        "course",
        "schema_name",
        "schema_version",
        "spent_before_traffic_at_utc",
        "state",
    }:
        raise LiveCaptureError(diagnostic)
    failed_runner_sha256 = _require_sha256(
        marker.get("capture_runner_sha256"),
        diagnostic,
    )
    if (
        marker["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or marker["authority_id"] != SLOW_DOWN_REPLACEMENT_AUTHORITY_ID
        or marker["authority_record_sha256"] != _sha256(authority_payload)
        or marker["authority_subject_sha256"] != offline.AUTHORITY_SUBJECT_SHA256
        or marker["course"] != "slow-down"
        or marker["schema_name"] != "bpr.rfc0016.slow_down_reservation"
        or type(marker["schema_version"]) is not int
        or marker["schema_version"] != 1
        or marker["state"] != "spent"
    ):
        raise LiveCaptureError(diagnostic)
    spent_at = _timestamp(marker["spent_before_traffic_at_utc"], diagnostic)
    if authorized_at > spent_at:
        raise LiveCaptureError(diagnostic)
    return failed_runner_sha256, spent_at


def _reserve_slow_down_recovery_once(
    capture_runner_sha256: str,
) -> SlowDownReservation:
    """Spend the single parser-repair recovery poll before any traffic."""
    _require_sha256(capture_runner_sha256, "capture_runner_digest_invalid")
    state_paths = (
        SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH,
        SLOW_DOWN_SPENT_MARKER_PATH,
        SLOW_DOWN_RECOVERY_AUTHORITY_PATH,
        SLOW_DOWN_RECOVERY_SPENT_MARKER_PATH,
    )
    try:
        parents_and_names = tuple(_private_parent(path) for path in state_paths)
    except (LiveCaptureError, OSError) as error:
        raise LiveCaptureError("slow_down_recovery_authority_required") from error
    state_parents = {parent for parent, _name in parents_and_names}
    if len(state_parents) != 1:
        raise LiveCaptureError("slow_down_state_path_invalid")
    original_authority, original_authority_payload = _read_private_canonical_json(
        SLOW_DOWN_REPLACEMENT_AUTHORITY_PATH,
        "slow_down_original_attempt_required",
    )
    original_marker, original_marker_payload = _read_private_canonical_json(
        SLOW_DOWN_SPENT_MARKER_PATH,
        "slow_down_original_attempt_required",
    )
    failed_runner_sha256, original_spent_at = _validate_original_slow_down_attempt(
        original_authority,
        original_authority_payload,
        original_marker,
    )
    recovery_authority, recovery_authority_payload = _read_private_canonical_json(
        SLOW_DOWN_RECOVERY_AUTHORITY_PATH,
        "slow_down_recovery_authority_required",
    )
    if not isinstance(recovery_authority, dict) or set(recovery_authority) != {
        "authority_amendment_id",
        "authority_id",
        "authority_subject_sha256",
        "authorized_at_utc",
        "course",
        "failed_attempt_authority_sha256",
        "failed_attempt_capture_runner_sha256",
        "failed_attempt_spent_marker_sha256",
        "parser_repair_capture_runner_sha256",
        "recovery_early_poll_limit",
        "schema_name",
        "schema_version",
        "state",
    }:
        raise LiveCaptureError("slow_down_recovery_authority_invalid")
    offline = _offline_module()
    recovery_authorized_at = _timestamp(
        recovery_authority["authorized_at_utc"],
        "slow_down_recovery_authority_invalid",
    )
    if (
        recovery_authority["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or recovery_authority["authority_id"] != SLOW_DOWN_RECOVERY_AUTHORITY_ID
        or recovery_authority["authority_subject_sha256"]
        != offline.AUTHORITY_SUBJECT_SHA256
        or recovery_authority["course"] != "slow-down"
        or recovery_authority["failed_attempt_authority_sha256"]
        != _sha256(original_authority_payload)
        or recovery_authority["failed_attempt_capture_runner_sha256"]
        != failed_runner_sha256
        or recovery_authority["failed_attempt_spent_marker_sha256"]
        != _sha256(original_marker_payload)
        or recovery_authority["parser_repair_capture_runner_sha256"]
        != capture_runner_sha256
        or type(recovery_authority["recovery_early_poll_limit"]) is not int
        or recovery_authority["recovery_early_poll_limit"] != 1
        or recovery_authority["schema_name"]
        != "bpr.rfc0016.slow_down_parser_repair_recovery_authority"
        or type(recovery_authority["schema_version"]) is not int
        or recovery_authority["schema_version"] != 1
        or recovery_authority["state"] != "parser_repair_recovery_authorized"
        or failed_runner_sha256 == capture_runner_sha256
        or recovery_authorized_at < original_spent_at
    ):
        raise LiveCaptureError("slow_down_recovery_authority_invalid")
    spent_at_utc = _execution_timestamp()
    spent_at = _timestamp(spent_at_utc, "slow_down_recovery_reservation_invalid")
    if spent_at < recovery_authorized_at:
        raise LiveCaptureError("slow_down_recovery_reservation_invalid")
    recovery_marker = {
        "authority_amendment_id": AUTHORITY_AMENDMENT_ID,
        "authority_id": SLOW_DOWN_RECOVERY_AUTHORITY_ID,
        "authority_record_sha256": _sha256(recovery_authority_payload),
        "authority_subject_sha256": offline.AUTHORITY_SUBJECT_SHA256,
        "capture_runner_sha256": capture_runner_sha256,
        "course": "slow-down",
        "failed_attempt_authority_sha256": _sha256(original_authority_payload),
        "failed_attempt_spent_marker_sha256": _sha256(original_marker_payload),
        "schema_name": "bpr.rfc0016.slow_down_parser_repair_recovery_reservation",
        "schema_version": 1,
        "spent_before_traffic_at_utc": spent_at_utc,
        "state": "spent",
    }
    recovery_marker_payload = offline.canonical_json(recovery_marker)
    state_parent = next(iter(state_parents))
    recovery_spent_name = parents_and_names[-1][1]
    directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    if hasattr(os, "O_NOFOLLOW"):
        directory_flags |= os.O_NOFOLLOW
    parent_fd = os.open(state_parent, directory_flags)
    try:
        try:
            _write_private(parent_fd, recovery_spent_name, recovery_marker_payload)
        except FileExistsError as error:
            raise LiveCaptureError("slow_down_recovery_reservation_spent") from error
        os.fsync(parent_fd)
    finally:
        os.close(parent_fd)
    return SlowDownReservation(
        _sha256(original_marker_payload),
        _sha256(recovery_marker_payload),
        capture_runner_sha256,
    )


def load_device_flow_transition_observation(
    path: Path,
    *,
    executed_at_utc: str,
) -> bytes:
    _document, payload = _read_private_canonical_json(
        path,
        "device_flow_transition_invalid",
    )
    return load_device_flow_transition_observation_from_bytes(
        payload,
        executed_at_utc=executed_at_utc,
    )


def load_device_flow_transition_observation_from_bytes(
    payload: bytes,
    *,
    executed_at_utc: str,
) -> bytes:
    document = _closed_json(payload)
    if _offline_module().canonical_json(document) != payload:
        raise LiveCaptureError("device_flow_transition_invalid")
    if set(document) != {
        "application_id",
        "authority_amendment_id",
        "authority_subject_sha256",
        "observation_method",
        "owner_login",
        "public_client_id",
        "schema_name",
        "schema_version",
        "states",
    }:
        raise LiveCaptureError("device_flow_transition_invalid")
    offline = _offline_module()
    if (
        document["application_id"] != APPROVED_APPLICATION_ID
        or document["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or document["authority_subject_sha256"] != offline.AUTHORITY_SUBJECT_SHA256
        or document["observation_method"] != "attended_github_settings_page"
        or document["owner_login"] != "MikeMarcin"
        or document["public_client_id"] != APPROVED_PUBLIC_CLIENT_ID
        or document["schema_name"]
        != "bpr.rfc0016.device_flow_transition_observation"
        or document["schema_version"] != 1
    ):
        raise LiveCaptureError("device_flow_transition_invalid")
    states = document["states"]
    expected = (
        ("before_enabled", True),
        ("disabled_for_capture", False),
        ("restored_enabled", True),
    )
    if not isinstance(states, list) or len(states) != len(expected):
        raise LiveCaptureError("device_flow_transition_invalid")
    observed_times: list[datetime] = []
    for state_value, (phase, enabled) in zip(states, expected, strict=True):
        if not isinstance(state_value, dict) or set(state_value) != {
            "device_flow_enabled",
            "observed_at_utc",
            "phase",
        }:
            raise LiveCaptureError("device_flow_transition_invalid")
        if (
            state_value["phase"] != phase
            or state_value["device_flow_enabled"] is not enabled
        ):
            raise LiveCaptureError("device_flow_transition_invalid")
        observed_times.append(
            _timestamp(
                state_value["observed_at_utc"],
                "device_flow_transition_invalid",
            )
        )
    if not observed_times[0] < observed_times[1] < observed_times[2]:
        raise LiveCaptureError("device_flow_transition_invalid")
    execution_time = _timestamp(executed_at_utc, "course_timestamp_invalid")
    if not observed_times[1] <= execution_time <= observed_times[2]:
        raise LiveCaptureError("device_flow_transition_invalid")
    return payload


def confirm_device_flow_restored() -> None:
    script = (
        'display dialog "Restore Device Flow in the designated GitHub App, '
        'record the complete attended settings transition, then continue." '
        'with title "BetterPullReview device-flow capture" '
        'buttons {"Continue"} default button 1'
    )
    subprocess.run(
        ["/usr/bin/osascript", "-"],
        input=script,
        text=True,
        check=True,
        env=FIXED_ENVIRONMENT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def _registration_observation(offline, client_id: str) -> dict[str, object]:
    return {
        "application_kind": "github_app",
        "application_visibility": "private",
        "callback_url_present": False,
        "device_flow_enabled": True,
        "installation_owner_login": "MikeMarcin",
        "installation_policy": "owner_only",
        "installation_repositories": ["MikeMarcin/BPR-Test"],
        "installation_repository_selection": "selected_repositories",
        "oauth_client_type": "public",
        "oauth_scopes": [],
        "owner_kind": "user",
        "owner_login": "MikeMarcin",
        "permissions": offline.REGISTRATION_PERMISSIONS,
        "public_client_id": client_id,
        "refresh_policy": "expiring_user_token_with_refresh",
        "user_access_token_expiration_enabled": True,
        "webhook_active": False,
    }


@dataclass(frozen=True)
class RegistrationCapture:
    captured_at_utc: str
    observation: dict[str, object]


def load_registration_observation(path: Path) -> RegistrationCapture:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_RESPONSE_BYTES:
        raise LiveCaptureError("registration_observation_invalid")
    document = _closed_json(path.read_bytes())
    if set(document) != {"captured_at_utc", "observation", "schema_name", "schema_version"}:
        raise LiveCaptureError("registration_observation_invalid")
    if (
        document.get("schema_name") != "bpr.rfc0016.registration_observation"
        or document.get("schema_version") != 1
        or not isinstance(document.get("captured_at_utc"), str)
        or not isinstance(document.get("observation"), dict)
    ):
        raise LiveCaptureError("registration_observation_invalid")
    captured_at = document["captured_at_utc"]
    observed = document["observation"]
    offline = _offline_module()
    expected = _registration_observation(offline, APPROVED_PUBLIC_CLIENT_ID)
    if observed != expected:
        raise LiveCaptureError("registration_observation_invalid")
    offline.sanitize_registration(
        observed,
        captured_at_utc=captured_at,
        source_kind="unattested_capture",
    )
    return RegistrationCapture(captured_at, observed)


def _course_execution_document(
    course: str,
    evidence: CourseEvidence,
    registration_record_sha256: str,
) -> dict[str, object]:
    transition_sha256 = (
        _sha256(evidence.device_flow_transition)
        if evidence.device_flow_transition is not None
        else None
    )
    document = {
        "authority_amendment_id": AUTHORITY_AMENDMENT_ID,
        "authority_subject_sha256": _offline_module().AUTHORITY_SUBJECT_SHA256,
        "bindings": {
            "device_flow_transition_sha256": transition_sha256,
            "slow_down_original_reservation_sha256": (
                evidence.slow_down_original_reservation_sha256
            ),
            "slow_down_recovery_reservation_sha256": (
                evidence.slow_down_recovery_reservation_sha256
            ),
        },
        "capture_runner_sha256": evidence.capture_runner_sha256,
        "course": course,
        "executed_at_utc": evidence.executed_at_utc,
        "registration_record_sha256": registration_record_sha256,
        "schema_name": "bpr.rfc0016.course_execution",
        "schema_version": 1,
    }
    _validate_course_execution_document(
        course,
        document,
        registration_record_sha256=registration_record_sha256,
    )
    return document


def _validate_course_execution_document(
    course: str,
    value: object,
    *,
    registration_record_sha256: str | None = None,
) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != {
        "authority_amendment_id",
        "authority_subject_sha256",
        "bindings",
        "capture_runner_sha256",
        "course",
        "executed_at_utc",
        "registration_record_sha256",
        "schema_name",
        "schema_version",
    }:
        raise LiveCaptureError("course_execution_invalid")
    offline = _offline_module()
    if (
        course not in COURSES
        or value["course"] != course
        or value["authority_amendment_id"] != AUTHORITY_AMENDMENT_ID
        or value["authority_subject_sha256"] != offline.AUTHORITY_SUBJECT_SHA256
        or value["schema_name"] != "bpr.rfc0016.course_execution"
        or value["schema_version"] != 1
    ):
        raise LiveCaptureError("course_execution_invalid")
    _timestamp(value["executed_at_utc"], "course_execution_invalid")
    _require_sha256(value["capture_runner_sha256"], "course_execution_invalid")
    observed_registration = _require_sha256(
        value["registration_record_sha256"],
        "course_execution_invalid",
    )
    if (
        registration_record_sha256 is not None
        and observed_registration != registration_record_sha256
    ):
        raise LiveCaptureError("course_execution_invalid")
    bindings = value["bindings"]
    if not isinstance(bindings, dict) or set(bindings) != {
        "device_flow_transition_sha256",
        "slow_down_original_reservation_sha256",
        "slow_down_recovery_reservation_sha256",
    }:
        raise LiveCaptureError("course_execution_invalid")
    transition_digest = bindings["device_flow_transition_sha256"]
    original_reservation_digest = bindings[
        "slow_down_original_reservation_sha256"
    ]
    recovery_reservation_digest = bindings[
        "slow_down_recovery_reservation_sha256"
    ]
    if course == "device-flow-disabled":
        _require_sha256(transition_digest, "course_execution_invalid")
    elif transition_digest is not None:
        raise LiveCaptureError("course_execution_invalid")
    if course == "slow-down":
        _require_sha256(original_reservation_digest, "course_execution_invalid")
        _require_sha256(recovery_reservation_digest, "course_execution_invalid")
    elif (
        original_reservation_digest is not None
        or recovery_reservation_digest is not None
    ):
        raise LiveCaptureError("course_execution_invalid")
    return value


def _course_projections(
    registration_capture: RegistrationCapture,
    course: str,
    evidence: CourseEvidence,
) -> dict[str, bytes]:
    expected_outcomes = {
        "access-denied": {"authorization_pending", "access_denied"},
        "device-flow-disabled": {"device_flow_disabled"},
        "expired-token": {"expired_token"},
        "slow-down": {"authorization_pending", "slow_down"},
        "token-success": {"authorization_pending", "token_success"},
    }
    if course not in expected_outcomes or set(evidence.outcomes) != expected_outcomes[course]:
        raise LiveCaptureError("course_evidence_invalid")
    if (course == "device-flow-disabled") != (evidence.device_reply is None):
        raise LiveCaptureError("course_evidence_invalid")
    if (course == "token-success") != (evidence.route_probes is not None):
        raise LiveCaptureError("course_evidence_invalid")
    if (course == "device-flow-disabled") != (
        evidence.device_flow_transition is not None
    ):
        raise LiveCaptureError("course_evidence_invalid")
    has_slow_down_recovery = (
        evidence.slow_down_original_reservation_sha256 is not None
        and evidence.slow_down_recovery_reservation_sha256 is not None
    )
    if (course == "slow-down") != has_slow_down_recovery:
        raise LiveCaptureError("course_evidence_invalid")
    if (
        evidence.slow_down_original_reservation_sha256 is None
    ) != (evidence.slow_down_recovery_reservation_sha256 is None):
        raise LiveCaptureError("course_evidence_invalid")
    _timestamp(evidence.executed_at_utc, "course_timestamp_invalid")
    _require_sha256(
        evidence.capture_runner_sha256,
        "capture_runner_digest_invalid",
    )

    offline = _offline_module()
    client_id = registration_capture.observation.get("public_client_id")
    if not isinstance(client_id, str):
        raise LiveCaptureError("registration_observation_invalid")
    validate_client_id(client_id)
    registration = offline.sanitize_registration(
        registration_capture.observation,
        captured_at_utc=registration_capture.captured_at_utc,
        source_kind="unattested_capture",
    )
    registration_payload = offline.canonical_json(registration)
    registration_digest = offline._sha256(registration_payload)
    projections: dict[str, bytes] = {
        "course-execution.json": offline.canonical_json(
            _course_execution_document(course, evidence, registration_digest)
        ),
        "registration.json": registration_payload,
    }
    if evidence.device_flow_transition is not None:
        transition_payload = load_device_flow_transition_observation_from_bytes(
            evidence.device_flow_transition,
            executed_at_utc=evidence.executed_at_utc,
        )
        projections["device-flow-transition.json"] = transition_payload
    if evidence.device_reply is not None:
        projections["outcomes/device_code_success.json"] = offline.canonical_json(
            offline.sanitize_outcome(
                "device_code_success",
                evidence.device_reply.body,
                http_status=evidence.device_reply.status,
                captured_at_utc=evidence.executed_at_utc,
                source_kind="unattested_capture",
                registration_record_sha256=registration_digest,
            )
        )
    for outcome, reply in evidence.outcomes.items():
        projections[f"outcomes/{outcome}.json"] = offline.canonical_json(
            offline.sanitize_outcome(
                outcome,
                reply.body,
                http_status=reply.status,
                captured_at_utc=evidence.executed_at_utc,
                source_kind="unattested_capture",
                registration_record_sha256=registration_digest,
            )
        )
    if evidence.route_probes is not None:
        projections["route-probes.json"] = offline.canonical_json(
            offline.sanitize_route_probes(
                evidence.route_probes,
                captured_at_utc=evidence.executed_at_utc,
                source_kind="unattested_capture",
                registration_record_sha256=registration_digest,
            )
        )
    return projections


def _private_parent(output_root: Path) -> tuple[Path, str]:
    if output_root.name in {"", ".", ".."}:
        raise LiveCaptureError("output_name_invalid")
    declared_parent = output_root.parent.absolute()
    parent = output_root.parent.resolve(strict=True)
    if declared_parent != parent:
        raise LiveCaptureError("output_parent_link_refused")
    metadata = os.stat(parent, follow_symlinks=False)
    if (
        not stat.S_ISDIR(metadata.st_mode)
        or metadata.st_uid != os.getuid()
        or metadata.st_mode & 0o077
    ):
        raise LiveCaptureError("output_parent_not_private")
    return parent, output_root.name


def _publish_projection_tree(output_root: Path, projections: Mapping[str, bytes]) -> None:
    parent, final_name = _private_parent(output_root)
    directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    if hasattr(os, "O_NOFOLLOW"):
        directory_flags |= os.O_NOFOLLOW
    parent_fd = os.open(parent, directory_flags)
    staging_name = f".bpr-rfc0016-staging-{secrets.token_hex(12)}"
    staging_fd = -1
    outcomes_fd = -1
    try:
        try:
            os.stat(final_name, dir_fd=parent_fd, follow_symlinks=False)
        except FileNotFoundError:
            pass
        else:
            raise LiveCaptureError("output_root_exists")
        os.mkdir(staging_name, 0o700, dir_fd=parent_fd)
        staging_fd = os.open(staging_name, directory_flags, dir_fd=parent_fd)
        os.mkdir("outcomes", 0o700, dir_fd=staging_fd)
        outcomes_fd = os.open("outcomes", directory_flags, dir_fd=staging_fd)
        for relative, payload in sorted(projections.items()):
            if relative.startswith("outcomes/"):
                _write_private(outcomes_fd, relative.removeprefix("outcomes/"), payload)
            else:
                _write_private(staging_fd, relative, payload)
        os.fsync(outcomes_fd)
        os.fsync(staging_fd)
        os.rename(
            staging_name,
            final_name,
            src_dir_fd=parent_fd,
            dst_dir_fd=parent_fd,
        )
        os.fsync(parent_fd)
    finally:
        if outcomes_fd >= 0:
            os.close(outcomes_fd)
        if staging_fd >= 0:
            os.close(staging_fd)
        os.close(parent_fd)


def persist_sanitized_course(
    output_root: Path,
    registration_capture: RegistrationCapture,
    course: str,
    evidence: CourseEvidence,
) -> None:
    """Validate all projections, then atomically publish one attended course."""
    client_id = registration_capture.observation.get("public_client_id")
    if not isinstance(client_id, str):
        raise LiveCaptureError("registration_observation_invalid")
    validate_client_id(client_id)
    projections = _course_projections(registration_capture, course, evidence)
    _publish_projection_tree(output_root, projections)


COURSE_ARTIFACTS = {
    "access-denied": {
        "course-execution.json",
        "registration.json",
        "outcomes/device_code_success.json",
        "outcomes/authorization_pending.json",
        "outcomes/access_denied.json",
    },
    "device-flow-disabled": {
        "course-execution.json",
        "device-flow-transition.json",
        "registration.json",
        "outcomes/device_flow_disabled.json",
    },
    "expired-token": {
        "course-execution.json",
        "registration.json",
        "outcomes/device_code_success.json",
        "outcomes/expired_token.json",
    },
    "slow-down": {
        "course-execution.json",
        "registration.json",
        "outcomes/device_code_success.json",
        "outcomes/authorization_pending.json",
        "outcomes/slow_down.json",
    },
    "token-success": {
        "course-execution.json",
        "registration.json",
        "route-probes.json",
        "outcomes/device_code_success.json",
        "outcomes/authorization_pending.json",
        "outcomes/token_success.json",
    },
}


def _read_course_root(course: str, root: Path) -> dict[str, bytes]:
    expected = COURSE_ARTIFACTS.get(course)
    if expected is None or root.is_symlink() or not root.is_dir():
        raise LiveCaptureError("course_root_invalid")
    metadata = root.stat(follow_symlinks=False)
    if metadata.st_uid != os.getuid() or metadata.st_mode & 0o077:
        raise LiveCaptureError("course_root_not_private")
    files: dict[str, bytes] = {}
    total = 0
    for path in root.rglob("*"):
        if path.is_symlink() or (not path.is_dir() and not path.is_file()):
            raise LiveCaptureError("course_tree_invalid")
        if path.is_dir():
            if path.stat(follow_symlinks=False).st_mode & 0o077:
                raise LiveCaptureError("course_tree_not_private")
            continue
        if path.stat(follow_symlinks=False).st_mode & 0o077:
            raise LiveCaptureError("course_tree_not_private")
        relative = path.relative_to(root).as_posix()
        payload = path.read_bytes()
        total += len(payload)
        if len(payload) > MAX_RESPONSE_BYTES or total > 512 * 1024:
            raise LiveCaptureError("course_tree_too_large")
        files[relative] = payload
    if set(files) != expected:
        raise LiveCaptureError("course_tree_inventory_invalid")
    execution_payload = files["course-execution.json"]
    execution = _closed_json(execution_payload)
    if _offline_module().canonical_json(execution) != execution_payload:
        raise LiveCaptureError("course_execution_invalid")
    execution = _validate_course_execution_document(course, execution)
    if course == "device-flow-disabled":
        transition_payload = files["device-flow-transition.json"]
        load_device_flow_transition_observation_from_bytes(
            transition_payload,
            executed_at_utc=execution["executed_at_utc"],
        )
        bindings = execution["bindings"]
        if (
            not isinstance(bindings, dict)
            or bindings["device_flow_transition_sha256"]
            != _sha256(transition_payload)
        ):
            raise LiveCaptureError("course_execution_invalid")
    return files


def _assembled_artifacts(course_records: Mapping[str, Mapping[str, bytes]]) -> dict[str, bytes]:
    if set(course_records) != set(COURSE_ARTIFACTS):
        raise LiveCaptureError("course_set_invalid")
    registrations = {records["registration.json"] for records in course_records.values()}
    if len(registrations) != 1:
        raise LiveCaptureError("course_registration_mismatch")
    registration_payload = next(iter(registrations))
    _course_execution_records(course_records, registration_payload)
    artifacts = {
        "registration.json": registration_payload,
        "route-probes.json": course_records["token-success"]["route-probes.json"],
        "outcomes/access_denied.json": course_records["access-denied"][
            "outcomes/access_denied.json"
        ],
        "outcomes/authorization_pending.json": course_records["slow-down"][
            "outcomes/authorization_pending.json"
        ],
        "outcomes/device_code_success.json": course_records["token-success"][
            "outcomes/device_code_success.json"
        ],
        "outcomes/device_flow_disabled.json": course_records["device-flow-disabled"][
            "outcomes/device_flow_disabled.json"
        ],
        "outcomes/expired_token.json": course_records["expired-token"][
            "outcomes/expired_token.json"
        ],
        "outcomes/slow_down.json": course_records["slow-down"]["outcomes/slow_down.json"],
        "outcomes/token_success.json": course_records["token-success"][
            "outcomes/token_success.json"
        ],
    }
    offline = _offline_module()
    manifest = offline.build_manifest(sorted(artifacts.items()))
    artifacts["manifest.json"] = offline.canonical_json(manifest)
    return artifacts


def _course_execution_records(
    course_records: Mapping[str, Mapping[str, bytes]],
    registration_payload: bytes,
) -> dict[str, dict[str, object]]:
    registration_digest = _sha256(registration_payload)
    executions: dict[str, dict[str, object]] = {}
    for course, records in course_records.items():
        payload = records.get("course-execution.json")
        if not isinstance(payload, bytes):
            raise LiveCaptureError("course_execution_invalid")
        document = _closed_json(payload)
        if _offline_module().canonical_json(document) != payload:
            raise LiveCaptureError("course_execution_invalid")
        executions[course] = _validate_course_execution_document(
            course,
            document,
            registration_record_sha256=registration_digest,
        )
    runner_digests = {
        execution["capture_runner_sha256"] for execution in executions.values()
    }
    if len(runner_digests) != 1:
        raise LiveCaptureError("course_runner_mismatch")
    return executions


def _attestation(
    course_records: Mapping[str, Mapping[str, bytes]],
    artifacts: Mapping[str, bytes],
) -> bytes:
    registration_payloads = {
        records["registration.json"] for records in course_records.values()
    }
    if len(registration_payloads) != 1:
        raise LiveCaptureError("course_registration_mismatch")
    executions = _course_execution_records(
        course_records,
        next(iter(registration_payloads)),
    )
    course_digests = {}
    for course, records in sorted(course_records.items()):
        identity = b"".join(
            relative.encode("utf-8") + b"\0" + _sha256(payload).encode("ascii") + b"\n"
            for relative, payload in sorted(records.items())
        )
        course_digests[course] = _sha256(identity)
    document = {
        "artifacts": [
            {"path": relative, "sha256": _sha256(payload)}
            for relative, payload in sorted(artifacts.items())
        ],
        "authority_amendment_id": AUTHORITY_AMENDMENT_ID,
        "authority_subject_sha256": _offline_module().AUTHORITY_SUBJECT_SHA256,
        "capture_runner_sha256": next(
            iter(
                {
                    execution["capture_runner_sha256"]
                    for execution in executions.values()
                }
            )
        ),
        "course_execution": {
            course: {
                "bindings": execution["bindings"],
                "capture_runner_sha256": execution["capture_runner_sha256"],
                "executed_at_utc": execution["executed_at_utc"],
            }
            for course, execution in sorted(executions.items())
        },
        "course_tree_sha256": course_digests,
        "schema_name": "bpr.rfc0016.live_run_attestation",
        "schema_version": 1,
        "state": "attended_runner_completed_unattested",
    }
    return _offline_module().canonical_json(document)


def _publish_capture_bundle(
    output_root: Path,
    artifacts: Mapping[str, bytes],
    attestation: bytes,
) -> None:
    parent, final_name = _private_parent(output_root)
    directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    if hasattr(os, "O_NOFOLLOW"):
        directory_flags |= os.O_NOFOLLOW
    parent_fd = os.open(parent, directory_flags)
    staging_name = f".bpr-rfc0016-bundle-{secrets.token_hex(12)}"
    staging_fd = fixture_fd = outcomes_fd = -1
    try:
        try:
            os.stat(final_name, dir_fd=parent_fd, follow_symlinks=False)
        except FileNotFoundError:
            pass
        else:
            raise LiveCaptureError("output_root_exists")
        os.mkdir(staging_name, 0o700, dir_fd=parent_fd)
        staging_fd = os.open(staging_name, directory_flags, dir_fd=parent_fd)
        os.mkdir("fixture", 0o700, dir_fd=staging_fd)
        fixture_fd = os.open("fixture", directory_flags, dir_fd=staging_fd)
        os.mkdir("outcomes", 0o700, dir_fd=fixture_fd)
        outcomes_fd = os.open("outcomes", directory_flags, dir_fd=fixture_fd)
        for relative, payload in sorted(artifacts.items()):
            if relative.startswith("outcomes/"):
                _write_private(outcomes_fd, relative.removeprefix("outcomes/"), payload)
            else:
                _write_private(fixture_fd, relative, payload)
        _write_private(staging_fd, "live-run-attestation.json", attestation)
        for descriptor in (outcomes_fd, fixture_fd, staging_fd):
            os.fsync(descriptor)
        os.rename(staging_name, final_name, src_dir_fd=parent_fd, dst_dir_fd=parent_fd)
        os.fsync(parent_fd)
    finally:
        for descriptor in (outcomes_fd, fixture_fd, staging_fd, parent_fd):
            if descriptor >= 0:
                os.close(descriptor)


def assemble_capture_bundle(output_root: Path, course_roots: Mapping[str, Path]) -> None:
    records = {course: _read_course_root(course, root) for course, root in course_roots.items()}
    artifacts = _assembled_artifacts(records)
    attestation = _attestation(records, artifacts)
    _publish_capture_bundle(output_root, artifacts, attestation)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    capture = commands.add_parser("capture")
    capture.add_argument("registration_observation", type=Path)
    capture.add_argument("output_root", type=Path)
    capture.add_argument(
        "--course",
        choices=sorted(COURSES),
        required=True,
    )
    capture.add_argument(
        "--device-flow-transition-observation",
        type=Path,
    )
    assemble = commands.add_parser("assemble")
    assemble.add_argument("output_root", type=Path)
    for course in sorted(COURSES):
        assemble.add_argument(
            f"--{course}",
            dest=course.replace("-", "_"),
            type=Path,
            required=True,
        )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if sys.platform != "darwin":
        print("RFC0016_ATTENDED_CAPTURE_REFUSED:platform", file=sys.stderr)
        return 1
    try:
        if args.command == "assemble":
            assemble_capture_bundle(
                args.output_root,
                {
                    course: getattr(args, course.replace("-", "_"))
                    for course in COURSES
                },
            )
            print("RFC0016_ATTENDED_CAPTURE_BUNDLE_OK")
            return 0
        registration_capture = load_registration_observation(
            args.registration_observation
        )
        client_id_value = registration_capture.observation.get("public_client_id")
        if not isinstance(client_id_value, str):
            raise LiveCaptureError("registration_observation_invalid")
        client_id = validate_client_id(client_id_value)
        transition_path = args.device_flow_transition_observation
        if (args.course == "device-flow-disabled") != (transition_path is not None):
            raise LiveCaptureError("device_flow_transition_required")
        slow_down_reservation = (
            _reserve_slow_down_recovery_once(LOADED_RUNNER_SHA256)
            if args.course == "slow-down"
            else None
        )
        wire = HttpsWire()
        if transition_path is None:
            evidence = run_authorization_course(
                wire,
                client_id,
                args.course,
                probe=run_read_only_probes if args.course == "token-success" else None,
                slow_down_reservation=slow_down_reservation,
            )
        else:
            try:
                evidence = run_authorization_course(
                    wire,
                    client_id,
                    args.course,
                )
            finally:
                confirm_device_flow_restored()
            evidence.device_flow_transition = load_device_flow_transition_observation(
                transition_path,
                executed_at_utc=evidence.executed_at_utc,
            )
        persist_sanitized_course(
            args.output_root,
            registration_capture,
            args.course,
            evidence,
        )
    except LiveCaptureError as error:
        diagnostic = str(error)
        if not re.fullmatch(r"[a-z0-9_]+", diagnostic):
            diagnostic = "closed_failure"
        print(f"RFC0016_ATTENDED_CAPTURE_FAILED:{diagnostic}", file=sys.stderr)
        return 1
    except (Exception, KeyboardInterrupt):
        print("RFC0016_ATTENDED_CAPTURE_FAILED:closed_failure", file=sys.stderr)
        return 1
    print("RFC0016_ATTENDED_CAPTURE_COURSE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
