## Problem

Reading data can crash when that data is deleted concurrently.

## Solution

Retain an entry lifetime handle during the read, acquiring it before releasing the lookup lock. This addresses entry lifetime, not general synchronization of mutable contents.

## Review guide

[Start the author's review tour](https://github.example.invalid/acme/demo/pull/42#pullrequestreview-900)

## Tracking

Jira: DEMO-42 (synthetic reference; no ticket lookup performed).
