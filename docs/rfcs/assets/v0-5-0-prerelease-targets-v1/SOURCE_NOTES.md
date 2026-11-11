# Source notes, 2026-11-10

The owner's notes from a day with the 0.4.2 prerelease, as pasted into the
authoring session. This file is the preserved original: do not edit a note
once it is here; later notes from the owner are appended under their own
date. Each
note is numbered here only so the RFCs and the decision record can cite it;
the wording is the owner's.

1. we need "returned to me" as a status for my PRs on home for when i have
   action to take on one of my reviews like responding to comments. Or
   alternatively it can just go back into Needs me or My pull request, but it
   would need a clear reason attached so I can tell that someone is waiting on
   me or the PR is ready to merge etc. I'm not sure which is better.
2. we need to improve this tour publish ux, should at least give progress
   updates as its doing multi-part publishes. And the error like route doesnt
   support are hard to tell they're even important. And if we know what the
   route does or doesnt support why ont we just pick the right one?
3. we need to fix the tour ux on overview
4. Drafted tour stop references to code in the tour stops can these become
   real links or snippets instead of saying something like (lines N-M).
5. I would like if agent tab and side chat responses could make links
   clickable to navigate to files in the PR. There are many
   filenames:NNN-MMM references in the responses and it would be nice to
   follow them.
6. Tour publish shouldnt be hidden away at the bottom.
7. Tour model picker should stay pinned at the bottom not be off screen when
   there are many stops.
8. Tour Enhance should run through the side chat for that section's session.
9. Tour stops that talk about code behind feature flags where the existing
   path is maintained wholly should not talk about that existing behavior if
   it's just being moved into a conditional branch.
10. The tour side chat window UX got made ugly as heck by Codex, we need to
    fix it especially at the bottom. I feel like the little blue text that
    says apply chat to the code is sufficient. I also feel like the
    individual stop sessions tend to respond with an immediate accept prompt
    which seems much nicer than the whole tour right now where if I want to
    make a change it seems like it always takes one turn to tell it what i
    want and a second turn to apply it to the tour.
11. Agent tab shows agent spend cost at the bottom after a turn ends, tour
    should too. It seems like cost is cumulative not per turn so maybe it
    shouldn't be in the turn footer but in the session footer? Or maybe cost
    should be the per turn amount. Not sure. It's also printing cost out with
    an absurd precision, I think dollars and cents is sufficient.
12. In side chat and chat we should change from cmd+enter send | enter
    newline to enter send | shift+enter new line. This matches much better
    with the existing agent desktop apps.
13. It seems like after I left a review that had the review tour tab with
    side chat open and went back to home and opened a new review that new
    review had the tout tab with side chat open once I went back to the
    review mode. The tab state should have been reset back to the first tab
    with fresh state for a new review.
14. We should probably maintain that indigo means agentic feature on buttons
    throughout the app, aka agent tab too. Things that don't cost money and
    are purely mechanical stay the normal colors.
15. Agent review check said "I can't write files in this session, so a
    script-based parity check is out." and then proceeded to do something
    that was almost certainly more expensive than if it had written a little
    disposable script. Is there a way to let the agent write scratch scripts
    without mutating the tracked repo files? Being able to opt-in to let it
    build and run tests would be cool too, but not as important.
16. When I go to approve a PR after having used the app for a while it
    doesn't actually approve. The technical details disclosure says
    submitting as me but nothing happens. The top area where it's supposed to
    show that progress indicator nothing happens there. This fixes itself
    after I close the app navigate back to the same PR and try again. I dunno
    what the cause is, I certainly went through several PRs, agent workflows,
    tours, etc before trying to do that approve. It's not the first time I've
    seen this behavior so I'm pretty sure it predates tours work.
17. The suggestions from the overview mode agent tab don't work. "Review this
    pull request" errors with "Not sent, Select the lines to ask about in the
    diff then send". I mean the review this pull request suggestion shouldn't
    be sending selected lines as context anyways that doesn't make much
    sense. The whole "what this agent may read" field is confusing to me.
    Like the agent can read the whole PR and workspace. So does this really
    mean like extra context? If I pick whole PR is it going to send the whole
    PR in the prompt? If that's the case I usually wan't no extra context but
    that's not an option. So selected lines is a good extra context default
    except there is no way to unselect all lines right now so that seems to
    confuse the prompt. Maybe if escape in the diff view would unselect all
    lines then we could keep that as a default. But I'm still unclear about
    the purpose and cost of that option. Even when I selected "Pull request"
    for that option and used the suggested prompt of Review this PR the
    response I got back started with "I reviewed the one file in this
    review's scope" and named hte file I had open and ignored the rest of the
    PR.
18. So one request to review a large PR responded with "I've started three
    parallel reviewers" but then there is no UX in the agents tab to view
    background/sub-agents or their progress so I have no idea if the agent is
    running or salled or anything. It would be great to see thinking in
    general like we can in claude code but I don't necessairly need that, but
    seeing that there is active agent work happening seems pretty imporant.
    Best as I can tell it must have silently failed because it's been 15
    minutes and there was no further response and my external API spend
    tracker hasn't moved.

## Added later the same day

19. One thing I forgot in my notes, I think going to a tour stop should go to
    the code by default, right now it's an extra click. The current live UX
    between a tour stop edit mode and non-edit mode is also not super clear.
    Like it's clear that we're in edit mode or not but there feels like too
    many options outside of edit mode still.

## Said during authoring, same day

These four remarks were made in the authoring conversation in reply to
design proposals for note 19. They are the owner's words.

20. I feel like Ready-next is awkward
21. Clicking the ring seems a bit dubious to me, we use that for navigation
    and opening/collapsing the stop already I feel.
22. I dont think I've ever clicked suggest an outline. I'm not sure it's a
    useful button to keep. The draft tour is the only one I've used.
23. Retire outline, but I think accept still has value, and we already have
    a visual language for that for the files in the PR review. I think when
    a stop changes it makes sense to accept it, eg the review gets updated
    and the hunk its attached to or a link in the stop comment is changed.

    I think the yellow/green ring is good. The button, especially as
    currently conceived, is bad.

## On the local PR drafts pack, same day

24. reoncile with /Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1
    make sure when we implement that pack we keep all the decisions we made
    here in this one
25. alternatively we can absorb bpr-local-pr-drafts-rfc-pack-v1  into our
    0.5.0 pack
26. I think I would be more comfortable with the 0.5.0 pack being the one
    pack we import and implement here, just take the good from the other
    pack

## On the agent accent, same day

27. Agent accent color is configurable in settings and just defaulted to
    indigo right

    (Told it is fixed in code today and asked whether to make it a
    configurable Appearance colour defaulting to indigo:) Yes, configurable

28. (Shown the six proposed fitting rules for the absorbed RFCs as a list
    and asked to confirm them as written:) Confirm all six
29. (Asked whether the local PR draft milestones are part of the 0.5.0
    release:) All of them
