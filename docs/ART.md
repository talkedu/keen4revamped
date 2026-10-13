# Art pipeline and the remaster renderer

Milestone 6 adds a second renderer next to the classic one. The game logic is the
same; only how frames are drawn changes.

| | Classic (`--classic`) | Remaster (default) |
| --- | --- | --- |
| View | 320x200, 4:3 like a CRT | 356x200, 16:9 at the original height |
| Resolution | 320x200 scaled up | native window resolution |
| Art | original EGA graphics | `art/` (with `--curated`, `art-curated/` first), else the original scaled 4x (Scale4x) |
| Motion | 35 frames/s, as on DOS | interpolated between game frames at the display's refresh rate |

Press **F11** to switch renderers while playing, **F12** to reload the art manifests and
**Shift+F12** to show sprite hitboxes over the art.

## How the remaster renderer works

1. Every refresh of the tile scroller (`RF_Refresh`) records a scene: the camera, the
   tiles around the view and the sprites in draw order (`src/render/scene.h`). The
   classic 320x200 frame is drawn exactly as before; frame hashes of the four demos
   are unchanged.
2. The SDL backend redraws that scene with textures at the window's resolution
   (`src/platform/sdl3/remaster_renderer.cpp`): background tiles, foreground tiles,
   sprites by priority, then the foreground tiles that cover sprites.
3. Camera and sprites move smoothly between the last two scenes. Anything the game
   draws over the play view (status window, dialogs) is found by comparing the frame
   with the scene and shown from the classic frame; screens without a play view
   (title, menus, help) are shown classic.
4. The wider view never shows the map border the classic camera hides.
5. Collision boxes always come from the original sprite table; art cannot change
   gameplay.

## Making art

```sh
keen4-extract --data path/to/keen4 --out extracted-art
python3 tools/keen4-upscale/keen4_upscale.py extracted-art art            # Scale4x
python3 tools/keen4-upscale/keen4_upscale.py extracted-art art --method realesrgan
keen4 --art art
```

- **keen4-extract** writes every tile, foreground tile, sprite and picture as a 1x PNG,
  Keen's own sprites a second time in Tom's colours (`sprite2/`, see below),
  sheets of all tiles laid out 18 wide as the levels use them, `index.txt` with the
  sheets and sprite animation strips, and a context patch per tile: the tile with the 8
  tiles around its first use in a level.
- **keen4-upscale** builds the 4x set: tiles are upscaled with their real level
  neighbours (the centre of each context patch, falling back to the sheets for tiles no
  level uses) so edges match; sprites per animation strip; sprite transparency on its
  own, re-thresholded to a hard edge; foreground tiles keep the original outline. `--method realesrgan` runs the Real-ESRGAN program
  (`realesrgan-ncnn-vulkan`, model `realesrgan-x4plus-anime` by default); check a
  model's licence before using its output. It also writes `review.html` with each
  original next to its result.
- Real-ESRGAN runs once per image size: the program returned corrupted images when one
  run mixed sizes. A Windows `realesrgan-ncnn-vulkan.exe` can be driven from WSL. The
  full set (about 2,900 images in 127 runs) takes about 7 minutes on an RTX 5070 and needs about
  15 GB of RAM for the large tile sheets.
- Extracted and generated art is derived from the game data and stays out of the
  repository, like the data itself.

## Tom, player 2 in co-op

In co-op, player 2 is Tom, Billy's twin brother: Keen's sprites with a red helmet, a
green shirt and gray pants. The classic view draws him through a colour table. The
remaster draws him from `sprite2` assets, Tom's copy of each of Keen's sprite chunks
(on foot, on the world map and swimming), listed by the same chunk numbers:

- keen4-extract recolours the indexed original first (`sprite2/NNNN.png`, `strip2`
  lines in `index.txt`), so the upscaler works from Tom's colours rather than
  recolouring the upscaled Billy.
- keen4-upscale upscales them per animation strip like the other sprites and lists them
  as `sprite2 N sprite2/NNNN.png`.
- Without a `sprite2` entry, the remaster recolours the original and scales it up itself.
- keen4-curate takes `sprite2` as a kind, so Tom can be curated like any sprite.

## Curated art over the AI set

Art folders are layered. Hand-drawn or hand-fixed art goes in its own folder, listed
before the generated set, and replaces it one tile or sprite at a time:

```sh
python3 tools/keen4-curate/keen4_curate.py add sprite 130-139 --from art-ai --to art-curated
# paint over art-curated/sprite/0130.png ... then press F12 in the game
keen4 --curated --art art-ai      # same as --art art-curated --art art-ai
```

- Curated art is only used with `--curated`, which puts `art-curated/` in front of the other
  folders. Without it the game shows just the upscaled set.
- `--art` can be given several times; the first folder whose manifest lists an asset
  provides it. Without `--art` the upscaled set is `art/`.
- `bad` in a curated manifest line skips that folder for the asset, so it shows the next
  folder's art; a missing or unreadable file does the same, with a message on stderr.
- **keen4-curate** `add` copies an asset's current image and anchor from the generated set
  into the curated folder and lists it there, so the artist starts from the AI version;
  `remove` drops the line again (the file stays); `list` shows what is curated. Numbers can
  be ranges (`130-139`).
- Curated art may be bigger than the original (a scarf, a glow): set `anchor ax ay` to where
  the original's top-left sits in the new image.
- Hitboxes never come from the art. **Shift+F12** (or `--hitboxes`) outlines every sprite's
  art, yellow from the first folder, cyan from later ones and grey for the built-in
  upscale, with the original hitbox from the sprite table in red. Art that reaches well
  outside its red box, or leaves parts of it empty, will look wrong when Keen touches it.
- `keen4 --headless --hd-shots --hitboxes --curated --art art-ai ...` renders the
  same view to PNG files for review without a window.
- Curated art stays out of this repository like the generated set (`/art-*/` is ignored).

## manifest.txt

One line per asset, art at 4x density, paths relative to the manifest (the same format in
every art folder):

```
tile16 120 tile16/0120.png
tile16m 45 tile16m/0045.png
sprite 140 sprite/0140.png anchor 8 4
sprite 141 sprite/0141.png bad
```

- `tile16` background tiles, `tile16m` foreground tiles, `sprite` sprite chunks, by
  their original numbers; `sprite2` Tom's copy of one of Keen's sprite chunks (co-op).
- `anchor ax ay`: where the original's top-left sits in the new image, in art pixels,
  for art bigger than the original (default 0 0).
- `bad`: ignore this entry and use the built-in Scale4x until the asset is redone.
- Assets not listed use the built-in Scale4x (`--nearest` for plain 4x pixels).
- The score box (sprite 253) always comes from the original: the game draws the score
  into it. Tom's score box is his own copy of it.
