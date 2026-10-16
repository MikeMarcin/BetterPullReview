<!--
Thanks for opening a pull request! Please fill in each section below.
Lines inside this comment are hidden when the description is rendered.

- Link the issue this fixes.
- Describe how you tested the change.
-->

## Summary

Moves the cache writer onto the serialized lane.
The reader no longer races the writer during a refresh,
and a cancelled refresh leaves the previous snapshot in place.

<!-- Reviewers: the interesting part is in cache_writer.cpp. -->

## Related issues

Fixes #1234
Follows up on the design in #1200[^design].

## How was this tested?

- [x] Unit tests for the writer lane
- [x] Ran the refresh course 50 times
- [ ] Tested on Windows

```sh
ctest --preset dev -R cache_writer
# <!-- this comment is part of the code and stays -->
```

> [!NOTE]
> The on-disk format is unchanged.
> Existing caches keep working after the upgrade.

> [!WARNING]
> Downgrading after this change discards the pending queue.

## Checklist

- [x] I have read the `<!-- contributing -->` guide
- [x] Documentation is updated[^docs]

[^design]: The lane design was agreed in the March architecture review.
[^docs]: See docs/architecture/cache.md.
