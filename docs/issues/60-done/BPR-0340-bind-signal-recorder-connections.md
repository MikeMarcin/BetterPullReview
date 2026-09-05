---
schema: "implement-rfc.card.v1"
id: "BPR-0340"
title: "Bind the landing-model signal recorder's connections to its lifetime"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: []
completed: "2026-08-17"
---

# Bind the landing-model signal recorder's connections to its lifetime

`qt_review_landing_model` failed on Linux, inside and outside the test
sandbox alike. The model was never wrong.

`signal_recorder` is a stack object that counts model signals. Its constructor
called the three-argument `QObject::connect(sender, signal, functor)`, which
names no receiver. That overload ties the connection's lifetime to the
*sender*, so each connection outlived the recorder that owned the counters,
and the lambda kept capturing a `this` that no longer pointed at a live
object.

`test_an_added_item_inserts_and_a_departed_one_removes` builds two recorders in
sequence against the same model, in sibling scopes. The second occupies the
storage the first just released. When the second `apply` ran, both the live
lambda and the dead one incremented, and they incremented the same address, so
`Removals` reached 2 where the test required 1.

This is undefined behavior, not a platform difference. macOS never showed it
because the outcome depends on stack reuse, and reuse depends on the compiler
and the frame layout. Linux/GCC happened to place the two recorders on top of
each other. That is what "works on the other platform" is worth here.

## The repair

Each connection now names a `QObject Context` member as receiver, so the
connection is removed when the recorder is destroyed. Nothing else changed:
not the counters, not the assertions, and not the model.

The diagnosis rests on that being the only change. Binding a connection's
lifetime cannot alter what the model emits, so a defect in the model could not
have been repaired by it.

The model's behavior was correct throughout. Removing item 2 from {1, 2, 3}
issues exactly one `beginRemoveRows`, and the survivor needs no move because
the removal already shifts it.

## Result

`qt_review_landing_model` passes on Ubuntu 26.04 x86_64 with GCC 15.2 and Qt
6.10.3, directly and through CTest.
