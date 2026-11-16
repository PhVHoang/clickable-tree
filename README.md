# ctree

`ctree` is a small, dependency-free C program that prints a directory tree and,
in an interactive terminal, lets you inspect and enter directories with a
keyboard or mouse. It builds on macOS and Linux with a C compiler and `make`.

## How it works

The executable scans and renders the tree itself. When attached to a terminal,
it switches to the alternate screen, enables xterm-compatible SGR mouse
reporting by default, and draws a small box with a directory's immediate
children. A second click on the same directory within 450 ms selects it.
Keyboard controls provide the same preview and selection actions without a
mouse; `--keyboard-only` skips mouse reporting entirely.
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

Put `$HOME/.local/bin` on `PATH`, then source the installed shell function in
your shell's startup file. This function is required because a child process
cannot change the working directory of its parent shell.

**Bash** (`~/.bashrc`):

```bash
source "$HOME/.local/share/ctree/ctree.bash"
```

**Zsh** (`~/.zshrc`):

```zsh
source "$HOME/.local/share/ctree/ctree.zsh"
```

**Fish** (`~/.config/fish/config.fish`):

```fish
source "$HOME/.local/share/ctree/ctree.fish"
```

Open a new shell, or source the startup file, then run `ctree`.

## Usage

```sh
ctree                     # current directory, interactive on a terminal
ctree -a -L 2 ~/Projects  # hidden files and a two-level limit
ctree -d --dirsfirst      # directories only
ctree --keyboard-only .   # interactive, with no mouse reporting
ctree --plain .            # static output in terminal scrollback
ctree -L 3 . > tree.txt   # static output when redirected
```

Use `j`/`k` or Up/Down to select a directory. Press Space or Right to see its
direct children in a floating box. Press Enter to change the shell directory.
Esc or Left closes the box; pressing Space or Right again also toggles it off.
Page Up/Down moves five rows, and `q`, Ctrl-C, or Ctrl-Z exits. The box does
not open files.

With a mouse, one click previews a directory and a second click on that same
directory within 450 ms changes the shell directory. The wheel moves five rows.

Supported tree-style options are `-a`, `-d`, `-L N`, `-I PATTERN`, `-P PATTERN`,
`-f`, `-p`, `-s`, `-h`, `-t`, `-r`, `-i`, `-F`, `-n`, `--dirsfirst`, and
`--noreport`. `--help` lists them. Options must precede directory arguments.
Multiple roots are accepted. Patterns use shell glob syntax, so quote them to
prevent your shell from expanding them.

## Terminal and shell limits

- The popup is drawn **inside the terminal**, not as a native OS window.
  Interactive mode requires an alternate screen (`1049`). Mouse use also needs
  SGR events (`1000` and `1006`). `--keyboard-only` avoids those mouse modes;
  use it if a terminal or multiplexer intercepts clicks.
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

`make test` checks static rendering and uses a pseudo-terminal to test mouse
and keyboard previews, directory selection, terminal-mode cleanup, and `cd`
through Bash and Zsh when those shells are installed. The test suite needs
Python 3; the installed program does not.
