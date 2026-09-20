# Modified file with a missing selected endpoint

Synthetic, credential-free data only. The two-file reading fixture is transformed
so the selected base has no `review.cpp`, while head has its nonempty regular
blob. `secondary.cpp` remains nonempty and different on both sides. The serving
overlay deliberately reports both files as modified, matching the observed class
where provider metadata and the exact selected comparison have different bases.

Generate deterministically with
`python3 tests/scripts/verify_modified_endpoint_fixture.py --generate`; omit the
flag to verify immutable bytes and strict installed-Git import. The production
composition regression must refuse M plus an absent endpoint, clear stale source,
retain exact anchors and successfully load the intact control file afterwards.
