# Archive validation record

Authored 2026-09-23 America/Los_Angeles. This record concerns the downloadable RFC
pack, not BPR source or a live imported plan.

The final archive was checked using the bundled read-only pack validator and its
19 validator unit tests (including negative cases) under Python 3.10+ in the authoring environment. The validator
checks file integrity/inventory, five RFC frontmatters, 27 card identities and
dependencies, eight milestones, all 48 requirement owners, complete label catalog,
parser/edit fixture consistency, exact predecessor sidecar strings and normalized
counter conservation. It exhausts 4,096 label subsets for two resolution modes
across four scopes (32,768 projections). Counter checks operate on normalized
synthetic facts; they are not a test of BPR's actual parser or provider normalizer.

Commands used from the extracted pack root:

```sh
python3 -B scripts/verify_pack.py
python3 -B -m unittest discover -s tests -p 'test_*.py' -v
```

Archive extraction, payload SHA-256 inventory and the external ZIP checksum are
also verified during packaging. No application build, C++ test, live import,
GitHub request, model invocation or visual/native acceptance was performed.
The upstream ZIP payloads were inaccessible; their attached checksum sidecars
are recorded, not presented as successful validation of those upstream archives.
All implementation/UX/service requirements remain pending product evidence.
