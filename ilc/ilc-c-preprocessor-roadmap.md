# ILC / C-Preprocessor Roadmap

ILC is centered on persistent interpreter state for a user or session. Its basic command model stays simple:

```text
inputs -> command -> outputs
```

Outputs can be typed and persistent, then supplied as inputs to later commands. This makes a sequence of small, independently useful operations into a reusable workflow.

It is useful to keep three lifetimes conceptually distinct:

- **Command temporaries:** exist only while a command runs.
- **Session state:** remains available across commands in an interpreter session.
- **Persistent user state:** remains available for later work beyond one session.

Storage and format/type are separate concerns. Storage determines where and for how long a value is retained; its format or type determines what it means and which operations can consume it.

## Staged Workflow

A path toward a C preprocessor and C/C++ reader can be assembled from small commands:

```text
directory listing
-> filter by extension
-> open/read files
-> text operations
-> tokenize
-> preprocess
-> C/C++ reader
-> metadata
```

Each stage produces something that a later stage can consume, while remaining useful on its own.

## Runtime Types and Metadata

Eventually, struct metadata can be used to generate virtual runtime types. The metadata can come from two complementary paths:

- Source processed through a C preprocessor and C/C++ reader.
- Compiler or debug-symbol extraction when the physical layout is needed.

## Priority

The first priority is the small foundational commands: directory and file operations, filtering, text handling, and tokenization. They provide immediate independent value and later become the infrastructure for preprocessing, source reading, and metadata extraction.
