HD textures for the world (0.5.197)
===================================

The port can show sharper copies of the game's own textures: bushes, trees,
walls, ground, vehicles and the guns in your hands. They are made on your PC
from your copy of the game and live only on your graphics card; the game's
memory is not touched, so they need no extra game memory.

Make them for a map
-------------------
1. Close the game. Open CoD3Recomp.ini (beside CoD3MP.exe) and set
       texture_dump = 1
2. Start the game and play the map you want for a few minutes. Walk around
   the whole map: every texture you see is saved once (textures\dump) and
   listed for that map (textures\maps\<map>.txt).
3. Quit the game and double-click UPSCALE-TEXTURES.bat. The first time it
   offers to download two free tools (Real-ESRGAN, the upscaler, and
   Microsoft's texconv) into tools\upscale. Pick the map. It upscales every
   texture of that map on your graphics card (4x; 2x for the big ones,
   never past 2048), keeps leaf and fence edges crisp, leaves normal maps
   alone, and writes them to textures\replace\hd as compressed .dds files.
   A map takes a few minutes to half an hour, depending on your card.
4. Set texture_dump = 0 again and start the game.

The log (CoD3MP.log) says how many replacements are loaded and how much
graphics memory they take ("textures: 300 replacements loaded so far, 900 MB
on the graphics card"). If your card runs short, run the script with
-MaxSize 1024 after deleting textures\replace\hd.

Sharing: MAKE-TEXTURE-PACK.bat zips textures\replace (HD textures
included) for friends on the same port. Never upload them publicly: they are
made from the game's own art.

To go back to the original textures, delete textures\replace\hd.
