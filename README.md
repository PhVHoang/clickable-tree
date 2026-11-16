# ctree

`ctree` is a small, dependency-free C program that prints a directory tree and,
in an interactive terminal, lets you inspect and enter directories with the
mouse. It builds on macOS and Linux with a C compiler and `make`.

## How it works

The executable scans and renders the tree itself. When attached to a terminal,
it switches to the alternate screen, enables xterm-compatible SGR mouse
reporting, and draws a small box with the clicked directory's immediate
children. A second click on the same directory within 450 ms selects it.
The executable writes the selected absolute path as NUL-terminated data;
a shell function receives it and runs `cd` in the current shell.

This design uses POSIX `termios` and ANSI terminal sequences. It needs no UI
framework, terminal library, or installed `tree` program. The binary is built
from one C source file.

## Install

```sh
make
make test
make install PREFIX="$HOME/.local"
```

Put `$HOME/.local/bin` on `PATH`, then source one shell function in your shell's
startup file. Use the absolute path to this checkout, or copy the appropriate
file to a stable location.

**Bash** (`~/.bashrc`):

```bash
source /absolute/path/to/clickable-tree/shell/ctree.bash
```

**Zsh** (`~/.zshrc`):

```zsh
source /absolute/path/to/clickable-tree/shell/ctree.zsh
```

**Fish** (`~/.config/fish/config.fish`):

```fish
source /absolute/path/to/clickable-tree/shell/ctree.fish
```

Open a new shell, or source the startup file, then run `ctree`.

## Usage

```sh
ctree                     # current directory, interactive on a terminal
ctree -a -L 2 ~/Projects  # hidden files and a two-level limit
ctree -d --dirsfirst      # directories only
ctree --plain .            # static output in terminal scrollback
ctree -L 3 . > tree.txt   # static output when redirected
```

Click a directory once to see its direct children in a floating box. Click
it again within 450 ms to change the shell directory. You can also move with
`j`/`k` or the arrow keys and press Enter. The wheel and Page Up/Down move five
rows. `Esc` closes the box; `q`, Ctrl-C, or Ctrl-Z exits. The box does not open
files.

Supported tree-style options are `-a`, `-d`, `-L N`, `-I PATTERN`, `-P PATTERN`,
`-f`, `-p`, `-s`, `-h`, `-t`, `-r`, `-i`, `-F`, `-n`, `--dirsfirst`, and
`--noreport`. `--help` lists them. Options must precede directory arguments.
Multiple roots are accepted. Patterns use shell glob syntax, so quote them to
prevent your shell from expanding them.

## Terminal and shell limits

- The popup is drawn **inside the terminal**, not as a native OS window.
  It requires an xterm-compatible terminal that sends SGR mouse events
  (`1000` and `1006`) and supports the alternate screen (`1049`). If clicks
  are intercepted by the terminal or a multiplexer, use the keyboard controls.
- An executable cannot change its parent shell's working directory. Source
  the provided shell function for double-click `cd`. Running the executable
  directly prints the selected path instead.
- Piped or redirected output is a static tree; terminal mouse mode is never
  enabled there. `--plain` forces static output on a terminal.
- This is a focused subset of `tree`, not a byte-for-byte clone. It has no
  color, HTML/JSON export, symlink-directory traversal, or full `tree` pattern
  syntax. `-P` filters files while retaining directory branches for navigation;
  `-I` accepts one glob. Unsupported options fail with a clear error.
- The popup shows at most nine child rows and marks overflow with `...`.
  The full tree remains navigable with scrolling. Filenames containing control
  characters are displayed with `?` substitutions.

The [xterm control-sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.pdf)
defines SGR mouse events and alternate-screen mode. The
[Bash manual](https://www.gnu.org/software/bash/manual/bash.html) explains why
shell functions run in the current shell context and can perform `cd` there.

## Development

`make test` checks static rendering and uses a pseudo-terminal to test the
single-click popup, double-click path selection, terminal-mode cleanup, and
`cd` through Bash and Zsh when those shells are installed. The test suite
needs Python 3; the installed program does not.
