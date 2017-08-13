# jitternator
Lessons learnt from hunting jitter issues


## Introduction

In the past I've spent weeks painstakingly hunting down jitter issues - visible stutter in games. I decided to document the lessons and techniques I picked up along the way in the hope that it might help others.

I have also added an experimental implementation of attaching timestamps to data to enforce consistency and detect timing issues at runtime.


## Preparation

The following preparation will help greatly when diagnosing jitter issues.

Check how dt is computed in the game update loop - typically the dt for frame N is the measured real time that passed while processing the previous frame N-1.
This time might be clamped, filtered, etc.
It can be useful to turn any such frame time modifications off when debugging jitter, to simplify the behaviour and avoid confusion.
Similarly if there is a max update dt or max substep count for physics, consider removing it, or checking that the game won't hit this limit for you while testing, as this would cause "legitimate" jitter which would not occur in practice.

Obtain an unsteady FPS.
Many/most jitter issues will not be visible when framerate is constant and consistent. UE4 has a great feature for this - t.UnsteadyFPS, which will set the frame dt to a random value each frame.
Do all of the tests below with unsteady FPS.
Something else I've done in the past is to insert a really long frame every 5 seconds using a sleep statement or equivalent at the beginning of the update, which might give more predictable/repeatable results.


## Plot Data vs Time

A good quick sanity check to see if some game data is smooth/not jittering is to print out the end frame time and end frame values to the log. I frequently hack in something like the below which generates an output that is very quick to pasted into Excel and plot:

```cpp
static bool doPrint = false;
if( doPrint )
{
	printf("time:\t$f\tvalue1:\t%f\tvalue2:\t%f\n", endFrameTime, value1, value2 );
}
```

The static can be switched on at run time by setting a breakpoint and then poking a value in through the debugger (in VS mouse over doPrint and click the Pin, then set the value to 0 or 1 to turn it on and off).

In the left plot below, there is a long frame around 0.5s, but an incorrect dt is being used and a discontinuity appears in the plot as a step. The right hand side shows the correct result - the trajectory is continuous and smooth despite the long frame.

![PlotData](https://raw.githubusercontent.com/huwb/jitternator/master/img/plot_data.png)  


## Update Analysis

A good way to build a picture of a game update is to put breakpoints in all the update functions for major parts of the engine, as well as a breakpoint in the highest level game loop, and check the order in which updates get called. Some good suggestions for things to breakpoint include physics update, normal actor update, camera update, blueprint update, animation system update, etc. 

Write down each update major component of the update in order in a list. Next to the list draw two vertical lines, one for the start frame time, one for the end frame time. Now draw arrows wherever one system reads data from another. If the camera system updates after the character and reads the characters position, draw an arrow from the end frame line at the character row to the beginning of the camera update.

Ideally, the entire game state would be cached off at the beginning of the frame, and all game components/systems would read from the left line, and then all use the start frame data to move themselves across to the end frame state. In practice this is usually not the case - typically the physics is ticked near the beginning of the frame, and then everything else in the frame is then taking the end frame state of any physics objects. This is not necessarily a problem - but whenever one finds arrows drawn from both start frame AND end frame states, this is typically bad news - the system is sampling data from two different points in simulated time and this kind of situation leads to jitter. The way to address this is to shuffle around the update order of game systems by moving their update hooks (UE4 - change TickGroup, Unity - change between Update and LateUpdate, or specify script execution order).


## Debugging Experiments / Tests

Unfortunately I've found jitter is usually caused by a number of overlapping issues which need to be teased apart individually. This section gives a list of experiments to test and validate each part of the update. All of these should be performed with an unsteady framerate - as described above.

1. Check debug drawing works. At the end of the camera update, draw a debug sphere somewhere in front of the final camera viewpoint, perhaps in the lower half of the screen. Run the game - the sphere should be absolutely, 100% stable. If not, investigate further. Perhaps the debug draw command is skipping the render queue - try drawing the sphere in front of the camera a frame or two later (I've encountered this in the past). Debug draw is invaluable for solving jitter issues - don't proceed further until you have a stable sphere at unsteady FPS.
2. Test the camera can move at a constant velocity. Using a simple bit of code in the game update, move a debug cube through the world. Use a constant velocity vector and integrate it onto a position each frame by multiplying by the total frame dt. Now run the game and follow the cube with the in game camera. Is the cube jitter free? If it is not, something is wrong with the camera update - it's not updating to the shutter time/render time/end frame time. Strip away layers/behaviours from the camera until it becomes very simple. Is it taking the correct dt? If so, then perhaps some input to the camera update is at the wrong time. The following points test whether the character/actor is updating properly, and whether values coming from the animation system have the correct timestamps.
3. Knock out all of the camera code, then add a few lines that move the camera programmatically at a fixed velocity (similar to how the cube was moved in item 2). This will put the camera at a known correct end-frame position ready for rendering.
4. Move the character/vehicle/etc in front of the camera, while the camera moving at constant speed. Does the character appear to jitter? If so, it is not being updated to its correct end frame position. Perhaps it is updating with the wrong dt, or the renderer is not taking its final position for some reason - perhaps draw a debug sphere at the characters position to verify it is not a problem with the rendering of the character. If the character is physics-driven (its position is updated during physics update), then it will be on a different update type (fixed steps) compared to the frame update and the state (pos, vel, orient) need to be interpolated (if the physics steps past the shutter time) or extrapolated. Unity exposes these options on rigidbodies but it is not on by default. Note that Unity does not step the physics past the shutter time. When set to interpolation, Unity interpolates the state to a fixed offset of 1/physics rate before the shutter time. This means that the actor is technically not at the correct pos etc for the shutter time, but is always offset by a fixed amount.
5. The hacked camera on a rail from item 3 can be used to verify the animation system is producing values at the correct time. Animate a cube nearby the camera using keyframes. The cube should appear frame-perfect without jitter. If the cube appears to jitter, the animation is not being evaluated at the camera shutter time, which means something is broken.
6. Dig in for the long haul, and persevere. If jitter is occurring, there will be a good reason! The inputs/gameplay/etc may break down at low/unsteady FPS, but there should never be visible jitter!

Each of the above were actual jitter issues I've encountered in the past, and usually more than one at a time.


## Adding Timestamps To Data

As an experiment I added some code in this repository which allows timestamps to be associated with data (floats only). The aim is to bring the concept of simulation time to the forefront and enforce consistency and correctness in updates, inspired somewhat by the data ownership patterns that Rust enforces. The results were interesting - there were points where the update code was not strictly correct; having timestamps forces the developer to either fix the issue, or explicitly acknowledge and workaround these issues in the code.

The first issue it caught was when I mocked up some code to compute an acceleration to integrate onto a velocity, which in turn is integrated onto a position, as follows:

```cpp
		// compute acceleration by comparing a target position with the current position
		FloatTime accel = targetPos - pos;

		// move state of vel forward in time
		vel.Integrate( accel, dt );

		// move state of pos forward in time
		pos.Integrate( vel, dt );
```

 This is wrong because the position and velocity should both be updated from the start frame state - it's wrong to update vel and then used the updated vel to update pos. The start frame state should be used to update both. In this case, the consistency checking throws an error and it can be seen that the data timestamps don't match in the pos.Integrate() line. I tried to switch the order of the pos and vel integration as follows, which I felt confident would fix it:

```cpp
		// move state of pos forward in time
		pos.Integrate( vel, dt );

		// compute acceleration by comparing a target position with the current position
		FloatTime accel = targetPos - pos;

		// move state of vel forward in time
		vel.Integrate( accel, dt );
```

However this also throws an error because the accel computation is now using the end frame position, instead of taking consistent, start frame state. The real fix is this:

```cpp
		// compute acceleration by comparing a target position with the current position
		FloatTime accel = targetPos - pos;

		// move state of pos forward in time
		pos.Integrate( vel, dt );

		// move state of vel forward in time
		vel.Integrate( accel, dt );
```

Now the accel has a start frame timestamp because both targetPos and pos are at start frame values, and both pos and vel update from start frame value to end frame value. The final fix is counterintuitive and surprising to me, and *really* easy to accidentally get wrong without consistency checking.

A more involved example of an issue being caught and made explicit is a physics object which reads the player input and a keyframe animated value each physics substep. Both of these values are only updated once at the start of the frame, not for each physics substep, and therefore the timestamps won't match and the code will fail, unless it is explicitly told to ignore the timestamp. Which may be ok - using only the start frame user input values would be considered normal, but still better to be explicit about it IMO. On the other hand, the animated value having the wrong time might be more serious. One of the trickier jitter issues I've looked at in the past was using PD control to make a rigidbody follow an animated transform. An interpolator was necessary to give target transforms at the correct times for each physics substep.

Implementing this into a C++ engine would be super-invasive and does not feel practical, at least in its current form. It might be an easier fit to other languages though.

The code is a (hastily hacked together) VS2015 project.
