# jitternator

Lessons learnt from hunting jitter issues!


## Introduction

In the past I've spent weeks painstakingly hunting down jitter issues - visible stutter in games. I decided to document the lessons and techniques I picked up along the way in the hope that it might help others.

If some element of your game is jittering, the first thing to check is if you are updating the element (or the camera!) in fixed time steps? This is a specific type of jitter related to fixed time steps that is a subset of general jitter issues, but happen frequently enough that they are worth calling out specifically (if you have a camera moving in Update() chasing a moving rigidbody in Unity, it will jitter by default). There are notes and links in the Misc Notes section below about this specific case.

If the jitter is not related to fixed update, get ready to dig in for the long haul, and persevere. If jitter is occurring, there will be a good reason! The gameplay may break down at low/unsteady FPS, but there should *never* be visible jitter!

I have also added an experimental implementation of attaching timestamps to data to enforce consistency and detect timing issues at runtime, find details below.


## Preparation

The following preparation will help greatly when diagnosing jitter issues.

Check how dt is computed in the game update loop - typically the dt for frame N is the measured real time that passed while processing the previous frame N-1.
This time might be clamped, filtered, etc.
It can be useful to turn any such frame time modifications off when debugging jitter, to simplify the behaviour and avoid confusion.
Similarly if there is a max update dt or max substep count for physics, consider removing it, or checking that the game won't hit this limit for you while testing, as this may cause "legitimate" jitter which would not occur in practice.

Obtain an unsteady FPS.
Many/most jitter issues will not be visible when framerate is constant and consistent. UE4 has a great feature for this - t.UnsteadyFPS, which will set the frame dt to a random value each frame.
Do all of the tests below with unsteady FPS.
Something else I've done in the past is to insert a really long frame every 5 seconds using a sleep statement or equivalent at the beginning of the update, which might give more predictable/repeatable results.


## Plot Data vs Time

A good quick sanity check to see if some game data is smooth/not jittering is printing it out each frame to the output log, together with associated times. For example print out the end frame data values along with the end frame time. Keep in mind the physics time and frame end time may not coincide. I frequently hack in something like the below which generates an output that is very quick to paste into Excel and plot:

```cpp
static bool doPrint = false;
if( doPrint )
{
	printf("time:\t$f\tvalue1:\t%f\tvalue2:\t%f\n", endFrameTime, value1, value2 );
}
```

The static can be switched on at run time by setting a breakpoint and then poking a value in through the debugger (in VS mouse over doPrint and click the Pin, then set the value to 0 or 1 to turn it on and off).

Copy-paste the output onto an Excel spreadsheet, select the time and value columns, and insert an **X Y (Scatter)** chart. Be sure to use this chart type, not a Line chart which won't use the timestamps from the data.

Once plotted, visually inspect the graph for any non-smooth looking behaviour. As an example that may not be an obvious jitter problem at first, in the left plot below, there is a long frame around 0.5s, but an incorrect dt is being used and a discontinuity appears in the plot as a step. The right hand side shows the correct result - the trajectory is continuous and smooth despite the long frame.

![PlotData](https://raw.githubusercontent.com/huwb/jitternator/master/img/plot_data.png)

If there are multiple things moving together (such as a vehicle and a camera), make sure they both are smooth like the good graph above, and that they both take the jump across the long frame together at the same time.


## Update Analysis

A good way to build a picture of a game update is to put breakpoints in all the update functions for major parts of the engine, as well as a breakpoint in the highest level game loop, and check the order in which updates get called. Some good suggestions for things to breakpoint include physics update, normal actor update, camera update, blueprint update, animation system update, etc (when learning a codebase I like to step through all of he codebase and generated an indented bullet list of all the major update steps in the order they happen).

To analyse the flow of data during the update, write down each major component of the update in order in a list (left hand side of diagram below). Next to the list draw two vertical lines, one for the start frame time, one for the end frame time. Draw left-to-right arrows which depicts how each component updates (gray below). Now draw arrows (red) for all dependencies - when one component reads data from another.

![UpdateAnalysisBad](https://raw.githubusercontent.com/huwb/jitternator/master/img/update_analysis_bad.png)

The above engine evaluates animations at the end frame time first, then updates physics, then all gameplay code, and finally starts the render process. There are a few issues here, that will be addressed afterwards:

* The physics update is pulling animation data at the end frame time. If the animated value is a target position for PD control of a rigidbody (i.e. use a damped spring to match a position and velocity), this will jitter.
* The physics update is also pulling data from gameplay code like forces that influence the physics at the start frame time only. It is getting stale data in the second physics update, and the fact that this component pulls data from both the start frame time and end frame time has a bad smell.
* The render process takes the latest physics state for rendering. This data has a slightly different time - the timestamp of the latest physics data is greater than the end frame time/shutter time. This will jitter.

The following update order looks much healthier:

![UpdateAnalysisGood](https://raw.githubusercontent.com/huwb/jitternator/master/img/update_analysis_good.png)

* An interpolator has been added to provide the correct animation data at every point in time.
* The portion of the gameplay code that affects physics has been moved from Update() to PhysicsUpdate()/FixedUpdate(), and gives each physics update step fresh data.
* An interpolator has been added to provide the physics state at the camera shutter time.

The goal here is to have the right order of update of the components, with each update step for each component taking data for the current time (vertical red arrows on the diagram).

Ideally, the entire game state would be cached off at the beginning of the frame, and all game components/systems would read from the left line, and then all use the start frame data to move themselves across to the end frame state.
In practice this is usually not the case - typically the physics is ticked near the beginning of the frame, and then everything else in the frame is then taking the end frame state of any physics objects.
This is not necessarily a problem - but whenever one finds arrows drawn from both start frame AND end frame states, this is typically bad news - the system is sampling data from two different points in simulated time and this kind of situation leads to jitter.


## Debugging Experiments / Tests

There are a number of experiments that can be performed to probe the system and validate each part of the update flow. Often when I'm investigating jitter I find a number of simultaneous issues which conspire to produce complex and confusing behaviour. The below might help to validate each part of the update one at a time. All of these should be performed with an unsteady framerate - as described above.

1. Check debug drawing works. At the end of the camera update, draw a debug sphere somewhere in front of the final camera viewpoint, perhaps in the lower half of the screen. Run the game - the sphere should be absolutely, 100% stable. If not, investigate further. Perhaps the debug draw command is jumping the render queue - try drawing the sphere in front of the camera a frame or two later. Debug draw is invaluable for solving jitter issues - don't proceed further until you have a stable sphere at unsteady FPS.
2. Test the camera can move at a constant velocity. Using a simple bit of code in the game update, move a debug cube through the world. Use a constant velocity vector and integrate it onto a position each frame by multiplying by the total frame dt. Now run the game and follow the cube with the in game camera. Is the cube jitter free? If it is not, something is wrong with the camera update - it's not updating to the shutter time/render time/end frame time. Strip away layers/behaviours from the camera until it becomes very simple. Is it taking the correct dt? If so, then perhaps some input to the camera update is at the wrong time. The following points test whether the character/actor is updating properly, and whether values coming from the animation system have the correct timestamps.
3. Knock out all of the camera code, then add a few lines that move the camera programmatically at a fixed velocity (similar to how the cube was moved in item 2). This will put the camera at a known correct end-frame position ready for rendering. Now move the character/vehicle/etc in front of the camera, while the camera moving at constant speed. Does the character appear to jitter? If so, it is not being updated to its correct end frame position. Perhaps it is updating with the wrong dt, or the renderer is not taking its final position for some reason - perhaps draw a debug sphere at the characters position to verify it is not a problem with the rendering of the character. If the character is physics-driven (its position is updated during physics update), then it will be on a different update type (fixed steps) compared to the frame update and the state (pos, vel, orient) and it will need an interpolator as was done above. See Misc Notes below for more on physics related jitter.
4. The hacked camera on a rail from the previous item can be used to verify the animation system is producing values at the correct time. Animate a cube nearby the camera using keyframes. The cube should appear frame-perfect without jitter. If the cube appears to jitter, the animation is not being evaluated at the camera shutter time, which means something is broken.

Each of the above tests have revealed bugs for me in the past.


## Adding Timestamps To Data

After seeing the damage that mixing data from different times causes, I had the idea to associate a time with each bit of data in the simulation, and then to check consistency when data is manipulated to automatically detect many of the issues described above. This idea was inspired somewhat by the data ownership patterns that *Rust* enforces. There is a prototype in this repos which tests this concept, albeit in a hacky way (and floats only for now). The format is a VS2015 project.

The results were interesting - I mocked up some update code to test it out and it detected issues where the update flow was not strictly correct and forces the developer to either fix the issue, or explicitly acknowledge and workaround these issues in the code (the system must be intentionally overridden).

As a simple example of the kinds of issues this will catch, I mocked up some code below to compute an acceleration to integrate onto a velocity, which in turn is integrated onto a position, as follows:

```cpp
		// compute acceleration by comparing a target position with the current position
		FloatTime accel = targetPos - pos;

		// move state of vel forward in time
		vel.Integrate( accel, dt );

		// move state of pos forward in time
		pos.Integrate( vel, dt );
```

This is wrong because the position and velocity should both be updated from the start frame state - it's wrong to update vel and then used the updated vel to update pos:

![UpdateAnalysisVelPosBad1](https://raw.githubusercontent.com/huwb/jitternator/master/img/update_analysis_velpos_bad1.png)

Both should be updated purely from the start frame state. In this case, the consistency checking throws an error and it can be seen that the data timestamps don't match in the pos.Integrate() line. I tried to switch the order of the pos and vel integration as follows, which I felt confident would fix it:

```cpp
		// move state of pos forward in time
		pos.Integrate( vel, dt );

		// compute acceleration by comparing a target position with the current position
		FloatTime accel = targetPos - pos;

		// move state of vel forward in time
		vel.Integrate( accel, dt );
```

However this also throws an error because the accel computation is now using the end frame position, instead of taking consistent, start frame state. Diagram:

![UpdateAnalysisVelPosBad2](https://raw.githubusercontent.com/huwb/jitternator/master/img/update_analysis_velpos_bad2.png)

 The real fix is this:

```cpp
		// compute acceleration by comparing a target position with the current position
		FloatTime accel = targetPos - pos;

		// move state of pos forward in time
		pos.Integrate( vel, dt );

		// move state of vel forward in time
		vel.Integrate( accel, dt );
```

Now the accel has a start frame timestamp because both targetPos and pos are at start frame values, and both pos and vel update from start frame value to end frame value. Final update diagram:

![UpdateAnalysisVelPosGood](https://raw.githubusercontent.com/huwb/jitternator/master/img/update_analysis_velpos_good.png)

This final fix is unintuitive and surprised me, and is *really* easy to accidentally get wrong in practice.

Implementing this into a C++ engine would be super-invasive and does not feel practical, at least in its current form. It might be an easier fit to other languages though.

Such validation could probably be built into the compiler and could be an interesting direction for future work.


## Misc Notes

* If you have known update dependencies between scripts **do** explicitly set the update order, and put comments next to each update function recording the dependency, as future you or someone else may move the update code elsewhere later on. Even better document the update flow, something as simple as an indented list works well IMO (see [example](https://github.com/huwb/crest-oceanrender/blob/master/README.md#update-order)).
* Rigidbodies should be controlled using forces (you can prescribe a target position and velocity using PD control which essentially a damped spring) instead of placing them directly using their transform component, unless the rigidbody is set to be kinematic. I've seen jitter from a dynamic rigidbody being placed manually each frame in a position that intersects static collision or other rigidbodies. Dumping out collision pairs from the physics engine can help to identify this.
* As detailed above, anything on fixed update/physics update will jitter if its position is not interpolated at the end. Some engines will update the physics past the camera shutter time, so that the state at the shutter time can be interpolated and used for rendering. Unity rigidbodies are updated to within one fixed timestep of the target time, then:
  * If the *interpolation* property is set to *Extrapolate*, the rigidbody state is extrapolated to *shutter time* for rendering. Such linear extrapolation may be wrong under high accelerations and which may produce jitter (I have not encountered this though).
  * If the property is set to *Interpolate*, the render state will then be interpolated to *shutter time* - *fixed delta time*. This produces a jitter-free result because the render state is at a *constant* offset back in time. The side effect though is that it will render slightly behind its actual end frame state, due to the offset.
* There is a nice article about physics-related jitter by Gaffer On Games: https://gafferongames.com/post/fix_your_timestep/ . Note that this implements the equivalent of the Unity Interpolate mode described in the previous note, and therefore interpolates the physics to *target time* minus *fixed dt*.
