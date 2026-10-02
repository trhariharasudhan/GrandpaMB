"""Run:  python -m pytest tools -q   (no board / simulator needed)"""
from grandpa_link import GrandpaMBClient, parse_user_line


def test_parse_user_line():
    assert parse_user_line("say Vanakkam da") == ("say", {"text": "Vanakkam da"})
    assert parse_user_line("beep 250") == ("beep", {"ms": 250})
    assert parse_user_line("state thinking") == ("state", {"value": "thinking"})
    assert parse_user_line("info") == ("info", {})


class FakeBoard(GrandpaMBClient):
    """Skips the serial port; records calls and answers like the firmware."""

    def __init__(self):
        self.sent = []

    def call(self, cmd, **params):
        self.sent.append((cmd, params))
        if cmd == "relay" and params.get("value") == "on":
            return {"ok": True, "confirm_required": True, "action": "relay_on", "token": "ABC123"}
        return {"ok": True, "msg": cmd}


def test_relay_on_requires_human_yes():
    b = FakeBoard()
    b.relay(True, confirm_cb=lambda q: False)
    assert b.sent[-1] == ("cancel", {})            # human said no -> cancelled, never confirmed


def test_relay_on_confirmed_with_board_token():
    b = FakeBoard()
    b.relay(True, confirm_cb=lambda q: True)
    assert b.sent[-1] == ("confirm", {"token": "ABC123"})


def test_relay_off_needs_no_confirmation():
    b = FakeBoard()
    b.relay(False, confirm_cb=lambda q: (_ for _ in ()).throw(AssertionError("must not ask")))
    assert b.sent == [("relay", {"value": "off"})]
