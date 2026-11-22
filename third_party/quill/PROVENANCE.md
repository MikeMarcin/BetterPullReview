Quill v13.0.0

Source: https://codeload.github.com/odygrd/quill/tar.gz/refs/tags/v13.0.0

SHA256: 88b4a1542125577a4d51cf444c51e34d63618c422ba6a4fa9bd23894b49d696b

Archive size: 2061083 bytes. Header-only controlled provider; upstream CMake, examples, tests and install rules are not executed. MIT license and bundled fmt notice retained alongside this record.

The provider backports the thread-context retirement ordering from Quill v13.1.0.
It applies the header change after archive verification and checks both the
input and patched header digests. The exact replacement is in
[`cmake/BprQuillProvider.cmake`](../../cmake/BprQuillProvider.cmake).
[Upstream commit d15b208](https://github.com/odygrd/quill/commit/d15b208c8e83f7f25eca59fea7c8b909e8d34174).
