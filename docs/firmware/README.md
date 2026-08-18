# Local firmware mirror

`syrup.bin` is replaced on every firmware pack (`cmake --build` / `./compile-with-docker.sh`).
Previous `docs/firmware/*.bin` files are deleted first so the flasher never serves a stale image.

The “远程获取” button loads this file from the site; GitHub Releases is only a fallback
when the mirror is missing.
