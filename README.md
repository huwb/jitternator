# jitternator
Lessons learnt from hunting jitter issues


## Introduction

I've spent weeks and weeks painstakingly hunting down jitter issues - visible stutter in games.

I want to document the lessons and techniques I picked up along the way.

I also have a toy implementation of attaching timestamps to data to enforce consistency and detect timing issues at runtime.


## Update Analysis

A good way to develop an understanding of a system is to put breakpoints in all the update functions for major parts of the engine, as well as a breakpoint in the highest level game loop, and check the order in which updates get called. Some good suggestions for things to put breakpoints in include physics update, normal actor update, camera update, blueprint update, etc. Also in the game loop how the dt is computed - typically the dt for frame N is the measured real-time that passed while processing the previous frame N-1. This time might be clamped, filtered, etc. It can be useful to turn any frame time manipulations off when debugging jitter, to simplify complexity.

Write down each update major component of the update in order in a list. Next to the list draw two vertical lines, one for the start frame time, one for the end frame time. Now draw arrows wherever one system reads data from another. If the camera system updates after the character and reads the characters position, draw an arrow from the end frame line at the character row to the beginning of the camera update.

Ideally, the entire game state would be cached off at the beginning of the frame, and all game components/systems would read from the left line, and then all use the start frame data to move themselves across to the end frame state. In practice this is usually not the case - typically the physics is ticked near the beginning of the frame, and then everything else in the frame is then taking the end frame state of any physics objects. This is not necessarily a problem - but whenever I find arrows drawn from both start frame AND end frame states, this is typically bad news - the system is sampling data from two different points in simulated time and this kind of situation leads to jitter. The way to address this is to shuffle around the update order of game systems by moving their update hooks (UE4 - change TickGroup, Unity - change between Update and LateUpdate, or specify script execution order).


## Debugging Steps

Unfortunately I've found jitter is usually caused by a number of overlapping issues which need to be teased apart individually. This section gives a list of experiments to test and validate each part of the update. All of these should be performed with an unstead framerate - see point 1 below.

1. Obtain an unsteady FPS. Many/most jitter issues will not be visible when framerate is consistent. UE4 has a great feature for this - t.UnsteadyFPS, which will set the frame dt to a random value each frame. Do all of the tests below with unsteady FPS. Something else I've done in the past is to insert a really long frame every 5s, which might give more predictable/repeatable results.
2. Check debug drawing works. At the end of the camera update, draw a debug sphere somewhere in front of the final camera viewpoint, perhaps in the lower half of the screen. Run the game - the sphere should be absolutely, 100% stable. If not, investigate further. Perhaps the debug draw command is skipping the render queue - try drawing the sphere in front of the camera a frame or two later (I've encountered this in the past). Debug draw is invaluable for solving jitter issues - don't proceed further until you have a stable sphere at unsteady FPS.
3. Test the camera can moved at a constant velocity. Using a simple bit of code perhaps at the end of the camera code, move a debug cube through the world. Use a constant velocity vector and integrate it onto a position each frame by multiplying by the total frame dt. Now run the game and follow the cube with the in game camera. Is the cube jitter free? If it is not, something is wrong with the camera update - it's not updating to the shutter time/render time/end frame time. Strip away layers/behaviours from the camera until it becomes very simple. Is it taking the correct dt? If so, then perhaps some input to the camera update is at the wrong time. The following points test whether the character/actor is updating properly, and whether values coming from the animation system have the correct timestamps.
4. Knock out all of the camera code, then add a few lines that move the camera programmatically at a fixed velocity (similar to how the cube was moved in point 3). This will put the camera at a known correct end-frame position ready for rendering.
5. Move the character/vehicle/etc in front of the camera, while the camera moving at constant speed. Does the character appear to jitter? If so, it is not being updated to its correct end frame position. Perhaps it is updating with the wrong dt, or the renderer is not taking its final position for some reason - perhaps draw a debug sphere at the characters position to verify it is not a problem with the rendering of the character. If the character is physics-driven (its position is updated during physics update), then it will be on a different update type (fixed steps) compared to the frame update and the state (pos, vel, orient) need to be interpolated (if the physics steps past the shutter time) or extrapolated. Unity exposes these options on rigidbodies but it is not on by default. Unity does not step the physics past the shutter time. When set to interpolation, Unity interpolates the state to a fixed offset of 1/physics rate before the shutter time. This means that the actor is technically not at the correct pos etc for the shutter time, but is always offset by a fixed amount.
6. The hacked camera on a rail from point 4 can be used to verify the animation system is producing values at the correct time. Animate a cube nearby the camera using keyframes. The cube should appear frame-perfect without jitter. If the cube appears to jitter, 

Each of the above were actual jitter issues I've encountered in the past, and usually more than one at a time.


## Adding Timestamps To Data

As an experiment I added some code in this repository which allows timestamps to be associated with data (floats only). The aim is to bring the concept of simulation time to the forefront and enforce consistency and correctness in updates, in a similar way to which Rust data ownership patterns. The results were interesting - there were points where the update code was not strictly correct; having timestamps forces the developer to acknowledge these issues in the code.

One example of an issue being caught and made expllicit is a physical object which reads the player input and a keyframe animated value during physics update. Both of these values are only updated once at the start of the frame, not for each physics substep, and therefore the timestamps won't match and the code will fail, unless it is explicitly told to ignore the timestamp. Using only the start frame user input values would be considered ok in general (but still interesting to be aware of). On the other hand, the animated value having the wrong time might be more serious. One of the trickier jitter issues I've looked at in the past was using PD control to make a rigidbody follow an animated transform. An interpolator was necessary to give target transforms at the correct times for each physics substep.
