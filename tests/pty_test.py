"""Exercise mouse selection and terminal cleanup using a real pseudo-terminal."""
import os
import pty
import select
import shlex
import shutil
import signal
import sys
import time

root = sys.argv[1]
read_fd, write_fd = os.pipe()
pid, master = pty.fork()
if pid == 0:
    os.close(read_fd)
    os.dup2(write_fd, 1)
    os.close(write_fd)
    os.environ["TERM"] = "xterm"
    os.execv("./ctree", ["./ctree", "--emit-cd", "-L", "1", root])
os.close(write_fd)

try:
    deadline = time.monotonic() + 5
    screen = b""
    while b"Space/Right preview" not in screen and time.monotonic() < deadline:
        ready, _, _ = select.select([master], [], [], 0.1)
        if ready:
            try:
                screen += os.read(master, 65536)
            except OSError:
                break
    assert b"Space/Right preview" in screen, "interactive screen did not appear"
    # The first child is alpha; its row is 2. One click opens the popup.
    os.write(master, b"\x1b[<0;2;2m")
    time.sleep(0.05)
    ready, _, _ = select.select([master], [], [], 0.5)
    if ready:
        screen += os.read(master, 65536)
    assert b"file.txt" in screen, "popup did not show immediate child"
    os.write(master, b"\x1b[<0;2;2m")
    deadline = time.monotonic() + 5
    while True:
        done, status = os.waitpid(pid, os.WNOHANG)
        if done:
            break
        ready, _, _ = select.select([master], [], [], 0)
        if ready:
            try:
                screen += os.read(master, 65536)
            except OSError:
                pass
        if time.monotonic() > deadline:
            raise AssertionError("double click did not exit; redraws=" + str(screen.count(b"Space/Right preview")) + "; tail=" + repr(screen[-500:]))
        time.sleep(0.05)
    assert os.waitstatus_to_exitcode(status) == 0
    result = os.read(read_fd, 4096)
    assert result == os.fsencode(os.path.join(root, "alpha")) + b"\0", result
    # On exit, the application must disable mouse mode and leave alt screen.
    while True:
        ready, _, _ = select.select([master], [], [], 0.1)
        if not ready:
            break
        try:
            chunk = os.read(master, 65536)
        except OSError:
            break
        if not chunk:
            break
        screen += chunk
    assert b"\x1b[?1000l" in screen
    assert b"\x1b[?1049l" in screen
finally:
    try:
        os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    os.close(read_fd)
    os.close(master)


def read_until(pty_master, marker, timeout=3):
    output = b""
    deadline = time.monotonic() + timeout
    while marker not in output and time.monotonic() < deadline:
        ready, _, _ = select.select([pty_master], [], [], 0.1)
        if ready:
            try:
                output += os.read(pty_master, 65536)
            except OSError:
                break
    assert marker in output, f"terminal output missing {marker!r}: {output[-300:]!r}"
    return output


def check_shell(shell_name, init_file, keyboard=False):
    shell = shutil.which(shell_name)
    if not shell:
        return
    mode = "--keyboard-only " if keyboard else ""
    command = f"source {shlex.quote(init_file)}; ctree {mode}-L 1 {shlex.quote(root)}; pwd"
    child, pty_master = pty.fork()
    if child == 0:
        os.environ["TERM"] = "xterm"
        os.environ["PATH"] = os.getcwd() + os.pathsep + os.environ.get("PATH", "")
        os.execv(shell, [shell, "-c", command])
    try:
        output = read_until(pty_master, b"Space/Right preview")
        if keyboard:
            assert b"\x1b[?1000h" not in output, "keyboard-only mode enabled mouse tracking"
            os.write(pty_master, b"\x1b[B")  # Down selects alpha.
            output += read_until(pty_master, b"Space/Right preview")
            os.write(pty_master, b" ")  # Space previews alpha.
            preview = read_until(pty_master, b"Space/Right preview")
            assert b"file.txt" in preview, "Space did not open the popup"
            os.write(pty_master, b"\x1b[D")  # Left closes the preview.
            closed = read_until(pty_master, b"Space/Right preview")
            assert b"file.txt" not in closed, "Left did not close the popup"
            os.write(pty_master, b"\x1b[C")  # Right reopens it.
            preview = read_until(pty_master, b"Space/Right preview")
            assert b"file.txt" in preview, "Right did not open the popup"
            os.write(pty_master, b"\r")
        else:
            os.write(pty_master, b"\x1b[<0;2;2m\x1b[<0;2;2m")
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            done, status = os.waitpid(child, os.WNOHANG)
            ready, _, _ = select.select([pty_master], [], [], 0.05)
            if ready:
                try:
                    output += os.read(pty_master, 65536)
                except OSError:
                    pass
            if done:
                assert os.waitstatus_to_exitcode(status) == 0
                expected = os.fsencode(os.path.join(root, "alpha")) + b"\r\n"
                assert expected in output, f"{shell_name} did not cd: {output[-300:]!r}"
                return
        raise AssertionError(f"{shell_name} wrapper timed out")
    finally:
        try:
            os.kill(child, signal.SIGKILL)
        except ProcessLookupError:
            pass
        os.close(pty_master)


check_shell("bash", "shell/ctree.bash")
check_shell("zsh", "shell/ctree.zsh")
check_shell("bash", "shell/ctree.bash", keyboard=True)
check_shell("zsh", "shell/ctree.zsh", keyboard=True)
