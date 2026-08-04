---
id: BPR-0147
title: Core verification evidence: what it may and may not conclude
status: Done
rfc: RFC-BPR-0009
depends_on: [BPR-0146]
---

# Core verification evidence: what it may and may not conclude

See the RFC-BPR-0009 verification plan for this slice's place in the sequence.

The rule this RFC exists to protect: **a nearby code change is evidence, not a
verdict.** Evidence reaches `needs verification` and never `verified`. Only an
explicit reviewer intent verifies, and this slice must not open a path around
that however convenient it would be.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- No path through this layer reaches `verified` without the explicit reviewer
  intent.
- Every refusal carries its own typed reason.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

`observe` records evidence against a defect. The **only** state it can produce
is `needs_verification`. There is no argument, no accumulation, and no special
kind that reaches `verified`: a reviewer verifies, or nobody does.

### Every kind, from every state, tested

The test drives all three evidence kinds from all three states that can receive
them and asserts each reaches `needs_verification` and none reaches `verified`.
That is nine cases rather than one illustrative example, because the failure
this guards against is a special case being added later for one kind.

### The case most likely to be wired wrongly

`thread_resolved_externally` is the sharpest. Someone resolving the thread in
GitHub is shared state changing, and it is exactly what a verification looks
like from outside — which is precisely why it must not verify. The reviewer who
owns this defect has said nothing.

Breaking this so an external resolution verifies is the most plausible
"helpful" change anyone would make here, and it fails immediately.

### Evidence never disturbs a decision

Evidence against a `verified`, `withdrawn`, or `accepted_as_is` defect changes
nothing — and that is not an error. Things happen near settled defects.
Reopening is a separate explicit action, and letting a late reply undo a
reviewer's judgement would be evidence overruling the person it exists to
inform.

Further evidence on a defect already awaiting the reviewer changes nothing
either. More evidence does not ask harder.

### The intended path still works

A reviewer may verify what evidence flagged — that is what evidence is *for*.
The verification is theirs, not the evidence's, and the test shows the sequence
completing.

### Non-vacuity

Two behaviours were broken separately, each failing its own assertion, then
restored to green:

- An external resolution verifying — "evidence asks the reviewer to look".
- Evidence reopening a verified defect — "evidence leaves a decided defect
  exactly as it was".

### A tidy finding worth keeping

USAGE: clang-tidy [options] <source0> [... <sourceN>]

OPTIONS:

Generic Options:

  --help                           - Display available options (--help-hidden for more)
  --version                        - Display the version of this program

clang-tidy options:

  --allow-no-checks                - Allow empty enabled checks. This suppresses
                                     the "no checks enabled" error when disabling
                                     all of the checks.
  --checks=<string>                - Comma-separated list of globs with optional '-'
                                     prefix. Globs are processed in order of
                                     appearance in the list. Globs without '-'
                                     prefix add checks with matching names to the
                                     set, globs with the '-' prefix remove checks
                                     with matching names from the set of enabled
                                     checks. This option's value is appended to the
                                     value of the 'Checks' option in .clang-tidy
                                     file, if any.
  --config=<string>                - Specifies a configuration in YAML/JSON format:
                                       -config="{Checks: '*',
                                                 CheckOptions: {x: y}}"
                                     When the value is empty, clang-tidy will
                                     attempt to find a file named .clang-tidy for
                                     each source file in its parent directories.
  --config-file=<string>           - Specify the path of .clang-tidy or custom config file:
                                      e.g. --config-file=/some/path/myTidyConfigFile
                                     This option internally works exactly the same way as
                                      --config option after reading specified config file.
                                     Use either --config-file or --config, not both.
  --dump-config                    - Dumps configuration in the YAML format to
                                     stdout. This option can be used along with a
                                     file name (and '--' if the file is outside of a
                                     project with configured compilation database).
                                     The configuration used for this file will be
                                     printed.
                                     Use along with -checks=* to include
                                     configuration of all checks.
  --enable-check-profile           - Enable per-check timing profiles, and print a
                                     report to stderr.
  --enable-module-headers-parsing  - Enables preprocessor-level module header parsing
                                     for C++20 and above, empowering specific checks
                                     to detect macro definitions within modules. This
                                     feature may cause performance and parsing issues
                                     and is therefore considered experimental.
  --exclude-header-filter=<string> - Regular expression matching the names of the
                                     headers to exclude diagnostics from. Diagnostics
                                     from the main file of each translation unit are
                                     always displayed.
                                     Must be used together with --header-filter.
                                     Can be used together with -line-filter.
                                     This option overrides the 'ExcludeHeaderFilterRegex'
                                     option in .clang-tidy file, if any.
  --experimental-custom-checks     - Enable experimental clang-query based
                                     custom checks.
                                     see https://clang.llvm.org/extra/clang-tidy/QueryBasedCustomChecks.html.
  --explain-config                 - For each enabled check explains, where it is
                                     enabled, i.e. in clang-tidy binary, command
                                     line or a specific configuration file.
  --export-fixes=<filename>        - YAML file to store suggested fixes in. The
                                     stored fixes can be applied to the input source
                                     code with clang-apply-replacements.
  --extra-arg=<string>             - Additional argument to append to the compiler command line
  --extra-arg-before=<string>      - Additional argument to prepend to the compiler command line
  --fix                            - Apply suggested fixes. Without -fix-errors
                                     clang-tidy will bail out if any compilation
                                     errors were found.
  --fix-errors                     - Apply suggested fixes even if compilation
                                     errors were found. If compiler errors have
                                     attached fix-its, clang-tidy will apply them as
                                     well.
  --fix-notes                      - If a warning has no fix, but a single fix can
                                     be found through an associated diagnostic note,
                                     apply the fix.
                                     Specifying this flag will implicitly enable the
                                     '--fix' flag.
  --format-style=<string>          - Style for formatting code around applied fixes:
                                       - 'none' (default) turns off formatting
                                       - 'file' (literally 'file', not a placeholder)
                                         uses .clang-format file in the closest parent
                                         directory
                                       - '{ <json> }' specifies options inline, e.g.
                                         -format-style='{BasedOnStyle: llvm, IndentWidth: 8}'
                                       - 'llvm', 'google', 'webkit', 'mozilla'
                                     See clang-format documentation for the up-to-date
                                     information about formatting styles and options.
                                     This option overrides the 'FormatStyle` option in
                                     .clang-tidy file, if any.
  --header-filter=<string>         - Regular expression matching the names of the
                                     headers to output diagnostics from. The default
                                     value is '.*', i.e. diagnostics from all non-system
                                     headers are displayed by default. Diagnostics
                                     from the main file of each translation unit are
                                     always displayed.
                                     Can be used together with -line-filter.
                                     This option overrides the 'HeaderFilterRegex'
                                     option in .clang-tidy file, if any.
  --line-filter=<string>           - List of files and line ranges to output diagnostics from.
                                     The range is inclusive on both ends. Can be used together
                                     with -header-filter. The format of the list is a JSON
                                     array of objects. For example:
                                     
                                       [
                                         {"name":"file1.cpp","lines":[[1,3],[5,7]]},
                                         {"name":"file2.h"}
                                       ]
                                     
                                     This will output diagnostics from 'file1.cpp' only for
                                     the line ranges [1,3] and [5,7], as well as all from the
                                     entire 'file2.h'.
  --list-checks                    - List all enabled checks and exit. Use with
                                     -checks=* to list all available checks.
  --load=<pluginfilename>          - Load the specified plugin
  -p <string>                      - Build path
  --quiet                          - Run clang-tidy in quiet mode. This suppresses
                                     printing statistics about ignored warnings and
                                     warnings treated as errors if the respective
                                     options are specified.
  --removed-arg=<string>           - List of arguments to remove from the command
                                     line sent to the compiler. Please note that
                                     removing arguments might change the semantic
                                     of the analyzed code, possibly leading to
                                     compiler errors, false positives or
                                     false negatives. This option is applied 
                                     before --extra-arg and --extra-arg-before
  --store-check-profile=<prefix>   - By default reports are printed in tabulated
                                     format to stderr. When this option is passed,
                                     these per-TU profiles are instead stored as JSON.
  --system-headers                 - Display the errors from system headers.
                                     This option overrides the 'SystemHeaders' option
                                     in .clang-tidy file, if any.
  --use-color                      - Use colors in diagnostics. If not set, colors
                                     will be used if the terminal connected to
                                     standard output supports colors.
                                     This option overrides the 'UseColor' option in
                                     .clang-tidy file, if any.
  --verify-config                  - Check the config files to ensure each check and
                                     option is recognized without running any checks.
  --vfsoverlay=<filename>          - Overlay the virtual filesystem described by file
                                     over the real file system.
  --warnings-as-errors=<string>    - Upgrades warnings to errors. Same format as
                                     '-checks'.
                                     This option's value is appended to the value of
                                     the 'WarningsAsErrors' option in .clang-tidy
                                     file, if any. rejected the first version for two consecutive identical switch
branches. It was right: `needs_verification` and the three decided states were
written apart for their comments but did the same thing.

They are now one branch with both reasons in the comment. The distinction is
real — a defect already awaiting the reviewer is already asking, and a decided
defect has had a decision taken — but it lives in the reasoning rather than in
the code, and pretending otherwise was duplication.
