#!/usr/bin/env python3
"""Admit RFC-BPR-0016's distributable identity from committed evidence alone.

RFC-BPR-0016 cannot close, and no production platform can become available,
while the only registered GitHub App is BPR-0276's private owner-only
verification identity. BPR-0318 records a second, distributable identity under
the accepted `rfc0016-production-app-registration-v001` authority, and this
script is what decides whether that record is real.

It reads committed evidence and never opens a socket. The registration happens
in an attended sitting on GitHub and the designated read-only course runs under
the attended runner; both leave files behind. The question here is not whether
those files exist but whether they say the exact thing the owner approved.

So the checks are seams rather than fields. The course's own registration
record must name the client the sanitized observation names, and neither may be
the verification client. The execution record's digest must be the digest of
the registration bytes beside it. Every probe URL is rebuilt from the
designated repository instead of pattern-matched, so a probe against another
repository fails rather than looking plausible. The token projection must omit
the token and report the empty scope the approved permission set implies. A
byte scan then re-reads every evidence file for material that should never have
been written down at all.

`--require-complete` demands the course. Without it the script admits the
identity alone, which is the state between the registration and the sitting.
`--validate-existing` is the closeout mode: it additionally requires every
evidence file to be tracked and unmodified, so a replay at the final head
cannot pass on a working-tree edit.

Owned by BPR-0318. BPR-0310 and BPR-0311 consume the same recorded identity
before their own target-native enablement, and BPR-0285 replays this script at
the immutable final head.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

GATE = "rfc0016-production-app-registration-v001"
APPROVED_CLAIM = "rfc0016_production_app_registration_v001_approved"

OFFICIAL_VALIDATOR = pathlib.PurePosixPath(
    ".agents/skills/bpr-attended-review/scripts/validate_operator_packet.py"
)

# fact_1: the private owner-only verification client is never widened, reused,
# or represented as the distributable identity. fact_8 states the consequence
# this script can actually check — the production client differs from it.
VERIFICATION_CLIENT_ID = "Iv23lil08fH3933w9MkW"
PUBLIC_CLIENT_ID = re.compile(r"^[A-Za-z0-9.]{16,64}$")

# fact_4 and fact_5: the one designated target, and nothing else.
DESIGNATED_OWNER = "MikeMarcin"
DESIGNATED_NAME = "BPR-Test"
DESIGNATED_REPOSITORY = f"{DESIGNATED_OWNER}/{DESIGNATED_NAME}"
DESIGNATED_DATABASE_ID = 1328277631
DESIGNATED_PULL_REQUEST = 1

# fact_3, exactly. Compared whole, so an added permission is a failure rather
# than an unread key.
APPROVED_REPOSITORY_PERMISSIONS = {
    "actions": "none",
    "administration": "none",
    "checks": "read",
    "commit_statuses": "none",
    "contents": "read",
    "issues": "none",
    "metadata": "read",
    "pull_requests": "write",
    "workflows": "none",
}
PERMISSION_SCOPES = ("account", "enterprise", "organization", "repository")

DEVICE_CODE_URL = "https://github.com/login/device/code"
TOKEN_URL = "https://github.com/login/oauth/access_token"
GRAPHQL_URL = "https://api.github.com/graphql"

REQUIRED_OUTCOMES = ("device_code_success", "token_success")
OPTIONAL_OUTCOMES = ("authorization_pending",)

# The transient material a projection must drop, per outcome. A projection that
# recorded any of these would be the secret itself, not a record of one.
OMITTED_TRANSIENT_FIELDS = {
    "device_code_success": ("device_code", "user_code"),
    "token_success": ("access_token", "refresh_token"),
    "authorization_pending": ("error_description", "error_uri"),
}

SECRET_VALUE_KEYS = frozenset(
    {
        "access_token",
        "client_secret",
        "device_code",
        "refresh_token",
        "user_code",
    }
)

# Name-then-separator, so an evidence file may list `access_token` as a field it
# deliberately omitted without that reading as a leak, while `"access_token":`
# with a value beside it does not survive.
SECRET_ASSIGNMENT = re.compile(
    rb"(?i)[\"']?(?:access_token|refresh_token|client_secret|device_code"
    rb"|user_code|password|credential|secret)[\"']?\s*[:=]"
)
TOKEN_SHAPES = (
    re.compile(rb"gh[oprsu]_[A-Za-z0-9]{16,}"),
    re.compile(rb"github_pat_[A-Za-z0-9_]{16,}"),
)
USER_CODE_SHAPE = re.compile(rb"\b[A-Z0-9]{4}-[A-Z0-9]{4}\b")


class IdentityFailure(RuntimeError):
    """A checked property did not hold."""


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise IdentityFailure(reason)


def run(command: list[str], *, cwd: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        errors="replace",
        timeout=300,
        check=False,
    )


def sha256(payload: bytes) -> str:
    return f"sha256:{hashlib.sha256(payload).hexdigest()}"


def load_json(path: pathlib.Path, diagnostic: str) -> dict[str, object]:
    if not path.is_file():
        raise IdentityFailure(f"{diagnostic}:missing")
    try:
        document = json.loads(path.read_bytes().decode("utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise IdentityFailure(f"{diagnostic}:malformed") from error
    if not isinstance(document, dict):
        raise IdentityFailure(f"{diagnostic}:not-an-object")
    return document


def require_schema(
    document: dict[str, object],
    name: str,
    diagnostic: str,
) -> None:
    require(document.get("schema_name") == name, f"{diagnostic}:schema-name")
    # `is not True` would admit `1`; the version is compared as an exact int and
    # a bool is rejected outright, which is the same alias trap.
    version = document.get("schema_version")
    require(
        isinstance(version, int) and not isinstance(version, bool) and version == 1,
        f"{diagnostic}:schema-version",
    )


def designated_route_catalog() -> dict[str, tuple[str, str, str]]:
    """The exact read-only catalog, rebuilt from the designated target.

    Built rather than matched: a probe against another repository cannot pass by
    resembling one of these, because the string it must equal names
    `MikeMarcin/BPR-Test` and pull request 1 and nothing else.
    """
    api = f"https://api.github.com/repos/{DESIGNATED_REPOSITORY}"
    git = f"https://github.com/{DESIGNATED_REPOSITORY}.git"
    number = DESIGNATED_PULL_REQUEST
    page = "per_page=100&page=1"
    return {
        "metadata": ("GET", "rest", api),
        "contents_info_refs": (
            "GET",
            "smart_http",
            f"{git}/info/refs?service=git-upload-pack",
        ),
        "contents_upload_pack": ("POST", "smart_http", f"{git}/git-upload-pack"),
        "checks": ("GET", "rest", f"{api}/commits/HEAD/check-runs"),
        "pull_request": ("GET", "rest", f"{api}/pulls/{number}"),
        "pull_request_files": ("GET", "rest", f"{api}/pulls/{number}/files?{page}"),
        "pull_request_commits": (
            "GET",
            "rest",
            f"{api}/pulls/{number}/commits?{page}",
        ),
        "pull_request_reviews": (
            "GET",
            "rest",
            f"{api}/pulls/{number}/reviews?{page}",
        ),
        "pull_request_issue_comments": (
            "GET",
            "rest",
            f"{api}/issues/{number}/comments?{page}",
        ),
        "review_threads": ("POST", "graphql", GRAPHQL_URL),
    }


def check_registration_fields(record: dict[str, object], diagnostic: str) -> str:
    """The configuration facts, wherever they are recorded. Returns the client."""
    require(record.get("application_kind") == "github_app", f"{diagnostic}:not-a-github-app")
    require(
        record.get("application_visibility") == "public",
        f"{diagnostic}:not-publicly-installable",
    )
    require(
        record.get("installation_policy") == "any_account",
        f"{diagnostic}:installation-policy-not-any-account",
    )
    require(record.get("owner_login") == DESIGNATED_OWNER, f"{diagnostic}:owner-login")
    require(record.get("owner_kind") == "user", f"{diagnostic}:owner-kind")
    require(
        record.get("installation_owner_login") == DESIGNATED_OWNER,
        f"{diagnostic}:installation-owner-login",
    )

    require(record.get("device_flow_enabled") is True, f"{diagnostic}:device-flow-off")
    require(
        record.get("user_access_token_expiration_enabled") is True,
        f"{diagnostic}:token-expiration-off",
    )
    require(
        record.get("refresh_policy") == "expiring_user_token_with_refresh",
        f"{diagnostic}:refresh-policy",
    )
    require(record.get("webhook_active") is False, f"{diagnostic}:webhook-active")
    require(
        record.get("callback_url_present") is False,
        f"{diagnostic}:callback-url-present",
    )
    require(record.get("oauth_client_type") == "public", f"{diagnostic}:client-not-public")
    require(record.get("oauth_scopes") == [], f"{diagnostic}:oauth-scope-requested")

    permissions = record.get("permissions")
    require(isinstance(permissions, dict), f"{diagnostic}:permissions-missing")
    assert isinstance(permissions, dict)
    require(
        set(permissions) == set(PERMISSION_SCOPES),
        f"{diagnostic}:permission-scopes-not-exact",
    )
    for scope in ("account", "enterprise", "organization"):
        require(permissions.get(scope) == {}, f"{diagnostic}:non-repository-permission:{scope}")
    require(
        permissions.get("repository") == APPROVED_REPOSITORY_PERMISSIONS,
        f"{diagnostic}:repository-permissions-not-exactly-approved",
    )

    require(
        record.get("installation_repository_selection") == "selected_repositories",
        f"{diagnostic}:installation-not-limited-to-selected-repositories",
    )
    require(
        record.get("installation_repositories") == [DESIGNATED_REPOSITORY],
        f"{diagnostic}:installation-repositories-not-exactly-designated",
    )

    client = record.get("public_client_id")
    require(isinstance(client, str), f"{diagnostic}:public-client-id-missing")
    assert isinstance(client, str)
    require(
        PUBLIC_CLIENT_ID.fullmatch(client) is not None,
        f"{diagnostic}:public-client-id-shape",
    )
    require(
        client != VERIFICATION_CLIENT_ID,
        f"{diagnostic}:client-is-the-private-verification-identity",
    )
    digest = record.get("public_client_id_sha256")
    if digest is not None:
        require(
            digest == sha256(client.encode("utf-8")),
            f"{diagnostic}:public-client-id-digest-mismatch",
        )
    return client


def check_recorded_decision(review_directory: pathlib.Path) -> str:
    """Read the accepted authority off disk. Returns the subject digest.

    Split from the Git replay below so that what the record *says* can be
    checked without a repository: the two halves fail for different reasons and
    a reviewer is better served knowing which.
    """
    record_path = review_directory / "validation-record.json"
    record = load_json(record_path, "validation-record")
    require_schema(record, "bpr.operator_response_validation", "validation-record")
    require(record.get("gate") == GATE, "validation-record:gate")
    require(record.get("result") == "pass", "validation-record:result")
    require(record.get("outcome") == "approved", "validation-record:outcome")

    checks = record.get("checks")
    require(isinstance(checks, dict), "validation-record:checks-missing")
    assert isinstance(checks, dict)
    require(checks.get("overall") == "pass", "validation-record:checks-overall")
    for name in (
        "approval_statement_exact",
        "committed_artifacts_exact",
        "decision_subject_digest_bound",
    ):
        require(checks.get(name) is True, f"validation-record:check-not-true:{name}")

    contract = load_json(review_directory / "claims-contract.json", "claims-contract")
    require_schema(contract, "bpr.operator_claim_contract", "claims-contract")
    require(contract.get("gate") == GATE, "claims-contract:gate")
    require(contract.get("approved_claim") == APPROVED_CLAIM, "claims-contract:approved-claim")
    adjacent = contract.get("adjacent_false_claims")
    require(isinstance(adjacent, list) and bool(adjacent), "claims-contract:adjacent-claims")
    assert isinstance(adjacent, list)

    claims = record.get("claims")
    require(isinstance(claims, dict), "validation-record:claims-missing")
    assert isinstance(claims, dict)
    require(claims.get(APPROVED_CLAIM) is True, "validation-record:approved-claim-not-true")
    for name in adjacent:
        require(
            claims.get(name) is False,
            f"validation-record:adjacent-claim-not-false:{name}",
        )
    # Nothing beyond the contract may be asserted at all. A claim this script has
    # never heard of is an authority it cannot bound.
    require(
        set(claims) == {APPROVED_CLAIM} | set(adjacent),
        "validation-record:claims-not-exactly-the-contract",
    )

    # Every artifact the record binds must still hash to the digest it recorded.
    subject_digest = ""
    for field, name in (
        ("claims_contract", "claims-contract.json"),
        ("decision_subject", "decision-subject.txt"),
        ("operator_response", "operator-response.md"),
        ("reviewed_packet", "operator-packet.md"),
    ):
        binding = record.get(field)
        require(isinstance(binding, dict), f"validation-record:{field}-missing")
        assert isinstance(binding, dict)
        payload = (review_directory / name).read_bytes()
        require(
            binding.get("sha256") == sha256(payload),
            f"validation-record:{field}-digest-mismatch",
        )
        require(
            binding.get("byte_length") == len(payload),
            f"validation-record:{field}-length-mismatch",
        )
        if field == "decision_subject":
            subject_digest = str(binding.get("sha256"))
    return subject_digest


def check_operator_record(
    root: pathlib.Path,
    review_directory: pathlib.Path,
) -> tuple[str, str]:
    """Replay the accepted authority. Returns (record commit, subject digest)."""
    subject_digest = check_recorded_decision(review_directory)
    record_path = review_directory / "validation-record.json"

    relative = record_path.relative_to(root).as_posix()
    revision = run(["git", "log", "-1", "--format=%H", "--", relative], cwd=root)
    require(revision.returncode == 0, "record-commit-unreadable")
    record_commit = revision.stdout.strip()
    require(len(record_commit) == 40, "record-commit-absent")

    replay = run(
        [
            sys.executable,
            str(root / OFFICIAL_VALIDATOR),
            "--repository-root",
            str(root),
            "--output",
            relative,
            "--validate-existing",
            "--record-commit",
            record_commit,
        ],
        cwd=root,
    )
    require(
        f"OPERATOR_RESPONSE_VALID:{GATE}" in replay.stdout,
        f"operator-record-replay-failed:{replay.stderr.strip()[:400]}",
    )
    return record_commit, subject_digest


def check_registration_observation(review_directory: pathlib.Path) -> str:
    """The sanitized production observation. Returns the production client."""
    path = review_directory / "registration-observation.json"
    document = load_json(path, "registration-observation")
    require_schema(document, "bpr.rfc0016.registration_observation", "registration-observation")
    observation = document.get("observation")
    require(isinstance(observation, dict), "registration-observation:observation-missing")
    assert isinstance(observation, dict)
    return check_registration_fields(observation, "registration-observation")


def check_provenance(
    provenance: object,
    *,
    diagnostic: str,
    subject_digest: str,
    registration_digest: str,
) -> None:
    require(isinstance(provenance, dict), f"{diagnostic}:provenance-missing")
    assert isinstance(provenance, dict)
    require(provenance.get("gate") == GATE, f"{diagnostic}:provenance-gate")
    require(
        provenance.get("authority_subject_sha256") == subject_digest,
        f"{diagnostic}:provenance-not-bound-to-approved-subject",
    )
    require(
        provenance.get("registration_record_sha256") == registration_digest,
        f"{diagnostic}:provenance-registration-digest-mismatch",
    )
    require(provenance.get("service") == "github.com", f"{diagnostic}:provenance-service")


def check_outcome(
    path: pathlib.Path,
    outcome: str,
    *,
    subject_digest: str,
    registration_digest: str,
) -> None:
    diagnostic = f"outcome:{outcome}"
    document = load_json(path, diagnostic)
    require_schema(document, "bpr.rfc0016.device_contract", diagnostic)
    require(document.get("outcome") == outcome, f"{diagnostic}:outcome-name")
    require(document.get("http_status") == 200, f"{diagnostic}:http-status")
    expected_url = DEVICE_CODE_URL if outcome == "device_code_success" else TOKEN_URL
    require(document.get("endpoint_url") == expected_url, f"{diagnostic}:endpoint-url")
    check_provenance(
        document.get("provenance"),
        diagnostic=diagnostic,
        subject_digest=subject_digest,
        registration_digest=registration_digest,
    )

    projection = document.get("projection")
    require(isinstance(projection, dict), f"{diagnostic}:projection-missing")
    assert isinstance(projection, dict)
    omitted = projection.get("omitted_transient_fields")
    require(isinstance(omitted, list), f"{diagnostic}:omitted-fields-missing")
    assert isinstance(omitted, list)
    for field in OMITTED_TRANSIENT_FIELDS[outcome]:
        require(field in omitted, f"{diagnostic}:transient-field-not-omitted:{field}")

    values = projection.get("values")
    require(isinstance(values, dict), f"{diagnostic}:values-missing")
    assert isinstance(values, dict)
    require(
        not (set(values) & SECRET_VALUE_KEYS),
        f"{diagnostic}:projection-recorded-transient-material",
    )
    if outcome == "token_success":
        # fact_3: a GitHub App user token carries the app's permissions, so a
        # successful response reports no scope at all. A non-empty scope would
        # mean a wider grant than the one approved.
        require(values.get("scope") == "", f"{diagnostic}:scope-not-empty")
        require(values.get("token_type") == "bearer", f"{diagnostic}:token-type")


def check_route_probes(
    path: pathlib.Path,
    *,
    subject_digest: str,
    registration_digest: str,
) -> int:
    document = load_json(path, "route-probes")
    require_schema(document, "bpr.rfc0016.read_only_route_probes", "route-probes")
    check_provenance(
        document.get("provenance"),
        diagnostic="route-probes",
        subject_digest=subject_digest,
        registration_digest=registration_digest,
    )

    repository = document.get("repository")
    require(isinstance(repository, dict), "route-probes:repository-missing")
    assert isinstance(repository, dict)
    require(
        repository
        == {
            "database_id": DESIGNATED_DATABASE_ID,
            "name": DESIGNATED_NAME,
            "owner": DESIGNATED_OWNER,
            "pull_request_number": DESIGNATED_PULL_REQUEST,
        },
        "route-probes:repository-not-exactly-designated",
    )

    document_text = document.get("graphql_document")
    require(isinstance(document_text, str), "route-probes:graphql-document-missing")
    assert isinstance(document_text, str)
    require(
        document.get("graphql_document_sha256") == sha256(document_text.encode("utf-8")),
        "route-probes:graphql-document-digest-mismatch",
    )
    require("mutation" not in document_text, "route-probes:graphql-document-mutates")
    require(
        document.get("graphql_variables")
        == {
            "name": DESIGNATED_NAME,
            "number": DESIGNATED_PULL_REQUEST,
            "owner": DESIGNATED_OWNER,
        },
        "route-probes:graphql-variables-not-designated",
    )

    catalog = designated_route_catalog()
    probes = document.get("probes")
    require(isinstance(probes, list), "route-probes:probes-missing")
    assert isinstance(probes, list)

    seen: set[str] = set()
    for probe in probes:
        require(isinstance(probe, dict), "route-probes:probe-not-an-object")
        assert isinstance(probe, dict)
        operation = probe.get("operation")
        require(
            isinstance(operation, str) and operation in catalog,
            f"route-probes:operation-outside-catalog:{operation}",
        )
        assert isinstance(operation, str)
        require(operation not in seen, f"route-probes:operation-repeated:{operation}")
        seen.add(operation)

        method, transport, url = catalog[operation]
        require(probe.get("method") == method, f"route-probes:method:{operation}")
        require(probe.get("transport") == transport, f"route-probes:transport:{operation}")
        require(probe.get("url") == url, f"route-probes:url-not-designated:{operation}")
        require(probe.get("result") == "success", f"route-probes:result:{operation}")
        require(probe.get("http_status") == 200, f"route-probes:http-status:{operation}")
        digest = probe.get("request_contract_sha256")
        require(
            isinstance(digest, str) and digest.startswith("sha256:"),
            f"route-probes:request-contract-digest:{operation}",
        )

    require(seen == set(catalog), "route-probes:catalog-not-completely-exercised")
    return len(catalog)


def check_course_evidence(
    course_root: pathlib.Path,
    *,
    production_client: str,
    subject_digest: str,
) -> int:
    """The designated read-only course, bound to the identity that was approved."""
    require(course_root.is_dir(), "course-evidence:missing")

    registration_path = course_root / "registration.json"
    registration_bytes = (
        registration_path.read_bytes() if registration_path.is_file() else b""
    )
    registration = load_json(registration_path, "course-registration")
    require_schema(registration, "bpr.rfc0016.github_app_registration", "course-registration")
    course_client = check_registration_fields(registration, "course-registration")
    # The seam the whole card turns on: the course must have run as the identity
    # the sanitized observation records, not as some other registered app.
    require(
        course_client == production_client,
        "course-registration:client-differs-from-recorded-production-identity",
    )
    registration_digest = sha256(registration_bytes)

    execution = load_json(course_root / "course-execution.json", "course-execution")
    require_schema(execution, "bpr.rfc0016.course_execution", "course-execution")
    require(execution.get("course") == "token-success", "course-execution:course-name")
    require(execution.get("gate") == GATE, "course-execution:gate")
    require(
        execution.get("authority_subject_sha256") == subject_digest,
        "course-execution:not-bound-to-approved-subject",
    )
    require(
        execution.get("registration_record_sha256") == registration_digest,
        "course-execution:registration-digest-mismatch",
    )
    executed_at = execution.get("executed_at_utc")
    require(
        isinstance(executed_at, str) and executed_at.endswith("Z"),
        "course-execution:executed-at-missing",
    )

    outcomes_root = course_root / "outcomes"
    require(outcomes_root.is_dir(), "course-outcomes:missing")
    present = sorted(path.name for path in outcomes_root.iterdir() if path.is_file())
    permitted = {
        f"{name}.json" for name in (*REQUIRED_OUTCOMES, *OPTIONAL_OUTCOMES)
    }
    require(
        set(present) <= permitted,
        f"course-outcomes:unexpected-file:{sorted(set(present) - permitted)}",
    )
    for outcome in REQUIRED_OUTCOMES:
        require(f"{outcome}.json" in present, f"course-outcomes:required-missing:{outcome}")
    for outcome in (*REQUIRED_OUTCOMES, *OPTIONAL_OUTCOMES):
        path = outcomes_root / f"{outcome}.json"
        if path.is_file():
            check_outcome(
                path,
                outcome,
                subject_digest=subject_digest,
                registration_digest=registration_digest,
            )

    return check_route_probes(
        course_root / "route-probes.json",
        subject_digest=subject_digest,
        registration_digest=registration_digest,
    )


def evidence_files(*roots: pathlib.Path) -> list[pathlib.Path]:
    found: dict[pathlib.Path, None] = {}
    for root in roots:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*")):
            if path.is_file():
                found[path.resolve()] = None
    return list(found)


def check_secret_free(paths: list[pathlib.Path]) -> int:
    """fact_6 and fact_8: no evidence file carries material that is a secret."""
    for path in paths:
        payload = path.read_bytes()
        require(
            SECRET_ASSIGNMENT.search(payload) is None,
            f"secret-bearing-evidence:{path.name}:assignment",
        )
        for shape in TOKEN_SHAPES:
            require(
                shape.search(payload) is None,
                f"secret-bearing-evidence:{path.name}:token-shape",
            )
        if path.suffix == ".json":
            require(
                USER_CODE_SHAPE.search(payload) is None,
                f"secret-bearing-evidence:{path.name}:user-code-shape",
            )
    return len(paths)


def check_committed(root: pathlib.Path, paths: list[pathlib.Path]) -> None:
    """Closeout replays committed bytes, never a working-tree edit."""
    relative: list[str] = []
    for path in paths:
        try:
            relative.append(path.relative_to(root).as_posix())
        except ValueError:
            # Evidence Git cannot name is evidence a replay cannot pin. Saying
            # so beats reporting an internal error for a real reviewer mistake.
            raise IdentityFailure(f"evidence-outside-repository:{path.name}") from None
    result = run(["git", "status", "--porcelain", "--", *relative], cwd=root)
    require(result.returncode == 0, "git-status-failed")
    dirty = result.stdout.strip()
    require(dirty == "", f"evidence-not-committed:{dirty.splitlines()[0] if dirty else ''}")


def check_selected_client(binary: pathlib.Path, production_client: str) -> None:
    """The shipped artifact selects the recorded identity and not the other one."""
    require(binary.is_file(), "release-binary:missing")
    payload = binary.read_bytes()
    require(
        production_client.encode("ascii") in payload,
        "release-binary:does-not-select-the-recorded-production-client",
    )
    require(
        VERIFICATION_CLIENT_ID.encode("ascii") not in payload,
        "release-binary:carries-the-private-verification-identity",
    )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Admit the RFC-BPR-0016 distributable GitHub App identity from the "
            "committed attended record, sanitized registration observation, and "
            "designated read-only course evidence. Performs no network traffic."
        )
    )
    parser.add_argument(
        "--registration-observation",
        type=pathlib.Path,
        required=True,
        help="Sanitized production registration observation JSON.",
    )
    parser.add_argument(
        "--course-evidence-root",
        type=pathlib.Path,
        help="Directory holding the designated read-only course evidence.",
    )
    parser.add_argument(
        "--require-complete",
        action="store_true",
        help="Require the designated read-only course, not the identity alone.",
    )
    parser.add_argument(
        "--validate-existing",
        action="store_true",
        help="Closeout replay: every evidence file must be tracked and unmodified.",
    )
    parser.add_argument(
        "--release-binary",
        type=pathlib.Path,
        help="Shipped artifact that must select the recorded production client.",
    )
    parser.add_argument(
        "--repository-root",
        type=pathlib.Path,
        default=REPOSITORY_ROOT,
        help="Repository root the evidence paths and Git identity belong to.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        root = arguments.repository_root.resolve()
        observation_path = arguments.registration_observation.resolve()
        review_directory = observation_path.parent

        record_commit, subject_digest = check_operator_record(root, review_directory)
        production_client = check_registration_observation(review_directory)

        course_root: pathlib.Path | None = None
        if arguments.course_evidence_root is not None:
            course_root = arguments.course_evidence_root.resolve()
        require(
            not arguments.require_complete or course_root is not None,
            "require-complete-without-course-evidence-root",
        )

        probes = 0
        if course_root is not None and (arguments.require_complete or course_root.is_dir()):
            probes = check_course_evidence(
                course_root,
                production_client=production_client,
                subject_digest=subject_digest,
            )

        scanned = evidence_files(
            review_directory,
            *(() if course_root is None else (course_root,)),
        )
        files = check_secret_free(scanned)

        if arguments.validate_existing:
            check_committed(root, scanned)

        if arguments.release_binary is not None:
            check_selected_client(arguments.release_binary, production_client)
    except IdentityFailure as error:
        print(f"rfc0016-production-identity:fail:{error}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as error:
        print(
            f"rfc0016-production-identity:fail:timeout:{error.cmd[0]}",
            file=sys.stderr,
        )
        return 1
    except OSError as error:
        print(
            f"rfc0016-production-identity:fail:unreadable:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1
    except Exception as error:  # noqa: BLE001 - reported, never swallowed
        print(
            f"rfc0016-production-identity:fail:internal:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1

    print(f"RFC0016_PRODUCTION_GATE:{GATE}")
    print(f"RFC0016_PRODUCTION_RECORD_COMMIT:{record_commit}")
    print(f"RFC0016_PRODUCTION_CLIENT:{production_client}")
    print(f"RFC0016_PRODUCTION_PERMISSIONS:{len(APPROVED_REPOSITORY_PERMISSIONS)}")
    print(f"RFC0016_PRODUCTION_COURSE_PROBES:{probes}")
    print(
        "RFC0016_PRODUCTION_COURSE:"
        f"{'complete' if probes else 'identity-only'}"
    )
    print(f"RFC0016_PRODUCTION_EVIDENCE_FILES:{files}")
    print("RFC0016_PRODUCTION_IDENTITY_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
