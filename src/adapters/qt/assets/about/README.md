# About page assets

`game-core-tech-logo.png` is the Game Core Tech mark Help › About shows
([[BPR-1720]]). Game Core Tech is the owner's own name, and the mark is
the owner's. It is this project's asset, under this project's licence and
copyright, and needs no attribution or third-party notice (owner, 2026-10-21).

It was copied from the owner's AllTheThings project, whose About window shows
the same mark: `Resources/GameCoreTechLogo.png` there, 1254 × 1254 RGB,
SHA-256 `0342e7e6b649455b860bec3b88a8744ce06065d6db05737d69a9f1e72fd926a2`.
That is the only version of the mark; no vector exists. The copy here is that
image downscaled once with Lanczos resampling to 640 × 640, enough for the
page's 176-pixel plate at 3x, and saved as a lossless PNG
(SHA-256 `39d76bf97150a22e894031b35b5c447879cdf47bcb8c8197872e5d265215ea39`):

```sh
python3 -I -c "from PIL import Image; Image.open('GameCoreTechLogo.png').convert('RGB').resize((640, 640), Image.LANCZOS).save('game-core-tech-logo.png', optimize=True)"
```

The page scales it once more, with area averaging, to the exact device size it
draws at, so 1x and 2x are each drawn from this master.
