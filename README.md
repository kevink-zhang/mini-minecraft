# Overview
## Instructions
Clone the repo and run in Qt Creator. Use the UPenn CIS 4600 Qt setup guide for specific settings.\
Demo video: https://www.youtube.com/watch?v=4Lo2-ypq7No

## Setting up and joining servers
To set up a server, enter your desired port number to host the server at in the startup screen. Once started, your ip address will be displayed in the info subscreen.\
To join a server, enter the ip address of the host and the matching port number.

## Game Controls 
Standard movement: WASD\
Manual rotation: Arrow keys\
Toggle flying mode: F\
Jump/fly up: Space\
Fly down: Shift\
Toggle inventory: E\
Interact with inventory: Right and left click and drag\
Select hotbar: 0-9\
Toggle sky: M\
Toggle chat/send message: Enter

# Milestone 3
# Kevin Zhang
## Armor and Health
Added armor and health bars. Armor bars depend on the armor equipped in the slots, while health can be lost from damage sources and regained through passive regeneration. Players also take fall damage proportional to fall distance (similar to actual Minecraft) and this damage is negated by water. 

## Player vs Player
Added server-side damage calculation of players based on armor and weapons. Hitting players also applies knockback effects on them. When players die, a death screen is applied and players can choose to respawn.

## Inventory blocks
Blocks can be held in the hotbar and used to place down blocks of a desired type.

## Fixes
Made rivers and oceans look nicer and less noisy. Multithreading was also improved so that running a server-client didn't block terrain generation as much. 


# Leon Kabue
## Day and Night Cycle
Created a procedural sky using ray tracing and had a day cycle in which the sun would move from one side to the other and night cycle in which the moon would stay at a position in the sky. Added clouds and a glow effect when the sun was rising or setting and interpollated between night and day sky at sunrise and sunset. Modified terrain light to use the sun or moon during day and night cycles respectively.

## Textures
Fixed issues with texture transparency and added more block textures to the game.


# Milestone 2

# Kevin Zhang
## Multithreading
See milestone 1
## Server
Set up a peer-to-peer server and packet system. Players can join other players servers by ip. Currently only packets for player movement and world initialization are fully implemented.
The server also calculates a safe world spawn point for survival-based gameplay.
## Font
Added a fontface texture that constructs and draws text from strings. This is used to indicate items in stacks and also for item name descriptions, player tags, and player chat.
## Inventory and Items
Added item and inventory GUI textures. A crosshair was added to the inventory and normal gameplay to identify the cursor position. Items can be moved around in the inventory with left and right click to separate and merge item stacks. The hotbar selection can also be changed with the numpad.

# Aaron Cheng
## Post-Process/Caves
## Caves
Added caves via 3D perlin noise, with a hard cut-off for lava. Used perlin worms for better terrain.
## Post-Process
Set up post-processing pipeline to create a water/lava effect.
## Physics
Played bobs in liquid (water/lava) and liquids now cause you to sink (non-collidable). Velocity in liquids is slowed.

# Leon Kabue
## Textures
Created a textures class for loading images as textures and binding them for drawing in mygl.
Enabled alpha blending of textures and used seperate vectors to store transparent blocks in a chunk and appended the transparent blocks at the end of the VBOinter so that they could be rendered after the opaque blocks.
Replaced color vbo with uv vbo for chunks and added new blocktypes, and configured their uv's using a seperate texture map from the one provided.
Altered the shaderpogram to include uv's and created handles for the in the respective shaders.
Animated water and lava texture blocks by sampling from different blocks adjacent to each other.

# Milestone 1

# Leon Kabue
## Tick 
Modified the key events to only update the input bundle when a key is pressed or released and only update a players position every tick
Implemented flight mode that allows player to defy gravity and move through walls and normal mode in which player is affected by gravity, had a boolean to keep track on which mode the player was in and this boolean.
Enabled rotation about global y axis whenever the mouse is clicked and dragged in the x direciton to allow rotation of the camera and player accordingly.
## Player Physics
Checked for collisions between player and blocks by casting a ray from each of the 12 corners of the imaginary player blocks, 4 representing the top, 4 for the middle and 4 for the bottom. I checked for each of the 3 axis seperately to allow the player to slide on walls when moving against them.
Added a negative y-acceleration to the velocity of the player each tick to simulate gravity and reduced the players velocity by 15 percent every tick to simulate drag
## Blocks
Enabled the player to remove the block at the center of the screen by casting a ray from the center and grid matching to see if any block was hit, and if so, placing an empty block.
Enabled the player to add a block to the block at the center of the screen by using the same method to grid match then replicating that block.
# Aaron Cheng
## Chunk Optimization
Created interleaved VBOs (pos/nor/col) and created new draw() in ShaderProgram accordingly. VBO data only consists of faces that are adjacent to an empty block. Also implemented terrain generation radius checks (check if chunk should be generated based off player position). Worked on resolving conflicts in merging all parts together.

# Kevin Zhang
## Biomes
 Used a bedrock map and a rainfall-temperature map with fBm and Perlin Noise to decide the placement of 10 biomes:
 (ocean, beaches, tundra, plains, desert, taiga, forest, savanna, swamp, rainforest, river).
 Also added BlockTypes to display some of these biomes in better contrast.
## Terrain height
 Used an "erosion" factor to blend a different set of height functions for each biome.
 Height functions used a variety of noise including "hybrid-multifractal" noise, fBm, Worley and Perlin. 
 Beaches create a shoreline by eroding ground terrain above bedrock next to oceans. Shoreline also varies in size by fBm noise.
 Snow, rock, and dirt height lines were also determined using noise functions for variance
## Rivers
 Used as a biome classification in our project. Generated using 2d Perlin worms and carved out during height map generation. River depth is still WIP, as alpha-rendering is still not implemented.
## Trees
 Made assets for oak and spruce trees, which have limited randomness. They are spawned in appropriate biomes.
 Added metadata storage to terrain class to store block updates on uninitialized threads to handle overflow into different chunks.
## Multithreading
 Terrain generation utilizes multithreading on a chunk by chunk basis. Used QThreadPool for threads, std mutexes for mutexes. Chunks are also deloaded from the VBO once 2 chunks outside of render distance.
 multithreading exists for BlockTypeWorkers, VBOWorkers, and StructureWorkers, which generate structures.
## Villages
 Attempt to generate one in an X by X area, which places a village center.
 To generate the village, I first created the road network with a L-system and path constraints to prevent road spaghetti.
 Then, buildings are placed next to roads by iterating through road segments and probabilistically attempting to place a building.
 Two building types modelled so far: library and small house. Buildings have different spawn chances depending on their distance from village center to simulate districts like public ultilities, commerical, residential, etc. Buildings will also carve out terrain and build ground platforms if generating in awkward areas.
 
