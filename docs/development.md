# Development

Build and run the test suite from the repository root:

```sh
make
make test
```

`make test` checks static rendering and uses a pseudo-terminal to test mouse
and keyboard previews, directory selection, terminal-mode cleanup, and `cd`
through Bash and Zsh when those shells are installed. The test suite needs
Python 3; the installed program does not.

The README animation is generated from a small macOS Swift program:

```sh
swift docs/assets/generate-demo.swift
```

[Back to the README](../README.md)
