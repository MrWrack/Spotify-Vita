# CI Build — v10

v10 adds an automated VitaSDK cross-build using GitHub Actions.

## Workflow

File:

```text
.github/workflows/vita-build.yml
```

Triggers:

- push to `main`
- push to `master`
- pull request
- manual `workflow_dispatch`

The job uses the official VitaSDK Docker image:

```text
vitasdk/vitasdk:2026.08
```

The VitaSDK site documents this image series and shows the same CMake toolchain
pattern for Docker builds.

## What the CI checks

The build fails unless both exist:

```text
build/spotify-vita.vpk
build/eboot.bin
```

On success it uploads a GitHub Actions artifact named:

```text
spotify-vita-vpk
```

containing the VPK and eboot.

## Local Docker build

If Docker is installed:

```sh
./build-docker.sh
```

This uses the same image and build commands as CI.

## Why this matters

The ChatGPT execution environment used to create v9/v10 does not contain
`arm-vita-eabi-gcc`, so it cannot perform a real Vita cross-link locally.

The GitHub workflow moves that validation into an environment that does contain
the actual VitaSDK toolchain. Any compiler or linker failure will therefore be
a real actionable VitaSDK error rather than a static guess.

## Next after the first failed/successful CI run

If CI fails, copy/upload the Actions log and the source can be patched from the
exact error messages.

If CI succeeds, download:

```text
spotify-vita-vpk
```

from the workflow run and install `spotify-vita.vpk` through VitaShell for the
hardware test.
