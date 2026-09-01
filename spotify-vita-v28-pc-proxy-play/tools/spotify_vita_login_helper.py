#!/usr/bin/env python3
"""
Spotify Vita v26 PC Login Helper

Runs entirely on your PC:
1. Generates PKCE verifier/challenge.
2. Opens Spotify authorization in your modern default browser.
3. Receives Spotify callback on http://127.0.0.1:8000/callback
4. Exchanges the authorization code for tokens.
5. Writes spotify_vita_tokens.txt for transfer to:
   ux0:data/spotify-vita/import_tokens.txt

No client secret is used.
"""

import base64
import hashlib
import json
import os
import secrets
import threading
import time
import urllib.parse
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

CLIENT_ID = "fc8dbfc6eecf4635b948af24981384ba"
REDIRECT_URI = "http://127.0.0.1:8000/callback"
SCOPES = "user-read-playback-state user-read-currently-playing user-modify-playback-state"

AUTH_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"

state = secrets.token_urlsafe(24)
verifier = secrets.token_urlsafe(64)
challenge = base64.urlsafe_b64encode(
    hashlib.sha256(verifier.encode("ascii")).digest()
).decode("ascii").rstrip("=")

result = {"done": False, "error": None, "code": None}

class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != "/callback":
            self.send_response(404)
            self.end_headers()
            return

        q = urllib.parse.parse_qs(parsed.query)
        got_state = q.get("state", [""])[0]
        error = q.get("error", [""])[0]
        code = q.get("code", [""])[0]

        if got_state != state:
            result["error"] = "state_mismatch"
        elif error:
            result["error"] = error
        elif not code:
            result["error"] = "missing_code"
        else:
            result["code"] = code

        result["done"] = True

        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        if result["error"]:
            body = "<h1>Spotify Vita</h1><p>Login failed: %s</p>" % result["error"]
        else:
            body = "<h1>Spotify Vita</h1><p>Login accepted. You can close this tab.</p>"
        self.wfile.write(body.encode("utf-8"))

def exchange_code(code):
    data = urllib.parse.urlencode({
        "client_id": CLIENT_ID,
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": REDIRECT_URI,
        "code_verifier": verifier,
    }).encode("ascii")

    req = urllib.request.Request(
        TOKEN_URL,
        data=data,
        method="POST",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode("utf-8"))

def main():
    params = {
        "client_id": CLIENT_ID,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
        "state": state,
        "code_challenge_method": "S256",
        "code_challenge": challenge,
    }
    url = AUTH_URL + "?" + urllib.parse.urlencode(params)

    print("Spotify Vita v26 PC Login Helper")
    print("--------------------------------")
    print("Opening Spotify login in your default browser...")
    print()
    print("If the browser does not open, paste this URL into Chrome/Edge/Firefox:")
    print(url)
    print()

    server = HTTPServer(("127.0.0.1", 8000), Handler)
    thread = threading.Thread(target=server.handle_request, daemon=True)
    thread.start()

    webbrowser.open(url)

    deadline = time.time() + 300
    while not result["done"] and time.time() < deadline:
        time.sleep(0.2)

    server.server_close()

    if not result["done"]:
        raise SystemExit("Timed out waiting for Spotify callback.")
    if result["error"]:
        raise SystemExit("Spotify login failed: " + result["error"])

    tokens = exchange_code(result["code"])
    access_token = tokens.get("access_token", "")
    refresh_token = tokens.get("refresh_token", "")
    expires_in = int(tokens.get("expires_in", 3600))

    if not access_token:
        raise SystemExit("Spotify returned no access_token.")

    out = Path(__file__).resolve().parent / "spotify_vita_tokens.txt"
    out.write_text(
        "access_token=" + access_token + "\n"
        "refresh_token=" + refresh_token + "\n"
        "expires_in=" + str(expires_in) + "\n",
        encoding="utf-8",
    )

    print()
    print("LOGIN OK")
    print("Created:")
    print(out)
    print()
    print("Copy this file to your Vita as:")
    print("ux0:data/spotify-vita/import_tokens.txt")
    print("Then start Spotify Vita v26.")

if __name__ == "__main__":
    main()
