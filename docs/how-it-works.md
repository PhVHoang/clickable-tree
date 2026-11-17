# How ctree works

The executable scans and renders the tree itself. When attached to a terminal,
it switches to the alternate screen, enables xterm-compatible SGR mouse
reporting by default, and draws a small box with a directory's immediate
children. A second click on the same directory within 450 ms selects it.
Keyboard controls provide the same preview and selection actions without a
mouse; `--keyboard-only` skips mouse reporting entirely.

The executable writes the selected absolute path as NUL-terminated data. A
shell function receives it and runs `cd` in the current shell. This design uses
POSIX `termios` and ANSI terminal sequences. It needs no UI framework, terminal
library, or installed `tree` program. The binary is built from one C source
file.

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

[Back to the README](../README.md)
