# Persistent Blitter range benchmark

Build `benchmark_blitter` through `llt.slnx` in Release x64. This project copies the existing `blit` project settings and links the same `llc.lib` and `blitter.lib`.

From a directory containing a Blitter configuration and its database, run:

`benchmark_blitter.exe --config=blitter.json --expected=response.json --db=payments --offset=12796 --limit=8 --requests=10000`

For the read-only reverse-index path, set a `search_format` on the database and run:

`benchmark_blitter.exe --config=blitter.json --expected=search_results.json --db=payments --search=mrc_96 --requests=10000`

The search result is a JSON array of absolute record indices. The first request includes index construction; subsequent requests use the retained index. The expected file is checked byte for byte before and after timing.

The program loads configuration once, measures the first `blt::queryProcess()` call, checks its entire response against the expected UTF-8 file, then measures repeated calls on the retained `SBlitter`. Every repeated response is fully read by LLC's CRC function; the aggregate checksum is printed. The run excludes process startup, HTTP parsing, network transport, and response delivery. `blit.exe` creates a new `SBlitter` per CLI/CGI invocation, so this is a persistent-host scenario rather than its current executable lifecycle.
