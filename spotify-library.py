#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import base64
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import sys
import tempfile
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlencode, urlsplit
from urllib.request import Request, urlopen

CONFIG = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "plasma-spotify-hover"
CREDENTIALS = CONFIG / "spotify.json"
PORT = 8765
REDIRECT = f"http://127.0.0.1:{PORT}/callback"
ACCOUNTS = "https://accounts.spotify.com"
DASHBOARD = "https://developer.spotify.com/dashboard"


def token_request(**fields):
    request = Request(f"{ACCOUNTS}/api/token", data=urlencode(fields).encode("ascii"))
    with urlopen(request, timeout=8) as response:
        return json.load(response)


def write_credentials(client_id, refresh_token):
    fd, name = tempfile.mkstemp(dir=CONFIG)
    try:
        with os.fdopen(fd, "w") as output:
            json.dump({"client_id": client_id, "refresh_token": refresh_token}, output)
        os.replace(name, CREDENTIALS)
    finally:
        Path(name).unlink(missing_ok=True)


def authorize(client_id):
    # https://developer.spotify.com/documentation/web-api/tutorials/code-pkce-flow
    verifier = secrets.token_urlsafe(64)
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
    state = secrets.token_urlsafe(32)
    result = {}
    query = urlencode({
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": REDIRECT,
        "scope": "user-library-modify",
        "code_challenge_method": "S256",
        "code_challenge": challenge,
        "state": state,
    })

    class Callback(BaseHTTPRequestHandler):
        def setup(self):
            super().setup()
            self.connection.settimeout(3)

        def do_GET(self):
            url = urlsplit(self.path)
            params = parse_qs(url.query)
            if url.path != "/callback" or params.get("state") != [state]:
                self.send_error(400, "Invalid OAuth callback")
                return
            result.update(params)
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(b"Return to the terminal to check Spotify authorization.")

        def log_message(self, *_args):
            pass

    with HTTPServer(("127.0.0.1", PORT), Callback) as server:
        server.timeout = 1
        consent_url = f"{ACCOUNTS}/authorize?{query}"
        print(f"Approve Spotify access in your browser. If it does not open, visit:\n{consent_url}", flush=True)
        open_browser(consent_url)
        deadline = time.monotonic() + 180
        while not result and time.monotonic() < deadline:
            server.handle_request()

    if "code" not in result:
        raise ValueError("Spotify authorization was denied or timed out.")
    token = token_request(
        grant_type="authorization_code",
        client_id=client_id,
        code=result["code"][0],
        redirect_uri=REDIRECT,
        code_verifier=verifier,
    )
    with (CONFIG / "lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        write_credentials(client_id, token["refresh_token"])
    print("Spotify authorization saved.")


def open_browser(url):
    try:
        webbrowser.open(url)
    except (OSError, webbrowser.Error):
        pass  # The URL is also printed for manual use.


def connect(force=False):
    if not force and CREDENTIALS.is_file():
        try:
            credentials = json.loads(CREDENTIALS.read_text())
            if credentials.get("client_id") and credentials.get("refresh_token"):
                print("Spotify is already connected.")
                return
        except (ValueError, AttributeError):
            pass

    if not sys.stdin.isatty():
        raise ValueError("Run ./install.sh in a terminal to connect your Spotify account.")

    print(
        f"Open your Spotify Developer Dashboard at {DASHBOARD} and create or select an app.\n"
        f"Enable Web API access and add this redirect URI in the app settings:\n{REDIRECT}\n"
        "Then copy the app's Client ID. Spotify requires Premium for Development Mode app owners.",
        flush=True,
    )
    open_browser(DASHBOARD)
    authorize(valid_client_id(input("Spotify Client ID: ").strip()))


def valid_client_id(client_id):
    if not re.fullmatch(r"[a-fA-F0-9]{32}", client_id):
        raise ValueError("Enter the Spotify app's 32-character Client ID.")
    return client_id


def save(uri):
    with (CONFIG / "lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if not CREDENTIALS.is_file():
            raise ValueError("Run ./install.sh from the PlasmaSpotifyHover project to connect Spotify.")
        credentials = json.loads(CREDENTIALS.read_text())
        try:
            token = token_request(grant_type="refresh_token", **credentials)
        except HTTPError as error:
            if error.code in (400, 401):
                raise ValueError("Spotify authorization expired. Run ./install.sh reconnect.") from None
            raise
        if token.get("refresh_token"):
            write_credentials(credentials["client_id"], token["refresh_token"])

    # https://developer.spotify.com/documentation/web-api/reference/save-library-items
    query = urlencode({"uris": uri})
    request = Request(
        f"https://api.spotify.com/v1/me/library?{query}",
        method="PUT",
        headers={"Authorization": f"Bearer {token['access_token']}"},
    )
    with urlopen(request, timeout=8):
        pass


def main():
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="action", required=True)
    commands.add_parser("auth").add_argument("client_id")
    commands.add_parser("save").add_argument("uri")
    commands.add_parser("connect")
    commands.add_parser("reconnect")
    args = parser.parse_args()
    try:
        if args.action == "save" and not re.fullmatch(r"spotify:track:[a-zA-Z0-9]{22}", args.uri):
            raise ValueError("Expected a Spotify track URI.")
        CONFIG.mkdir(mode=0o700, parents=True, exist_ok=True)
        if args.action == "auth":
            authorize(valid_client_id(args.client_id))
        elif args.action in {"connect", "reconnect"}:
            connect(force=args.action == "reconnect")
        else:
            save(args.uri)
    except HTTPError as error:
        print(f"Spotify returned HTTP {error.code}.", file=sys.stderr)
        return 1
    except (OSError, ValueError, KeyError, EOFError) as error:
        print(f"Spotify setup or request failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
