SgsQtSpine - Pure QML + spine-c 3.6.38 + OpenGL (Qt 6.10.2 MinGW)

This version renders Region + Mesh attachments (no clipping yet).

Setup:
1) Put spine-c 3.6.38 runtime into third_party/spine-c:
   third_party/spine-c/include/spine/spine.h
   third_party/spine-c/include/spine/extension.h
   third_party/spine-c/src/*.c

2) Put your exported assets into ./assets (working directory / exe dir):
   assets/XingXiang.atlas
   assets/XingXiang.skel
   assets/BeiJing.atlas
   assets/BeiJing.skel
   plus ALL png pages referenced by the atlas files.

If atlas loads but textures fail, check atlas png paths.

Fixes: PMA-aware blending (reduces bright halo) + real delta-time animation speed (avoids running too fast on 120/144Hz).

QML composition: This build outputs premultiplied alpha in the fragment shader and uses premultiplied blending to reduce edge halos in Qt Quick.


Run loop: QML Timer drives SpineViewport.tick() at ~60fps (16ms). Renderer does not call update() every frame.
