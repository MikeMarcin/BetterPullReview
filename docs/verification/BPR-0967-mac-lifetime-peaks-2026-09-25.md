# BPR-0967 Mac lifetime footprint peaks — 2026-09-25

Code candidate `8b6407fd`, macOS 26.6.2, Apple M4 Pro, 48 GiB RAM,
arm64 Debug, Qt 6.11.2. The real composed large-review fixture ran through
the network-isolated CTest courses. Four affected rows passed 4/4 with the
new peak gate enabled, in 442.35 seconds total. The cache-byte course took
339.13 seconds, cache entries 73.46, loaded close 13.95 and resources 15.79.

The Mac sampler now reads `task_vm_info.ledger_phys_footprint_peak` and checks
its process-lifetime high-water mark against the course's existing footprint
growth allowance. Growth subtracts the `before_application` physical-footprint
sample, so any earlier startup peak is included conservatively. The measured
baseline lifetime peak equaled that sample in all four final runs. This native
counter observes a peak between fixture checkpoints; it does not locate the
peak within a phase or measure live-heap peaks. No threshold was raised.

| Course | Largest sampled growth | Lifetime-peak growth | Existing limit | Largest sampled cache |
| --- | ---: | ---: | ---: | ---: |
| Loaded close | 136.61 MiB | 136.61 MiB | 160 MiB | 3.35 MiB |
| Resources | 124.27 MiB | **139.22 MiB** | 160 MiB | 3.35 MiB |
| Cache entries | 87.19 MiB | 87.20 MiB | 160 MiB | 0.20 MiB |
| Cache bytes | 171.41 MiB | 171.41 MiB | 184 MiB | 63.74 MiB |

The resources course's 14.95 MiB gap between sampled and lifetime-peak growth
is a concrete previously unsampled high point. Cache-byte lifetime growth is
12.59 MiB below its existing limit in this run. The suite also retained its
exact-source, cache-ceiling, eviction, cleanup and shutdown assertions. Live
heap growth after shutdown was 62.31–67.71 MiB across the four runs, and
12.64–14.17 MiB after application destruction.

Machine-readable reports remain local build outputs. Their SHA-256 digests
bind the observations without committing transient artifacts:

| Report in `build/dev/tests/end_to_end/` | SHA-256 |
| --- | --- |
| `large-review-loaded-close.json` | `444702d3a97593d571d2d12e1339464cdcdb84686255ea1e26ef46f55c9e8ffa` |
| `large-review-resources.json` | `007d888189a95d2315272650fb5cffe791074290950f4c8085554fbef7befc83` |
| `large-review-cache-entries.json` | `672fbaaa5a1c145f25e2085ab6cfbf305ab1938e40b254bafbd522d325092f5c` |
| `large-review-cache-bytes.json` | `b3d3328fb408a1850f9c16221c7b2012824892952cd27c77beed9d22524fb297` |

This establishes a Mac physical-footprint peak gate for these four fixture
courses. BPR-0967 still needs native Windows/Linux memory and responsiveness
measurements, and the broader launch, metadata, first-render, navigation,
drawers, theme, active-work input and cancellation budgets. The Mac counter
does not establish peak live heap or cross-platform behavior.
