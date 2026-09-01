#!/usr/bin/env python3
import base64, hashlib, json, secrets, socket, threading, time
import urllib.parse, urllib.request, webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

CLIENT_ID = "fc8dbfc6eecf4635b948af24981384ba"
REDIRECT_URI = "http://127.0.0.1:8000/callback"
SCOPES = "user-read-playback-state user-read-currently-playing user-modify-playback-state"
AUTH_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"
API = "https://api.spotify.com"

TOKEN_FILE = Path(__file__).resolve().parent / "spotify_vita_session.json"

session = {"access_token":"", "refresh_token":"", "expires_at":0}

def save_session():
    TOKEN_FILE.write_text(json.dumps(session), encoding="utf-8")

def load_session():
    if TOKEN_FILE.exists():
        try:
            session.update(json.loads(TOKEN_FILE.read_text(encoding="utf-8")))
        except Exception:
            pass

def refresh():
    if not session.get("refresh_token"):
        return False
    data = urllib.parse.urlencode({
        "client_id": CLIENT_ID,
        "grant_type": "refresh_token",
        "refresh_token": session["refresh_token"]
    }).encode()
    req = urllib.request.Request(TOKEN_URL, data=data, method="POST",
        headers={"Content-Type":"application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req, timeout=30) as r:
        obj = json.loads(r.read().decode())
    session["access_token"] = obj["access_token"]
    if obj.get("refresh_token"):
        session["refresh_token"] = obj["refresh_token"]
    session["expires_at"] = int(time.time()) + int(obj.get("expires_in",3600)) - 60
    save_session()
    return True

def ensure_token():
    if session.get("access_token") and int(time.time()) < int(session.get("expires_at",0)):
        return True
    return refresh()

def api_request(method, path, body=None):
    if not ensure_token():
        raise RuntimeError("not_logged_in")
    headers = {"Authorization":"Bearer " + session["access_token"]}
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(API + path, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            raw = r.read()
            return r.status, raw
    except urllib.error.HTTPError as e:
        if e.code == 401 and refresh():
            headers["Authorization"] = "Bearer " + session["access_token"]
            req = urllib.request.Request(API + path, data=data, method=method, headers=headers)
            with urllib.request.urlopen(req, timeout=30) as r:
                return r.status, r.read()
        return e.code, e.read()

def login():
    state = secrets.token_urlsafe(24)
    verifier = secrets.token_urlsafe(64)
    challenge = base64.urlsafe_b64encode(
        hashlib.sha256(verifier.encode()).digest()
    ).decode().rstrip("=")
    result = {"done":False,"code":None,"error":None}

    class Callback(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            if q.get("state",[""])[0] != state:
                result["error"] = "state_mismatch"
            elif q.get("error"):
                result["error"] = q["error"][0]
            else:
                result["code"] = q.get("code",[""])[0]
            result["done"] = True
            self.send_response(200); self.end_headers()
            self.wfile.write(b"Spotify Vita login OK. You can close this tab.")

    srv = ThreadingHTTPServer(("127.0.0.1",8000), Callback)
    threading.Thread(target=srv.handle_request, daemon=True).start()

    url = AUTH_URL + "?" + urllib.parse.urlencode({
        "client_id":CLIENT_ID,
        "response_type":"code",
        "redirect_uri":REDIRECT_URI,
        "scope":SCOPES,
        "state":state,
        "code_challenge_method":"S256",
        "code_challenge":challenge,
    })
    print("Opening Spotify login...")
    webbrowser.open(url)

    deadline=time.time()+300
    while not result["done"] and time.time()<deadline:
        time.sleep(.2)
    srv.server_close()

    if result["error"] or not result["code"]:
        raise RuntimeError(result["error"] or "login_timeout")

    data = urllib.parse.urlencode({
        "client_id":CLIENT_ID,
        "grant_type":"authorization_code",
        "code":result["code"],
        "redirect_uri":REDIRECT_URI,
        "code_verifier":verifier,
    }).encode()
    req=urllib.request.Request(TOKEN_URL,data=data,method="POST",
        headers={"Content-Type":"application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req,timeout=30) as r:
        obj=json.loads(r.read().decode())
    session["access_token"]=obj["access_token"]
    session["refresh_token"]=obj.get("refresh_token","")
    session["expires_at"]=int(time.time())+int(obj.get("expires_in",3600))-60
    save_session()

def local_ip():
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8",80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()

class VitaAPI(BaseHTTPRequestHandler):
    def log_message(self, fmt,*args):
        print("[Vita]", fmt % args)

    def send_json(self, code, obj):
        raw=json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type","application/json; charset=utf-8")
        self.send_header("Content-Length",str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        u=urllib.parse.urlparse(self.path)
        q=urllib.parse.parse_qs(u.query)

        if u.path == "/ping":
            return self.send_json(200, {"ok":True})

        if u.path == "/search":
            query=q.get("q",[""])[0]
            if not query:
                return self.send_json(400,{"error":"missing_query"})
            path="/v1/search?"+urllib.parse.urlencode({
                "q":query,"type":"track","limit":"8"
            })
            status, raw=api_request("GET",path)
            if status != 200:
                return self.send_json(status,{"error":"spotify","status":status})
            data=json.loads(raw.decode())
            out=[]
            for t in data.get("tracks",{}).get("items",[]):
                out.append({
                    "name":t.get("name",""),
                    "artist":", ".join(a.get("name","") for a in t.get("artists",[])),
                    "uri":t.get("uri",""),
                })
            return self.send_json(200,{"tracks":out})

        if u.path == "/play":
            uri=q.get("uri",[""])[0]
            if not uri:
                return self.send_json(400,{"error":"missing_uri"})
            status, raw=api_request("PUT","/v1/me/player/play",{"uris":[uri]})
            return self.send_json(200 if status in (200,202,204) else status,
                                  {"ok":status in (200,202,204),"spotify_status":status})

        if u.path == "/pause":
            status, raw=api_request("PUT","/v1/me/player/pause")
            return self.send_json(200 if status in (200,202,204) else status,
                                  {"ok":status in (200,202,204),"spotify_status":status})

        if u.path == "/next":
            status, raw=api_request("POST","/v1/me/player/next")
            return self.send_json(200 if status in (200,202,204) else status,
                                  {"ok":status in (200,202,204),"spotify_status":status})

        self.send_json(404,{"error":"not_found"})

def main():
    load_session()
    if not ensure_token():
        login()

    ip=local_ip()
    print()
    print("Spotify Vita PC Companion")
    print("=========================")
    print("PC IP:", ip)
    print("Vita proxy:", f"http://{ip}:8080")
    print("KEEP THIS WINDOW OPEN while using Spotify Vita.")
    print()

    srv=ThreadingHTTPServer(("0.0.0.0",8080), VitaAPI)
    srv.serve_forever()

if __name__=="__main__":
    main()
