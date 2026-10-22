# Security policy

## Supported versions

Only the latest commit on `main` is supported.

## Reporting a vulnerability

Please report security issues privately through GitHub:
https://github.com/hellpuffyt/imgpipe/security/advisories/new

Do not open a public issue for a vulnerability. Include the commit, the exact
command line, and (if possible) the input file that triggers the problem.

## Scope

imgpipe reads one local image file and writes one output file; it makes no
network calls. In scope: memory-safety bugs, crashes or unbounded resource
use on crafted image or baseline JSON files, and problems in the vendored
`stb_image` / `stb_image_write` headers as used here (fixes for the latter may
need to go upstream to https://github.com/nothings/stb).
