# Source notes, 2026-11-10

The owner's words from the authoring session, in order. This file is the
preserved original: do not edit a note once it is here. Each note is numbered
only so the RFCs and the decision record can cite it. Where a note answers a
question, the question is given first in the author's wording.

1. Quoting "Self-review on a local PR draft" from a list of places an agent
   could add value:

   > I think we could also let the agent fix findings on the local PR draft.
   > I think local PR draft should allow us to edit the files and commit them /
   > push them, and let the agent do the merges squashes whatever.
   >
   > We have a pretty darned good text editor already it'd be cool to use it.
   >
   > I also kinda wonder about building real intellisense/syntax highlighting
   > info if the agent knows how to do the build maybe we could get like goto
   > symbol, real non-fake AST syntax highlighting. This might be a stretch,
   > and should probably not happen automatically it's probably expensive...
   > although maybe not on your own PR.

2. Asked which editor was meant:

   > I just meant we have a generally good text editing interface with virtual
   > spaces, multi-edit, etc.

3. After being told that in-place editing needs a write port, a safe save and
   a check of whole-file scale, and that go-to-definition needs a compiler:

   > Hmm I think we can introduce the text editor with real mutation and
   > saving with git operations too. I think the goto definition stuff can
   > stay on the wishlist for now, it doesn't offer so much value if you're
   > going to do major edits you probably want to open your poject in VSCode
   > or Visual Studio to do that anyways. But minor self edits and let the
   > agent do most of the changes is probably good.

4. Asked "Reopen PUBLISH-01 so BPR runs commit and push through your own
   Git?", after a message that proposed the split (the author edits, BPR
   commits and pushes with ordinary buttons, the agent fixes, merges, squashes
   and rebases), said that this needs a second Git runner beside the isolated
   one, and listed three limits (the second runner works only on a local
   draft's own worktree and only on the author's press; nothing force-pushes,
   and a rewrite of an already-pushed branch stops and asks; a save writes
   only if the file still matches what the draft captured):

   > Yes, reopen it

5. The instruction that produced this pack:

   > so /author-rfc our final suggestions as a pack and import it

6. Sent while the pack was being imported:

   > make sure that self-review gets its own prompt in agent settings
