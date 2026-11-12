# Ksusha cooks a pie 3 — 3DS port (flat layout, no folders)

Files: source code (`main.c game.c game.h gamedata.c ui.c ui.h`), `Makefile`, `app.rsf`,
`icon.png` + `banner.png` + `banner.wav`, **`assets.zip`** (all textures/sounds, unpacked automatically
by the build), `build.yml` (GitHub build + QR code), `extras.zip` (converter, Linux simulator, screenshots — optional).

## Get the CIA + QR code with GitHub (works from a phone, no folders needed)
1. Create a **public** GitHub repository (public so the 3DS can download the release).
2. *Add file → Upload files*: select every file from this zip (including `assets.zip`, leave it zipped) and commit.
3. *Add file → Create new file*, type the name `.github/workflows/build.yml` (typing `/` makes the folders),
   paste the full text of `build.yml`, commit.
4. Open the **Actions** tab and wait for "Build 3DS game + QR code" (about 5–10 min).
5. Open **Releases → latest**: you get `KsushaPie3DS.cia`, `KsushaPie3DS.3dsx` and **`KsushaPie3DS_QR.png`**.
6. On the 3DS (custom firmware): **FBI → Remote Install → Scan QR Code**, scan the QR image from another screen, install.
   The QR always points to the same "latest" link, so it stays valid after future builds.

If a step in Actions fails, open the failed step's log — that is where a typo or a changed tool download URL would show up.

## Build locally instead
Install devkitPro (3ds-dev) and run `make` (unpacks `assets.zip` to `romfs/` itself) → `KsushaPie3DS.3dsx`.

## Controls
Touch bottom screen = tap/drag · Circle Pad / D-Pad = move cursor · A/B/X/Y = tap · R/L = fast/slow cursor ·
SELECT = restart · START = exit. Sound needs `sdmc:/3ds/dspfirm.cdc`.
If sprites rotate the wrong way, set `ROT_SIGN` to `-1.0f` at the top of `main.c`.

Status: game logic/assets/layout tested in the Linux simulator (`sh sim/run_sim.sh` after unzipping `extras.zip`);
`main.c` and the CIA build steps have not been run on real hardware/CI yet.
