# Spotify Vita starter v9

v9 is the **BUILD-FIX-FINAL source pass**.

It contains the OAuth/PKCE login flow, local callback server, Spotify HTTP and
playback clients, background playback state worker, Vita controls/touch,
libvita2d neon UI, real textured album covers, cover download/decode pipeline,
dedup, priorities and a 16 MB LRU cache.

## Build

You need a working VitaSDK installation and libvita2d.

```sh
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"

./build-vita.sh
```

The build should produce:

```text
build/eboot.bin
build/spotify-vita.vpk
```

## Spotify setup

Set your client ID in:

```text
include/spotify_config.h
```

Register this exact redirect URI in the Spotify Developer Dashboard:

```text
http://127.0.0.1:43891/callback
```

## v9 build fixes

- canonical VitaSDK CMake target / `eboot.bin`
- `SceSysmodule_stub`
- load/unload `SCE_SYSMODULE_NET`
- load/unload HTTP + SSL + HTTPS sysmodules
- token directory creation
- persisted monotonic-expiry bug fixed
- percent-decoded OAuth callback parameters
- explicit OAuth error callback handling
- one-command `build-vita.sh`

See `BUILD-FIX-REPORT.md` for what was verified and what still requires actual
VitaSDK/hardware.

## Important

This project controls Spotify through the official Web API / Spotify Connect
playback model. It does not decrypt, download, or bypass Spotify-protected
audio.


## Automated VitaSDK build (v10)

This package now includes:

```text
.github/workflows/vita-build.yml
build-docker.sh
CI-BUILD.md
```

GitHub Actions builds with the official `vitasdk/vitasdk:2026.08` Docker image
and uploads `spotify-vita.vpk` plus `eboot.bin` as the `spotify-vita-vpk`
artifact.

For a local Docker build:

```sh
./build-docker.sh
```

This is the preferred way to get the first real compiler/linker result when
VitaSDK is not installed directly on the host.
