# BUILD-FIX-FINAL report — v9

## Environment result

A real Vita cross-build was attempted as the first step.

This execution environment does **not** contain VitaSDK:

```text
VITASDK=
arm-vita-eabi-gcc: not found
```

Therefore no claim is made that the VPK is compiled or hardware-tested here.

## Fixes applied against current upstream headers/samples

### 1. Canonical VitaSDK CMake target

Changed from a target literally named `spotify-vita.elf` to:

```cmake
add_executable(${PROJECT_NAME} ...)
vita_create_self(eboot.bin ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk SPVT00001 eboot.bin ...)
```

### 2. Added `SceSysmodule_stub`

`vita2d` upstream sample links `SceSysmodule_stub`, and the application now
explicitly uses `sceSysmoduleLoadModule()`.

### 3. NET sysmodule lifecycle

`vita_network_init()` now loads `SCE_SYSMODULE_NET` before `sceNetInit()`,
matching VitaSDK's `net_http` sample.

It only unloads the module on shutdown if this application loaded it.

### 4. HTTP / SSL / HTTPS sysmodules

`spotify_http_init()` now loads:

```text
SCE_SYSMODULE_HTTP
SCE_SYSMODULE_SSL
SCE_SYSMODULE_HTTPS
```

before `sceHttpInit()` and tracks module ownership for clean shutdown.

### 5. Persistent token expiry fixed

`expires_at_ms` is based on `sceKernelGetProcessTimeWide()`. That timestamp
cannot be restored across app launches.

v9 now restores the refresh token but sets:

```c
auth->expires_at_ms = 0;
```

so the first authenticated API call forces a token refresh.

### 6. Token directory creation

Before writing:

```text
ux0:data/spotify-vita/session.bin
```

the app now calls:

```c
sceIoMkdir("ux0:data/spotify-vita", 0777);
```

### 7. OAuth callback decoding

Callback parameters now support percent decoding and `+`.

The parser also detects Spotify's OAuth `error=` callback separately instead
of treating it as a missing authorization code.

### 8. Build helper

Run on a VitaSDK machine:

```sh
./build-vita.sh
```

Expected final output:

```text
build/eboot.bin
build/spotify-vita.vpk
```

## Remaining hardware/build validation

The following cannot be confirmed without an actual VitaSDK install and Vita:

- exact final link set for the installed libvita2d package
- HTTPS certificate/TLS behavior on the target firmware
- system browser suspension/resume while loopback callback server is active
- whether the browser can reach 127.0.0.1 while the homebrew process is suspended
- physical rendering and input behavior
- JPEG/PNG cover decoding on device
- Spotify Web API behavior with a real client ID/account/device

The source now fails honestly if those platform layers fail rather than
pretending a VPK was produced.
