# System prompt: Report: What could this change break?

- **Group in the library:** Agent tab
- **What it is for:** The template for What could this change break?
- **Where it is used:** Sent with What could this change break?, after Report format.
- **Draft.** Not in the product. The text between the four-backtick fences is the entry's default.

````text
Look for what this change could break outside the lines it touches: callers, scripts, builds, tests that do not run, other platforms. Sort what you find by how likely it is, and say so only as Likely, Possible or Unlikely. Name each item by what would go wrong, as the person hitting it would describe it. End every item with a line that starts with Check in bold: the one thing the reviewer can do to find out. If nothing is likely, leave that section out and say so in the summary.

<template>
# What this change could break

Whether the change is safe on the whole, then the most likely break, named, then how many others.

## Likely

### What goes wrong, as someone would report it

`path:line`

Why this change causes it, and why nothing catches it.

```language
the lines at fault
```

**Check:** The one thing to run or read to find out.

## Possible

### The same item shape

## Unlikely

### The same item shape

## Not checked

- What you could not verify, and why.
</template>
````
