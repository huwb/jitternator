// This file is subject to the MIT License as seen in the root of this folder structure (LICENSE)
//

#pragma once

#include "FloatTime.h"

/**
* Mock game simulation
*/
class GameSimulation
{
public:
	void Update( const FloatTime& frameDt )
	{
		InputsUpdate( frameDt );
		AnimationUpdate( frameDt );
		//CameraUpdateWithRestOfGame( frameDt );
		PhysicsUpdate( frameDt );
		MainUpdate( frameDt );

		//CameraUpdateNoSim( frameDt );
		CameraUpdateEndFrame( frameDt );
	}

	void InputsUpdate( const FloatTime& frameDt )
	{
		_inputValLast = _inputVal;

		// get keyboard input - here just use dt as an arbitrary mock input.
		// assume our input value comes from the frame start time. may not always be the case!
		_inputVal = FloatTime( 30.0f, frameDt );
	}

	FloatTime SampleAnimation( const FloatTime& frameDt )
	{
		// evaluate animation at end frame time - I've seen this on previous projects

		// animated value is just a linear curve
		FloatTime val = FloatTime( 5.0f * frameDt.Time(), frameDt );

		// now move val forward to end frame time
		val.FinishedUpdate( frameDt );

		return val;
	}

	void AnimationUpdate( const FloatTime& frameDt )
	{
		// assume that the sampled animation gives the end-frame (rendered) values

		// in this case, the start frame values are the end frame values from the previous frame
		_carAnimTargetPos = _carAnimTargetPosEndFrame;

		// compute a new end frame value
		_carAnimTargetPosEndFrame = SampleAnimation( frameDt );
	}

	void PhysicsUpdate( const FloatTime& frameDt )
	{
		// _physTimeBalance is the cumulative delta between game update time and physics update time. this could be changed
		// to be just a float instead of a FloatTime, but putting this delta on the physics time gives a little bit of
		// additional validation that the simulation is consistent. 
		_physTimeBalance += FloatTime( frameDt.Value(), _physicsDt );

		if( _physTimeBalance.Value() <= 0.0f )
		{
			// nothing to be done - physics is already up to date
			return;
		}

		// this will be used to interpolate physics -> camera time
		CarState lastState;

		// loop while we still have outstanding time to simulate - while physics is behind camera shutter time
		do
		{
			lastState = _carStateLatest;

			PhysicsUpdateStep( frameDt, _physicsDt );

			// update balance and move value forward in time in one fell swoop, by using the integrate function
			// with a velocity of -1, which will make the value decrease by the amount of time moved forward.
			_physTimeBalance.Integrate( FloatTime( -1.0f, _physicsDt ), _physicsDt );

			AdvanceDt( _physicsDt );

		} while( _physTimeBalance.Value() > 0.0f );

		// now interpolate the physics state at the camera shutter time

		// cam shutter time is current time + delta time
		FloatTime camShutterTime = FloatTime( frameDt.Time(), frameDt ) + frameDt;
		
		_carStateCurrent._pos = FloatTime::LerpToTime( lastState._pos, _carStateLatest._pos, camShutterTime );
		_carStateCurrent._vel = FloatTime::LerpToTime( lastState._vel, _carStateLatest._vel, camShutterTime );

		// optional assert to ensure two separate values are in sync
		CheckConsistency( _carStateCurrent._pos, _carStateCurrent._vel );
	}

	void PhysicsUpdateStep( const FloatTime& frameDt, const FloatTime& physicsDt )
	{
		// we do multiple physics updates in a frame. here the update takes values from 2 sources:
		
		// - animation data - we could potentially subsample the animation to give fresh data for each physics step. instead we
		//   knowingly and explicitly use the state start frame value by resampling at the current physics time:
		CheckConsistency( _carAnimTargetPos, frameDt );
		FloatTime carAnimTargetPos_const = FloatTime( _carAnimTargetPos.Value(), physicsDt );
		
		// - input values - again we explicitly reuse stale data, but in some scnarios (VR) we may sample up to date fresh values here
		//   and would not need to hack this:
		CheckConsistency( _inputVal, frameDt );
		FloatTime inputVal_const = FloatTime( _inputVal.Value(), physicsDt );
		
		FloatTime accel = inputVal_const + (carAnimTargetPos_const - _carStateLatest._pos);

		_carStateLatest._pos.Integrate( _carStateLatest._vel, physicsDt );
		_carStateLatest._vel.Integrate( accel, physicsDt );
	}

	void MainUpdate( const FloatTime& frameDt )
	{
		// systems update - ai, logic, etc
	}

	// this can be run after the other bits in the game are updated. it doesn't use the dt value in the time update.
	void CameraUpdateNoSim( const FloatTime& frameDt )
	{
		// use the frame time giver, advanced to the end of the frame
		FloatTime cameraDt = frameDt;
		AdvanceDt( cameraDt );

		// place camera two units behind car (locked - no dynamics!)
		_cameraPos = _carStateCurrent._pos - FloatTime( 2.0f, cameraDt );

		// add a bit of user input. we decide here to take the start frame input values, and therefore set the time manually
		_cameraPos += FloatTime( 0.1f * _inputVal.Value(), cameraDt );
	}

	// this one should be run with start frame data (i.e. before car updates)
	void CameraUpdateWithRestOfGame( const FloatTime& frameDt )
	{
		// lerp camera towards car
		_cameraPos = FloatTime::Lerp( _cameraPos, _carStateCurrent._pos, FloatTime( 6.0f, frameDt ) * frameDt );

		// add influence from changing input
		if( _inputVal.Time() > _inputValLast.Time() )
		{
			_cameraPos += FloatTime( 0.2f, frameDt ) * Vel( _inputValLast, _inputVal );
		}

		// add influence from speed
		_cameraPos -= _carStateCurrent._vel * FloatTime( 0.1f, frameDt );

		_cameraPos.FinishedUpdate( frameDt );
	}

	// a scheme i often see is cameras are updated at the end of the frame, using end frame values. this function tries to implement this,
	// but is not consistent, see the comments below.
	void CameraUpdateEndFrame( const FloatTime& frameDt )
	{
		// SCHEME: sample car position etc at the end frame values, and then simulate forwards from end this frame state. so the from-time is
		// the end frame time, and to to-time must then be 1 frame ahead. unfortunately this does not work in this strict framework because
		// we don't know how far forward to advance the camera sim, because we don't know the next frame dt (this is typically measured from
		// real time at the end of the frame).

		// so instead we take the end frme state but set the times to be the start frame state, and then update from there. this feels similar to
		// operator splitting for solving differential equations. there is some error from this but i dont have a clear understanding
		// of if or when this error would manifest as jitter.

		// sample at start frame time
		FloatTime camCarPos = FloatTime( _carStateCurrent._pos.Value(), frameDt );
		FloatTime camCarVel = FloatTime( _carStateCurrent._vel.Value(), frameDt );

		// lerp camera towards car
		_cameraPos = FloatTime::Lerp( _cameraPos, camCarPos, FloatTime( 6.0f, frameDt ) * frameDt );

		// add influence from changing input
		if( _inputVal.Time() > _inputValLast.Time() )
		{
			_cameraPos += FloatTime( 0.2f, frameDt ) * Vel( _inputValLast, _inputVal );
		}

		// add influence from speed
		_cameraPos -= camCarVel * FloatTime( 0.1f, frameDt );

		_cameraPos.FinishedUpdate( frameDt );
	}

	// sample end frame state and render
	void Render( const FloatTime& frameDt )
	{
		// sample simulation state
		const FloatTime renderCarPos = _carStateCurrent._pos;
		const FloatTime renderCarVel = _carStateCurrent._vel;
		const FloatTime renderCamPos = _cameraPos;
		
		// strong check - everything should be at end frame time
		FloatTime endFrameTime = frameDt;
		AdvanceDt( endFrameTime );

		CheckConsistency( renderCarPos, endFrameTime );
		CheckConsistency( renderCarVel, endFrameTime );
		CheckConsistency( renderCamPos, endFrameTime );

		// the 'render'
		printf( "Car pos: %f\n", _carStateCurrent._pos.Value() );
	}

	struct CarState
	{
		FloatTime _pos = FloatTime::SimStartValue( 0.0f );
		FloatTime _vel = FloatTime::SimStartValue( 0.0f );
	};

	CarState _carStateLatest;
	CarState _carStateCurrent;

	FloatTime _inputVal = FloatTime::SimStartValue( 0.0f );
	FloatTime _inputValLast = FloatTime::SimStartValue( 0.0f );

	FloatTime _carAnimTargetPos = FloatTime::SimStartValue( 0.0f );
	FloatTime _carAnimTargetPosEndFrame = FloatTime::SimStartValue( 0.0f );

	FloatTime _physTimeBalance = FloatTime::SimStartValue( 0.0f );
	FloatTime _physicsDt = FloatTime::SimStartValue( 1.0f / 64.0f );

	FloatTime _cameraPos = FloatTime::SimStartValue( 0.0f );
};
