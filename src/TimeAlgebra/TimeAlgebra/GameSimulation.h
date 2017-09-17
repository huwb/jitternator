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
	GameSimulation()
	{
	}

	void Init( const FloatTime& firstFrameDt )
	{
		_carStateLatest._pos = FloatTime( 0.0f, _physicsDt );
		_carStateLatest._vel = FloatTime( 0.0f, _physicsDt );

		_cameraPos.FinishedUpdate( firstFrameDt );
	}

	void Update( const FloatTime& frameDt )
	{
		InputsUpdate( frameDt );
		AnimationUpdate( frameDt );
		PhysicsUpdate( frameDt );
		MainUpdate( frameDt );

		CameraUpdate( frameDt );
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
		// animated value is just a linear curve
		float value = 5.0f * frameDt.Time();
		// evaluate animation at end frame time - I've seen this on previous projects
		float time = frameDt.Time() + frameDt.Value();

		return DebugConstructFloatTime(value, time);
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

	void CameraUpdate( const FloatTime& frameDt )
	{
		// scheme: sample car position etc at the end frame values, and then simulate forwards from end frame. so the from-time is
		// the end frame time, and to to-time must then be 1 frame ahead
		//_cameraDt = FloatTime( frameDt.Value(), frameDt.Value() + frameDt.Time() );
		_cameraDt = frameDt;
		AdvanceDt( _cameraDt );

		// does not work!! we don't know the dt for the next frame, so we don't know how far forward to simulate the camera!
		// HACK FIX!! reset the time
		_cameraPos = FloatTime( _cameraPos.Value(), _cameraDt );

		// lerp camera towards car
		_cameraPos = FloatTime::Lerp( _cameraPos, _carStateCurrent._pos, FloatTime( 6.0f * _cameraDt.Value(), _cameraDt ) );

		// add influence from changing input
		if( _inputVal.Time() > _inputValLast.Time() )
		{
			FloatTime velDep = Vel( _inputValLast, _inputVal );

			// another thing that does not work! we don't have the end-frame inputs! we have the inputs from the beginning of the frame.
			// this might be deemed "ok", or the inputs could be sampled at the end up the main update but before cameras..!?

			// HACK FIX!! force time to be correct
			_cameraPos += FloatTime( velDep.Value(), _cameraDt );
		}

		// add influence from speed
		_cameraPos -= _carStateCurrent._vel * FloatTime( 0.1f, _cameraDt );

		_cameraPos.FinishedUpdate( _cameraDt );

		// the print function could also check time consistency? any output flow should check.
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

	FloatTime _cameraDt = FloatTime::SimStartValue( 0.0f );
	FloatTime _cameraPos = FloatTime::SimStartValue( 0.0f );
};
