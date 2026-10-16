<p align="center">
  <b>Release 4.2</b> &middot; <a href="https://example.test/notes">Release notes</a>
</p>

## Summary

Speeds up the cache writer :rocket: and fixes the crash on resume :bug:.
Press <kbd>Cmd</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> to rebuild the index.

The p<sub>99</sub> write time drops from 120&nbsp;ms to 40&nbsp;ms (3&times; faster, see<sup>[2]</sup>).

<details>
<summary>Benchmark output</summary>

```text
cache_writer/serial     120 ms
cache_writer/lane        40 ms
```

| Case | Before | After |
| --- | --- | --- |
| cold | 120 ms | 40 ms |
| warm | 30 ms | 12 ms |
</details>

<details>
<summary>Checklist :white_check_mark:</summary>

- [x] Tests pass
- [x] Docs updated
</details>

<table>
  <tr><th>Platform</th><th>Status</th></tr>
  <tr><td>macOS</td><td>:white_check_mark: passing</td></tr>
  <tr><td>Windows</td><td>:warning: flaky</td></tr>
</table>

Thanks :+1:
