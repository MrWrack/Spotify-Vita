# v11 build fix

Fixed the first real VitaSDK GitHub Actions compiler errors:

- `SCE_KERNEL_EVF_WAITMODE_OR` -> `SCE_EVENT_WAITOR`
- `SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT` -> `SCE_EVENT_WAITCLEAR_PAT`
- renamed the `test_cover.h` include guard so it no longer collides with
  the `TEST_COVER_H` height constant.

Push v11 and rerun **VitaSDK Build**.
