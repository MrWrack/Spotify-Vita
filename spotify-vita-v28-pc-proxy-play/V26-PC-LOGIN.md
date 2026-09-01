# Spotify Vita v26 — PC Login Helper

Why:
The PS Vita browser fails to connect to Spotify's modern HTTPS login page.
Spotify's supported user-login flow for a public/native client is Authorization
Code with PKCE. Spotify does not document a Device Authorization Grant.

How to use:
1. In Spotify Developer Dashboard keep this Redirect URI:
   http://127.0.0.1:8000/callback
2. On Windows, open:
   tools/START-LOGIN.bat
3. Sign in to Spotify in Chrome/Edge/Firefox on the PC.
4. The helper creates:
   tools/spotify_vita_tokens.txt
5. Copy that file to the Vita with VitaShell/FTP as:
   ux0:data/spotify-vita/import_tokens.txt
6. Start Spotify Vita v26. The app imports the tokens and removes the import file.

Security:
- No Client Secret is embedded.
- The helper uses PKCE.
- Treat spotify_vita_tokens.txt as private because it contains OAuth tokens.

Implementation note:
- The imported token is loaded into AppController.auth.
- imported sessions are marked authenticated and persisted to session.bin.
- the one-time import_tokens.txt is deleted after a successful import.
