# Diff blocks

A unified diff, as `git diff` writes it:

```diff
diff --git a/src/cache.cpp b/src/cache.cpp
index 3f2a1c0..9b8e7d4 100644
--- a/src/cache.cpp
+++ b/src/cache.cpp
@@ -12,7 +12,8 @@ void cache::refresh()
 {
-	auto guard = std::lock_guard{ Mutex };
-	Snapshot = read();
+	auto next = read();
+	auto guard = std::lock_guard{ Mutex };
+	Snapshot = std::move(next);
 	Generation += 1;
 }
\ No newline at end of file
```

A patch block, with only changed lines:

```patch
-old line
+new line
 context line
```

Signs without a space, a lone sign, and lines that only look like headers:

```diff
+
-
++double plus
--double minus
@@ not a range
@@@ -1,2 -1,2 +1,3 @@@ combined
*** a/old.txt
--- b/new.txt
***************
*** 1,2 ****
=== modified
+++
---
```

A `udiff` block:

```udiff
@@ -1 +1 @@
-a
+b
```

Indented diff is ordinary code:

    -not removed
    +not added
