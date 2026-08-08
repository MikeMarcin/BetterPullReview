(function (root) {
  "use strict";

  const STORAGE_PREFIX = "bpr.attended-review.v1";
  const VERDICT_SCHEMA = "bpr.attended_review_verdict";

  function identityKey(subject) {
    return `${subject.subject_id}@${subject.subject_digest}`;
  }

  function storageKey(batchId, subject) {
    return `${STORAGE_PREFIX}:${batchId}:${subject.subject_id}:${subject.subject_digest}`;
  }

  function createMemoryStorage() {
    const values = new Map();
    return {
      getItem(key) {
        return values.has(key) ? values.get(key) : null;
      },
      setItem(key, value) {
        values.set(key, String(value));
      },
      removeItem(key) {
        values.delete(key);
      },
    };
  }

  function availableStorage() {
    try {
      if (!root.localStorage) {
        return createMemoryStorage();
      }
      const probe = `${STORAGE_PREFIX}:probe`;
      root.localStorage.setItem(probe, "1");
      root.localStorage.removeItem(probe);
      return root.localStorage;
    } catch (_error) {
      return createMemoryStorage();
    }
  }

  function normalizeSaved(value) {
    if (!value || typeof value !== "object") {
      return { decision: null, notes: "" };
    }
    const decision = value.decision === "accept" || value.decision === "reject"
      ? value.decision
      : null;
    return {
      decision,
      notes: typeof value.notes === "string" ? value.notes : "",
    };
  }

  function auditFailed(subject) {
    return Boolean(subject.audit_verdict && subject.audit_verdict.status === "fail");
  }

  class ReviewController {
    constructor(config, storage) {
      if (!config || !Array.isArray(config.subjects)) {
        throw new Error("review config must contain subjects");
      }
      if (!config.downstream_claims || typeof config.downstream_claims !== "object") {
        throw new Error("review config must declare downstream_claims");
      }
      this.config = config;
      this.groups = new Map();
      for (const group of config.selection_groups || []) {
        this.groups.set(group.group_id, group);
      }
      for (const subject of config.subjects) {
        const token = subject.choice_group;
        if (token && !this.groups.has(token)) {
          this.groups.set(token, { group_id: token, min_accept: 1, max_accept: 1 });
        }
      }
      this.storage = storage || availableStorage();
      this.states = new Map();
      for (const subject of config.subjects) {
        const raw = this.storage.getItem(storageKey(config.batch_id, subject));
        let parsed = null;
        if (raw !== null) {
          try {
            parsed = JSON.parse(raw);
          } catch (_error) {
            parsed = null;
          }
        }
        const state = normalizeSaved(parsed);
        if (state.decision === "accept" && auditFailed(subject)) {
          state.decision = null;
        }
        this.states.set(identityKey(subject), state);
      }
    }

    subject(subjectId, subjectDigest) {
      const found = this.config.subjects.find(
        (item) => item.subject_id === subjectId && item.subject_digest === subjectDigest
      );
      if (!found) {
        throw new Error(`unknown subject identity: ${subjectId}@${subjectDigest}`);
      }
      return found;
    }

    state(subject) {
      return this.states.get(identityKey(subject));
    }

    save(subject) {
      this.storage.setItem(
        storageKey(this.config.batch_id, subject),
        JSON.stringify(this.state(subject))
      );
    }

    subjectGroupId(subject) {
      return subject.selection_group || subject.choice_group || null;
    }

    groupMembers(groupId) {
      return this.config.subjects.filter((item) => this.subjectGroupId(item) === groupId);
    }

    groupTally(groupId) {
      const group = this.groups.get(groupId);
      const members = this.groupMembers(groupId);
      let chosen = 0;
      let rejected = 0;
      for (const member of members) {
        const decision = this.state(member).decision;
        if (decision === "accept") chosen += 1;
        if (decision === "reject") rejected += 1;
      }
      const undecided = members.length - chosen - rejected;
      const satisfied = (chosen >= group.min_accept && chosen <= group.max_accept)
        || (chosen === 0 && rejected === members.length);
      return { group, members, chosen, rejected, undecided, satisfied };
    }

    setDecision(subjectId, subjectDigest, decision) {
      if (decision !== "accept" && decision !== "reject" && decision !== null) {
        throw new Error(`invalid decision: ${decision}`);
      }
      const subject = this.subject(subjectId, subjectDigest);
      if (decision === "accept" && auditFailed(subject)) {
        throw new Error("subject failed its mechanical audit; accept is unavailable");
      }
      const groupId = this.subjectGroupId(subject);
      if (decision === "accept" && groupId && this.groups.has(groupId)) {
        const tally = this.groupTally(groupId);
        const alreadyChosen = this.state(subject).decision === "accept";
        if (!alreadyChosen && tally.chosen >= tally.group.max_accept) {
          if (tally.group.max_accept === 1) {
            for (const member of tally.members) {
              const memberState = this.state(member);
              if (memberState.decision === "accept") {
                memberState.decision = null;
                this.save(member);
              }
            }
          } else {
            throw new Error(
              `selection group '${groupId}' allows at most ${tally.group.max_accept} chosen alternatives; clear one first`
            );
          }
        }
      }
      this.state(subject).decision = decision;
      this.save(subject);
      return this.summary();
    }

    setNotes(subjectId, subjectDigest, notes) {
      const subject = this.subject(subjectId, subjectDigest);
      this.state(subject).notes = String(notes || "");
      this.save(subject);
      return this.summary();
    }

    summary() {
      let accepted = 0;
      let rejected = 0;
      let notChosen = 0;
      let pending = 0;
      for (const subject of this.config.subjects) {
        const decision = this.state(subject).decision;
        if (decision === "accept") accepted += 1;
        if (decision === "reject") rejected += 1;
        if (!decision && !this.subjectGroupId(subject)) pending += 1;
      }
      for (const groupId of this.groups.keys()) {
        const tally = this.groupTally(groupId);
        if (tally.satisfied) {
          notChosen += tally.undecided;
        } else {
          pending += tally.undecided || 1;
        }
      }
      const total = this.config.subjects.length;
      return {
        accepted,
        rejected,
        notChosen,
        resolved: total - pending,
        pending,
        total,
      };
    }

    buildVerdict() {
      for (const groupId of this.groups.keys()) {
        const tally = this.groupTally(groupId);
        if (!tally.satisfied) {
          throw new Error(
            `cannot export: selection group '${groupId}' is unresolved; choose between `
            + `${tally.group.min_accept} and ${tally.group.max_accept} alternatives or reject every member`
          );
        }
      }
      const summary = this.summary();
      if (summary.pending !== 0) {
        throw new Error(`cannot export: ${summary.pending} subject(s) remain pending`);
      }
      const accepted = [];
      const rejected = [];
      const notChosen = [];
      const notes = {};
      for (const subject of this.config.subjects) {
        const state = this.state(subject);
        const identity = {
          subject_id: subject.subject_id,
          subject_digest: subject.subject_digest,
        };
        if (state.decision === "accept") {
          accepted.push(identity);
        } else if (state.decision === "reject") {
          rejected.push(identity);
        } else {
          notChosen.push(identity);
        }
        if (state.notes.trim()) {
          notes[identityKey(subject)] = state.notes;
        }
      }
      const verdict = {
        schema_name: VERDICT_SCHEMA,
        schema_version: 1,
        batch_id: this.config.batch_id,
        review_gate: this.config.review_gate,
        review_scope: this.config.review_scope,
        accepted_subjects: accepted,
        rejected_subjects: rejected,
        notes_by_subject_identity: notes,
        downstream_claims: { ...this.config.downstream_claims },
      };
      if (this.groups.size > 0) {
        verdict.not_chosen_subjects = notChosen;
      }
      return verdict;
    }
  }

  async function copyText(text, environment) {
    const env = environment || root;
    if (env.navigator && env.navigator.clipboard && env.navigator.clipboard.writeText) {
      try {
        await env.navigator.clipboard.writeText(text);
        return "clipboard";
      } catch (_error) {
        // Local file origins frequently expose Clipboard API but deny writes.
        // Continue to the legacy selection-based copy path in that case.
      }
    }
    const document = env.document;
    if (!document || !document.execCommand) {
      throw new Error("clipboard copy is unavailable; copy the displayed JSON manually");
    }
    const textarea = document.createElement("textarea");
    textarea.value = text;
    textarea.setAttribute("readonly", "");
    textarea.style.position = "fixed";
    textarea.style.opacity = "0";
    document.body.appendChild(textarea);
    textarea.select();
    const copied = document.execCommand("copy");
    document.body.removeChild(textarea);
    if (!copied) {
      throw new Error("clipboard copy failed; copy the displayed JSON manually");
    }
    return "execCommand";
  }

  function initializePage(document) {
    const configNode = document.getElementById("attended-review-config");
    if (!configNode) return null;
    const config = JSON.parse(configNode.textContent);
    const controller = new ReviewController(config);
    const cards = Array.from(document.querySelectorAll("[data-subject-id]"));
    const progress = document.getElementById("review-progress");
    const totals = document.getElementById("review-totals");
    const pendingOnly = document.getElementById("pending-only");
    const exportButton = document.getElementById("export-verdict");
    const exportOutput = document.getElementById("verdict-output");
    const reviewStatus = document.getElementById("review-status");

    function cardSubject(card) {
      return controller.subject(card.dataset.subjectId, card.dataset.subjectDigest);
    }

    function render() {
      const summary = controller.summary();
      progress.textContent = `${summary.resolved}/${summary.total} reviewed`;
      const segments = [`${summary.accepted} accepted`, `${summary.rejected} rejected`];
      if (controller.groups.size > 0) {
        segments.push(`${summary.notChosen} not chosen`);
      }
      segments.push(`${summary.pending} pending`);
      totals.textContent = segments.join(" · ");
      exportButton.disabled = summary.pending !== 0;
      if (summary.pending !== 0) {
        exportButton.textContent = "Copy the complete verdict";
      }
      const tallies = new Map();
      for (const groupId of controller.groups.keys()) {
        tallies.set(groupId, controller.groupTally(groupId));
      }
      for (const card of cards) {
        const subject = cardSubject(card);
        const state = controller.state(subject);
        let reviewState = state.decision || "pending";
        const groupId = controller.subjectGroupId(subject);
        if (!state.decision && groupId) {
          const tally = tallies.get(groupId);
          if (tally && tally.satisfied) {
            reviewState = "not-chosen";
          }
        }
        card.dataset.reviewState = reviewState;
        card.hidden = Boolean(pendingOnly.checked && reviewState !== "pending");
        for (const button of card.querySelectorAll("[data-decision]")) {
          button.setAttribute("aria-pressed", String(button.dataset.decision === state.decision));
        }
      }
      for (const node of document.querySelectorAll("[data-selection-group-status]")) {
        const tally = tallies.get(node.dataset.selectionGroupStatus);
        if (!tally) continue;
        const range = tally.group.min_accept === tally.group.max_accept
          ? String(tally.group.max_accept)
          : `${tally.group.min_accept}-${tally.group.max_accept}`;
        if (tally.satisfied && tally.chosen === 0) {
          node.textContent = `Every alternative rejected: this question has no acceptable answer (needs ${range}).`;
        } else if (tally.satisfied) {
          node.textContent = `Satisfied: ${tally.chosen} chosen of ${range}; ${tally.undecided} not chosen, ${tally.rejected} rejected.`;
        } else {
          node.textContent = `Unresolved: ${tally.chosen} chosen of ${range} needed; ${tally.undecided} undecided, ${tally.rejected} rejected.`;
        }
      }
    }

    for (const card of cards) {
      const subject = cardSubject(card);
      const state = controller.state(subject);
      const notes = card.querySelector("[data-review-notes]");
      notes.value = state.notes;
      for (const button of card.querySelectorAll("[data-decision]")) {
        button.addEventListener("click", () => {
          let decision = button.dataset.decision === "clear" ? null : button.dataset.decision;
          if (decision && controller.state(subject).decision === decision) {
            decision = null;
          }
          try {
            controller.setDecision(subject.subject_id, subject.subject_digest, decision);
          } catch (error) {
            reviewStatus.textContent = error.message;
            return;
          }
          exportOutput.value = "";
          reviewStatus.textContent = "Decision saved in this browser.";
          exportButton.textContent = "Copy the complete verdict";
          render();
        });
      }
      notes.addEventListener("input", () => {
        controller.setNotes(subject.subject_id, subject.subject_digest, notes.value);
        exportOutput.value = "";
        reviewStatus.textContent = "Notes saved in this browser.";
        exportButton.textContent = "Copy the complete verdict";
      });
    }

    pendingOnly.addEventListener("change", render);
    exportButton.addEventListener("click", async () => {
      try {
        const text = `${JSON.stringify(controller.buildVerdict(), null, 2)}\n`;
        exportOutput.value = text;
        const method = await copyText(text);
        exportButton.textContent = "Copied — return it to the agent";
        reviewStatus.textContent = method === "clipboard"
          ? "Complete verdict copied. Paste it back to the requesting agent session."
          : "Complete verdict copied with the local fallback. Paste it back to the requesting agent session.";
      } catch (error) {
        exportButton.textContent = "Copy the complete verdict";
        exportOutput.focus();
        exportOutput.select();
        reviewStatus.textContent = `${error.message}. The complete JSON below is selected for manual copy.`;
      }
    });
    render();
    return controller;
  }

  const api = {
    ReviewController,
    copyText,
    createMemoryStorage,
    identityKey,
    initializePage,
    storageKey,
  };
  root.BprAttendedReview = api;
  if (root.document) {
    if (root.document.readyState === "loading") {
      root.document.addEventListener("DOMContentLoaded", () => initializePage(root.document));
    } else {
      initializePage(root.document);
    }
  }
})(typeof globalThis !== "undefined" ? globalThis : this);

// Full-resolution inspection. Opening and closing the viewer must not
// touch decision state or the export lock, so this only reads the DOM.
(function attachFullResolutionViewer() {
  // The runtime is loaded browserless by the skill's own tests, so it must not
  // touch the DOM at import time.
  if (typeof document === 'undefined' || !document.body) { return; }
  var viewer = document.createElement('div');
  viewer.className = 'media-viewer';
  viewer.hidden = true;
  viewer.setAttribute('role', 'dialog');
  viewer.setAttribute('aria-modal', 'true');
  viewer.innerHTML =
    '<div class="media-viewer-bar">' +
    '<span class="media-viewer-label"></span>' +
    '<span class="media-viewer-counter"></span>' +
    '<button type="button" class="media-viewer-close">Close</button>' +
    '</div><div class="media-viewer-surface"><img alt=""></div>' +
    '<button type="button" class="media-viewer-nav media-viewer-prev" aria-label="Previous image">&#8249;</button>' +
    '<button type="button" class="media-viewer-nav media-viewer-next" aria-label="Next image">&#8250;</button>';
  document.body.appendChild(viewer);

  var image = viewer.querySelector('.media-viewer-surface img');
  var label = viewer.querySelector('.media-viewer-label');
  var counter = viewer.querySelector('.media-viewer-counter');
  var closeButton = viewer.querySelector('.media-viewer-close');
  var prevButton = viewer.querySelector('.media-viewer-prev');
  var nextButton = viewer.querySelector('.media-viewer-next');
  var lastTrigger = null;

  function visibleTriggers() {
    return Array.prototype.filter.call(
      document.querySelectorAll('.media-zoom'),
      function (candidate) { return candidate.offsetParent !== null; }
    );
  }

  function close() {
    viewer.hidden = true;
    image.removeAttribute('src');
    if (lastTrigger) { lastTrigger.focus(); lastTrigger = null; }
  }

  function open(trigger, focusClose) {
    lastTrigger = trigger;
    image.src = trigger.getAttribute('data-full-source');
    image.alt = trigger.getAttribute('data-full-label') || '';
    label.textContent = trigger.getAttribute('data-full-label') || '';
    var list = visibleTriggers();
    var index = list.indexOf(trigger);
    var navigable = list.length > 1;
    counter.textContent = navigable && index >= 0 ? (index + 1) + ' / ' + list.length : '';
    prevButton.hidden = !navigable;
    nextButton.hidden = !navigable;
    viewer.hidden = false;
    if (focusClose !== false) { closeButton.focus(); }
  }

  function step(delta) {
    var list = visibleTriggers();
    if (!list.length) { return; }
    var index = list.indexOf(lastTrigger);
    var next = ((index < 0 ? 0 : index + delta) + list.length) % list.length;
    open(list[next], false);
  }

  document.addEventListener('click', function (event) {
    var trigger = event.target.closest ? event.target.closest('.media-zoom') : null;
    if (trigger) { event.preventDefault(); open(trigger); return; }
    if (viewer.hidden) { return; }
    if (event.target.closest && event.target.closest('.media-viewer-prev')) { step(-1); return; }
    if (event.target.closest && event.target.closest('.media-viewer-next')) { step(1); return; }
    if (event.target === viewer || event.target === closeButton) { close(); }
  });
  document.addEventListener('keydown', function (event) {
    if (viewer.hidden) { return; }
    if (event.key === 'Escape') { close(); return; }
    if (event.key === 'ArrowLeft') { event.preventDefault(); step(-1); return; }
    if (event.key === 'ArrowRight') { event.preventDefault(); step(1); }
  });
})();
