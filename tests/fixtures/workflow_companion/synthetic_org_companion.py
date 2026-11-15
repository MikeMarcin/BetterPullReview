#!/usr/bin/env python3
"""Synthetic organization companion for RFC-BPR-0112 (BPR-2033, BPR-2034).

Reads one bpr.workflow-request and writes one bpr.workflow-response. The
organization's rules live in the upstream folder, never in BetterPullReview.
Nothing here reaches a network or creates a pull request.
"""

import argparse
import hashlib
import json
import os
import sys


def respond(path, request, outcome, **fields):
    answer = {
        "format": "bpr.workflow-response",
        "version": 1,
        "invocation": request["invocation"],
        "draft": request["draft"],
        "action": request["action"]["id"],
        "outcome": outcome,
    }
    answer.update({key: value for key, value in fields.items() if value})
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(answer, handle, indent=2)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--request", required=True)
    parser.add_argument("--response", required=True)
    parser.add_argument("--upstream", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "upstream"))
    arguments = parser.parse_args()

    with open(arguments.request, encoding="utf-8") as handle:
        request = json.load(handle)
    if request.get("format") != "bpr.workflow-request" or request.get("version") != 1:
        print("not a version 1 workflow request", file=sys.stderr)
        return 2
    rules_path = os.path.join(arguments.upstream, "RULES.json")
    with open(rules_path, "rb") as handle:
        raw = handle.read()
    rules = json.loads(raw)
    upstream = {"name": rules["name"], "fingerprint": hashlib.sha256(raw).hexdigest()[:16]}
    slot = request["action"]["slot"]
    context = request["context"]
    limitations = []
    if not os.access(os.path.join(arguments.upstream, "helpers", "validate"), os.X_OK):
        limitations.append({"kind": "missing_helper", "detail": "helpers/validate is not installed"})

    if slot == "gather_context":
        if not request.get("answer"):
            respond(arguments.response, request, "input_request", question="Which ticket does this change close?", upstream=upstream)
            return 0
        ticket = request["answer"].strip()
        body = "## Why\n\nCloses " + ticket + ". " + context["description"].strip() + "\n\n## Testing\n\nDescribe how it was tested.\n"
        respond(arguments.response, request, "proposal", message="Proposed from " + ticket, upstream=upstream,
                proposals=[{"target": "description", "text": body}], limitations=limitations)
        return 0

    if slot == "check_preparation":
        findings = []
        for section in rules["required_sections"]:
            present = section in context["description"]
            findings.append({"check": "Section " + section, "state": "met" if present else "unmet"})
        if rules.get("self_review_marker") == "required" and not os.path.exists(os.path.join(arguments.upstream, "SELF_REVIEWED")):
            limitations.append({"kind": "review_marker", "detail": "run the upstream tool's own self-review; the companion does not write its marker"})
        if not rules.get("creation_dry_run"):
            message = "Checked with the separately callable validator; the creation script has no dry run and was not used."
        else:
            message = "Checked."
        respond(arguments.response, request, "findings", message=message, findings=findings, limitations=limitations, upstream=upstream)
        return 0

    if slot == "publish_draft":
        plan = request["plan"]
        with open(os.path.join(arguments.upstream, "published-plans.jsonl"), "a", encoding="utf-8") as handle:
            handle.write(json.dumps(plan, sort_keys=True) + "\n")
        # The upstream tool's own behaviour, as the course sets it up: an
        # answer lost on the way, or the number GitHub gave the pull request.
        if os.path.exists(os.path.join(arguments.upstream, "LOSE_ANSWER")):
            os.remove(os.path.join(arguments.upstream, "LOSE_ANSWER"))
            return 0
        number_path = os.path.join(arguments.upstream, "CREATED_PULL_REQUEST")
        if os.path.exists(number_path):
            with open(number_path, encoding="utf-8") as handle:
                number = handle.read().strip()
            respond(arguments.response, request, "completed", message="The upstream tool created the draft pull request.", upstream=upstream,
                    references=[{"kind": "pull_request", "state": "created", "id": number}])
        else:
            respond(arguments.response, request, "uncertain", message="The upstream tool exited before saying.", upstream=upstream)
        return 0

    if slot == "finish_publication":
        made = {(item["kind"], item["id"]) for item in request.get("previous", []) if item["state"] in ("created", "updated")}
        references = []
        if ("ticket", "ENG-42") not in made:
            references.append({"kind": "ticket", "state": "updated", "id": "ENG-42", "detail": "linked to the pull request"})
        if ("visual", "screenshot") not in made:
            uploaded = os.path.exists(os.path.join(arguments.upstream, "UPLOAD_OK"))
            references.append({"kind": "visual", "state": "created" if uploaded else "failed", "id": "screenshot",
                               "detail": "" if uploaded else "the upload was refused"})
        respond(arguments.response, request, "completed", message="Follow-up done as far as it went.", references=references, upstream=upstream)
        return 0

    respond(arguments.response, request, "failed", message="This companion has no " + slot + " step.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
