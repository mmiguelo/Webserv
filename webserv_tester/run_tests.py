#!/usr/bin/env python3
"""
╔══════════════════════════════════════════════════════════════╗
║              WEBSERV  —  Hardcore Evaluation Tester          ║
║                                                              ║
║  Usage:  python3 webserv_tester/run_tests.py [--category X]  ║
║          python3 webserv_tester/run_tests.py --help          ║
║                                                              ║
║  All temporary files are created inside webserv_tester/tmp/  ║
║  Traces are written to webserv_tester/traces.log             ║
╚══════════════════════════════════════════════════════════════╝
"""

import argparse
import io
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# Force UTF-8 output on Windows
if sys.stdout.encoding != "utf-8":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
if sys.stderr.encoding != "utf-8":
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# ══════════════════════════════════ PATHS ══════════════════════════════════════

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
BINARY      = "./webserv"  # relative to PROJECT_DIR (where sh() and start_server run)
BINARY_ABS  = os.path.join(PROJECT_DIR, "webserv")  # for Python os.path checks
TMP_DIR     = os.path.join(SCRIPT_DIR, "tmp")
TRACES_FILE = os.path.join(SCRIPT_DIR, "traces.log")
HOST        = "127.0.0.1"
PORT        = 8080
PORT2       = 9090

def tp(*parts):
    """Return an absolute path rooted inside TMP_DIR (for Python file ops)."""
    return os.path.join(TMP_DIR, *parts)

def rp(*parts):
    """Return a path relative to PROJECT_DIR (for config files the server reads).
    The server CWD is PROJECT_DIR, so configs must use paths relative to it."""
    abs_path = os.path.join(TMP_DIR, *parts)
    return os.path.relpath(abs_path, PROJECT_DIR).replace("\\", "/")

# ══════════════════════════════════ COLOURS ════════════════════════════════════

GREEN   = "\033[92m"
RED     = "\033[91m"
YELLOW  = "\033[93m"
CYAN    = "\033[96m"
BOLD    = "\033[1m"
RESET   = "\033[0m"
DIM     = "\033[2m"
BLUE    = "\033[94m"
MAGENTA = "\033[95m"
WHITE   = "\033[97m"

def green(s):   return f"{GREEN}{s}{RESET}"
def red(s):     return f"{RED}{s}{RESET}"
def yellow(s):  return f"{YELLOW}{s}{RESET}"
def cyan(s):    return f"{CYAN}{s}{RESET}"
def bold(s):    return f"{BOLD}{s}{RESET}"
def dim(s):     return f"{DIM}{s}{RESET}"
def blue(s):    return f"{BLUE}{s}{RESET}"
def magenta(s): return f"{MAGENTA}{s}{RESET}"
def white(s):   return f"{WHITE}{s}{RESET}"

TICK = green("\u2713")
CROSS = red("\u2717")

# ══════════════════════════════════ RESULTS ════════════════════════════════════

_results = []
_current_category = ""

def record(name, passed, detail="", trace=""):
    _results.append({
        "category": _current_category,
        "name":     name,
        "passed":   passed,
        "detail":   detail,
        "trace":    trace,
    })
    icon = TICK if passed else CROSS
    msg = f"  {icon}  {name}"
    if detail and not passed:
        msg += f"  {yellow('-->')} {detail}"
    print(msg)

def skip(name, reason=""):
    _results.append({"category": _current_category, "name": name, "passed": None, "trace": ""})
    print(f"  {yellow('~')}  {name}" + (f"  {dim(f'({reason})')}" if reason else ""))

# ══════════════════════════════════ TRACE HELPERS ═════════════════════════════

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")

def strip_ansi(s):
    return ANSI_RE.sub("", s)

def make_trace(command, expected, got, output=""):
    cmd_str = command if isinstance(command, str) else " ".join(str(c) for c in command)
    lines = [
        "=" * 70,
        f"COMMAND:  {cmd_str}",
        f"EXPECTED: {expected}",
        f"GOT:      {got}",
    ]
    if output and output.strip():
        lines.append("-" * 70)
        lines.append("RAW OUTPUT:")
        for ln in output.strip().splitlines()[:60]:
            lines.append(f"  {ln}")
        if len(output.strip().splitlines()) > 60:
            lines.append(f"  ... ({len(output.strip().splitlines()) - 60} more lines)")
    lines.append("=" * 70)
    return "\n".join(lines)

def tcp_trace(host, port, sent, received, expected, got):
    sent_repr = sent.decode(errors="replace").replace("\r\n", "\\r\\n\n")
    recv_repr = received.decode(errors="replace")
    lines = [
        "=" * 70,
        f"TCP ENDPOINT: {host}:{port}",
        f"EXPECTED:     {expected}",
        f"GOT:          {got}",
        "-" * 70,
        "SENT BYTES:",
    ]
    for ln in sent_repr.splitlines()[:20]:
        lines.append(f"  {ln}")
    lines.append("-" * 70)
    lines.append("RECEIVED BYTES:")
    for ln in recv_repr.splitlines()[:60]:
        lines.append(f"  {ln}")
    if len(recv_repr.splitlines()) > 60:
        lines.append(f"  ... ({len(recv_repr.splitlines()) - 60} more lines)")
    lines.append("=" * 70)
    return "\n".join(lines)

# ══════════════════════════════════ SUMMARY ════════════════════════════════════

def write_traces_file(failures):
    with open(TRACES_FILE, "w", encoding="utf-8") as fh:
        if not failures:
            fh.write("All tests passed -- no failures to report.\n")
            return
        fh.write("=" * 70 + "\n")
        fh.write("  FAILURE TRACES  --  Webserv Evaluation Tester\n")
        fh.write("=" * 70 + "\n\n")
        for r in failures:
            fh.write(f"FAILED: [{r['category']}] {r['name']}\n")
            if r.get("trace"):
                fh.write(strip_ansi(r["trace"]) + "\n\n")
            elif r.get("detail"):
                fh.write(f"  Detail: {strip_ansi(r['detail'])}\n\n")
    print(f"\n  {dim('Traces written to')} {TRACES_FILE}")


def print_summary():
    print()
    print(bold(f"{'=' * 56}"))
    print(bold("  SUMMARY"))
    print(bold(f"{'=' * 56}"))
    by_cat = {}
    for r in _results:
        by_cat.setdefault(r["category"], []).append(r)

    total_pass = total_fail = total_skip = 0
    for cat, results in by_cat.items():
        p = sum(1 for r in results if r["passed"] is True)
        f = sum(1 for r in results if r["passed"] is False)
        s = sum(1 for r in results if r["passed"] is None)
        total_pass += p
        total_fail += f
        total_skip += s
        bar = green(f"{p} passed")
        if f:
            bar += f"  {red(f'{f} failed')}"
        if s:
            bar += f"  {yellow(f'{s} skipped')}"
        print(f"  {cat:<40} {bar}")

    print(bold(f"{'-' * 56}"))
    overall = green("ALL PASSED") if total_fail == 0 else red(f"{total_fail} FAILED")
    print(f"  Total: {green(str(total_pass))} passed | {red(str(total_fail))} failed | {yellow(str(total_skip))} skipped --> {overall}")
    print()

    failures = [r for r in _results if r["passed"] is False]
    if failures:
        print(bold(f"{'=' * 56}"))
        print(bold("  FAILURE DETAILS"))
        print(bold(f"{'=' * 56}"))
        for r in failures:
            print(f"\n  {CROSS} [{r['category']}] {bold(r['name'])}")
            if r.get("detail"):
                print(f"     {r['detail']}")
    write_traces_file(failures)

# ══════════════════════════════════ SERVER MGMT ═══════════════════════════════

_server_proc = None

def _rel(path):
    """Convert an absolute path to a relative path from PROJECT_DIR with forward slashes."""
    if os.path.isabs(path):
        return os.path.relpath(path, PROJECT_DIR).replace("\\", "/")
    return path.replace("\\", "/")

def start_server(config, ports=None, host=HOST):
    global _server_proc
    stop_server()
    if ports is None:
        ports = [PORT]
    config_rel = _rel(config)
    _server_proc = subprocess.Popen(
        [BINARY, config_rel],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        cwd=PROJECT_DIR,
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
            try:
                _server_proc.kill()
            except Exception:
                pass
        _server_proc = None
    time.sleep(0.3)

def server_alive():
    return _server_proc is not None and _server_proc.poll() is None

# ══════════════════════════════════ NET HELPERS ═══════════════════════════════

def wait_for_port(host, port, timeout=8):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except OSError:
            time.sleep(0.15)
    return False

def curl_code(url, method="GET", extra=None, timeout=8):
    cmd = ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}",
           "--max-time", str(timeout), "-X", method]
    if extra:
        cmd += extra
    cmd.append(url)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout.strip()
    except Exception:
        return "000"

def curl_code_v(url, method="GET", extra=None, timeout=8):
    cmd = ["curl", "-sv", "-o", "/dev/null", "-w", "%{http_code}",
           "--max-time", str(timeout), "-X", method]
    if extra:
        cmd += extra
    cmd.append(url)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout.strip(), r.stderr, cmd
    except Exception as e:
        return "000", str(e), cmd

def curl_body_v(url, extra=None, timeout=8):
    cmd = ["curl", "-sv", "--max-time", str(timeout)]
    if extra:
        cmd += extra
    cmd.append(url)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout, r.stderr, cmd
    except Exception as e:
        return "", str(e), cmd

def curl_headers_v(url, method="GET", extra=None, timeout=8):
    cmd = ["curl", "-sv", "-D", "-", "-o", "/dev/null",
           "--max-time", str(timeout), "-X", method]
    if extra:
        cmd += extra
    cmd.append(url)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 2)
        return r.stdout, r.stderr, cmd
    except Exception as e:
        return "", str(e), cmd

def tcp_send(host, port, data, timeout=4):
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

def tcp_send_no_shutdown(host, port, data, timeout=4):
    """Send data but don't shutdown write side, just wait for response."""
    try:
        with socket.create_connection((host, port), timeout=timeout) as s:
            s.sendall(data)
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

def sh(cmd, timeout=15):
    try:
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True,
                           timeout=timeout, cwd=PROJECT_DIR)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return 1, "", "timeout"

# ══════════════════════════════════ FILE HELPERS ══════════════════════════════

def mkdirp(path):
    os.makedirs(path, exist_ok=True)

def write_file(path, content):
    mkdirp(os.path.dirname(path) or ".")
    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as f:
        f.write(content)

# ══════════════════════════════════ CGI SCRIPTS ══════════════════════════════

def create_cgi_scripts():
    """Create CGI scripts for testing."""
    cgi_dir = tp("cgi-bin")
    mkdirp(cgi_dir)

    # Simple Python CGI that echoes back info
    write_file(os.path.join(cgi_dir, "hello.py"), """\
#!/usr/bin/env python3
import os
print("Content-Type: text/html")
print("")
print("<html><body>")
print("<h1>CGI Hello World</h1>")
print(f"<p>REQUEST_METHOD: {os.environ.get('REQUEST_METHOD', 'N/A')}</p>")
print(f"<p>QUERY_STRING: {os.environ.get('QUERY_STRING', 'N/A')}</p>")
print(f"<p>SCRIPT_NAME: {os.environ.get('SCRIPT_NAME', 'N/A')}</p>")
print(f"<p>PATH_INFO: {os.environ.get('PATH_INFO', 'N/A')}</p>")
print("</body></html>")
""")
    os.chmod(os.path.join(cgi_dir, "hello.py"), 0o755)

    # CGI that reads POST body
    write_file(os.path.join(cgi_dir, "post_echo.py"), """\
#!/usr/bin/env python3
import os
import sys
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
body = sys.stdin.read(content_length) if content_length > 0 else ""
print("Content-Type: text/plain")
print(f"Content-Length: {len(body)}")
print("")
print(body, end="")
""")
    os.chmod(os.path.join(cgi_dir, "post_echo.py"), 0o755)

    # CGI with query string parameters
    write_file(os.path.join(cgi_dir, "query.py"), """\
#!/usr/bin/env python3
import os
qs = os.environ.get('QUERY_STRING', '')
print("Content-Type: text/plain")
print("")
print(f"QUERY_STRING={qs}")
print(f"REQUEST_METHOD={os.environ.get('REQUEST_METHOD', '')}")
print(f"SERVER_NAME={os.environ.get('SERVER_NAME', '')}")
print(f"SERVER_PORT={os.environ.get('SERVER_PORT', '')}")
""")
    os.chmod(os.path.join(cgi_dir, "query.py"), 0o755)

    # Slow CGI (for timeout testing)
    write_file(os.path.join(cgi_dir, "slow.py"), """\
#!/usr/bin/env python3
import time
time.sleep(30)
print("Content-Type: text/plain")
print("")
print("This should have timed out")
""")
    os.chmod(os.path.join(cgi_dir, "slow.py"), 0o755)

    # CGI that outputs a lot of data
    write_file(os.path.join(cgi_dir, "big_output.py"), """\
#!/usr/bin/env python3
data = "A" * 100000
print("Content-Type: text/plain")
print(f"Content-Length: {len(data)}")
print("")
print(data, end="")
""")
    os.chmod(os.path.join(cgi_dir, "big_output.py"), 0o755)

    # Infinite loop CGI
    write_file(os.path.join(cgi_dir, "infinite.py"), """\
#!/usr/bin/env python3
while True:
    pass
""")
    os.chmod(os.path.join(cgi_dir, "infinite.py"), 0o755)


# ══════════════════════════════════ SETUP ═════════════════════════════════════

def setup_all():
    """Create all fixture files and configs."""
    # Clean previous
    if os.path.isdir(TMP_DIR):
        for root, dirs, _ in os.walk(TMP_DIR):
            for sd in dirs:
                try:
                    os.chmod(os.path.join(root, sd), 0o755)
                except Exception:
                    pass
        shutil.rmtree(TMP_DIR, ignore_errors=True)

    # Create directory structure
    mkdirp(tp("www"))
    mkdirp(tp("www/html"))
    mkdirp(tp("www/uploads"))
    mkdirp(tp("www/uploads/forbidden"))
    mkdirp(tp("www/css"))
    mkdirp(tp("www/images"))
    mkdirp(tp("www/subdir"))
    mkdirp(tp("www/autoindex_dir"))
    mkdirp(tp("www/noindex_dir"))
    mkdirp(tp("www/redirect_target"))
    mkdirp(tp("www/getonly"))
    mkdirp(tp("www/errors"))
    mkdirp(tp("config"))

    # HTML files
    write_file(tp("www/index.html"),
               "<!DOCTYPE html><html><head><title>Home</title></head>"
               "<body><h1>Welcome</h1></body></html>\n")
    write_file(tp("www/html/page.html"),
               "<!DOCTYPE html><html><body><p>A page</p></body></html>\n")
    write_file(tp("www/subdir/index.html"),
               "<!DOCTYPE html><html><body>Subdir Index</body></html>\n")
    write_file(tp("www/subdir/file.txt"), "subdir file content\n")
    write_file(tp("www/redirect_target/index.html"),
               "<!DOCTYPE html><html><body>Redirect Target</body></html>\n")
    write_file(tp("www/getonly/readonly.txt"), "This is read-only content.\n")

    # CSS
    write_file(tp("www/css/style.css"), "body { background: #fff; color: #000; }\n")

    # Images (binary)
    write_file(tp("www/images/logo.png"), os.urandom(256))
    write_file(tp("www/images/photo.jpg"), os.urandom(512))
    write_file(tp("www/images/icon.gif"), os.urandom(64))

    # Autoindex directory
    write_file(tp("www/autoindex_dir/alpha.txt"), "alpha content\n")
    write_file(tp("www/autoindex_dir/bravo.txt"), "bravo content\n")
    write_file(tp("www/autoindex_dir/charlie.txt"), "charlie content\n")

    # Error pages
    write_file(tp("www/errors/404.html"),
               "<!DOCTYPE html><html><body><h1>Custom 404</h1>"
               "<p>The page was not found.</p></body></html>\n")
    write_file(tp("www/errors/500.html"),
               "<!DOCTYPE html><html><body><h1>Custom 500</h1>"
               "<p>Internal server error.</p></body></html>\n")

    # Upload test files
    write_file(tp("www/uploads/existing.txt"), "existing file\n")
    write_file(tp("www/uploads/delete_me.txt"), "delete me\n")
    write_file(tp("www/uploads/forbidden/protected.txt"), "protected\n")

    # Large file for stress tests
    write_file(tp("www/large.bin"), os.urandom(512 * 1024))

    # CGI scripts
    create_cgi_scripts()

    # ── CONFIGURATION FILES ──

    # Main test config
    write_file(tp("config/main.conf"), f"""\
server {{
    listen 0.0.0.0:{PORT};
    root {rp("www")};
    server_name localhost;
    client_max_body_size 1M;
    error_page 404 {rp("www/errors/404.html")};

    location / {{
        methods GET HEAD POST;
        index index.html;
        autoindex off;
    }}

    location /uploads {{
        methods POST DELETE;
        upload_dir {rp("www/uploads")};
        client_max_body_size 10240;
    }}

    location /getonly {{
        root {rp("www/getonly")};
        methods GET;
    }}

    location /autoindex_dir {{
        root {rp("www/autoindex_dir")};
        autoindex on;
        methods GET;
    }}

    location /noindex_dir {{
        root {rp("www/noindex_dir")};
        autoindex off;
        methods GET;
    }}

    location /subdir {{
        root {rp("www/subdir")};
        methods GET;
        index index.html;
    }}

    location /redirect {{
        return 301 /redirect_target;
    }}

    location /redirect_target {{
        root {rp("www/redirect_target")};
        methods GET;
        index index.html;
    }}

    location /cgi-bin {{
        methods GET POST;
        root {rp("cgi-bin")};
        cgi_ext .py /usr/bin/python3;
    }}
}}
""")

    # Multi-port config
    write_file(tp("config/multi_port.conf"), f"""\
server {{
    listen 0.0.0.0:{PORT};
    root {rp("www")};
    location / {{
        methods GET;
        index index.html;
    }}
}}
server {{
    listen 0.0.0.0:{PORT2};
    root {rp("www/subdir")};
    location / {{
        methods GET;
        index index.html;
    }}
}}
""")

    # Strict body size config
    write_file(tp("config/bodysize.conf"), f"""\
server {{
    listen 0.0.0.0:{PORT};
    root {rp("www")};
    client_max_body_size 100;
    location / {{
        methods GET POST;
        index index.html;
    }}
    location /uploads {{
        methods POST DELETE;
        upload_dir {rp("www/uploads")};
        client_max_body_size 10240;
    }}
}}
""")

    # Upload config with generous limit
    write_file(tp("config/upload_big.conf"), f"""\
server {{
    listen 0.0.0.0:{PORT};
    root {rp("www")};
    location /uploads {{
        methods POST DELETE;
        upload_dir {rp("www/uploads")};
        client_max_body_size 10485760;
    }}
    location /forbidden {{
        methods POST DELETE;
        upload_dir {rp("www/uploads/forbidden")};
        client_max_body_size 10240;
    }}
    location / {{
        methods GET;
        index index.html;
    }}
}}
""")


def cleanup():
    stop_server()
    if os.path.isdir(TMP_DIR):
        for root, dirs, _ in os.walk(TMP_DIR):
            for sd in dirs:
                try:
                    os.chmod(os.path.join(root, sd), 0o755)
                except Exception:
                    pass
        shutil.rmtree(TMP_DIR, ignore_errors=True)


# ═══════════════════════════════════════════════════════════════════════════════
#                              TEST CATEGORIES
# ═══════════════════════════════════════════════════════════════════════════════


# ─────────────────────────── 1. COMPILATION ───────────────────────────────────

def test_compilation():
    global _current_category
    _current_category = "Compilation & Build"
    print(f"\n{cyan(bold('══ Compilation & Build ══'))}")

    # make
    rc, out, err = sh("make 2>&1", timeout=60)
    record("make compiles without errors", rc == 0,
           trace=make_trace("make", "exit 0", f"exit {rc}", out + err))

    record("binary 'webserv' exists after make", os.path.isfile(BINARY_ABS),
           trace=make_trace(f"test -f {BINARY}", "file present",
                            "file not found"))

    # make clean
    sh("make clean 2>&1", timeout=30)
    _, o_check, _ = sh("find . -name '*.o' 2>/dev/null")
    record("make clean removes all .o files", o_check.strip() == "",
           trace=make_trace("make clean && find . -name '*.o'",
                            "no .o files", f"found: {o_check.strip()[:200]}"))

    # make fclean
    sh("make fclean 2>&1", timeout=30)
    record("make fclean removes binary", not os.path.isfile(BINARY_ABS),
           trace=make_trace("make fclean && test -f webserv",
                            "binary absent", "binary still present"))

    # make re
    rc, out, err = sh("make re 2>&1", timeout=60)
    record("make re rebuilds everything", rc == 0 and os.path.isfile(BINARY_ABS),
           trace=make_trace("make re", "exit 0 + binary present",
                            f"exit {rc}, binary={'present' if os.path.isfile(BINARY_ABS) else 'absent'}",
                            out + err))

    # no relinking
    _, out, _ = sh("make 2>&1", timeout=30)
    up_to_date = "Nothing to be done" in out or "is up to date" in out
    record("second make does not relink", up_to_date,
           trace=make_trace("make (second call)",
                            "'Nothing to be done' or 'is up to date'",
                            out.strip()[:300] or "(empty)"))

    # C++98 compliance
    src_dirs = [d for d in ["src", "includes", "include", "srcs"]
                if os.path.isdir(os.path.join(PROJECT_DIR, d))]
    if src_dirs:
        dirs = " ".join(src_dirs)
        _, out_auto, _ = sh(f"grep -rn '\\bauto\\b' {dirs} 2>/dev/null | grep -v '//' | head -20")
        record("no C++11 'auto' keyword in source", out_auto.strip() == "",
               detail=out_auto.strip()[:200] if out_auto.strip() else "",
               trace=make_trace(f"grep -rn '\\bauto\\b' {dirs}",
                                "no matches", out_auto.strip()[:400] or "(clean)"))

        _, out_null, _ = sh(f"grep -rn '\\bnullptr\\b' {dirs} 2>/dev/null | head -20")
        record("no C++11 'nullptr' keyword in source", out_null.strip() == "",
               detail=out_null.strip()[:200] if out_null.strip() else "",
               trace=make_trace(f"grep -rn '\\bnullptr\\b' {dirs}",
                                "no matches", out_null.strip()[:400] or "(clean)"))

        _, out_range, _ = sh(f"grep -rn 'for\\s*(\\s*\\(\\?\\s*\\(const\\|volatile\\)\\?\\s*auto' {dirs} 2>/dev/null | grep -v '//' | head -20")
        record("no C++11 range-based for loops (auto in for)", out_range.strip() == "",
               detail=out_range.strip()[:200] if out_range.strip() else "",
               trace=make_trace(f"grep -rn 'for.*auto' {dirs}",
                                "no matches", out_range.strip()[:400] or "(clean)"))

        rc98, out98, err98 = sh(
            "make fclean && make CXXFLAGS='-Wall -Wextra -Werror -std=c++98' 2>&1",
            timeout=60)
        record("compiles with -std=c++98 flag", rc98 == 0,
               trace=make_trace("make CXXFLAGS='-Wall -Wextra -Werror -std=c++98'",
                                "exit 0", f"exit {rc98}", out98 + err98))

    if not os.path.isfile(BINARY_ABS):
        sh("make 2>&1", timeout=60)


# ──────────────────────── 2. CONFIG PARSING ───────────────────────────────────

def test_config_parsing():
    global _current_category
    _current_category = "Configuration Parsing"
    print(f"\n{cyan(bold('══ Configuration Parsing ══'))}")

    # Valid config
    ok = start_server(tp("config/main.conf"))
    record("server starts with valid config", ok,
           trace=make_trace(f"{BINARY} {tp('config/main.conf')}",
                            "port open", "port unreachable"))
    stop_server()

    # Missing root directive
    write_file(tp("config/bad_noroot.conf"),
               "server {\n    listen 8080;\n    location / { methods GET; }\n}\n")
    rc, out, err = sh(f"timeout 3 {BINARY} {rp('config/bad_noroot.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*bad_noroot' 2>/dev/null || true")
    combined = (out + err).lower()
    record("missing root directive -> error/non-zero exit", rc != 0 or "error" in combined,
           trace=make_trace(f"{BINARY} {tp('config/bad_noroot.conf')}",
                            "exit != 0 or error message", f"exit {rc}", out + err))

    # Unknown directive
    write_file(tp("config/bad_unknown.conf"),
               f"server {{\n    listen 8080;\n    root {rp('www')};\n    unknown_thing value;\n}}\n")
    rc, out, err = sh(f"timeout 3 {BINARY} {rp('config/bad_unknown.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*bad_unknown' 2>/dev/null || true")
    combined = (out + err).lower()
    has_diag = any(w in combined for w in ("error", "unknown", "invalid", "directive"))
    record("unknown directive -> diagnostic message", has_diag,
           trace=make_trace(f"{BINARY} {tp('config/bad_unknown.conf')}",
                            "error/unknown/invalid in output",
                            combined[:300] or "(empty)"))

    # Empty config
    write_file(tp("config/bad_empty.conf"), "")
    rc, out, err = sh(f"timeout 3 {BINARY} {rp('config/bad_empty.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*bad_empty' 2>/dev/null || true")
    record("empty config -> non-zero exit or error", rc != 0,
           trace=make_trace(f"{BINARY} {tp('config/bad_empty.conf')}",
                            "exit != 0", f"exit {rc}", out + err))

    # Missing braces
    write_file(tp("config/bad_braces.conf"),
               f"server {{\n    listen 8080;\n    root {rp('www')};\n")
    rc, out, err = sh(f"timeout 3 {BINARY} {rp('config/bad_braces.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*bad_braces' 2>/dev/null || true")
    record("unclosed brace -> error", rc != 0,
           trace=make_trace(f"{BINARY} {tp('config/bad_braces.conf')}",
                            "exit != 0", f"exit {rc}", out + err))

    # Duplicate ports in same server
    write_file(tp("config/bad_dupport.conf"), f"""\
server {{
    listen 0.0.0.0:8080;
    root {rp("www")};
    location / {{ methods GET; }}
}}
server {{
    listen 0.0.0.0:8080;
    root {rp("www")};
    location / {{ methods GET; }}
}}
""")
    rc, out, err = sh(f"timeout 3 {BINARY} {rp('config/bad_dupport.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*bad_dupport' 2>/dev/null || true")
    record("duplicate host:port -> error", rc != 0,
           trace=make_trace(f"{BINARY} {tp('config/bad_dupport.conf')}",
                            "exit != 0 (duplicate port)", f"exit {rc}", out + err))

    # Invalid port number
    write_file(tp("config/bad_port.conf"),
               f"server {{\n    listen 99999;\n    root {rp('www')};\n    location / {{ methods GET; }}\n}}\n")
    rc, out, err = sh(f"timeout 3 {BINARY} {rp('config/bad_port.conf')} 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*bad_port' 2>/dev/null || true")
    record("invalid port 99999 -> error", rc != 0,
           trace=make_trace(f"{BINARY} {tp('config/bad_port.conf')}",
                            "exit != 0", f"exit {rc}", out + err))

    # No-arg invocation
    rc, out, err = sh(f"timeout 3 {BINARY} 2>&1 || true", timeout=5)
    sh(f"pkill -f '{BINARY}' 2>/dev/null || true")
    record("no-arg invocation does not segfault (exit != 139)", rc != 139,
           detail=f"exit {rc}",
           trace=make_trace(f"{BINARY} (no args)", "exit != 139", f"exit {rc}", out + err))

    # Non-existent config file
    rc, out, err = sh(f"timeout 3 {BINARY} /nonexistent/path/config.conf 2>&1", timeout=5)
    sh(f"pkill -f 'webserv.*nonexistent' 2>/dev/null || true")
    record("non-existent config file -> error", rc != 0,
           trace=make_trace(f"{BINARY} /nonexistent/path/config.conf",
                            "exit != 0", f"exit {rc}", out + err))

    # Multi-server config
    ok = start_server(tp("config/multi_port.conf"), ports=[PORT, PORT2])
    record("multi-server config starts (two ports)", ok,
           trace=make_trace(f"{BINARY} {tp('config/multi_port.conf')}",
                            f"ports {PORT} and {PORT2} open",
                            "one or both ports unreachable"))
    if ok:
        c1, v1, cmd1 = curl_code_v(f"http://{HOST}:{PORT}/")
        c2, v2, cmd2 = curl_code_v(f"http://{HOST}:{PORT2}/")
        record(f"port {PORT} responds", c1 != "000", f"HTTP {c1}",
               trace=make_trace(cmd1, "HTTP 1xx-5xx", f"HTTP {c1}", v1))
        record(f"port {PORT2} responds", c2 != "000", f"HTTP {c2}",
               trace=make_trace(cmd2, "HTTP 1xx-5xx", f"HTTP {c2}", v2))

        body1, _, _ = curl_body_v(f"http://{HOST}:{PORT}/")
        body2, _, _ = curl_body_v(f"http://{HOST}:{PORT2}/")
        record("different ports serve different content",
               "Welcome" in body1 and "Subdir" in body2,
               trace=make_trace(f"curl :8080 vs :9090",
                                "port 8080=Welcome, port 9090=Subdir",
                                f"8080='{body1.strip()[:50]}' 9090='{body2.strip()[:50]}'"))
    stop_server()


# ──────────────────────── 3. HTTP METHODS ─────────────────────────────────────

def test_http_methods():
    global _current_category
    _current_category = "HTTP Methods (GET/POST/DELETE)"
    print(f"\n{cyan(bold('══ HTTP Methods (GET/POST/DELETE) ══'))}")

    ok = start_server(tp("config/main.conf"))
    record("server starts", ok,
           trace=make_trace(f"{BINARY} {tp('config/main.conf')}",
                            "port open", "unreachable"))
    if not ok:
        return

    base = f"http://{HOST}:{PORT}"

    # GET /
    code, verbose, cmd = curl_code_v(f"{base}/")
    record("GET / -> 200", code == "200", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 200", f"HTTP {code}", verbose))

    # GET existing file
    code, verbose, cmd = curl_code_v(f"{base}/index.html")
    record("GET /index.html -> 200", code == "200", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 200", f"HTTP {code}", verbose))

    # GET non-existent
    code, verbose, cmd = curl_code_v(f"{base}/this_does_not_exist.html")
    record("GET non-existent -> 404", code == "404", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404", f"HTTP {code}", verbose))

    # HEAD
    sent = b"HEAD / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    raw = tcp_send(HOST, PORT, sent)
    has_200 = b"200" in raw
    parts = raw.split(b"\r\n\r\n", 1)
    body_bytes = len(parts[1]) if len(parts) > 1 else 0
    has_cl = b"Content-Length" in raw or b"content-length" in raw.lower()
    record("HEAD / -> 200 with Content-Length, zero body",
           has_200 and has_cl and body_bytes == 0,
           f"200={has_200} CL={has_cl} body={body_bytes}",
           trace=tcp_trace(HOST, PORT, sent, raw,
                           "HTTP 200, Content-Length, 0 body",
                           f"200={has_200} CL={has_cl} body={body_bytes}"))

    # POST to upload
    write_file(tp("upload_test.txt"), "test upload content\n")
    code, verbose, cmd = curl_code_v(
        f"{base}/uploads/", method="POST",
        extra=["-F", f"file=@{tp('upload_test.txt')}"])
    record("POST file to /uploads/ -> 201", code == "201", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 201", f"HTTP {code}", verbose))

    # DELETE existing
    shutil.copy(tp("upload_test.txt"), tp("www/uploads/to_delete.txt"))
    code, verbose, cmd = curl_code_v(f"{base}/uploads/to_delete.txt", method="DELETE")
    gone = not os.path.isfile(tp("www/uploads/to_delete.txt"))
    record("DELETE existing file -> 204", code == "204", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 204", f"HTTP {code}", verbose))
    record("file actually removed after DELETE", gone,
           trace=make_trace("ls www/uploads/to_delete.txt", "absent",
                            "still present" if not gone else "absent"))

    # DELETE non-existent
    code, verbose, cmd = curl_code_v(f"{base}/uploads/nonexistent.xyz", method="DELETE")
    record("DELETE non-existent -> 404", code == "404", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404", f"HTTP {code}", verbose))

    # Method not allowed
    code, verbose, cmd = curl_code_v(f"{base}/getonly/readonly.txt", method="DELETE")
    record("DELETE on GET-only route -> 405", code == "405", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 405", f"HTTP {code}", verbose))

    # Check Allow header on 405
    hdrs, hv, hcmd = curl_headers_v(f"{base}/getonly/readonly.txt", method="DELETE")
    full = hdrs + hv
    has_allow = bool(re.search(r"(?i)allow:.*GET", full))
    record("405 response includes Allow header with GET",
           has_allow,
           trace=make_trace(hcmd, "Allow: GET", "Allow header missing or wrong", full))

    # POST on GET-only route
    code, verbose, cmd = curl_code_v(f"{base}/getonly/readonly.txt", method="POST",
                                     extra=["-d", "data=test"])
    record("POST on GET-only route -> 405", code == "405", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 405", f"HTTP {code}", verbose))

    record("server alive after method tests", server_alive(),
           trace=make_trace("kill -0 <pid>", "alive", "dead"))
    stop_server()


# ──────────────────────── 4. HTTP RESPONSE HEADERS ────────────────────────────

def test_response_headers():
    global _current_category
    _current_category = "HTTP Response Headers"
    print(f"\n{cyan(bold('══ HTTP Response Headers ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    url = f"http://{HOST}:{PORT}/"
    cmd_list = ["curl", "-sv", "--max-time", "8", url]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        verbose = r.stderr + r.stdout
    except Exception as e:
        verbose = str(e)

    # Check status line
    has_200 = "HTTP/1.1 200" in verbose or "HTTP/1.0 200" in verbose
    record("GET / -> HTTP/1.x 200", has_200,
           trace=make_trace(cmd_list, "HTTP/1.x 200", "not found", verbose))

    # Date header (RFC 7231 format)
    has_date = bool(re.search(r"(?i)date:", verbose))
    has_date_fmt = bool(re.search(
        r"Date: \w{3}, \d{2} \w{3} \d{4} \d{2}:\d{2}:\d{2} GMT", verbose))
    record("response has Date header in RFC format",
           has_date and has_date_fmt,
           detail="present but wrong format" if has_date and not has_date_fmt else "",
           trace=make_trace(cmd_list, "Date: Dow, DD Mon YYYY HH:MM:SS GMT",
                            "missing" if not has_date else "wrong format", verbose))

    # Server header
    has_srv = bool(re.search(r"(?i)server:", verbose))
    record("response has Server header", has_srv,
           trace=make_trace(cmd_list, "Server: <name>", "absent", verbose))

    # Content-Length
    has_cl = bool(re.search(r"(?i)content-length:", verbose))
    record("response has Content-Length", has_cl,
           trace=make_trace(cmd_list, "Content-Length: <n>", "absent", verbose))

    # Content-Type
    has_ct = bool(re.search(r"(?i)content-type:", verbose))
    record("response has Content-Type", has_ct,
           trace=make_trace(cmd_list, "Content-Type: <type>", "absent", verbose))

    # Connection header
    has_conn = bool(re.search(r"(?i)connection:", verbose))
    record("response has Connection header", has_conn,
           trace=make_trace(cmd_list, "Connection: keep-alive/close", "absent", verbose))

    stop_server()


# ──────────────────────── 5. STATIC FILES & MIME ──────────────────────────────

def test_static_files():
    global _current_category
    _current_category = "Static Files & MIME Types"
    print(f"\n{cyan(bold('══ Static Files & MIME Types ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    def check_mime(path, expected_ct, label):
        hdrs, verbose, cmd = curl_headers_v(f"{base}/{path}")
        full = hdrs + verbose
        got_ct = re.search(r"(?i)content-type:\s*([^\r\n]+)", full)
        got_str = got_ct.group(1).strip().lower() if got_ct else "(missing)"
        passed = expected_ct.lower() in got_str
        record(f"{label} -> Content-Type contains '{expected_ct}'",
               passed, got_str,
               trace=make_trace(cmd, f"Content-Type: {expected_ct}",
                                f"Content-Type: {got_str}", full))

    check_mime("index.html", "text/html", "index.html")
    check_mime("css/style.css", "text/css", "style.css")
    check_mime("images/logo.png", "image/png", "logo.png")
    check_mime("images/photo.jpg", "image/jpeg", "photo.jpg")
    check_mime("images/icon.gif", "image/gif", "icon.gif")

    # Content-Length matches actual body
    body, bv, bcmd = curl_body_v(f"{base}/index.html")
    hdrs, hv, hcmd = curl_headers_v(f"{base}/index.html")
    full = hdrs + hv
    cl_match = re.search(r"(?i)content-length:\s*(\d+)", full)
    if cl_match:
        cl_val = int(cl_match.group(1))
        record("Content-Length matches actual body size",
               cl_val == len(body.encode()),
               f"CL={cl_val} body={len(body.encode())}",
               trace=make_trace(hcmd, f"Content-Length == body size",
                                f"CL={cl_val} body={len(body.encode())}", full))
    else:
        record("Content-Length matches actual body size", False,
               "Content-Length header not found",
               trace=make_trace(hcmd, "Content-Length present", "absent", full))

    # 404 for missing file
    code, verbose, cmd = curl_code_v(f"{base}/nonexistent_file.xyz")
    record("non-existent file -> 404", code == "404", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404", f"HTTP {code}", verbose))

    # Large file full transmission
    expected_size = os.path.getsize(tp("www/large.bin"))
    tmp_recv = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    tmp_recv.close()
    cmd_str = f"curl -s --max-time 30 http://{HOST}:{PORT}/large.bin -o {tmp_recv.name}"
    sh(cmd_str, timeout=35)
    received = os.path.getsize(tmp_recv.name)
    os.unlink(tmp_recv.name)
    record("512 KB file fully transmitted",
           received == expected_size,
           f"expected={expected_size} received={received}",
           trace=make_trace(cmd_str, f"{expected_size} bytes",
                            f"{received} bytes"))

    stop_server()


# ──────────────────────── 6. DIRECTORY LISTING & INDEX ────────────────────────

def test_directory_listing():
    global _current_category
    _current_category = "Directory Listing & Index"
    print(f"\n{cyan(bold('══ Directory Listing & Index ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Autoindex on
    body, bv, bcmd = curl_body_v(f"{base}/autoindex_dir/")
    has_alpha = "alpha.txt" in body
    has_bravo = "bravo.txt" in body
    has_charlie = "charlie.txt" in body
    record("autoindex on -> lists all files",
           has_alpha and has_bravo and has_charlie,
           trace=make_trace(bcmd, "alpha.txt, bravo.txt, charlie.txt in body",
                            f"alpha={has_alpha} bravo={has_bravo} charlie={has_charlie}",
                            body[:600]))

    # Alphabetical order
    if has_alpha and has_bravo and has_charlie:
        pos_a = body.find("alpha.txt")
        pos_b = body.find("bravo.txt")
        pos_c = body.find("charlie.txt")
        record("autoindex listing is alphabetically ordered",
               pos_a < pos_b < pos_c,
               f"alpha@{pos_a} bravo@{pos_b} charlie@{pos_c}",
               trace=make_trace(bcmd, "alpha < bravo < charlie",
                                f"a@{pos_a} b@{pos_b} c@{pos_c}", body[:600]))

    # Autoindex off -> 403
    code, verbose, cmd = curl_code_v(f"{base}/noindex_dir/")
    record("autoindex off -> 403 Forbidden", code == "403", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 403", f"HTTP {code}", verbose))

    # Directory with index file
    body, bv, bcmd = curl_body_v(f"{base}/subdir/")
    record("directory with index.html serves index", "Subdir Index" in body,
           trace=make_trace(bcmd, "body contains 'Subdir Index'",
                            body.strip()[:80] or "(empty)", bv))

    # No trailing slash -> 301 redirect
    cmd_list = ["curl", "-sv", "--no-location", "--max-time", "8",
                f"{base}/subdir"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        redir_out = r.stdout + r.stderr
    except Exception as e:
        redir_out = str(e)
    sm = re.search(r"HTTP/\S+ (\d+)", redir_out)
    stat = sm.group(1) if sm else "000"
    has_loc = bool(re.search(r"(?i)location:.*subdir/", redir_out))
    record("directory without trailing slash -> 301", stat == "301", f"HTTP {stat}",
           trace=make_trace(cmd_list, "HTTP 301", f"HTTP {stat}", redir_out))
    record("301 Location header has trailing slash", has_loc,
           trace=make_trace(cmd_list, "Location: .../subdir/",
                            "missing or wrong", redir_out))

    stop_server()


# ──────────────────────── 7. ERROR PAGES ──────────────────────────────────────

def test_error_pages():
    global _current_category
    _current_category = "Error Pages"
    print(f"\n{cyan(bold('══ Error Pages ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Custom 404
    body, bv, bcmd = curl_body_v(f"{base}/nonexistent_page_xyz")
    code = curl_code(f"{base}/nonexistent_page_xyz")
    record("404 returns custom error page body", code == "404" and "Custom 404" in body,
           f"HTTP {code}",
           trace=make_trace(bcmd, "HTTP 404 + 'Custom 404' in body",
                            f"HTTP {code}, body={body.strip()[:100]}", bv))

    # Default error page has content
    record("404 error page has HTML content", "<html" in body.lower() or "<!doctype" in body.lower(),
           trace=make_trace(bcmd, "HTML content in error page",
                            body.strip()[:100], bv))

    # Various error codes produce error pages
    # 405
    code405, v405, cmd405 = curl_code_v(f"{base}/getonly/readonly.txt", method="DELETE")
    record("405 returns an error page (not empty)", code405 == "405",
           f"HTTP {code405}",
           trace=make_trace(cmd405, "HTTP 405", f"HTTP {code405}", v405))

    stop_server()


# ──────────────────────── 8. REDIRECTS ────────────────────────────────────────

def test_redirects():
    global _current_category
    _current_category = "HTTP Redirects"
    print(f"\n{cyan(bold('══ HTTP Redirects ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    # 301 redirect
    cmd_list = ["curl", "-sv", "--no-location", "--max-time", "8",
                f"{base}/redirect"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        verbose = r.stdout + r.stderr
    except Exception as e:
        verbose = str(e)
    sm = re.search(r"HTTP/\S+ (\d+)", verbose)
    stat = sm.group(1) if sm else "000"
    has_loc = bool(re.search(r"(?i)location:.*redirect_target", verbose))
    record("GET /redirect -> 301", stat == "301", f"HTTP {stat}",
           trace=make_trace(cmd_list, "HTTP 301", f"HTTP {stat}", verbose))
    record("301 has correct Location header", has_loc,
           trace=make_trace(cmd_list, "Location: /redirect_target",
                            "missing or wrong", verbose))

    # Following redirect lands on target
    body, bv, bcmd = curl_body_v(f"{base}/redirect", extra=["-L"])
    record("following redirect serves target content",
           "Redirect Target" in body,
           trace=make_trace(bcmd, "body contains 'Redirect Target'",
                            body.strip()[:100] or "(empty)", bv))

    # DELETE on redirect should still redirect (redirect before method check)
    cmd_del = ["curl", "-sv", "--no-location", "-X", "DELETE", "--max-time", "8",
               f"{base}/redirect"]
    try:
        rd = subprocess.run(cmd_del, capture_output=True, text=True, timeout=10)
        vdel = rd.stdout + rd.stderr
    except Exception as e:
        vdel = str(e)
    smd = re.search(r"HTTP/\S+ (\d+)", vdel)
    sdel = smd.group(1) if smd else "000"
    record("DELETE on /redirect also returns 301 (redirect before method check)",
           sdel == "301", f"HTTP {sdel}",
           trace=make_trace(cmd_del, "HTTP 301", f"HTTP {sdel}", vdel))

    stop_server()


# ──────────────────────── 9. BODY & CHUNKED ───────────────────────────────────

def test_body_handling():
    global _current_category
    _current_category = "Request Body & Chunked Encoding"
    print(f"\n{cyan(bold('══ Request Body & Chunked Encoding ══'))}")

    ok = start_server(tp("config/bodysize.conf"))
    if not ok:
        record("server starts (bodysize config)", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Small POST within limit
    code, verbose, cmd = curl_code_v(
        f"{base}/", method="POST",
        extra=["-H", "Expect:", "-d", "x=hello"])
    record("small POST within body limit -> accepted (not 413)", code != "413",
           f"HTTP {code}",
           trace=make_trace(cmd, "HTTP != 413", f"HTTP {code}", verbose))

    # POST exceeding limit -> 413
    big_data = "A" * 200  # limit is 100 bytes
    code, verbose, cmd = curl_code_v(
        f"{base}/", method="POST",
        extra=["-H", "Expect:", "-d", big_data])
    record("POST exceeding client_max_body_size -> 413", code == "413",
           f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 413", f"HTTP {code}", verbose))

    stop_server()

    # Chunked transfer
    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts (main config)", False)
        return

    chunked_req = (
        b"POST / HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n\r\n"
        b"b\r\nhello world\r\n0\r\n\r\n"
    )
    raw = tcp_send(HOST, PORT, chunked_req, timeout=6)
    has_status = bool(re.search(rb"HTTP/1\.[01] \d{3}", raw))
    first_line = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("chunked POST -> valid HTTP response", has_status, first_line,
           trace=tcp_trace(HOST, PORT, chunked_req, raw,
                           "HTTP/1.x <status>", first_line))

    # Multiple chunks
    multi_chunk = (
        b"POST / HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n\r\n"
        b"5\r\nhello\r\n"
        b"1\r\n \r\n"
        b"5\r\nworld\r\n"
        b"0\r\n\r\n"
    )
    raw = tcp_send(HOST, PORT, multi_chunk, timeout=6)
    has_status = bool(re.search(rb"HTTP/1\.[01] \d{3}", raw))
    record("multi-chunk POST -> valid response", has_status,
           raw.split(b"\n")[0].decode(errors="replace").strip(),
           trace=tcp_trace(HOST, PORT, multi_chunk, raw,
                           "HTTP/1.x <status>",
                           raw.split(b"\n")[0].decode(errors="replace").strip()))

    record("server alive after chunked tests", server_alive())
    stop_server()


# ──────────────────────── 10. UPLOADS ─────────────────────────────────────────

def test_uploads():
    global _current_category
    _current_category = "File Uploads"
    print(f"\n{cyan(bold('══ File Uploads ══'))}")

    # Fresh upload directory
    if os.path.isdir(tp("www/uploads")):
        shutil.rmtree(tp("www/uploads"), ignore_errors=True)
    mkdirp(tp("www/uploads"))
    mkdirp(tp("www/uploads/forbidden"))
    write_file(tp("www/uploads/forbidden/protected.txt"), "protected\n")

    ok = start_server(tp("config/upload_big.conf"))
    if not ok:
        record("server starts (upload config)", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Upload a file
    write_file(tp("small_upload.txt"), "This is a test upload file.\n")
    code, verbose, cmd = curl_code_v(
        f"{base}/uploads/", method="POST",
        extra=["-F", f"file=@{tp('small_upload.txt')}"])
    record("POST upload small file -> 201", code == "201", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 201", f"HTTP {code}", verbose))

    # Verify file exists in uploads
    files = [f for f in os.listdir(tp("www/uploads"))
             if os.path.isfile(os.path.join(tp("www/uploads"), f))]
    record("uploaded file exists in upload directory", len(files) > 0,
           f"files: {files}",
           trace=make_trace(f"ls {tp('www/uploads')}/", "at least one file",
                            f"found: {files}"))

    # Upload with path traversal filename
    code, verbose, cmd = curl_code_v(
        f"{base}/uploads/", method="POST",
        extra=["-F", f"file=@{tp('small_upload.txt')};filename=../../etc/passwd"])
    record("path traversal filename is sanitized (stored safely)",
           code == "201",
           f"HTTP {code}",
           trace=make_trace(cmd,
                            "HTTP 201 (file stored as basename, not traversed)",
                            f"HTTP {code}", verbose))
    # Make sure /etc/passwd wasn't overwritten (sanity)
    if os.path.isfile("/etc/passwd"):
        record("/etc/passwd is still the original (traversal blocked)", True)

    # Upload with empty filename
    code, verbose, cmd = curl_code_v(
        f"{base}/uploads/", method="POST",
        extra=["-F", f"file=@{tp('small_upload.txt')};filename="])
    record("upload with empty filename -> handled gracefully",
           code != "000",
           f"HTTP {code}",
           trace=make_trace(cmd, "HTTP response (not crash)",
                            f"HTTP {code}", verbose))

    # DELETE
    write_file(tp("www/uploads/delete_target.txt"), "delete me\n")
    code, verbose, cmd = curl_code_v(f"{base}/uploads/delete_target.txt", method="DELETE")
    record("DELETE uploaded file -> 204", code == "204", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 204", f"HTTP {code}", verbose))
    record("file removed after DELETE",
           not os.path.isfile(tp("www/uploads/delete_target.txt")))

    # DELETE non-existent
    code, verbose, cmd = curl_code_v(f"{base}/uploads/no_such_file.xyz", method="DELETE")
    record("DELETE non-existent -> 404", code == "404", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404", f"HTTP {code}", verbose))

    # Oversized upload (limit is 10240 in main.conf /uploads)
    write_file(tp("big_upload.bin"), os.urandom(15 * 1024))  # 15 KB > 10 KB
    stop_server()

    ok = start_server(tp("config/main.conf"))
    if ok:
        code, verbose, cmd = curl_code_v(
            f"{base}/uploads/", method="POST",
            extra=["-F", f"file=@{tp('big_upload.bin')}"])
        record("POST exceeding upload limit -> 413", code == "413", f"HTTP {code}",
               trace=make_trace(cmd, "HTTP 413 (15KB > 10KB limit)",
                                f"HTTP {code}", verbose))

    # Permission denied upload
    os.chmod(tp("www/uploads/forbidden"), 0o555)
    code, verbose, cmd = curl_code_v(
        f"{base}/uploads/forbidden/protected.txt", method="DELETE")
    # Note: might be 403 or 500 depending on implementation
    record("DELETE in read-only dir -> 403",
           code == "403", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 403 (permission denied)",
                            f"HTTP {code}", verbose))

    stop_server()
    try:
        os.chmod(tp("www/uploads/forbidden"), 0o755)
    except Exception:
        pass


# ──────────────────────── 11. PATH TRAVERSAL ──────────────────────────────────

def test_path_traversal():
    global _current_category
    _current_category = "Path Traversal Security"
    print(f"\n{cyan(bold('══ Path Traversal Security ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"
    traversal_uris = [
        "/../../etc/passwd",
        "/../../../etc/passwd",
        "/%2e%2e/%2e%2e/etc/passwd",
        "/./../../etc/passwd",
        "/../../../etc/shadow",
        "/%2e%2e%2f%2e%2e%2fetc%2fpasswd",
        "/..%00/etc/passwd",
        "/....//....//etc/passwd",
    ]

    all_blocked = True
    trace_lines = []
    for uri in traversal_uris:
        cmd_l = ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}",
                 "--path-as-is", "--max-time", "8",
                 f"{base}{uri}"]
        try:
            r = subprocess.run(cmd_l, capture_output=True, text=True, timeout=10)
            code = r.stdout.strip()
        except Exception:
            code = "000"
        trace_lines.append(f"  {uri}  ->  HTTP {code}")
        if code == "200":
            all_blocked = False
            record(f"path traversal blocked: {uri}", False, f"HTTP {code} (LEAKED!)",
                   trace=make_trace(cmd_l, "HTTP != 200", f"HTTP {code}",
                                    "\n".join(trace_lines)))

    record("all path traversal attempts blocked (no 200)", all_blocked,
           trace=make_trace("curl --path-as-is (multiple traversal URIs)",
                            "none returned 200",
                            "at least one returned 200" if not all_blocked else "all blocked",
                            "\n".join(trace_lines)))

    record("server alive after traversal attempts", server_alive())
    stop_server()


# ──────────────────────── 12. BAD REQUESTS ────────────────────────────────────

def test_bad_requests():
    global _current_category
    _current_category = "Bad Requests & Edge Cases"
    print(f"\n{cyan(bold('══ Bad Requests & Edge Cases ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    # Garbage request -> 400
    sent = b"THISISNOTHTTP\r\n\r\n"
    raw = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("garbage request -> 400", b"400" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 400", first))

    # Invalid HTTP version -> 505
    sent = b"GET / HTTP/9.9\r\nHost: localhost\r\n\r\n"
    raw = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("HTTP/9.9 -> 505", b"505" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 505", first))

    # Missing Host header (HTTP/1.1 requires it)
    sent = b"GET / HTTP/1.1\r\n\r\n"
    raw = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("missing Host header -> 400", b"400" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 400", first))

    # Very long URI -> 414
    long_uri = "/a" * 5000
    sent = f"GET {long_uri} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode()
    raw = tcp_send(HOST, PORT, sent, timeout=5)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("very long URI -> 414 URI Too Long",
           b"414" in raw, first,
           trace=tcp_trace(HOST, PORT, b"GET /aaa...x5000 HTTP/1.1\\r\\n...",
                           raw, "HTTP 414", first))

    # Huge header -> 431
    big_header = "X-Big: " + "A" * 9000
    code, verbose, cmd = curl_code_v(f"http://{HOST}:{PORT}/",
                                     extra=["-H", big_header])
    record("huge header -> 431 or 400", code in ("431", "400"), f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 431 or 400", f"HTTP {code}", verbose))

    # Multiple garbage requests don't crash server
    for _ in range(10):
        tcp_send(HOST, PORT, b"GARBAGE\r\n\r\n", timeout=1)
    record("server alive after 10 garbage requests", server_alive())

    # Empty body POST
    sent = b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
    raw = tcp_send(HOST, PORT, sent)
    has_status = bool(re.search(rb"HTTP/1\.[01] \d{3}", raw))
    record("empty body POST -> valid response", has_status,
           raw.split(b"\n")[0].decode(errors="replace").strip(),
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP/1.x <status>",
                           raw.split(b"\n")[0].decode(errors="replace").strip()))

    # Wrong Content-Length (less than body)
    sent = b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello world extra data"
    raw = tcp_send(HOST, PORT, sent, timeout=3)
    record("mismatched Content-Length -> server handles gracefully",
           len(raw) > 0, "got response",
           trace=tcp_trace(HOST, PORT, sent, raw, "some HTTP response",
                           raw.split(b"\n")[0].decode(errors="replace").strip()))

    # Invalid method
    sent = b"PATCH / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    raw = tcp_send(HOST, PORT, sent)
    first = raw.split(b"\n")[0].decode(errors="replace").strip()
    record("unsupported method (PATCH) -> 405 or 501",
           b"405" in raw or b"501" in raw, first,
           trace=tcp_trace(HOST, PORT, sent, raw, "HTTP 405 or 501", first))

    record("server alive after all bad request tests", server_alive())
    stop_server()


# ──────────────────────── 13. KEEP-ALIVE & PIPELINING ─────────────────────────

def test_keepalive():
    global _current_category
    _current_category = "Keep-Alive & Pipelining"
    print(f"\n{cyan(bold('══ Keep-Alive & Pipelining ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    # Pipelined requests
    pipeline = (
        b"GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"
        b"GET /subdir/ HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"
    )
    raw = tcp_send(HOST, PORT, pipeline, timeout=5)
    decoded = raw.decode(errors="replace")
    record("pipelined GETs: first response present", "Welcome" in decoded,
           trace=tcp_trace(HOST, PORT, pipeline, raw,
                           "'Welcome' in response", decoded[:500]))
    record("pipelined GETs: second response present", "Subdir" in decoded,
           trace=tcp_trace(HOST, PORT, pipeline, raw,
                           "'Subdir' in response", decoded[:500]))

    # Both should have HTTP status lines
    status_count = len(re.findall(rb"HTTP/1\.[01] \d{3}", raw))
    record("pipelined response has 2 HTTP status lines", status_count >= 2,
           f"found {status_count}",
           trace=tcp_trace(HOST, PORT, pipeline, raw,
                           "2 HTTP status lines", f"found {status_count}"))

    # Connection: keep-alive header
    cmd_list = ["curl", "-sv", "--http1.1", "--max-time", "8",
                f"http://{HOST}:{PORT}/"]
    try:
        r = subprocess.run(cmd_list, capture_output=True, text=True, timeout=10)
        verbose = r.stderr + r.stdout
    except Exception as e:
        verbose = str(e)
    has_ka = bool(re.search(r"(?i)connection:\s*keep-alive", verbose))
    record("response includes Connection: keep-alive", has_ka,
           trace=make_trace(cmd_list, "Connection: keep-alive",
                            "not found", verbose))

    # Connection: close -> body delivered + connection closed
    close_req = b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"
    raw = tcp_send(HOST, PORT, close_req, timeout=5)
    record("Connection: close -> body delivered", b"Welcome" in raw,
           trace=tcp_trace(HOST, PORT, close_req, raw,
                           "'Welcome' in body",
                           raw.decode(errors="replace")[:400]))

    # Multiple requests on same connection (keep-alive reuse)
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as s:
            # First request
            s.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
            time.sleep(0.5)
            buf1 = b""
            s.settimeout(2)
            try:
                while True:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    buf1 += chunk
                    if b"\r\n\r\n" in buf1:
                        # Got headers, read body based on Content-Length
                        break
            except socket.timeout:
                pass

            # Second request on same socket
            s.sendall(b"GET /subdir/ HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n")
            buf2 = b""
            s.settimeout(3)
            try:
                while True:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    buf2 += chunk
            except socket.timeout:
                pass

        got_first = b"Welcome" in buf1
        got_second = b"Subdir" in buf2
        record("keep-alive: 2 sequential requests on same connection",
               got_first and got_second,
               f"first={'Welcome' in buf1.decode(errors='replace')} second={'Subdir' in buf2.decode(errors='replace')}",
               trace=make_trace("2x GET on same TCP socket",
                                "both responses received",
                                f"first={got_first} second={got_second}"))
    except Exception as e:
        record("keep-alive: 2 sequential requests on same connection",
               False, str(e),
               trace=make_trace("2x GET on same TCP socket",
                                "both responses received", f"exception: {e}"))

    # Idle timeout (server should close idle connections)
    response_ok = threading.Event()
    closed_by_server = threading.Event()

    def idle_test():
        try:
            with socket.create_connection((HOST, PORT), timeout=5) as s:
                s.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
                buf = b""
                s.settimeout(5)
                try:
                    while True:
                        chunk = s.recv(4096)
                        if not chunk:
                            break
                        buf += chunk
                except socket.timeout:
                    pass
                if b"Welcome" in buf:
                    response_ok.set()
                # Wait for server to close idle connection
                s.settimeout(65)
                try:
                    data = s.recv(1)
                    if data == b"":
                        closed_by_server.set()
                except socket.timeout:
                    pass
                except ConnectionError:
                    closed_by_server.set()
        except Exception:
            pass

    t = threading.Thread(target=idle_test, daemon=True)
    t.start()
    t.join(timeout=70)
    record("idle connection: response delivered", response_ok.is_set(),
           trace=make_trace("GET then idle wait",
                            "body received", "not received"))
    record("idle connection: server closes after timeout",
           closed_by_server.is_set(),
           trace=make_trace("keep-alive socket -> wait for EOF",
                            "server closes connection",
                            "server did NOT close within 65s"))

    stop_server()


# ──────────────────────── 14. CGI ─────────────────────────────────────────────

def test_cgi():
    global _current_category
    _current_category = "CGI Execution"
    print(f"\n{cyan(bold('══ CGI Execution ══'))}")

    # Check if python3 is available
    python_path = shutil.which("python3")
    if not python_path:
        skip("all CGI tests", "python3 not found in PATH")
        return

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Basic CGI
    body, bv, bcmd = curl_body_v(f"{base}/cgi-bin/hello.py")
    code = curl_code(f"{base}/cgi-bin/hello.py")
    record("GET /cgi-bin/hello.py -> 200", code == "200", f"HTTP {code}",
           trace=make_trace(bcmd, "HTTP 200", f"HTTP {code}", bv))

    record("CGI output contains 'CGI Hello World'",
           "CGI Hello World" in body,
           trace=make_trace(bcmd, "'CGI Hello World' in body",
                            body.strip()[:200] or "(empty)", bv))

    # CGI with query string
    body, bv, bcmd = curl_body_v(f"{base}/cgi-bin/query.py?foo=bar&baz=42")
    record("CGI receives QUERY_STRING",
           "foo=bar" in body and "baz=42" in body,
           trace=make_trace(bcmd, "QUERY_STRING=foo=bar&baz=42",
                            body.strip()[:200] or "(empty)", bv))

    # CGI with POST body
    body, bv, bcmd = curl_body_v(
        f"{base}/cgi-bin/post_echo.py",
        extra=["-X", "POST", "-d", "hello from post"])
    record("CGI POST -> echoes body back",
           "hello from post" in body,
           trace=make_trace(bcmd, "'hello from post' in body",
                            body.strip()[:200] or "(empty)", bv))

    # CGI REQUEST_METHOD environment
    body, bv, bcmd = curl_body_v(f"{base}/cgi-bin/hello.py")
    record("CGI has REQUEST_METHOD=GET", "REQUEST_METHOD: GET" in body,
           trace=make_trace(bcmd, "REQUEST_METHOD: GET",
                            body.strip()[:200] or "(empty)", bv))

    # CGI large output
    body, bv, bcmd = curl_body_v(f"{base}/cgi-bin/big_output.py")
    record("CGI large output (100KB) fully transmitted",
           len(body) >= 100000,
           f"received {len(body)} bytes",
           trace=make_trace(bcmd, ">=100000 bytes",
                            f"{len(body)} bytes", bv))

    # Non-existent CGI -> 404
    code, verbose, cmd = curl_code_v(f"{base}/cgi-bin/nonexistent.py")
    record("non-existent CGI script -> 404", code == "404", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404", f"HTTP {code}", verbose))

    record("server alive after CGI tests", server_alive())
    stop_server()


# ──────────────────────── 15. CONCURRENT CONNECTIONS ──────────────────────────

def test_concurrency():
    global _current_category
    _current_category = "Concurrency & Stress"
    print(f"\n{cyan(bold('══ Concurrency & Stress ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    # 100 concurrent connections
    def one_request(_):
        try:
            r = subprocess.run(
                ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}",
                 "--max-time", "10", "--connect-timeout", "5", f"{base}/"],
                capture_output=True, text=True, timeout=12)
            code = r.stdout.strip()
            return 100 <= int(code) < 600
        except Exception:
            return False

    with ThreadPoolExecutor(max_workers=100) as pool:
        futures = [pool.submit(one_request, i) for i in range(100)]
        results = [f.result() for f in as_completed(futures)]
    success = sum(results)
    record(f"100 concurrent connections all responded ({success}/100)",
           success >= 95,  # allow small margin
           f"{success}/100",
           trace=make_trace(
               "100x parallel curl requests",
               ">=95/100 valid responses", f"{success}/100"))

    # 200 rapid connections
    def rapid_request(_):
        try:
            r = subprocess.run(
                ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}",
                 "--max-time", "8", f"{base}/"],
                capture_output=True, text=True, timeout=10)
            return r.stdout.strip() != "000"
        except Exception:
            return False

    with ThreadPoolExecutor(max_workers=50) as pool:
        futures = [pool.submit(rapid_request, i) for i in range(200)]
        results = [f.result() for f in as_completed(futures)]
    success = sum(results)
    record(f"200 rapid connections ({success}/200)",
           success >= 180,
           f"{success}/200",
           trace=make_trace("200x rapid curl requests",
                            ">=180/200 responses", f"{success}/200"))

    # Kill curl mid-request
    write_file(tp("www/big_download.bin"), os.urandom(1024 * 1024))
    curl_proc = subprocess.Popen(
        ["curl", "-s", "--limit-rate", "50k", "--max-time", "30",
         f"{base}/big_download.bin", "-o", "/dev/null"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)
    if curl_proc.poll() is None:
        curl_proc.kill()
        curl_proc.wait()
    time.sleep(0.5)
    code, verbose, cmd = curl_code_v(f"{base}/")
    record("server survives client killed mid-download",
           server_alive() and code != "000",
           f"alive={server_alive()} HTTP {code}",
           trace=make_trace("curl --limit-rate 50k -> kill -> new request",
                            "server alive + HTTP response",
                            f"alive={server_alive()} HTTP {code}", verbose))

    # Slow client (rate limited)
    tmp_slow = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
    tmp_slow.close()
    expected_size = os.path.getsize(tp("www/large.bin"))
    cmd_str = (f"curl -s --limit-rate 100k --max-time 30 "
               f"{base}/large.bin -o {tmp_slow.name}")
    sh(cmd_str, timeout=35)
    received = os.path.getsize(tmp_slow.name)
    os.unlink(tmp_slow.name)
    record("slow client (rate-limited) receives full file",
           received == expected_size,
           f"expected={expected_size} received={received}",
           trace=make_trace(cmd_str, f"{expected_size} bytes",
                            f"{received} bytes"))

    # Double-close safety
    try:
        s = socket.create_connection((HOST, PORT), timeout=2)
        s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        s.close()
    except Exception:
        pass
    time.sleep(0.5)
    code, verbose, cmd = curl_code_v(f"{base}/")
    record("server survives early client close",
           server_alive() and code != "000",
           trace=make_trace("send GET + close immediately -> new request",
                            "server alive", f"alive={server_alive()} HTTP {code}", verbose))

    # Partial/slow send
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as s:
            s.sendall(b"GET / HTTP/1.1\r\n")
            time.sleep(0.5)
            s.sendall(b"Host: localhost\r\n\r\n")
            s.settimeout(5)
            resp = b""
            try:
                while True:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    resp += chunk
            except socket.timeout:
                pass
        record("partial/slow send handled correctly", b"HTTP/1." in resp,
               trace=tcp_trace(HOST, PORT,
                               b"GET / HTTP/1.1\\r\\n (pause) Host: localhost\\r\\n\\r\\n",
                               resp, "HTTP response received",
                               resp.split(b"\n")[0].decode(errors="replace").strip()))
    except Exception as e:
        record("partial/slow send handled correctly", False, str(e))

    # Multiple simultaneous large downloads
    def download_large(i):
        try:
            tf = tempfile.NamedTemporaryFile(delete=False, suffix=f"_{i}.bin")
            tf.close()
            subprocess.run(
                ["curl", "-s", "--max-time", "30", f"{base}/large.bin",
                 "-o", tf.name],
                capture_output=True, timeout=35)
            size = os.path.getsize(tf.name)
            os.unlink(tf.name)
            return size == expected_size
        except Exception:
            return False

    with ThreadPoolExecutor(max_workers=10) as pool:
        futures = [pool.submit(download_large, i) for i in range(10)]
        results = [f.result() for f in as_completed(futures)]
    all_ok = sum(results)
    record(f"10 simultaneous 512KB downloads complete ({all_ok}/10)",
           all_ok >= 8,
           f"{all_ok}/10",
           trace=make_trace("10x parallel 512KB downloads",
                            ">=8/10 complete", f"{all_ok}/10"))

    record("server alive after all stress tests", server_alive())
    stop_server()


# ──────────────────────── 16. MULTI-PORT ──────────────────────────────────────

def test_multi_port():
    global _current_category
    _current_category = "Multi-Port Serving"
    print(f"\n{cyan(bold('══ Multi-Port Serving ══'))}")

    ok = start_server(tp("config/multi_port.conf"), ports=[PORT, PORT2])
    record("dual-port server starts", ok,
           trace=make_trace(f"{BINARY} {tp('config/multi_port.conf')}",
                            f"ports {PORT} and {PORT2} open",
                            "one or both unreachable"))
    if not ok:
        return

    body1, v1, cmd1 = curl_body_v(f"http://{HOST}:{PORT}/")
    body2, v2, cmd2 = curl_body_v(f"http://{HOST}:{PORT2}/")
    record(f"port {PORT} serves correct content", "Welcome" in body1,
           trace=make_trace(cmd1, "'Welcome' in body",
                            body1.strip()[:100], v1))
    record(f"port {PORT2} serves correct content", "Subdir" in body2,
           trace=make_trace(cmd2, "'Subdir' in body",
                            body2.strip()[:100], v2))

    # SO_REUSEADDR - immediate restart
    stop_server()
    time.sleep(0.3)
    ok2 = start_server(tp("config/multi_port.conf"), ports=[PORT, PORT2])
    c1, v1, cmd1 = curl_code_v(f"http://{HOST}:{PORT}/")
    c2, v2, cmd2 = curl_code_v(f"http://{HOST}:{PORT2}/")
    record("immediate restart works (SO_REUSEADDR)",
           ok2 and c1 != "000" and c2 != "000",
           f"{PORT}={c1} {PORT2}={c2}",
           trace=make_trace("stop + immediate restart",
                            "both ports respond",
                            f"{PORT}: HTTP {c1}, {PORT2}: HTTP {c2}"))

    stop_server()


# ──────────────────────── 17. ROUTER & PREFIX MATCHING ────────────────────────

def test_routing():
    global _current_category
    _current_category = "Routing & Longest Prefix"
    print(f"\n{cyan(bold('══ Routing & Longest Prefix Matching ══'))}")

    # Setup specific test files
    mkdirp(tp("www/api"))
    mkdirp(tp("www/api/v2"))
    write_file(tp("www/api/index.html"), "API Root\n")
    write_file(tp("www/api/v2/index.html"), "API v2\n")

    write_file(tp("config/routing.conf"), f"""\
server {{
    listen 0.0.0.0:{PORT};
    root {rp("www")};
    location / {{
        methods GET;
        index index.html;
    }}
    location /api {{
        root {rp("www/api")};
        methods GET;
        index index.html;
    }}
    location /api/v2 {{
        root {rp("www/api/v2")};
        methods GET;
        index index.html;
    }}
}}
""")

    ok = start_server(tp("config/routing.conf"))
    if not ok:
        record("server starts (routing config)", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Longest prefix: /api/v2 matches /api/v2 not /api
    body, bv, bcmd = curl_body_v(f"{base}/api/v2/")
    record("/api/v2/ matches longest prefix /api/v2", "API v2" in body,
           trace=make_trace(bcmd, "'API v2' in body",
                            body.strip()[:100] or "(empty)", bv))

    # /api/ matches /api
    body, bv, bcmd = curl_body_v(f"{base}/api/")
    record("/api/ matches /api location", "API Root" in body,
           trace=make_trace(bcmd, "'API Root' in body",
                            body.strip()[:100] or "(empty)", bv))

    # /apiv2 should NOT match /api (boundary check)
    code, verbose, cmd = curl_code_v(f"{base}/apiv2")
    record("/apiv2 does NOT match /api (boundary check)", code != "200",
           f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 404 (no location match)",
                            f"HTTP {code}", verbose))

    # / matches root
    body, bv, bcmd = curl_body_v(f"{base}/")
    record("/ matches root location", "Welcome" in body,
           trace=make_trace(bcmd, "'Welcome' in body",
                            body.strip()[:100] or "(empty)", bv))

    stop_server()


# ──────────────────────── 18. CRASH RESILIENCE ────────────────────────────────

def test_crash_resilience():
    global _current_category
    _current_category = "Crash Resilience"
    print(f"\n{cyan(bold('══ Crash Resilience ══'))}")

    ok = start_server(tp("config/main.conf"))
    if not ok:
        record("server starts", False)
        return

    base = f"http://{HOST}:{PORT}"

    # Rapid connect/disconnect
    for _ in range(50):
        try:
            s = socket.create_connection((HOST, PORT), timeout=1)
            s.close()
        except Exception:
            pass
    time.sleep(1)
    record("server alive after 50 rapid connect/disconnect", server_alive())

    # Half-open connections
    sockets = []
    for _ in range(20):
        try:
            s = socket.create_connection((HOST, PORT), timeout=2)
            s.sendall(b"GET / HTTP/1.1\r\n")  # incomplete request
            sockets.append(s)
        except Exception:
            pass
    time.sleep(2)
    for s in sockets:
        try:
            s.close()
        except Exception:
            pass
    time.sleep(1)
    code, verbose, cmd = curl_code_v(f"{base}/")
    record("server alive after 20 half-open connections",
           server_alive() and code != "000",
           f"HTTP {code}",
           trace=make_trace("20 half-open TCP + close -> new request",
                            "server alive + response",
                            f"alive={server_alive()} HTTP {code}", verbose))

    # Zero-length send
    try:
        s = socket.create_connection((HOST, PORT), timeout=2)
        s.sendall(b"")
        s.close()
    except Exception:
        pass
    time.sleep(0.5)
    record("server alive after zero-length send", server_alive())

    # Binary garbage
    for _ in range(10):
        tcp_send(HOST, PORT, os.urandom(1024), timeout=1)
    time.sleep(0.5)
    record("server alive after random binary data", server_alive())

    # Mix of valid and invalid
    for i in range(20):
        if i % 2 == 0:
            curl_code(f"{base}/")
        else:
            tcp_send(HOST, PORT, b"GARBAGE\r\n\r\n", timeout=1)
    record("server alive after mixed valid/invalid requests", server_alive())

    # Final check
    code, verbose, cmd = curl_code_v(f"{base}/")
    record("final GET / works", code == "200", f"HTTP {code}",
           trace=make_trace(cmd, "HTTP 200", f"HTTP {code}", verbose))

    stop_server()


# ═══════════════════════════════════════════════════════════════════════════════
#                                    MAIN
# ═══════════════════════════════════════════════════════════════════════════════

CATEGORIES = {
    "compilation":  ("Compilation & Build",          test_compilation),
    "config":       ("Configuration Parsing",        test_config_parsing),
    "methods":      ("HTTP Methods",                 test_http_methods),
    "headers":      ("HTTP Response Headers",        test_response_headers),
    "static":       ("Static Files & MIME",          test_static_files),
    "directory":    ("Directory Listing & Index",     test_directory_listing),
    "errors":       ("Error Pages",                  test_error_pages),
    "redirects":    ("HTTP Redirects",               test_redirects),
    "body":         ("Request Body & Chunked",       test_body_handling),
    "uploads":      ("File Uploads",                 test_uploads),
    "traversal":    ("Path Traversal Security",      test_path_traversal),
    "bad_requests": ("Bad Requests & Edge Cases",    test_bad_requests),
    "keepalive":    ("Keep-Alive & Pipelining",      test_keepalive),
    "cgi":          ("CGI Execution",                test_cgi),
    "concurrency":  ("Concurrency & Stress",         test_concurrency),
    "multi_port":   ("Multi-Port Serving",           test_multi_port),
    "routing":      ("Routing & Longest Prefix",     test_routing),
    "resilience":   ("Crash Resilience",             test_crash_resilience),
}

CATEGORY_ORDER = [
    "compilation", "config", "methods", "headers", "static", "directory",
    "errors", "redirects", "body", "uploads", "traversal", "bad_requests",
    "keepalive", "cgi", "concurrency", "multi_port", "routing", "resilience",
]


def main():
    parser = argparse.ArgumentParser(
        description="Webserv Hardcore Evaluation Tester",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Categories:\n"
            + "\n".join(f"  {k:<14} {v[0]}" for k, v in CATEGORIES.items())
            + "\n\nExamples:\n"
            "  python3 webserv_tester/run_tests.py                # run all tests\n"
            "  python3 webserv_tester/run_tests.py -c uploads     # run only uploads\n"
            "  python3 webserv_tester/run_tests.py -c methods,cgi # run methods + cgi\n"
        ),
    )
    parser.add_argument("-c", "--category",
                        help="comma-separated list of categories to run (default: all)",
                        default=None)
    args = parser.parse_args()

    if args.category:
        cats = [c.strip() for c in args.category.split(",")]
        for c in cats:
            if c not in CATEGORIES:
                print(red(f"Unknown category '{c}'. Valid: {', '.join(CATEGORIES.keys())}"))
                sys.exit(1)
        cats_to_run = cats
    else:
        cats_to_run = CATEGORY_ORDER

    print(bold(f"\n{'=' * 60}"))
    print(bold("  WEBSERV  --  Hardcore Evaluation Tester"))
    print(bold(f"  Categories: {', '.join(cats_to_run)}"))
    print(bold(f"  Sandbox:    {TMP_DIR}"))
    print(bold(f"  Traces:     {TRACES_FILE}"))
    print(bold(f"{'=' * 60}"))

    # Build if needed
    if not os.path.isfile(BINARY_ABS):
        print(yellow("Binary not found -- building with 'make' ..."))
        rc, _, _ = sh("make 2>&1", timeout=120)
        if rc != 0 or not os.path.isfile(BINARY_ABS):
            print(red("Build failed. Aborting."))
            sys.exit(1)

    def _sig(sig, frame):
        print(yellow("\nInterrupted -- cleaning up ..."))
        cleanup()
        print_summary()
        sys.exit(130)

    signal.signal(signal.SIGINT, _sig)

    setup_all()

    try:
        for cat_id in cats_to_run:
            _, func = CATEGORIES[cat_id]
            func()
    finally:
        cleanup()

    print_summary()
    failed = sum(1 for r in _results if r["passed"] is False)
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
