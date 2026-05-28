COMP4033 Computer Graphics - Group Project
==========================================

Project: 3D Interactive Scene with 6 Rendering Effects
Engine:  OpenGL 3.3 (GLUT + GLEW + GLM)


HOW TO BUILD AND RUN
--------------------
1. Download updatedAssign0.zip from ispace and extract it (e.g. to a folder called "lightdemo").
2. Copy or clone the following into that folder:
     - Assign0\         (source files)
     - Assign0.sln      (solution file)
     - Debug\           (pre-bundled DLLs and resource files)
3. Open Assign0.sln with Visual Studio (2019 or 2022 recommended).
4. Select configuration: Debug | Win32
5. Build > Build Solution  (Ctrl+Shift+B)
6. The executable will appear at:  Debug\Assign0.exe
7. Double-click Debug\Assign0.exe to run.
   - The .exe must be run from the Debug\ folder (textures and shaders are loaded
     relative to the working directory).

If the build fails with "cannot open include file: GL/glew.h":
   - In VS, go to Project > Properties > C/C++ > General > Additional Include Directories
   - Point it to the folder that contains the GL\ subdirectory (e.g. glew-x.x\include)

If the build fails with "cannot open file glew32s.lib":
   - Go to Project > Properties > Linker > General > Additional Library Directories
   - Point it to the folder containing glew32s.lib


CONTROLS
--------
  W / S          Move forward / backward
  A / D          Strafe left / right
  Q / E          Move up / down
  Mouse drag     Rotate camera (left-button drag)
  Right-click    Reset camera to starting position
  Esc            Exit


IMPLEMENTED EFFECTS (8 total, well above the 6-effect bonus threshold)
-----------------------------------------------------------------------
1. Specular Reflections  [Requirement #5]
   - Blinn-Phong lighting model on all surfaces (rooms, actor, ball).
   - 3 point lights: one per room + one in the corridor.
   - Warm-tinted lights in the rooms, white light in the corridor.

2. Light Sources Visible to Camera  [Requirement #7]
   - Each of the 3 point lights has a small self-glowing emissive orb
     at ceiling height; orb brightness pulses in sync with light colour.

3. Animated Object - Bouncing Ball  [Requirement #2]
   - An orange-red sphere bounces around room 1, reflecting off all
     six surfaces (floor, ceiling, four walls).

4. Transparent Objects - Glass Panels  [Requirement #4]
   - Two blue-tinted glass panels stand in the corridor.
   - Alpha-blended, with specular highlights; rendered after all
     opaque geometry.

5. Particle System - Fire  [Requirement #3]
   - A torch fire effect in room 2 (near the south wall).
   - Particles use GL_POINTS with additive blending; colour fades from
     bright orange to transparent as particles age.

6. Shadow Mapping  [Requirement #6]
   - PCF (3x3 kernel) shadow map at 2048x2048 from an orthographic
     light above room 1.
   - The bouncing ball and the walking actor cast soft shadows on
     the floor. Best viewed by looking down at the floor in room 1.

7. Dynamic Textures  [Requirement #1]
   - Floor UV coordinates scroll continuously over time, creating the
     appearance of a slowly drifting surface pattern.
   - Room 1 and Room 2 warm lights pulse sinusoidally (brightness
     varies ~±10%), emissive orbs pulse in sync.

8. Bump Mapping  [Requirement #8]
   - Procedural bump mapping applied to all vertical wall surfaces.
   - Per-fragment tangent frame computed from dFdx/dFdy derivatives;
     a sine-based height field is used to perturb the surface normal,
     adding visible micro-detail to the wall texture under specular light.


SCENE DESCRIPTION
-----------------
  Room 1 (left):   Bouncing ball, Phong lighting, shadows on floor, table + chair.
  Corridor (centre): Two glass panels, visible light orb.
  Room 2 (right):  Walking actor destination, fire torch, table + chair.

The actor automatically walks between room 1 and room 2 via the corridor
and loops continuously.


FILE LIST
---------
  Assign0\assign0test.cpp   Main source (all rendering logic)
  Assign0\shaders.cpp/.h    Shader utility helpers
  Assign0\geometry3.h       Geometry utility (from framework)
  Assign0\stb_image.h       Image loading utility (from framework)
  Assign0\shaders\          GLSL shader reference files
  Assign0\*.ppm             Texture images (floor, wall, ceiling)
  Debug\                    Build output + runtime DLLs + textures
