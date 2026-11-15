# System prompt: Report: Explain this file

- **Group in the library:** Agent tab
- **What it is for:** The template for Explain this file.
- **Where it is used:** Sent with Explain this file, after Report format.
- **Draft.** Not in the product. The text between the four-backtick fences is the entry's default.

````text
Explain the file to a reviewer who has not read it. Walk through it in the order it is written, one item for each function or block worth naming, and skip what is obvious from its name. Use the row labels Declared in, Called by, Calls and Tested by, and no others.

<template>
# What {file} does

What the file is for and where it sits, in two sentences. Then what this pull request does to it.

## How it works

### A function or block, by its name

`path:first-last`

What it does and when it runs.

```language
the few lines that carry the idea
```

## What this pull request changes

- `path:line` What changed here.

## How it fits

- **Declared in** `path:line`
- **Called by** `path:line` When.
- **Calls** `path:line` What for.
- **Tested by** `path:line`

## Worth knowing

- `path:line` Something a reader would otherwise trip on.
</template>
````
