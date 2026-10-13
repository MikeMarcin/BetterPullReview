# M5 native Search design

Mandate `find-and-find-in-files-v1-r1`; full final `pr-search` binding.
Required frontend-design loaded before UI work; native palette/UI font/source font,
no QSS, native controls and focus rings take precedence over free aesthetic axes.
The Search tab is beside Files in the existing drawer. Query/options occupy two
compact rows, exact scope and qualified coverage are visible, and optional path
filters/commit identities/skipped details use a disclosure. Ordered immutable
records append into existing native tree groups without model resets. Enter opens
only a chosen leaf; query Enter flushes work. Escape returns to source; Close
cancels and restores prior drawer/tab state. No result opens while typing.

The window owns action/focus translation and core-projected legality. A separate
profile SearchEffects worker runs the existing PR corpus/matcher execution. The
controller retains independent runtime query state and one pending reveal sealed
by authority, exact catalogue/query generation, target tab, pair, path, side, blob
and original span. Existing workbench/epoch/tab ports load the file and grant exact
endpoints. At most sixteen tab-owned historical entries plus the next open are
retained outside Files membership; ordinary Files filters/receipts are unchanged.
Initial result opening from a window scope adopts the newly created active tab
without a second scan, while the reveal still binds the real target tab. Every
later explicit tab/pair/authority/reading gesture invalidates that continuation.

The completed design is accepted on product `7d56dc1e5` and full closeout
`b82247dc6`. Numeric tree indexes borrow immutable records; the 8 MiB projection/
tab reserve stays inside the 192 MiB component admission. A monotonically newer
reading gesture defeats pending A→B→A reveal even when document identities
coincide. Cold acquisition yields/retries while the original nonempty query
finishes full coverage. Complete evidence is in m5-full-criteria.json. Original
failed commands and inherited M3 global tidy limitations remain retained with
precise corrective/reuse dispositions. Main landing belongs to the coordinator;
no required human criterion remains.
