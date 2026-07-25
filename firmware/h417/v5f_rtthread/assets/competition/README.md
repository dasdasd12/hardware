# V5F competition boot assets

The source PNGs are official 2026 competition artwork:

- `source/socchina-logo-official.png` comes from the competition website header: <https://www.socchina.net/images/logo.png>
- `source/2026-slogan-official.png` comes from the official 2026 download-center item “2026应用赛道口号（打板丝印）”: <https://socoss.socchina.net/file/cacheFile/2026-3-3/9521a76c288246f8b32d75fdfcad97e1.png>
- Official context and wording: <https://www.socchina.net/details?id=b1f52d20f4544949bda6c89fefb66c74>

Pinned SHA-256 values:

- `socchina-logo-official.png`: `F149A459F218571F0110354FF4DE295E87D065D5219304086D72B841F9040AAD`
- `2026-slogan-official.png`: `F8A6C8928F319B5BDA4F8357D360E89AD93BC5251FAC022882D4CC5EF69FD388`

The website's tree mark is only 180×118 and includes a grey caption. The asset
builder selects the blue mark only, removes that caption, rescales the mark,
and packs the logo and slogan into two 1-bit masks. Firmware renders both masks
with the official slogan colour `#1F4E79` on white.

Regenerate from `hardware`:

```powershell
python .\firmware\h417\v5f_rtthread\tools\make_competition_ui_assets.py
```

Generated outputs:

- `applications/v5f_competition_assets.h` — firmware masks
- `generated/v5f_competition_boot_preview.png` — 800×480 visual preview
