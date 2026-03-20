#!/usr/bin/env python3
"""
webserv local integration tester
Usage:  python3 tester.py <task>
Example: python3 tester.py 4.1  →  runs tasks 1.1 → 2.1 → 2.2 → 2.3 → 3.1 → 3.2 → 3.3 → 4.1

All temporary files are created under ./tester/ and cleaned up from there only.
"""

import argparse
import os
import re
import signal
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# ─────────────────────────────── CONSTANTS ──────────────────────────────────

ALL_TASKS  = ["1.1", "1.2", "1.3", "2.1", "2.2", "2.3", "3.1", "3.2", "3.3", "4.1", "4.2", "4.3"]
BINARY     = "./webserv"
HOST       = "127.0.0.1"
PORT       = 8080
TESTER_DIR = "tester"          # ← all fixtures live here; cleanup only touches this

def tp(*parts: str) -> str:
    """Return a path rooted inside TESTER_DIR."""
    return os.path.join(TESTER_DIR, *parts)

# ─────────────────────────────── COLOURS ────────────────────────────────────

GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"
RESET  = "\033[0m"
DIM    = "\033[2m"
BLUE   = "\033[94m"
MAGENTA = "\033[95m"

def green(s):   return f"{GREEN}{s}{RESET}"
def red(s):     return f"{RED}{s}{RESET}"
def yellow(s):  return f"{YELLOW}{s}{RESET}"
def cyan(s):    return f"{CYAN}{s}{RESET}"
def bold(s):    return f"{BOLD}{s}{RESET}"
def dim(s):     return f"{DIM}{s}{RESET}"
def blue(s):    return f"{BLUE}{s}{RESET}"
def magenta(s): return f"{MAGENTA}{s}{RESET}"

# ─────────────────────────────── RESULTS ────────────────────────────────────

_results: list[dict] = []
_current_task = ""

def record(name: str, passed: bool, detail: str = "", trace: str = ""):
    """
    trace — multi-line string printed in the FAILURE DETAILS section.
    Provide it regardless of `passed`; it is only printed on failure.
    """
    _results.append({
        "task":   _current_task,
        "name":   name,
        "passed": passed,
        "detail": detail,
        "trace":  trace,
    })
    icon = green("PASS") if passed else red("FAIL")
    msg  = f"  [{icon}] {name}"
    if detail:
        msg += f"  {yellow('→')} {detail}"
    print(msg)

def skip(name: str, reason: str = ""):
    _results.append({"task": _current_task, "name": name, "passed": None, "trace": ""})
    print(f"  [{yellow('SKIP')}] {name}" + (f"  {dim(f'({reason})')}" if reason else ""))

# ─────────────────────────────── TRACE HELPERS ──────────────────────────────

_BOX_W = 56   # inner width of the trace box

def _trim(text: str, max_lines: int = 40) -> str:
    lines = text.splitlines()
    if len(lines) <= max_lines:
        return text
    return "\n".join(lines[:max_lines]) + f"\n{dim(f'… ({len(lines) - max_lines} more lines)')}"

def _box_top(label: str = "") -> str:
    if label:
        pad  = _BOX_W - len(label) - 2
        return f"  {dim('┌─')} {cyan(label)} {dim('─' * max(pad, 0) + '┐')}"
    return f"  {dim('┌' + '─' * (_BOX_W + 2) + '┐')}"

def _box_sep(label: str = "") -> str:
    if label:
        pad = _BOX_W - len(label) - 2
        return f"  {dim('├─')} {blue(label)} {dim('─' * max(pad, 0) + '┤')}"
    return f"  {dim('├' + '─' * (_BOX_W + 2) + '┤')}"

def _box_bot() -> str:
    return f"  {dim('└' + '─' * (_BOX_W + 2) + '┘')}"

def _box_row(content: str, color_fn=None) -> str:
    text = color_fn(content) if color_fn else content
    return f"  {dim('│')} {text}"

def make_trace(command, expected: str, got: str, output: str = "") -> str:
    """Return a structured, coloured trace block for a test result."""
    cmd_str  = command if isinstance(command, str) else " ".join(str(c) for c in command)
    mismatch = expected.strip() != got.strip()

    lines = [
        _box_top("TRACE"),
        _box_row(f"{dim('command')}  {cmd_str}"),
        _box_sep("result"),
        _box_row(f"{dim('expected')} {green(expected)}"),
        _box_row(f"{dim('got')}      {red(got) if mismatch else yellow(got)}"),
    ]

    if output.strip():
        lines.append(_box_sep("output"))
        for ln in _trim(output.strip()).splitlines():
            lines.append(_box_row(dim(ln)))

    lines.append(_box_bot())
    return "\n".join(lines)

def tcp_trace(host: str, port: int, sent: bytes, received: bytes,
              expected: str, got: str) -> str:
    sent_repr = sent.decode(errors="replace").replace("\r\n", "↵\n")
    recv_repr = received.decode(errors="replace")
    mismatch  = expected.strip() != got.strip()

    lines = [
        _box_top("TCP TRACE"),
        _box_row(f"{dim('endpoint')} {host}:{port}"),
        _box_sep("result"),
        _box_row(f"{dim('expected')} {green(expected)}"),
        _box_row(f"{dim('got')}      {red(got) if mismatch else yellow(got)}"),
        _box_sep("sent bytes"),
    ]
    for ln in sent_repr.splitlines():
        lines.append(_box_row(dim(ln)))

    lines.append(_box_sep("received bytes"))
    for ln in _trim(recv_repr).splitlines():
        lines.append(_box_row(dim(ln)))

    lines.append(_box_bot())
    return "\n".join(lines)

# ─────────────────────────────── SUMMARY ────────────────────────────────────

def print_summary():
    print()
    print(bold("━━━━━━━━━━━━━━  SUMMARY  ━━━━━━━━━━━━━━"))
    by_task: dict[str, list] = {}
    for r in _results:
        by_task.setdefault(r["task"], []).append(r)

    total_pass = total_fail = 0
    for task, results in by_task.items():
        p = sum(1 for r in results if r["passed"] is True)
        f = sum(1 for r in results if r["passed"] is False)
        s = sum(1 for r in results if r["passed"] is None)
        total_pass += p; total_fail += f
        bar = green(f"{p} passed")
        if f: bar += f"  {red(f'{f} failed')}"
        if s: bar += f"  {yellow(f'{s} skipped')}"
        print(f"  Task {task:<4}  {bar}")

    print(bold("────────────────────────────────────────"))
    overall = green("ALL PASSED") if total_fail == 0 else red(f"{total_fail} FAILED")
    print(f"  Total:  {green(str(total_pass))} passed  |  {red(str(total_fail))} failed  →  {overall}")
    print()

    # ── Failure traces ────────────────────────────────────────────────────────
    failures = [r for r in _results if r["passed"] is False]
    if not failures:
        return

    print(bold("━━━━━━━━━━━━  FAILURE DETAILS  ━━━━━━━━━━━━"))
    for r in failures:
        task_label = dim(f"[Task {r['task']}]")
        print(f"\n  {red('✗')} {task_label}  {bold(r['name'])}")
        if r.get("trace"):
            print(r["trace"])
        elif r.get("detail"):
            print(_box_row(f"{dim('detail')} {r['detail']}"))
    print()

# ─────────────────────────────── SERVER ─────────────────────────────────────

_server_proc: subprocess.Popen | None = None

def start_server(config: str, ports: list[int] | None = None, host: str = HOST) -> bool:
    global _server_proc
    stop_server()
    if ports is None:
        ports = [PORT]
    _server_proc = subprocess.Popen(
        [BINARY, config],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    for p in ports:
        if not wait_for_port(host, p, timeout=8):
            _server_proc.kill()
            _server_proc = None
            return False
    return True

def stop_server():
    global _server_proc
    if _server_proc is not None:
        try:
            _server_proc.terminate()
            _server_proc.wait(timeout=3)
        except Exception:
            try: _server_proc.kill()
            except Exception: pass
        _server_proc = None
    time.sleep(0.3)

def server_alive() -> bool:
    return _server_proc is not None and _server_proc.poll() is None

# ─────────────────────────────── NET/CURL HELPERS ───────────────────────────

def wait_for_port(host: str, port: int, timeout: float = 8) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except OSError:
            time.sleep(0.15)
    return False

def _build_curl(method: str, extra, url: str, timeout: int, flags: list) -> list:
    cmd = ["curl"] + flags + ["--max-time", str(timeout), "-X", method]
    if extra:
        cmd += extra
    cmd.append(url)
    return cmd

def curl_code(url: str, method: str = "GET", extra=None, timeout: int = 8) -> str:
    cmd = _build_curl(method, extra, url, timeout,
                      ["-s", "-o", "/dev/null", "-w", "%{http_code}"])
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout.strip()
    except Exception:
        return "000"

def curl_code_v(url: str, method: str = "GET", extra=None,
                timeout: int = 8) -> tuple[str, str, list]:
    """Returns (http_code, verbose_output, command_list)."""
    cmd = _build_curl(method, extra, url, timeout,
                      ["-sv", "-o", "/dev/null", "-w", "%{http_code}"])
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout.strip(), r.stderr, cmd
    except Exception as e:
        return "000", str(e), cmd

def curl_headers_v(url: str, method: str = "GET", extra=None,
                   timeout: int = 8) -> tuple[str, str, list]:
    """Returns (headers_str, verbose_stderr, command_list)."""
    cmd = _build_curl(method, extra, url, timeout,
                      ["-sv", "-D", "-", "-o", "/dev/null"])
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout, r.stderr, cmd
    except Exception as e:
        return "", str(e), cmd

def curl_body_v(url: str, extra=None,
                timeout: int = 8) -> tuple[str, str, list]:
    """Returns (body, verbose_stderr, command_list)."""
    cmd = ["curl", "-sv", "--max-time", str(timeout)]
    if extra:
        cmd += extra
    cmd.append(url)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout, r.stderr, cmd
    except Exception as e:
        return "", str(e), cmd

def curl_verbose(url: str, method: str = "GET", extra=None, timeout: int = 8) -> str:
    cmd = _build_curl(method, extra, url, timeout, ["-sv"])
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stderr + r.stdout
    except Exception:
        return ""

def tcp_send(host: str, port: int, data: bytes, timeout: float = 4) -> bytes:
    try:
        with socket.create_connection((host, port), timeout=timeout) as s:
            s.sendall(data)
            s.shutdown(socket.SHUT_WR)
            chunks = []
            s.settimeout(timeout)
            while True:
                try:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    chunks.append(chunk)
                except socket.timeout:
                    break
            return b"".join(chunks)
    except Exception:
        return b""

def sh(cmd: str, timeout: int = 15) -> tuple[int, str, str]:
    try:
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return 1, "", "timeout"

# ─────────────────────────────── FIXTURES ───────────────────────────────────

def mkdirp(path: str):
    os.makedirs(path, exist_ok=True)

def write_file(path: str, content):
    mkdirp(os.path.dirname(path) or ".")
    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as f:
        f.write(content)

def setup_defaults():
    """Create the default config and www layout under tester/."""
    mkdirp(tp("config"))
    mkdirp(tp("www/html"))
    mkdirp(tp("www/upload"))
    write_file(tp("www/html/index.html"), "<html><body>OK</body></html>\n")
    write_file(tp("config/default.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root {tp("www/html")};
    client_max_body_size 1M;
    location / {{
        root {tp("www/html")};
        index index.html;
        methods GET HEAD;
    }}
    location /upload {{
        root {tp("www/html")};
        methods GET HEAD POST;
        upload_dir {tp("www/upload")};
    }}
    location /echo {{
        root {tp("www/html")};
        methods GET HEAD POST;
    }}
}}
""")

def cleanup():
    stop_server()
    # Restore permissions on anything we may have chmod'd
    forbidden = tp("uploads/forbidden")
    if os.path.isdir(forbidden):
        try: os.chmod(forbidden, 0o755)
        except Exception: pass

    if os.path.isdir(TESTER_DIR):
        try:
            for root, dirs, _ in os.walk(TESTER_DIR):
                for sd in dirs:
                    try: os.chmod(os.path.join(root, sd), 0o755)
                    except Exception: pass
            shutil.rmtree(TESTER_DIR, ignore_errors=True)
        except Exception:
            pass

# ─────────────────────────────── TASK 1.1 ───────────────────────────────────

def task_1_1():
    global _current_task
    _current_task = "1.1"
    print(f"\n{cyan(bold('── Task 1.1  Scaffolding & epoll Skeleton ──'))}")

    rc, out, err = sh("make 2>&1", timeout=60)
    record("make compiles", rc == 0,
           trace=make_trace("make", "exit 0", f"exit {rc}", out + err))

    record("binary exists", os.path.isfile(BINARY),
           trace=make_trace(f"test -f {BINARY}", "file present", "file not found"))

    sh("make clean 2>&1", timeout=30)
    o_files = sh("find . -name '*.o' 2>/dev/null")[1].strip()
    record("make clean removes .o files", o_files == "",
           trace=make_trace("make clean && find . -name '*.o'",
                            "no .o files", f"found:\n{o_files}"))

    sh("make fclean 2>&1", timeout=30)
    record("make fclean removes binary", not os.path.isfile(BINARY),
           trace=make_trace("make fclean && test -f ./webserv",
                            "binary absent", "binary still present"))

    rc, out, err = sh("make re 2>&1", timeout=60)
    record("make re builds binary", rc == 0 and os.path.isfile(BINARY),
           trace=make_trace("make re", "exit 0 + binary present",
                            f"exit {rc}, binary={'present' if os.path.isfile(BINARY) else 'absent'}",
                            out + err))

    _, out, _ = sh("make 2>&1", timeout=30)
    up_to_date = "Nothing to be done" in out or "is up to date" in out
    record("make re avoids relinking (nothing to be done)", up_to_date,
           trace=make_trace("make  (second call)",
                            "'Nothing to be done' or 'is up to date'",
                            out.strip()[:300] or "(empty)"))

    src_dirs = [d for d in ["src", "includes", "include", "srcs"] if os.path.isdir(d)]
    if src_dirs:
        dirs = " ".join(src_dirs)
        _, out, _ = sh(f"grep -rn '\\bauto\\b' {dirs} 2>/dev/null")
        record("no 'auto' keyword (C++98)", out.strip() == "",
               trace=make_trace(f"grep -rn '\\bauto\\b' {dirs}",
                                "no matches", out.strip()[:400] or "(no match)"))

        _, out, _ = sh(f"grep -rn '\\bnullptr\\b' {dirs} 2>/dev/null")
        record("no 'nullptr' keyword (C++98)", out.strip() == "",
               trace=make_trace(f"grep -rn '\\bnullptr\\b' {dirs}",
                                "no matches", out.strip()[:400] or "(no match)"))

        rc, out, err = sh(
            "make fclean && make CXXFLAGS='-Wall -Wextra -Werror -std=c++98' 2>&1",
            timeout=60)
        record("compiles with -std=c++98", rc == 0,
               trace=make_trace("make CXXFLAGS='-std=c++98'",
                                "exit 0", f"exit {rc}", out + err))
    else:
        skip("no 'auto' keyword",   "no src/ dir found")
        skip("no 'nullptr' keyword", "no src/ dir found")
        skip("-std=c++98 compile",   "no src/ dir found")

    if not os.path.isfile(BINARY):
        sh("make 2>&1", timeout=60)

    ok = start_server(tp("config/default.conf"))
    record("server starts on port 8080", ok,
           trace=make_trace(f"{BINARY} {tp('config/default.conf')}  (wait for port 8080)",
                            "port 8080 open", "port 8080 unreachable"))

    if ok:
        code, verbose, cmd = curl_code_v(f"http://{HOST}:{PORT}/")
        record("GET / returns a response (not 000)", code != "000", f"HTTP {code}",
               trace=make_trace(cmd, "HTTP 1xx-5xx", f"HTTP {code}", verbose))

        record("server alive after connection", server_alive(),
               trace=make_trace("kill -0 <pid>", "process alive", "process dead"))

        strace_path = shutil.which("strace")
        if strace_path and os.path.isdir("/proc"):
            pid = _server_proc.pid
            rc, out, err = sh(
                f"timeout 2 strace -p {pid} -e trace=epoll_wait 2>&1", timeout=5)
            combined = out + err
            count = combined.count("epoll_wait")
            record("epoll_wait is being used (strace)", count >= 1,
                   f"{count} calls observed",
                   trace=make_trace(f"strace -p {pid} -e trace=epoll_wait",
                                    "at least 1 epoll_wait call",
                                    f"{count} calls", combined[:600]))
        else:
            skip("epoll_wait via strace", "strace not available")

    stop_server()

# ─────────────────────────────── TASK 1.2 ───────────────────────────────────

def task_1_2():
    global _current_task
    _current_task = "1.2"
    print(f"\n{cyan(bold('── Task 1.2  HTTP Request Parser ──'))}")

    ok = start_server(tp("config/default.conf"))
    record("server starts", ok,
           trace=make_trace(f"{BINARY} {tp('config/default.conf')}",
                            "port 8080 open", "port 8080 unreachable"))
    if not ok:
        return

    url = f"http://{HOST}:{PORT}/"

    code, verbose, cmd = curl_code_v(url)
    record("GET / returns a response (not 000)", code != "000", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 1xx-5xx", f"HTTP {code}", verbose))

    curl_code(f"http://{HOST}:{PORT}/path?foo=bar&baz=qux")
    record("GET with query string — server still alive", server_alive(),
           trace=make_trace(f"curl http://{HOST}:{PORT}/path?foo=bar",
                            "server alive", "server crashed"))

    code, verbose, cmd = curl_code_v(url, method="HEAD", extra=["-I"])
    record("HEAD method returns a response", code != "000", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 1xx-5xx", f"HTTP {code}", verbose))

    # garbage → 400
    sent = b"THISISNOTHTTP\r\n\r\n"
    raw  = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("garbage request → 400", b"400" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 400", first))

    # HTTP/9.9 → 505
    sent = b"GET / HTTP/9.9\r\nHost: localhost\r\n\r\n"
    raw  = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("HTTP/9.9 → 505", b"505" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 505", first))

    for _ in range(5):
        tcp_send(HOST, PORT, b"GARBAGE\r\n\r\n", timeout=1)
    record("server alive after 5 garbage requests", server_alive(),
           trace=make_trace("5x TCP GARBAGE\\r\\n\\r\\n",
                            "server alive", "server crashed"))

    code, verbose, cmd = curl_code_v(url)
    record("good request still works after bad ones", code != "000", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 1xx-5xx", f"HTTP {code}", verbose))

    big_header = "X-Big: " + "A" * 9000
    code, verbose, cmd = curl_code_v(url, extra=["-H", big_header])
    record("large header → 431 or 400", code in ("431", "400"), f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 431 or 400", f"HTTP {code}", verbose))
    record("server alive after large header", server_alive(),
           trace=make_trace("kill -0 <pid>", "server alive", "server crashed"))

    # partial/slow request
    partial_sent = b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as s:
            s.sendall(b"GET / HTTP/1.1\r\n")
            time.sleep(0.5)
            s.sendall(b"Host: localhost\r\n\r\n")
            s.settimeout(5); resp = b""
            try:
                while True:
                    chunk = s.recv(4096)
                    if not chunk: break
                    resp += chunk
            except socket.timeout:
                pass
        first = resp.split(b"\n")[0].decode(errors="replace").strip()
        record("partial/slow request handled", b"HTTP/1." in resp, first,
               trace=tcp_trace(HOST, PORT, partial_sent, resp,
                               "HTTP/1.x response", first))
    except Exception as e:
        record("partial/slow request handled", False, str(e),
               trace=make_trace("TCP partial send (split into two sends)",
                                "HTTP/1.x response", f"exception: {e}"))

    stop_server()

# ─────────────────────────────── TASK 1.3 ───────────────────────────────────

def task_1_3():
    global _current_task
    _current_task = "1.3"
    print(f"\n{cyan(bold('── Task 1.3  Configuration File Parser ──'))}")

    ok = start_server(tp("config/default.conf"))
    record("server starts with valid config", ok,
           trace=make_trace(f"{BINARY} {tp('config/default.conf')}",
                            "port 8080 open", "port 8080 unreachable"))
    stop_server()

    mkdirp(tp("config"))
    write_file(tp("config/missing_port.conf"),
               "server {\n    server_name example.com;\n    location / { root /var/www/html; }\n}\n")
    rc, out, err = sh(f"timeout 3 {BINARY} {tp('config/missing_port.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv {tp('config/missing_port')}' 2>/dev/null")
    record("missing port → non-zero exit", rc != 0,
           trace=make_trace(f"{BINARY} {tp('config/missing_port.conf')}",
                            "exit != 0", f"exit {rc}", out + err))

    write_file(tp("config/bad_syntax.conf"),
               "server {\n    listen 8080;\n    unknown_directive_xyz value;\n}\n")
    rc, out, err = sh(f"timeout 3 {BINARY} {tp('config/bad_syntax.conf')} 2>&1 || true", timeout=5)
    sh(f"pkill -f 'webserv {tp('config/bad_syntax')}' 2>/dev/null || true")
    combined = (out + err).lower()
    has_diag = any(w in combined for w in ("error","unknown","invalid","line","directive"))
    record("bad syntax → diagnostic output", has_diag,
           trace=make_trace(f"{BINARY} {tp('config/bad_syntax.conf')}",
                            "output contains: error/unknown/invalid/line/directive",
                            combined[:300] or "(empty output)"))

    mkdirp(tp("www/site1")); mkdirp(tp("www/site2"))
    write_file(tp("www/site1/index.html"), "<h1>Site 1</h1>\n")
    write_file(tp("www/site2/index.html"), "<h1>Site 2</h1>\n")
    write_file(tp("config/multi_server.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root {tp("www/site1")};
    location / {{ root {tp("www/site1")}; index index.html; methods GET; }}
}}
server {{
    listen 0.0.0.0:9090;
    root {tp("www/site2")};
    location / {{ root {tp("www/site2")}; index index.html; methods GET; }}
}}
""")
    ok = start_server(tp("config/multi_server.conf"), ports=[8080, 9090])
    record("multi-server starts (8080 + 9090)", ok,
           trace=make_trace(f"{BINARY} {tp('config/multi_server.conf')}",
                            "ports 8080 and 9090 open",
                            "one or both ports unreachable"))
    if ok:
        c1, v1, cmd1 = curl_code_v(f"http://{HOST}:8080/")
        record("port 8080 responds", c1 != "000", f"HTTP {c1}",
               trace=make_trace(cmd1, "HTTP 1xx-5xx", f"HTTP {c1}", v1))
        c2, v2, cmd2 = curl_code_v(f"http://{HOST}:9090/")
        record("port 9090 responds", c2 != "000", f"HTTP {c2}",
               trace=make_trace(cmd2, "HTTP 1xx-5xx", f"HTTP {c2}", v2))
    stop_server()

    rc, out, err = sh(f"timeout 3 {BINARY} 2>&1 || true", timeout=5)
    sh(f"pkill -f '{BINARY}' 2>/dev/null || true")
    record("no-arg invocation does not segfault", rc != 139, f"exit {rc}",
           trace=make_trace(f"{BINARY}  (no arguments)",
                            "exit != 139 (no SIGSEGV)", f"exit {rc}", out + err))

    write_file(tp("config/bodysize.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root {tp("www/html")};
    client_max_body_size 1M;
    location /upload {{ methods POST; upload_dir {tp("www/upload")}; }}
    location / {{ root {tp("www/html")}; methods GET; }}
}}
""")
    ok = start_server(tp("config/bodysize.conf"))
    record("server starts with body-size config", ok,
           trace=make_trace(f"{BINARY} {tp('config/bodysize.conf')}",
                            "port 8080 open", "port 8080 unreachable"))
    if ok:
        small_dat = os.urandom(100 * 1024)
        tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".dat")
        tmp.write(small_dat); tmp.close()
        cmd_list = ["curl", "-sv", "-o", "/dev/null", "-w", "%{http_code}",
                    "--max-time", "8", "-X", "POST", "-H", "Expect:",
                    "--data-binary", f"@{tmp.name}",
                    f"http://{HOST}:{PORT}/upload"]
        try:
            r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=12)
            code = r.stdout.strip(); verbose = r.stderr
        except Exception as e:
            code, verbose = "000", str(e)
        os.unlink(tmp.name)
        record("100 KB body (within 1 MB limit) → server responds", code != "000",
               f"HTTP {code}",
               trace=make_trace(cmd_list, "HTTP 1xx-5xx", f"HTTP {code}", verbose))
    stop_server()

# ─────────────────────────────── TASK 2.1 ───────────────────────────────────

def task_2_1():
    global _current_task
    _current_task = "2.1"
    print(f"\n{cyan(bold('── Task 2.1  Full Client Lifecycle & Non-blocking I/O ──'))}")

    large_path    = tp("www/html/large.bin")
    mkdirp(tp("www/html"))
    with open(large_path, "wb") as f:
        f.write(os.urandom(512 * 1024))
    expected_size = os.path.getsize(large_path)

    ok = start_server(tp("config/default.conf"))
    record("server starts", ok,
           trace=make_trace(f"{BINARY} {tp('config/default.conf')}",
                            "port 8080 open", "port 8080 unreachable"))
    if not ok:
        return

    # Test 1 — 100 concurrent connections
    def one_request(_):
        try:
            r = subprocess.run(
                ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}",
                 "--max-time", "10", "--connect-timeout", "5",
                 f"http://{HOST}:{PORT}/"],
                capture_output=True, text=True, timeout=12)
            code = r.stdout.strip()
            return 100 <= int(code) < 600
        except Exception:
            return False

    with ThreadPoolExecutor(max_workers=100) as pool:
        futures = [pool.submit(one_request, i) for i in range(100)]
        results = [f.result() for f in as_completed(futures)]
    success = sum(results)
    record("100 concurrent connections all responded", success == 100,
           f"{success}/100 succeeded",
           trace=make_trace(
               "100x  curl -s -o /dev/null -w %{http_code} http://localhost:8080/  (parallel)",
               "100/100 responses with HTTP 1xx-5xx",
               f"{success}/100 valid responses"))

    # Test 2 — idle timeout fd leak
    if os.path.isdir("/proc") and _server_proc:
        pid = _server_proc.pid
        try:    baseline = len(os.listdir(f"/proc/{pid}/fd"))
        except: baseline = 0

        idle_sock = socket.socket()
        idle_sock.connect((HOST, PORT))
        time.sleep(1)

        done_event = threading.Event()
        def _wait(): time.sleep(10); done_event.set()
        threading.Thread(target=_wait, daemon=True).start()
        done_event.wait(timeout=12)
        try: idle_sock.close()
        except: pass
        time.sleep(0.5)

        try:    after = len(os.listdir(f"/proc/{pid}/fd"))
        except: after = baseline
        diff = after - baseline
        record("idle connection timed out without fd leak", diff <= 1,
               f"baseline={baseline} after={after} diff={diff}",
               trace=make_trace(
                   f"open idle TCP socket → wait 10 s → check /proc/{pid}/fd",
                   "fd diff ≤ 1 (no leak)",
                   f"diff={diff}  (baseline={baseline}, after={after})"))
    else:
        skip("idle timeout fd-leak check", "/proc not available")

    # Test 3 — 500 rapid open/close fd stability
    if os.path.isdir("/proc") and _server_proc:
        pid = _server_proc.pid
        try:    baseline = len(os.listdir(f"/proc/{pid}/fd"))
        except: baseline = 0

        def quick_connect(_):
            try:
                s = socket.create_connection((HOST, PORT), timeout=1); s.close()
            except: pass

        with ThreadPoolExecutor(max_workers=50) as pool:
            list(pool.map(quick_connect, range(500)))
        time.sleep(2)

        try:    after = len(os.listdir(f"/proc/{pid}/fd"))
        except: after = baseline
        diff = after - baseline
        record("500 rapid open/close — fd count stable", diff <= 5,
               f"baseline={baseline} after={after} diff={diff}",
               trace=make_trace(
                   f"500x  socket.connect({HOST}:{PORT}) + close  (parallel)",
                   "fd diff ≤ 5 (no leak)",
                   f"diff={diff}  (baseline={baseline}, after={after})"))
    else:
        skip("500 rapid connections fd stability", "/proc not available")

    # Test 4 — kill curl mid-request
    curl_proc = subprocess.Popen(
        ["curl", "-s", "--limit-rate", "50k", "--max-time", "30",
         f"http://{HOST}:{PORT}/large.bin", "-o", "/dev/null"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)
    if curl_proc.poll() is None:
        curl_proc.kill(); curl_proc.wait()
    time.sleep(0.5)
    code, verbose, cmd = curl_code_v(f"http://{HOST}:{PORT}/")
    record("server survives curl killed mid-request", server_alive() and code != "000",
           f"HTTP {code}",
           trace=make_trace(
               f"curl --limit-rate 50k .../large.bin → kill -9 → {' '.join(cmd)}",
               "server alive + HTTP 1xx-5xx",
               f"alive={server_alive()}  HTTP {code}", verbose))

    # Test 5 — large file full transmission
    tmp_recv = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    tmp_recv.close()
    cmd_str = f"curl -s --max-time 30 http://{HOST}:{PORT}/large.bin -o {tmp_recv.name}"
    sh(cmd_str, timeout=35)
    received = os.path.getsize(tmp_recv.name)
    os.unlink(tmp_recv.name)
    record("512 KB file fully transmitted", received == expected_size,
           f"expected={expected_size} received={received}",
           trace=make_trace(cmd_str, f"{expected_size} bytes received",
                            f"{received} bytes received"))

    # Test 6 — slow client
    tmp_slow = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    tmp_slow.close()
    cmd_str = (f"curl -s --limit-rate 100k --max-time 30 "
               f"http://{HOST}:{PORT}/large.bin -o {tmp_slow.name}")
    sh(cmd_str, timeout=35)
    received = os.path.getsize(tmp_slow.name)
    os.unlink(tmp_slow.name)
    record("slow client (rate-limited) receives full file", received == expected_size,
           f"expected={expected_size} received={received}",
           trace=make_trace(cmd_str, f"{expected_size} bytes received",
                            f"{received} bytes received"))

    # Test 7 — garbage → 400
    sent = b"THISISNOTHTTP\r\n\r\n"
    raw  = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("garbage request → 400", b"400" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 400", first))

    # Test 8 — HEAD no body
    sent = b"HEAD / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    raw  = tcp_send(HOST, PORT, sent)
    has_200 = b"HTTP/1.1 200" in raw
    has_cl  = b"Content-Length" in raw or b"content-length" in raw.lower()
    parts   = raw.split(b"\r\n\r\n", 1)
    body_bytes = len(parts[1]) if len(parts) > 1 else 0
    record("HEAD → 200 + Content-Length + zero body",
           has_200 and has_cl and body_bytes == 0,
           f"200={has_200} CL={has_cl} body_bytes={body_bytes}",
           trace=tcp_trace(HOST, PORT, sent, raw,
                           "HTTP/1.1 200, Content-Length present, 0 body bytes",
                           f"200={has_200}  CL={has_cl}  body_bytes={body_bytes}"))

    # Test 9 — double-close safety
    try:
        s = socket.create_connection((HOST, PORT), timeout=2)
        s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        s.close()
    except: pass
    time.sleep(0.5)
    code, verbose, cmd = curl_code_v(f"http://{HOST}:{PORT}/")
    record("server alive after early client close (double-close safety)",
           server_alive() and code != "000", f"HTTP {code}",
           trace=make_trace(f"send GET + close immediately → {' '.join(cmd)}",
                            "server alive + HTTP 1xx-5xx",
                            f"alive={server_alive()}  HTTP {code}", verbose))

    stop_server()

# ─────────────────────────────── TASK 2.2 ───────────────────────────────────

def task_2_2():
    global _current_task
    _current_task = "2.2"
    print(f"\n{cyan(bold('── Task 2.2  Body Handling, Chunked & Response Builder ──'))}")

    ok = start_server(tp("config/default.conf"))
    record("server starts", ok,
           trace=make_trace(f"{BINARY} {tp('config/default.conf')}",
                            "port 8080 open", "unreachable"))
    if not ok:
        return

    url = f"http://{HOST}:{PORT}/"

    # Required headers
    cmd_list = ["curl", "-sv", "--max-time", "8", url]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        verbose = r.stderr + r.stdout
    except Exception as e:
        verbose = str(e)

    has_200      = "HTTP/1.1 200" in verbose
    has_date     = "Date:" in verbose
    has_srv      = "Server:" in verbose
    has_cl       = "Content-Length" in verbose or "content-length" in verbose.lower()
    has_ct       = "Content-Type:" in verbose or "content-type:" in verbose.lower()
    has_date_fmt = bool(re.search(
        r"Date: \w{3}, \d{2} \w{3} \d{4} \d{2}:\d{2}:\d{2} GMT", verbose))

    record("GET / → HTTP/1.1 200 OK", has_200,
           trace=make_trace(cmd_list, "HTTP/1.1 200 OK", "not 200 (see output)", verbose))
    record("response has Date header (RFC 7231 format)", has_date and has_date_fmt,
           "(present but bad format)" if has_date and not has_date_fmt else "",
           trace=make_trace(cmd_list,
                            "Date: Dow, DD Mon YYYY HH:MM:SS GMT",
                            "Date missing" if not has_date else "Date present but wrong format",
                            verbose))
    record("response has Server header", has_srv,
           trace=make_trace(cmd_list, "Server: <anything>", "Server header absent", verbose))
    record("response has Content-Length", has_cl,
           trace=make_trace(cmd_list, "Content-Length: <n>", "Content-Length absent", verbose))
    record("response has Content-Type", has_ct,
           trace=make_trace(cmd_list, "Content-Type: <anything>", "Content-Type absent", verbose))

    # POST
    code, verbose, cmd = curl_code_v(
        f"http://{HOST}:{PORT}/echo", method="POST", extra=["-d", "hello=world"])
    record("POST /echo → valid HTTP response (not 000)", code != "000", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 1xx-5xx", f"HTTP {code}", verbose))

    # oversized POST → 413
    tmp_big = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    tmp_big.write(b"\x00" * (2 * 1024 * 1024)); tmp_big.close()
    code, verbose, cmd = curl_code_v(
        url, method="POST",
        extra=["-H", "Expect:", "--data-binary", f"@{tmp_big.name}"])
    os.unlink(tmp_big.name)
    record("POST > 1 MB → 413", code == "413", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 413", f"HTTP {code}", verbose))

    # chunked POST
    chunked_req = (
        b"POST /echo HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n\r\n"
        b"b\r\nhello world\r\n0\r\n\r\n"
    )
    raw = tcp_send(HOST, PORT, chunked_req, timeout=6)
    has_status = bool(re.search(rb"HTTP/1\.[01] \d{3}", raw))
    first_line = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("chunked POST → valid HTTP response", has_status, first_line,
           trace=tcp_trace(HOST, PORT, chunked_req, raw,
                           "HTTP/1.x 2xx/4xx", first_line))

    # HEAD zero body
    sent = b"HEAD / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    raw  = tcp_send(HOST, PORT, sent)
    parts   = raw.split(b"\r\n\r\n", 1)
    body_sz = len(parts[1]) if len(parts) > 1 else 0
    has_200 = b"HTTP/1.1 200" in raw
    has_cl  = b"Content-Length" in raw or b"content-length" in raw.lower()
    record("HEAD → 200, Content-Length present, zero body bytes",
           has_200 and has_cl and body_sz == 0, f"body_bytes={body_sz}",
           trace=tcp_trace(HOST, PORT, sent, raw,
                           "HTTP/1.1 200, Content-Length, 0 body bytes",
                           f"200={has_200}  CL={has_cl}  body={body_sz}"))

    code, verbose, cmd = curl_code_v(url)
    record("server healthy after all 2.2 tests", server_alive() and code != "000",
           f"HTTP {code}",
           trace=make_trace(cmd, "server alive + HTTP 1xx-5xx",
                            f"alive={server_alive()} HTTP {code}", verbose))

    stop_server()

# ─────────────────────────────── TASK 2.3 ───────────────────────────────────

def task_2_3():
    global _current_task
    _current_task = "2.3"
    print(f"\n{cyan(bold('── Task 2.3  Router & Longest-Prefix Matching ──'))}")

    mkdirp(tp("www/var/www"));  write_file(tp("www/var/www/a.png"),       "IMAGES_ROOT_FILE\n")
    mkdirp(tp("www/api"));      write_file(tp("www/api/index.html"),      "api root\n")
    mkdirp(tp("www/api/v2"));   write_file(tp("www/api/v2/index.html"),   "api v2 root\n")
    mkdirp(tp("www/readonly")); write_file(tp("www/readonly/index.html"), "read only\n")
    mkdirp(tp("www/new"));      write_file(tp("www/new/index.html"),      "new page\n")

    # ── Test 1: root prefix stripping ──
    write_file(tp("config/test1_images.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root /;
    location /images {{ root {tp("www/var/www")}; methods GET; }}
}}
""")
    ok = start_server(tp("config/test1_images.conf"))
    if ok:
        url = f"http://{HOST}:{PORT}/images/a.png"
        code, verbose, cmd = curl_code_v(url)
        body, bv, _       = curl_body_v(url)
        passed = code == "200" and "IMAGES_ROOT_FILE" in body
        record("Test1: /images/a.png → 200, content from www/var/www", passed,
               f"HTTP {code}",
               trace=make_trace(cmd,
                                "HTTP 200, body contains 'IMAGES_ROOT_FILE'",
                                f"HTTP {code}  body={body[:80]}",
                                verbose))
    else:
        record("Test1: server starts with images config", False,
               trace=make_trace(f"{BINARY} {tp('config/test1_images.conf')}",
                                "port 8080 open", "unreachable"))
    stop_server()

    # ── Test 2: longest prefix ──
    write_file(tp("config/test2_prefix.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root /;
    location /api    {{ root {tp("www/api")};    methods GET; index index.html; }}
    location /api/v2 {{ root {tp("www/api/v2")}; methods GET; index index.html; }}
}}
""")
    ok = start_server(tp("config/test2_prefix.conf"))
    if ok:
        url  = f"http://{HOST}:{PORT}/api/v2/"
        body, verbose, cmd = curl_body_v(url)
        if "api v2 root" in body:
            passed, got = True, "api v2 root"
        elif "api root" in body:
            passed, got = False, "matched /api instead of /api/v2"
        else:
            tmp_code = curl_code(url)
            passed, got = tmp_code != "000", f"body={body[:80]}"
        record("Test2: /api/v2 matches longer prefix /api/v2", passed, got,
               trace=make_trace(cmd,
                                "body contains 'api v2 root' (matched /api/v2)",
                                got, verbose))

        url2 = f"http://{HOST}:{PORT}/apiv2"
        code2, verbose2, cmd2 = curl_code_v(url2)
        record("Test2b: /apiv2 does NOT match /api (false prefix)", code2 != "200",
               f"HTTP {code2}",
               trace=make_trace(cmd2,
                                "HTTP 404 (no match — /apiv2 is not a prefix of /api)",
                                f"HTTP {code2}", verbose2))
    else:
        record("Test2: server starts with prefix config", False,
               trace=make_trace(f"{BINARY} {tp('config/test2_prefix.conf')}",
                                "port 8080 open", "unreachable"))
    stop_server()

    # ── Test 3: DELETE on GET-only → 405 + Allow ──
    write_file(tp("config/test3_methods.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root /;
    location /readonly {{ root {tp("www/readonly")}; methods GET; index index.html; }}
}}
""")
    ok = start_server(tp("config/test3_methods.conf"))
    if ok:
        url      = f"http://{HOST}:{PORT}/readonly/"
        cmd_list = ["curl", "-sv", "-X", "DELETE", "--max-time", "8", url]
        try:
            r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=12)
            verbose = r.stderr + r.stdout
        except Exception as e:
            verbose = str(e)
        cm = re.search(r"HTTP/1\.1 (\d+)", verbose)
        code_str      = cm.group(1) if cm else "000"
        has_allow     = bool(re.search(r"(?i)allow:", verbose))
        allow_has_get = bool(re.search(r"(?i)allow:.*GET", verbose))

        record("Test3: DELETE on GET-only → 405", code_str == "405", f"HTTP {code_str}",
               trace=make_trace(cmd_list, "HTTP 405 Method Not Allowed",
                                f"HTTP {code_str}", verbose))
        record("Test3: 405 response includes Allow: GET header",
               has_allow and allow_has_get,
               trace=make_trace(cmd_list, "Allow: GET  (RFC 7231 §7.4.1)",
                                "Allow header missing or does not contain GET", verbose))
    else:
        record("Test3: server starts with methods config", False,
               trace=make_trace(f"{BINARY} {tp('config/test3_methods.conf')}",
                                "port 8080 open", "unreachable"))
    stop_server()

    # ── Test 4: redirect /old → 301 ──
    write_file(tp("config/test4_redirect.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    location /old {{ root {tp("www/old")}; return 301 /new; }}
    location /new {{ root {tp("www/new")}; methods GET; index index.html; }}
}}
""")
    ok = start_server(tp("config/test4_redirect.conf"))
    if ok:
        url      = f"http://{HOST}:{PORT}/old"
        cmd_list = ["curl", "-sv", "--no-location", "--max-time", "8", url]
        try:
            r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=12)
            verbose = r.stderr + r.stdout
        except Exception as e:
            verbose = str(e)
        cm       = re.search(r"HTTP/1\.1 (\d+)", verbose)
        code_str = cm.group(1) if cm else "000"
        has_loc  = "/new" in verbose.lower()

        record("Test4: /old → 301", code_str == "301", f"HTTP {code_str}",
               trace=make_trace(cmd_list, "HTTP 301", f"HTTP {code_str}", verbose))
        record("Test4: Location header points to /new", has_loc,
               trace=make_trace(cmd_list, "Location: /new",
                                "Location header missing or wrong", verbose))

        cmd2 = ["curl", "-sv", "-X", "DELETE", "--no-location", "--max-time", "8", url]
        try:
            r2 = subprocess.run(cmd2, capture_output=True, text=True, timeout=12)
            v2 = r2.stderr + r2.stdout
        except Exception as e:
            v2 = str(e)
        cm2 = re.search(r"HTTP/1\.1 (\d+)", v2)
        c2  = cm2.group(1) if cm2 else "000"
        record("Test4: DELETE on /old also 301 (redirect before method check)",
               c2 == "301", f"HTTP {c2}",
               trace=make_trace(cmd2,
                                "HTTP 301 (redirect evaluated before method check)",
                                f"HTTP {c2}", v2))
    else:
        record("Test4: server starts with redirect config", False,
               trace=make_trace(f"{BINARY} {tp('config/test4_redirect.conf')}",
                                "port 8080 open", "unreachable"))
    stop_server()

    # ── Test 5: path traversal ──
    write_file(tp("config/test5_traversal.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    location / {{ root {tp("www")}; methods GET; }}
}}
""")
    ok = start_server(tp("config/test5_traversal.conf"))
    if ok:
        traversal_uris = [
            "/../../etc/passwd",
            "/../../../etc/passwd",
            "/%2e%2e/%2e%2e/etc/passwd",
            "/./../../etc/passwd",
        ]
        blocked_all = True; trace_lines = []
        for uri in traversal_uris:
            cmd_l = ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}",
                     "--path-as-is", "--max-time", "8",
                     f"http://{HOST}:{PORT}{uri}"]
            try:
                r = subprocess.run(cmd_l, capture_output=True, text=True, timeout=10)
                code = r.stdout.strip()
            except: code = "000"
            trace_lines.append(f"  {uri}  →  HTTP {code}")
            if code == "200":
                blocked_all = False

        record("Test5: all path-traversal attempts blocked (no 200)", blocked_all,
               trace=make_trace("curl --path-as-is  (for each traversal URI)",
                                "HTTP != 200 for all URIs",
                                "at least one returned 200" if not blocked_all else "all blocked",
                                "\n".join(trace_lines)))
        record("Test5: server alive after traversal attempts", server_alive(),
               trace=make_trace("kill -0 <pid>", "server alive", "server crashed"))
    else:
        record("Test5: server starts with traversal config", False,
               trace=make_trace(f"{BINARY} {tp('config/test5_traversal.conf')}",
                                "port 8080 open", "unreachable"))
    stop_server()

    # ── Test 6: unmatched URI → 404 ──
    write_file(tp("config/test6_nomatch.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    location /exists {{ root {tp("www")}; methods GET; }}
}}
""")
    ok = start_server(tp("config/test6_nomatch.conf"))
    if ok:
        all_404 = True; trace_lines = []
        for uri in ["/doesnotexist", "/no/location/here", "/almost_exists"]:
            code, _, _ = curl_code_v(f"http://{HOST}:{PORT}{uri}")
            trace_lines.append(f"  {uri}  →  HTTP {code}")
            if code != "404":
                all_404 = False
        record("Test6: unmatched URIs → 404", all_404,
               trace=make_trace("curl (for each unmatched URI)",
                                "HTTP 404 for all",
                                "at least one returned != 404" if not all_404 else "all 404",
                                "\n".join(trace_lines)))
    else:
        record("Test6: server starts with no-match config", False,
               trace=make_trace(f"{BINARY} {tp('config/test6_nomatch.conf')}",
                                "port 8080 open", "unreachable"))
    stop_server()

# ─────────────────────────────── TASK 3.1 ───────────────────────────────────

def task_3_1():
    global _current_task
    _current_task = "3.1"
    print(f"\n{cyan(bold('── Task 3.1  Multi-Listen Socket ──'))}")

    mkdirp(tp("www/port8080")); write_file(tp("www/port8080/index.html"), "CONTENT_FROM_8080\n")
    mkdirp(tp("www/port9090")); write_file(tp("www/port9090/index.html"), "CONTENT_FROM_9090\n")
    mkdirp(tp("www/anybind"));  write_file(tp("www/anybind/index.html"),  "CONTENT_FROM_ANY\n")

    write_file(tp("config/test_dual_listen.conf"), f"""\
server {{
    listen 127.0.0.1:8080;
    root {tp("www/port8080")};
    location / {{ methods GET; index index.html; }}
}}
server {{
    listen 127.0.0.1:9090;
    root {tp("www/port9090")};
    location / {{ methods GET; index index.html; }}
}}
""")
    ok = start_server(tp("config/test_dual_listen.conf"), ports=[8080, 9090])
    record("Test1: :8080 and :9090 both accept connections", ok,
           trace=make_trace(f"{BINARY} {tp('config/test_dual_listen.conf')}",
                            "ports 8080 and 9090 open",
                            "one or both ports unreachable"))
    if ok:
        body_8080, v1, cmd1 = curl_body_v(f"http://{HOST}:8080/")
        record("Test2: :8080 returns port-8080 content",
               "CONTENT_FROM_8080" in body_8080, body_8080.strip(),
               trace=make_trace(cmd1,
                                "body contains 'CONTENT_FROM_8080'",
                                body_8080.strip()[:80] or "(empty)", v1))

        body_9090, v2, cmd2 = curl_body_v(f"http://{HOST}:9090/")
        record("Test2: :9090 returns port-9090 content",
               "CONTENT_FROM_9090" in body_9090, body_9090.strip(),
               trace=make_trace(cmd2,
                                "body contains 'CONTENT_FROM_9090'",
                                body_9090.strip()[:80] or "(empty)", v2))
    stop_server()

    # SO_REUSEADDR
    start_server(tp("config/test_dual_listen.conf"), ports=[8080, 9090])
    stop_server(); time.sleep(0.3)
    ok2 = start_server(tp("config/test_dual_listen.conf"), ports=[8080, 9090])
    c1, v1, cmd1 = curl_code_v(f"http://{HOST}:8080/")
    c2, v2, cmd2 = curl_code_v(f"http://{HOST}:9090/")
    record("Test3: immediate restart (SO_REUSEADDR) — no EADDRINUSE",
           ok2 and c1 != "000" and c2 != "000", f"8080={c1} 9090={c2}",
           trace=make_trace(
               f"stop + immediate restart of {BINARY} {tp('config/test_dual_listen.conf')}",
               "both ports respond", f"8080: HTTP {c1}  9090: HTTP {c2}",
               v1 + "\n---\n" + v2))
    stop_server()

    # 0.0.0.0 binding
    write_file(tp("config/test_anybind.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root {tp("www/anybind")};
    location / {{ methods GET; index index.html; }}
}}
""")
    ok = start_server(tp("config/test_anybind.conf"))
    record("Test4: 0.0.0.0:8080 starts", ok,
           trace=make_trace(f"{BINARY} {tp('config/test_anybind.conf')}",
                            "port 8080 open", "unreachable"))
    if ok:
        cl,  vl,  cl_cmd  = curl_code_v("http://localhost:8080/")
        cl2, vl2, cl2_cmd = curl_code_v(f"http://127.0.0.1:8080/")
        record("Test4: 0.0.0.0 accepts from localhost", cl  != "000", f"HTTP {cl}",
               trace=make_trace(cl_cmd,  "HTTP 1xx-5xx", f"HTTP {cl}",  vl))
        record("Test4: 0.0.0.0 accepts from 127.0.0.1", cl2 != "000", f"HTTP {cl2}",
               trace=make_trace(cl2_cmd, "HTTP 1xx-5xx", f"HTTP {cl2}", vl2))
    stop_server()

# ─────────────────────────────── TASK 3.2 ───────────────────────────────────

def task_3_2():
    global _current_task
    _current_task = "3.2"
    print(f"\n{cyan(bold('── Task 3.2  Static File Handler ──'))}")

    mkdirp(tp("www"))
    write_file(tp("www/index.html"),
               '<!DOCTYPE html><html><head><link rel="stylesheet" href="style.css">'
               '</head><body>Hello <img src="image.png"></body></html>\n')
    write_file(tp("www/style.css"), "body { background: #abc; }\n")
    write_file(tp("www/image.png"), os.urandom(128))

    write_file(tp("config/static.conf"), f"""\
server {{
    listen 127.0.0.1:8080;
    root {tp("www")};
    location / {{ methods GET HEAD; index index.html; }}
}}
""")
    ok = start_server(tp("config/static.conf"))
    record("server starts with static config", ok,
           trace=make_trace(f"{BINARY} {tp('config/static.conf')}",
                            "port 8080 open", "unreachable"))
    if not ok:
        return

    def _check_ct(path: str, expected_ct: str, test_name: str):
        url = f"http://{HOST}:{PORT}/{path}"
        hdrs, verbose, cmd = curl_headers_v(url)
        full = hdrs + verbose
        got_ct = re.search(r"(?i)content-type:\s*(\S+)", full)
        got_str = got_ct.group(1).lower().rstrip(";,") if got_ct else "(missing)"
        passed  = expected_ct.lower() in full.lower()
        record(test_name, passed, got_str,
               trace=make_trace(cmd,
                                f"Content-Type: {expected_ct}",
                                f"Content-Type: {got_str}",
                                hdrs))

    _check_ct("index.html", "text/html", "Test1: index.html → Content-Type: text/html; charset=utf-8")
    _check_ct("style.css",  "text/css",  "Test2: style.css  → Content-Type: text/css")
    _check_ct("image.png",  "image/png", "Test3: image.png  → Content-Type: image/png")

    # HEAD
    cmd_list = ["curl", "-sv", "-I", "--max-time", "8", f"http://{HOST}:{PORT}/"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        hdrs_output = r.stdout + r.stderr
    except Exception as e:
        hdrs_output = str(e)
    has_ct = "text/html" in hdrs_output.lower()
    record("Test4: HEAD / returns Content-Type: text/html", has_ct,
           trace=make_trace(cmd_list, "Content-Type: text/html",
                            "Content-Type missing or not text/html", hdrs_output))

    # 404
    code, verbose, cmd = curl_code_v(f"http://{HOST}:{PORT}/doesnotexist.txt")
    record("Test5: missing file → 404", code == "404", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404", f"HTTP {code}", verbose))

    # linked assets
    body, bv, bcmd = curl_body_v(f"http://{HOST}:{PORT}/index.html")
    c_css, vcss, cmdcss = curl_code_v(f"http://{HOST}:{PORT}/style.css")
    c_img, vimg, cmdimg = curl_code_v(f"http://{HOST}:{PORT}/image.png")
    record("Test6: HTML served + CSS 200 + image 200",
           "Hello" in body and c_css == "200" and c_img == "200",
           f"css={c_css} img={c_img}",
           trace=make_trace(
               f"{' '.join(bcmd)} (+ css + img)",
               "body contains 'Hello', CSS=200, image=200",
               f"'Hello' in body={'Hello' in body}  css={c_css}  img={c_img}",
               f"--- CSS verbose ---\n{vcss}\n--- IMG verbose ---\n{vimg}"))

    stop_server()

# ─────────────────────────────── TASK 3.3 ───────────────────────────────────

def task_3_3():
    global _current_task
    _current_task = "3.3"
    print(f"\n{cyan(bold('── Task 3.3  Autoindex & Error Pages ──'))}")

    mkdirp(tp("www/auto"))
    write_file(tp("www/auto/b.txt"), "content b\n")
    write_file(tp("www/auto/a.txt"), "content a\n")
    mkdirp(tp("www/noindex"))
    mkdirp(tp("www/withindex"))
    write_file(tp("www/withindex/index.html"),
               "<!DOCTYPE html><html><body>Index page</body></html>\n")
    mkdirp(tp("www/errors"))
    write_file(tp("www/errors/custom_404.html"),
               "<!DOCTYPE html><html><body>Custom 404 Page</body></html>\n")

    write_file(tp("config/dir.conf"), f"""\
server {{
    listen 127.0.0.1:8080;
    root {tp("www")};
    location /auto/       {{ root {tp("www/auto")};     autoindex on; }}
    location /noindex/    {{ root {tp("www/noindex")};  autoindex off; }}
    location /withindex/  {{ root {tp("www/withindex")}; index index.html; }}
    location /custom404/  {{ root {tp("www/errors")};   error_page 404 /errors/custom_404.html; }}
    location /missing404/ {{ root {tp("www/errors")};   error_page 404 /errors/missing_404.html; }}
}}
""")
    ok = start_server(tp("config/dir.conf"))
    record("server starts with dir/autoindex config", ok,
           trace=make_trace(f"{BINARY} {tp('config/dir.conf')}",
                            "port 8080 open", "unreachable"))
    if not ok:
        return

    # autoindex alphabetical
    body, bv, bcmd = curl_body_v(f"http://{HOST}:{PORT}/auto/")
    has_a, has_b = "a.txt" in body, "b.txt" in body
    pos_a, pos_b = body.find("a.txt"), body.find("b.txt")
    alpha = pos_a < pos_b if (has_a and has_b and pos_a >= 0 and pos_b >= 0) else False
    record("Test1: autoindex on → alphabetical listing",
           has_a and has_b and alpha, f"a@{pos_a} b@{pos_b}",
           trace=make_trace(bcmd,
                            "listing contains a.txt before b.txt",
                            f"has_a={has_a}  has_b={has_b}  alpha={alpha}",
                            body[:600]))

    # autoindex off → 403
    code, verbose, cmd = curl_code_v(f"http://{HOST}:{PORT}/noindex/")
    record("Test2: autoindex off → 403", code == "403", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 403", f"HTTP {code}", verbose))

    # directory with index
    body, bv, bcmd = curl_body_v(f"http://{HOST}:{PORT}/withindex/")
    record("Test3: /withindex/ serves index.html", "Index page" in body,
           trace=make_trace(bcmd, "body contains 'Index page'",
                            body.strip()[:80] or "(empty)", bv))

    # no trailing slash → 301
    cmd_list = ["curl", "-sv", "--no-location", "--max-time", "8",
                f"http://{HOST}:{PORT}/withindex"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        hdrs_v = r.stdout + r.stderr
    except Exception as e:
        hdrs_v = str(e)
    sm   = re.search(r"HTTP/\S+ (\d+)", hdrs_v)
    stat = sm.group(1) if sm else "000"
    has_trailing_loc = bool(re.search(r"(?i)location:.*withindex/", hdrs_v))
    record("Test4: /withindex (no slash) → 301", stat == "301", f"HTTP {stat}",
           trace=make_trace(cmd_list, "HTTP 301", f"HTTP {stat}", hdrs_v))
    record("Test4: Location header contains /withindex/", has_trailing_loc,
           trace=make_trace(cmd_list, "Location: .../withindex/",
                            "Location missing or wrong", hdrs_v))

    # missing custom 404 → fallback
    cmd5 = ["curl", "-sv", "--max-time", "8",
            f"http://{HOST}:{PORT}/missing404/doesnotexist"]
    try:
        r5 = subprocess.run(cmd5, capture_output=True, text=True, timeout=10)
        hdrs5, body5 = r5.stderr, r5.stdout
    except Exception as e:
        hdrs5, body5 = str(e), ""
    sm5 = re.search(r"HTTP/\S+ (\d+)", hdrs5)
    st5 = sm5.group(1) if sm5 else "000"
    record("Test5: missing custom 404 → fallback body with 404 status",
           st5 == "404" and "404" in body5 and "Custom 404 Page" not in body5,
           f"status={st5}",
           trace=make_trace(cmd5,
                            "HTTP 404 + '404' in body (server's own error page, not custom)",
                            f"status={st5}  '404' in body={'404' in body5}  "
                            f"custom served={'Custom 404 Page' in body5}",
                            hdrs5 + "\n" + body5[:400]))

    # existing custom 404
    cmd6 = ["curl", "-sv", "--max-time", "8",
            f"http://{HOST}:{PORT}/custom404/doesnotexist"]
    try:
        r6 = subprocess.run(cmd6, capture_output=True, text=True, timeout=10)
        hdrs6, body6 = r6.stderr, r6.stdout
    except Exception as e:
        hdrs6, body6 = str(e), ""
    sm6 = re.search(r"HTTP/\S+ (\d+)", hdrs6)
    st6 = sm6.group(1) if sm6 else "000"
    record("Test6: custom 404 page → 404 status", st6 == "404", f"HTTP {st6}",
           trace=make_trace(cmd6, "HTTP 404", f"HTTP {st6}", hdrs6))
    record("Test6: custom 404 body served", "404 Not Found" in body6,
           body6[:80],
           trace=make_trace(cmd6,
                            "body contains '404 Not Found'",
                            body6.strip()[:80] or "(empty)",
                            hdrs6 + "\n" + body6[:400]))

    stop_server()

# ─────────────────────────────── TASK 4.1 ───────────────────────────────────

def task_4_1():
    global _current_task
    _current_task = "4.1"
    print(f"\n{cyan(bold('── Task 4.1  Keep-Alive & Pipelining ──'))}")

    mkdirp(tp("www"))
    write_file(tp("www/index.html"), "<!DOCTYPE html><html><body>Home</body></html>\n")
    write_file(tp("www/one.html"),   "<!DOCTYPE html><html><body>One</body></html>\n")
    write_file(tp("www/two.html"),   "<!DOCTYPE html><html><body>Two</body></html>\n")

    write_file(tp("config/keep.conf"), f"""\
server {{
    listen 127.0.0.1:8080;
    root {tp("www")};
    location / {{ index index.html; }}
}}
""")
    ok = start_server(tp("config/keep.conf"))
    record("server starts with keep-alive config", ok,
           trace=make_trace(f"{BINARY} {tp('config/keep.conf')}",
                            "port 8080 open", "unreachable"))
    if not ok:
        return

    # Test 1 — pipelined GETs
    pipeline = (
        b"GET /one.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"
        b"GET /two.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"
    )
    raw = tcp_send(HOST, PORT, pipeline, timeout=5)
    decoded = raw.decode(errors="replace")
    record("Test1: pipelined GETs — first response body present", "One" in decoded,
           trace=tcp_trace(HOST, PORT, pipeline, raw,
                           "body contains 'One'", decoded[:500]))
    record("Test1: pipelined GETs — second response body present", "Two" in decoded,
           trace=tcp_trace(HOST, PORT, pipeline, raw,
                           "body contains 'Two'", decoded[:500]))

    # Test 2 — Connection: keep-alive in response
    cmd_list = ["curl", "-sv", "--http1.1", "--max-time", "8",
                f"http://{HOST}:{PORT}/"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        verbose = r.stderr + r.stdout
    except Exception as e:
        verbose = str(e)
    has_ka = bool(re.search(r"(?i)connection:\s*keep-alive", verbose))
    record("Test2: response includes Connection: keep-alive", has_ka,
           trace=make_trace(cmd_list,
                            "Connection: keep-alive",
                            "Connection: keep-alive not found in response", verbose))

    # Test 3 — Connection: close → body delivered
    close_req = b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"
    raw = tcp_send(HOST, PORT, close_req, timeout=5)
    record("Test3: Connection: close → body delivered", b"Home" in raw,
           trace=tcp_trace(HOST, PORT, close_req, raw,
                           "body contains 'Home'",
                           raw.decode(errors="replace")[:400]))

    # Test 4 — idle keep-alive timeout
    closed_by_server = threading.Event()
    response_ok      = threading.Event()

    def idle_test():
        try:
            with socket.create_connection((HOST, PORT), timeout=5) as s:
                s.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
                buf = b""; s.settimeout(5)
                try:
                    while True:
                        chunk = s.recv(4096)
                        if not chunk: break
                        buf += chunk
                except socket.timeout: pass
                if b"Home" in buf:
                    response_ok.set()
                s.settimeout(15)
                try:
                    if s.recv(1) == b"":
                        closed_by_server.set()
                except socket.timeout: pass
        except: pass

    t = threading.Thread(target=idle_test, daemon=True)
    t.start(); t.join(timeout=18)

    record("Test4: idle connection — response body delivered", response_ok.is_set(),
           trace=make_trace("GET / HTTP/1.1 (keep-alive) → read response",
                            "body contains 'Home'", "body not received"))
    record("Test4: idle connection closed by server after timeout",
           closed_by_server.is_set(),
           "(server must close idle connections; check idle/keep-alive timeout config)",
           trace=make_trace(
               "keep-alive socket → wait up to 15 s for EOF from server",
               "server sends EOF (closes idle connection)",
               "server did NOT close the connection within 15 s"))

    stop_server()

# ─────────────────────────────── TASK 4.2 ───────────────────────────────────

def task_4_2():
    global _current_task
    _current_task = "4.2"
    print(f"\n{cyan(bold('── Task 4.2  Uploads ──'))}")

    mkdirp(tp("uploads"))
    mkdirp(tp("uploads/forbidden"))
    write_file(tp("test.txt"), "hello world\n")
    write_file(tp("large.bin"), os.urandom(15 * 1024))   # 15 KB > 10 KB limit
    shutil.copy(tp("test.txt"), tp("uploads/test.txt"))
    shutil.copy(tp("test.txt"), tp("uploads/forbidden/test.txt"))
    os.chmod(tp("uploads/forbidden"), 0o555)

    write_file(tp("config/upload.conf"), f"""\
server {{
    listen 127.0.0.1:8080;
    root {tp("www")};
    location /upload/ {{
        upload_dir {tp("www/uploads")};
        methods POST DELETE;
        client_max_body_size 10240;
    }}
    location /forbidden/ {{
        upload_dir {tp("www/uploads/forbidden")};
        methods POST DELETE;
        client_max_body_size 10240;
    }}
}}
""")
    ok = start_server(tp("config/upload.conf"))
    record("server starts with upload config", ok,
           trace=make_trace(f"{BINARY} {tp('config/upload.conf')}",
                            "port 8080 open", "unreachable"))
    if not ok:
        return

    url = f"http://{HOST}:{PORT}/upload/"

    # Test 1: POST → 201
    cmd_list = ["curl", "-sv", "-X", "POST", "--max-time", "8",
                "-F", f"file=@{tp('test.txt')}", url]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=12)
        code = re.search(r"< HTTP/1\.\d (\d+)", r.stderr)
        code = code.group(1) if code else "000"
        verbose = r.stderr + r.stdout
    except Exception as e:
        code, verbose = "000", str(e)
    record("Test1: POST file → 201 Created", code == "201", f"HTTP {code}",
           trace=make_trace(cmd_list, "HTTP 201 Created",
                            f"HTTP {code}", verbose))

    files_in_uploads = [f for f in os.listdir(tp("uploads")) if f != "forbidden"]
    record("Test1: uploaded file present in uploads/", len(files_in_uploads) > 0,
           trace=make_trace(f"ls {tp('uploads')}/",
                            "at least one file present",
                            f"found: {files_in_uploads}"))

    # Test 2: path traversal sanitised
    cmd_list2 = ["curl", "-sv", "-X", "POST", "--max-time", "8",
                 "-F", f"file=@{tp('test.txt')};filename=../../etc/passwd", url]
    try:
        r2 = subprocess.run(cmd_list2, capture_output=True, text=True, timeout=12)
        code2 = re.search(r"< HTTP/1\.\d (\d+)", r2.stderr)
        code2 = code2.group(1) if code2 else "000"
        v2    = r2.stderr + r2.stdout
    except Exception as e:
        code2, v2 = "000", str(e)
    record("Test2: traversal filename sanitised to basename",
           code2 == "201",
           f"HTTP {code2}",
           trace=make_trace(cmd_list2,
                            "HTTP 201 (stored as 'passwd' in uploads/, not /etc/passwd)",
                            f"HTTP {code2}", v2))

    # Test 3: DELETE existing → 204
    shutil.copy(tp("test.txt"), tp("uploads/delete_me.txt"))
    url3      = f"http://{HOST}:{PORT}/upload/delete_me.txt"
    cmd_list3 = ["curl", "-sv", "-X", "DELETE", "--max-time", "8", url3]
    try:
        r3 = subprocess.run(cmd_list3, capture_output=True, text=True, timeout=12)
        code3 = re.search(r"< HTTP/1\.\d (\d+)", r3.stderr)
        code3 = code3.group(1) if code3 else "000"
        v3    = r3.stderr + r3.stdout
    except Exception as e:
        code3, v3 = "000", str(e)
    file_gone = not os.path.isfile(tp("uploads/delete_me.txt"))
    record("Test3: DELETE existing file → 204", code3 == "204", f"HTTP {code3}",
           trace=make_trace(cmd_list3, "HTTP 204 No Content",
                            f"HTTP {code3}", v3))
    record("Test3: file removed after DELETE", file_gone,
           trace=make_trace(f"ls {tp('uploads')}/delete_me.txt  (after DELETE)",
                            "file absent",
                            "file still present" if not file_gone else "file absent"))

    # Test 4: DELETE non-existent → 404
    code4, v4, cmd4 = curl_code_v(
        f"http://{HOST}:{PORT}/upload/no_such_file.txt", method="DELETE")
    record("Test4: DELETE non-existent → 404", code4 == "404", f"HTTP {code4}",
           trace=make_trace(cmd4, "HTTP 404", f"HTTP {code4}", v4))

    # Test 5: DELETE without permission → 403
    code5, v5, cmd5 = curl_code_v(
        f"http://{HOST}:{PORT}/forbidden/test.txt", method="DELETE")
    record("Test5: DELETE without write permission → 403", code5 == "403",
           f"HTTP {code5}",
           trace=make_trace(cmd5,
                            f"HTTP 403 ({tp('uploads/forbidden')} has mode 0555)",
                            f"HTTP {code5}", v5))

    # Test 6: oversized POST → 413
    cmd_list6 = ["curl", "-sv", "-X", "POST", "--max-time", "8",
                 "-F", f"file=@{tp('large.bin')}", url]
    try:
        r6 = subprocess.run(cmd_list6, capture_output=True, text=True, timeout=12)
        code6 = re.search(r"< HTTP/1\.\d (\d+)", r6.stderr)
        code6 = code6.group(1) if code6 else "000"
        v6    = r6.stderr + r6.stdout
    except Exception as e:
        code6, v6 = "000", str(e)
    record("Test6: POST > client_max_body_size → 413", code6 == "413",
           f"HTTP {code6}",
           trace=make_trace(cmd_list6,
                            "HTTP 413 (large.bin 15 KB > 10 KB limit)",
                            f"HTTP {code6}", v6))

    stop_server()
    # restore permissions so cleanup can remove the directory
    try: os.chmod(tp("uploads/forbidden"), 0o755)
    except Exception: pass

# ─────────────────────────────── TASK 4.3 ───────────────────────────────────

def task_4_3():
    global _current_task
    _current_task = "4.3"
    print(f"\n{cyan(bold('── Task 4.3  Integration & Upload Limits ──'))}")

    mkdirp(tp("www/getonly"))
    write_file(tp("www/getonly/test_read.txt"),
               "<!DOCTYPE html><html><body>Get-only file</body></html>\n")
    mkdirp(tp("uploads"))

    write_file(tp("config/integration.conf"), f"""\
server {{
    listen 127.0.0.1:8080;
    root {tp("www")};
    location /upload/ {{
        upload_dir {tp("uploads")};
        methods POST DELETE;
        client_max_body_size 10485760;
    }}
    location /getonly/ {{
        root {tp("www/getonly")};
        methods GET;
    }}
}}
""")
    ok = start_server(tp("config/integration.conf"))
    record("server starts with integration config", ok,
           trace=make_trace(f"{BINARY} {tp('config/integration.conf')}",
                            "port 8080 open", "unreachable"))
    if not ok:
        return

    # optional test_integration.py
    if os.path.isfile("test_integration.py"):
        rc, out, err = sh("python3 test_integration.py", timeout=60)
        record("Test1: test_integration.py passes", rc == 0,
               trace=make_trace("python3 test_integration.py",
                                "exit 0", f"exit {rc}", out + err))
    else:
        skip("Test1: test_integration.py", "file not present (optional)")

    # Upload size limits
    sizes = [(1, "1 MB", "201"), (5, "5 MB", "201"), (11, "11 MB", "413")]
    tmp_files = []
    for mb, label, expected in sizes:
        tf = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
        tf.write(os.urandom(mb * 1024 * 1024)); tf.close()
        tmp_files.append((tf.name, label, expected))

    for path, label, expected in tmp_files:
        cmd_list = ["curl", "-sv", "-X", "POST", "--max-time", "60",
                    "-F", f"file=@{path}", f"http://{HOST}:{PORT}/upload/"]
        try:
            r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=65)
            code = re.search(r"< HTTP/1\.\d (\d+)", r.stderr)
            code = code.group(1) if code else "000"
            verbose = r.stderr + r.stdout
        except Exception as e:
            code, verbose = "000", str(e)
        record(f"Test2: {label} upload → {expected}", code == expected,
               f"HTTP {code}",
               trace=make_trace(cmd_list,
                                f"HTTP {expected}",
                                f"HTTP {code}", verbose))

    for path, _, _ in tmp_files:
        try: os.unlink(path)
        except FileNotFoundError: pass

    # DELETE on GET-only → 405 + Allow
    cmd_list = ["curl", "-sv", "-X", "DELETE", "--max-time", "8",
                f"http://{HOST}:{PORT}/getonly/test_read.txt"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=12)
        verbose = r.stderr + r.stdout
    except Exception as e:
        verbose = str(e)
    sm            = re.search(r"HTTP/\S+ (\d+)", verbose)
    status        = sm.group(1) if sm else "000"
    has_allow_get = bool(re.search(r"(?i)allow:.*GET", verbose))
    record("Test3: DELETE on GET-only → 405", status == "405", f"HTTP {status}",
           trace=make_trace(cmd_list, "HTTP 405", f"HTTP {status}", verbose))
    record("Test3: 405 includes Allow: GET", has_allow_get,
           trace=make_trace(cmd_list, "Allow: GET  (RFC 7231 §7.4.1)",
                            "Allow header missing or does not contain GET", verbose))

    stop_server()
    if os.path.isdir(tp("uploads")):
        shutil.rmtree(tp("uploads"), ignore_errors=True)

# ─────────────────────────────── MAIN ───────────────────────────────────────

TASK_FUNCS = {
    "1.1": task_1_1, "1.2": task_1_2, "1.3": task_1_3,
    "2.1": task_2_1, "2.2": task_2_2, "2.3": task_2_3,
    "3.1": task_3_1, "3.2": task_3_2, "3.3": task_3_3,
    "4.1": task_4_1, "4.2": task_4_2, "4.3": task_4_3,
}

def main():
    parser = argparse.ArgumentParser(
        description="webserv integration tester",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  python3 tester.py 2.3   # runs 1.1 → 1.2 → 1.3 → 2.1 → 2.2 → 2.3\n"
            "  python3 tester.py 4.3   # runs all tasks\n\n"
            f"All temporary files are created under ./{TESTER_DIR}/ and cleaned up from there only."
        ),
    )
    parser.add_argument("task", help="target task  (e.g. 2.3 or 4.1)")
    args = parser.parse_args()

    target = args.task.strip()
    if target not in ALL_TASKS:
        print(red(f"Unknown task '{target}'. Valid tasks: {', '.join(ALL_TASKS)}"))
        sys.exit(1)

    idx          = ALL_TASKS.index(target)
    tasks_to_run = ALL_TASKS[:idx + 1]

    print(bold(f"\n{'━'*48}"))
    print(bold(f"  webserv tester  —  running up to task {target}"))
    print(bold(f"  Tasks : {', '.join(tasks_to_run)}"))
    print(bold(f"  Sandbox: ./{TESTER_DIR}/"))
    print(bold(f"{'━'*48}"))

    if not os.path.isfile(BINARY):
        print(yellow(f"Binary '{BINARY}' not found — attempting 'make' first …"))
        rc, _, _ = sh("make 2>&1", timeout=120)
        if rc != 0 or not os.path.isfile(BINARY):
            print(red("Build failed. Aborting."))
            sys.exit(1)

    def _sig(sig, frame):
        print(yellow("\nInterrupted — cleaning up …"))
        cleanup()
        print_summary()
        sys.exit(130)

    signal.signal(signal.SIGINT, _sig)
    setup_defaults()

    try:
        for task_id in tasks_to_run:
            TASK_FUNCS[task_id]()
    finally:
        cleanup()

    print_summary()
    failed = sum(1 for r in _results if r["passed"] is False)
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()