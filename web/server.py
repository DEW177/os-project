#!/usr/bin/env python3
"""
web/server.py — เว็บเดโมของ Secure Code Judge Sandbox

    make                       # build ./judge ก่อน
    python3 web/server.py      # แล้วเปิด http://localhost:8000

ใช้แค่ standard library ของ Python 3 ไม่ต้องติดตั้งอะไรเพิ่ม

API
    GET  /api/health     -> {"ok": true, "judge_built": bool}
    GET  /api/examples   -> {"examples": {"infinite_loop.c": "...source..."}}
    POST /api/judge      <- {"source": "...", "problem": "aplusb"}
                         -> {"result": {...ผลจาก ./judge --json...}, "stderr": "..."}

⚠ server รันโค้ดที่ส่งเข้ามาจริง — bind ไว้ที่ 127.0.0.1 เท่านั้น อย่าเปิดให้คนอื่นเข้าถึง
"""
import argparse
import json
import pathlib
import subprocess
import tempfile
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB_DIR = ROOT / "web"
JUDGE = ROOT / "judge"
PROBLEMS = ROOT / "tests" / "problems"
ATTACKS = ROOT / "tests" / "attacks"
MAX_SOURCE = 64 * 1024        # 64 KB
JUDGE_TIMEOUT = 60            # วินาที (กันกรณี judge เองค้าง)


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_DIR), **kwargs)

    # ---------- helpers ----------
    def send_json(self, obj, status=200):
        body = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    # ---------- GET ----------
    def do_GET(self):
        if self.path == "/api/health":
            return self.send_json({"ok": True, "judge_built": JUDGE.exists()})
        if self.path == "/api/examples":
            examples = {p.name: p.read_text(encoding="utf-8") for p in sorted(ATTACKS.glob("*.c"))}
            return self.send_json({"examples": examples})
        return super().do_GET()

    # ---------- POST ----------
    def do_POST(self):
        if self.path != "/api/judge":
            return self.send_json({"error": "not found"}, 404)

        length = int(self.headers.get("Content-Length", 0))
        if length > MAX_SOURCE + 1024:
            return self.send_json({"error": "ซอร์สโค้ดใหญ่เกิน 64 KB"}, 413)
        try:
            req = json.loads(self.rfile.read(length) or b"{}")
        except json.JSONDecodeError:
            return self.send_json({"error": "request ไม่ใช่ JSON"}, 400)

        source = req.get("source", "")
        problem = req.get("problem", "aplusb")
        available = {p.name for p in PROBLEMS.iterdir() if p.is_dir()}
        if problem not in available:
            return self.send_json({"error": f"ไม่มีโจทย์ {problem!r}"}, 400)
        if not source.strip():
            return self.send_json({"error": "ไม่มีซอร์สโค้ด"}, 400)
        if not JUDGE.exists():
            return self.send_json({"error": "ยังไม่ได้ build ./judge — รัน make ก่อน"}, 500)

        with tempfile.TemporaryDirectory(prefix="judge-web-") as tmp:
            src_path = pathlib.Path(tmp) / "submission.c"
            src_path.write_text(source, encoding="utf-8")
            cmd = [str(JUDGE), "--src", str(src_path), "--tests", str(PROBLEMS / problem),
                   "--time", "1000", "--mem", "256", "--output", "1024", "--json"]
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True,
                                      timeout=JUDGE_TIMEOUT, stdin=subprocess.DEVNULL, cwd=ROOT)
            except subprocess.TimeoutExpired:
                return self.send_json({"error": f"./judge ไม่ตอบภายใน {JUDGE_TIMEOUT} วินาที"}, 504)

        result = None
        for line in reversed(proc.stdout.splitlines()):
            if line.startswith("{"):
                try:
                    result = json.loads(line)
                    break
                except json.JSONDecodeError:
                    pass
        if result is None:
            return self.send_json({"error": "อ่านผล JSON จาก ./judge ไม่ได้",
                                   "stderr": (proc.stderr + proc.stdout)[-2000:]}, 500)
        return self.send_json({"result": result, "stderr": proc.stderr[-2000:]})

    def log_message(self, fmt, *args):
        print("[web]", fmt % args)


def main():
    ap = argparse.ArgumentParser(description="Judge Sandbox web demo")
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Judge demo → http://localhost:{args.port}   (judge built: {JUDGE.exists()})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
