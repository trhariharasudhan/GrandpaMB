"""
grandpa_link.py — PC-side bridge between Grandpa core and the GrandpaMB board.

Works with BOTH:
  * the Wokwi simulator   ->  python tools/grandpa_link.py --port rfc2217://localhost:4000
  * a real ESP32 board    ->  python tools/grandpa_link.py --port COM5

What it does:
  * keeps a heartbeat (ping) so the board shows "connected"
  * prints events/telemetry coming from the board
  * demo flow: WAKE button -> listening -> thinking -> speaking -> idle
  * risky actions (relay on) need a human "yes" on the PC AND the board's token
  * lets you type commands:  state thinking | say Hello | beep 200 | relay on | info

Install:  pip install pyserial
"""
from __future__ import annotations

import argparse
import itertools
import json
import logging
import queue
import threading
import time

import serial  # pyserial

log = logging.getLogger("grandpa_link")

HEARTBEAT_SEC = 10
REPLY_TIMEOUT_SEC = 3


class GrandpaMBClient:
    """Small reusable client. Grandpa core can import this later as a tool backend."""

    def __init__(self, port: str, baud: int = 115200):
        self.ser = serial.serial_for_url(port, baudrate=baud, timeout=0.2)
        self._ids = itertools.count(1)
        self._replies: dict[int, queue.Queue] = {}
        self._lock = threading.Lock()
        self.events: queue.Queue = queue.Queue()
        self._running = True
        threading.Thread(target=self._reader, daemon=True).start()

    # ---------- low level ----------
    def _reader(self) -> None:
        buf = b""
        while self._running:
            try:
                chunk = self.ser.read(256)
            except serial.SerialException as exc:
                log.error("serial error: %s", exc)
                self._running = False
                break
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip()
                if not line:
                    continue
                try:
                    msg = json.loads(line)
                except json.JSONDecodeError:
                    log.debug("non-json from board: %s", line)  # boot ROM noise etc.
                    continue
                rid = msg.get("id")
                if msg.get("type") == "reply" and rid in self._replies:
                    self._replies[rid].put(msg)
                else:
                    self.events.put(msg)

    def call(self, cmd: str, **params) -> dict:
        """Send a command and wait for its reply (raises TimeoutError)."""
        rid = next(self._ids)
        q: queue.Queue = queue.Queue(maxsize=1)
        self._replies[rid] = q
        payload = json.dumps({"id": rid, "cmd": cmd, **params}) + "\n"
        with self._lock:
            self.ser.write(payload.encode())
        try:
            return q.get(timeout=REPLY_TIMEOUT_SEC)
        except queue.Empty:
            raise TimeoutError(f"no reply to '{cmd}' within {REPLY_TIMEOUT_SEC}s") from None
        finally:
            self._replies.pop(rid, None)

    def close(self) -> None:
        self._running = False
        self.ser.close()

    # ---------- high level helpers ----------
    def set_state(self, state: str) -> dict:
        return self.call("state", value=state)

    def say(self, text: str) -> dict:
        return self.call("say", text=text[:60])

    def relay(self, on: bool, confirm_cb) -> dict:
        """Relay ON is high-risk: board returns a token, a human must approve on the PC."""
        if not on:
            return self.call("relay", value="off")
        r = self.call("relay", value="on")
        if not r.get("confirm_required"):
            return r
        if not confirm_cb(f"Board wants to switch RELAY ON (action={r['action']}). Allow?"):
            return self.call("cancel")
        return self.call("confirm", token=r["token"])


# ---------------- demo app ----------------
def ask_human(question: str) -> bool:
    return input(f"\n[CONFIRM] {question} [yes/NO]: ").strip().lower() == "yes"


def demo_conversation(mb: GrandpaMBClient) -> None:
    """Fake Grandpa turn. Replace the sleeps with real STT -> LLM -> TTS calls."""
    mb.set_state("listening")
    time.sleep(2)                      # TODO: record audio + STT (faster-whisper)
    mb.set_state("thinking")
    mb.say("Thinking...")
    time.sleep(2)                      # TODO: Grandpa AI Engine / Ollama
    mb.set_state("speaking")
    mb.say("Vanakkam Captain!")
    time.sleep(2)                      # TODO: TTS playback
    mb.set_state("idle")


def handle_event(mb: GrandpaMBClient, ev: dict) -> None:
    kind = ev.get("type")
    if kind == "telemetry":
        log.info("telemetry: %s", {k: v for k, v in ev.items() if k != "type"})
    elif kind == "event" and ev.get("name") == "wake":
        log.info("WAKE pressed -> running demo turn")
        threading.Thread(target=demo_conversation, args=(mb,), daemon=True).start()
    else:
        log.info("board: %s", ev)


def parse_user_line(line: str) -> tuple[str, dict]:
    cmd, _, rest = line.strip().partition(" ")
    rest = rest.strip()
    if cmd in ("say", "display"):
        return cmd, {"text": rest}
    if cmd == "beep":
        return cmd, {"ms": int(rest or 120)}
    return cmd, ({"value": rest} if rest else {})


def main() -> None:
    ap = argparse.ArgumentParser(description="Grandpa <-> GrandpaMB bridge")
    ap.add_argument("--port", default="rfc2217://localhost:4000",
                    help="rfc2217://localhost:4000 (Wokwi) or COMx (real board)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s", datefmt="%H:%M:%S")

    try:
        mb = GrandpaMBClient(args.port)
    except serial.SerialException as exc:
        log.error("cannot open %s: %s", args.port, exc)
        if args.port.startswith("rfc2217://"):
            log.error("Wokwi simulator is not running. In VS Code: F1 -> 'Wokwi: Start Simulator', "
                      "keep the diagram tab visible, then run this script again.")
        else:
            log.error("Check the COM port in Device Manager and close any other serial monitor.")
        raise SystemExit(1) from None
    log.info("connected to %s", args.port)
    print(mb.call("info"))

    def background():
        last_ping = 0.0
        while True:
            if time.time() - last_ping > HEARTBEAT_SEC:
                try:
                    mb.call("ping")
                except TimeoutError as exc:
                    log.warning("%s", exc)
                last_ping = time.time()
            try:
                handle_event(mb, mb.events.get(timeout=0.5))
            except queue.Empty:
                pass

    threading.Thread(target=background, daemon=True).start()

    print("Type commands (help, info, state thinking, say Hi, beep 200, relay on/off). Ctrl+C to quit.")
    try:
        while True:
            line = input("> ").strip()
            if not line:
                continue
            cmd, params = parse_user_line(line)
            try:
                if cmd == "relay":
                    print(mb.relay(params.get("value") == "on", ask_human))
                else:
                    print(mb.call(cmd, **params))
            except (TimeoutError, ValueError) as exc:
                print("error:", exc)
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        mb.close()


if __name__ == "__main__":
    main()
