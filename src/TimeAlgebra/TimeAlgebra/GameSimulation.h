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
	void Init( const FloatTime& firstFrameDt )
	{
		_carStateLatest.pos = FloatTime( 0.0f, _physicsDt.Time() );
		_carStateLatest.vel = FloatTime( 0.0f, _physicsDt.Time() );

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
		_inputVal = FloatTime( 30.0f, frameDt.Time() );
	}

	void AnimationUpdate( const FloatTime& frameDt )
	{
		_carAnimTargetPos = FloatTime( 5.0f * frameDt.Time(), frameDt.Time() /*+ frameDt.Value()*/ );
	}

	void PhysicsUpdate( const FloatTime& frameDt )
	{
		// _physTimeBalance straddles two timelines - it is updated in physics update which can
		// be ahead of frame update, so don't bother checking frame dt time
		_physTimeBalance += frameDt.StripTime();

		CarState lastState = _carStateLatest;

		int stepCount = 0;
		while( _physTimeBalance.Value() > 0.0f )
		{
			lastState = _carStateLatest;

			PhysicsUpdateStep( frameDt, _physicsDt );

			// update balance
			_physTimeBalance -= _physicsDt;
			_physTimeBalance.FinishedUpdate( _physicsDt );

			AdvanceDt( _physicsDt, _physicsDt.StripTime() );

			stepCount++;
		}

		if( stepCount > 0 )
		{
			float lerpAlpha = 1.0f + (_physTimeBalance / _physicsDt).Value();
			_carStateCurrent.pos = FloatTime::LerpInTime( lastState.pos, _carStateLatest.pos, lerpAlpha );
			_carStateCurrent.vel = FloatTime::LerpInTime( lastState.vel, _carStateLatest.vel, lerpAlpha );
		}
	}

	void PhysicsUpdateStep( const FloatTime& frameDt, const FloatTime& physicsDt )
	{
		// we are going to multiple physics updates using start frame values - we know we're going to be taking stale data here.
		// take time-stripped copies
		CheckConsistency( _carAnimTargetPos, frameDt );
		CheckConsistency( _inputVal, frameDt );
		FloatTime carAnimTargetPos_const = _carAnimTargetPos.StripTime();
		FloatTime inputVal_const = _inputVal.StripTime();

		FloatTime accel = inputVal_const + (carAnimTargetPos_const - _carStateLatest.pos);

		_carStateLatest.pos.Integrate( _carStateLatest.vel, physicsDt );
		_carStateLatest.vel.Integrate( accel, physicsDt );
	}

	void MainUpdate( const FloatTime& frameDt )
	{
		// systems update - ai, logic, etc
	}

	void CameraUpdate( const FloatTime& frameDt )
	{
		// scheme: sample car position etc at the end frame values, and then simulate forwards from end frame. so the from-time is
		// the end frame time, and to to-time must then be 1 frame ahead
		_cameraDt = FloatTime( frameDt.Value(), frameDt.Value() + frameDt.Time() );

		// does not work!! we don't know the dt for the next frame, so we don't know how far forward to simulate the camera!
		// HACK FIX!! reset the time
		_cameraPos = FloatTime( _cameraPos.Value(), _cameraDt.Time() );

		// lerp camera towards car
		_cameraPos = FloatTime::Lerp( _cameraPos, _carStateCurrent.pos, MakeConstant( 6.0f * _cameraDt.Value() ) );

		// add influence from changing input
		if( _inputVal.Time() > _inputValLast.Time() )
		{
			FloatTime velDep = Vel( _inputValLast, _inputVal );

			// another thing that does not work! we don't have the end-frame inputs! we have the inputs from the beginning of the frame.
			// this might be deemed "ok", or the inputs could be sampled at the end up the main update but before cameras..!?

			// HACK FIX!! strip time
			_cameraPos += velDep.StripTime();
		}

		// add influence from speed
		_cameraPos -= _carStateCurrent.vel * MakeConstant( 0.1f );

		_cameraPos.FinishedUpdate( _cameraDt );

		// the print function could also check time consistency? any output flow should check.
		printf( "Car pos: %f\n", _carStateCurrent.pos.Value() );
	}

	struct CarState
	{
		FloatTime pos;
		FloatTime vel;
	};

	CarState _carStateLatest;
	CarState _carStateCurrent;

	FloatTime _inputVal = FloatTime( 0.0f, 0.0f );
	FloatTime _inputValLast = FloatTime( 0.0f, 0.0f );

	FloatTime _carAnimTargetPos;

	FloatTime _physTimeBalance = FloatTime( 0.0f, 0.0f );
	FloatTime _physicsDt = FloatTime( 1.0f / 64.0f, 0.0f );

	FloatTime _cameraDt = FloatTime( 0.0f, 0.0f );
	FloatTime _cameraPos = FloatTime( 0.0f, 0.0f );
};
