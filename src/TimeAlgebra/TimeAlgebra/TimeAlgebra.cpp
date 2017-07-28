// This file is subject to the MIT License as seen in the root of this folder structure (LICENSE)
//

#include "stdafx.h"
#include <assert.h>
#include <cmath>

class FloatTime;
// could this be automatic through a conversion constructor?
FloatTime MakeConstant( float value );
void CheckConsistency( const FloatTime& a, const FloatTime& b, float& o_consistentTime );
void CheckConsistency( const FloatTime& a, const FloatTime& b, const FloatTime& c, float& o_consistentTime );


/**
 * A float value with a timestamp attached
 */
class FloatTime
{
public:
	FloatTime()
	{}

	explicit FloatTime( float value, float time )
	{
		_value = value;
		_time = time;
	}

	FloatTime operator+( const FloatTime& other ) const
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		FloatTime result( _value + other._value, consistentTime );
		return result;
	}
	FloatTime operator-( const FloatTime& other ) const
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		FloatTime result( _value - other._value, consistentTime );
		return result;
	}
	FloatTime operator*( const FloatTime& other ) const
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		FloatTime result( _value * other._value, consistentTime );
		return result;
	}
	FloatTime operator/( const FloatTime& other ) const
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		FloatTime result( _value / other._value, consistentTime );
		return result;
	}

	FloatTime& operator+=( const FloatTime& other )
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		_value += other.Value();
		_time = consistentTime;

		return *this;
	}
	FloatTime& operator-=( const FloatTime& other )
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		_value -= other.Value();
		_time = consistentTime;

		return *this;
	}
	FloatTime& operator*=( const FloatTime& other )
	{
		float consistentTime;
		CheckConsistency( *this, other, consistentTime );

		_value *= other._value;
		_time = consistentTime;

		return *this;
	}

	void Integrate( const FloatTime& rateOfChange, const FloatTime& dt )
	{
		if( !HasTime() )
			__debugbreak(); // cant integrate a non-time-varying value!

		float ct;
		CheckConsistency( *this, rateOfChange, dt, ct );

		*this += rateOfChange * dt;

		_time = Time() + dt.Value();
	}

	void FinishedUpdate( const FloatTime& dt )
	{
		if( !HasTime() )
			__debugbreak(); // cant update a non-time-varying value!

		float ct;
		CheckConsistency( *this, dt, ct );

		_time = Time() + dt.Value();
	}

	float Value() const
	{
		return _value;
	}

	bool HasTime() const
	{
		return _time != NO_TIME;
	}

	FloatTime StripTime() const
	{
		return MakeConstant( _value );
	}

	float Time() const
	{
		if( !HasTime() )
			__debugbreak();

		return _time;
	}

	static FloatTime Lerp( const FloatTime& a, const FloatTime& b, const FloatTime& s )
	{
		return (MakeConstant( 1.0f ) - s) * a + s * b;
	}

	// lerp between floats at two different simulation times. this is a special case and should
	// only be used in low level time/state management code!
	// lerp alpha has no units - it is not time
	static FloatTime LerpInTime( const FloatTime& a, const FloatTime& b, float s )
	{
		float val = (1.0f - s) * a.Value() + s * b.Value();

		float time;

		if( !a.HasTime() )
		{
			if( !b.HasTime() )
			{
				// both a and b are constants, return constant
				time = NO_TIME;
			}
			else
			{
				// only a is constant
				time = b.Time();
			}
		}
		else if( !b.HasTime() )
		{
			// only b is constant
			time = a.Time();
		}
		else
		{
			// both a and b are time varying, lerp the time
			time = (1.0f - s) * a.Time() + s * b.Time();
		}

		return FloatTime( val, time );
	}

	const static float NO_TIME;

private:
	friend void AdvanceDt( FloatTime& io_dt, const FloatTime& newDt );

	float _value = -1.0f;
	float _time = -1.0f;
};

const float FloatTime::NO_TIME = -1000.0f;

bool ApproxEqual( float a, float b, float eps = 0.0001f )
{
	return abs( a - b ) < eps;
}

void CheckConsistency( const FloatTime& a, const FloatTime& b )
{
	bool consistent = !a.HasTime() || !b.HasTime() || ApproxEqual( a.Time(), b.Time() );
	if( !consistent )
		__debugbreak();
}

void CheckConsistency( const FloatTime& a, const FloatTime& b, float& o_consistentTime )
{
	CheckConsistency( a, b );
	o_consistentTime = a.HasTime() ? a.Time() : (b.HasTime() ? b.Time() : FloatTime::NO_TIME);
}

// three way consistency
void CheckConsistency( const FloatTime& a, const FloatTime& b, const FloatTime& c, float& o_consistentTime )
{
	float ct0, ct1, ct2;
	CheckConsistency( a, b, ct0 );
	CheckConsistency( b, c, ct1 );
	CheckConsistency( a, c, ct2 );
	o_consistentTime = ct0 != FloatTime::NO_TIME ? ct0 : (ct1 != FloatTime::NO_TIME ? ct1 : ct2);
}

FloatTime MakeConstant( float value )
{
	return FloatTime( value, FloatTime::NO_TIME );
}

void AdvanceDt( FloatTime& io_dt, const FloatTime& newDt )
{
	// this function requires dt to be non-constant/time-varying
	if( !io_dt.HasTime() )
		__debugbreak();

	// if newDt is non-constant (was computed from some time-varying things), then ensure consistency
	float consistentTime;
	CheckConsistency( io_dt, newDt, consistentTime );

	// use new dt value, and advance time by the dt
	io_dt = FloatTime( newDt.Value(), io_dt._time + io_dt.Value() );
}

// Vel is interesting as we have timestamps for the values
FloatTime Vel( const FloatTime& val_t0, const FloatTime& val_t1 )
{
	if( val_t1.Time() < val_t0.Time() )
		__debugbreak(); // expected arg order violated. this could be gracefully handled but erring on strictness for now..

	//// returning 0 is not ideal, so do nothing for now
	//if( ApproxEqual( val_t0.Time(), val_t1.Time() ) )
	//	return 0.0f;

	float vel = (val_t1.Value() - val_t0.Value()) / (val_t1.Time() - val_t0.Time());
	float time = val_t1.Time(); // take most up to date / latest time?

	return FloatTime( vel, time );
}


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

void TestGame()
{
	GameSimulation game;
	
	FloatTime frameDt( 1.0f / 30.0f, 0.0f );
	game.Init( frameDt );

	for( int i = 0; i < 20; i++ )
	{
		game.Update( frameDt );

		AdvanceDt( frameDt, frameDt );

		frameDt += MakeConstant( 0.001f );
	}
}

void TestSimple()
{
	FloatTime pos( 1.0f, 0.0f );
	FloatTime vel( 2.0f, 0.0f );

	FloatTime dt = FloatTime( 1.0f / 32.0f, 0.0f );

	FloatTime lastTarget;
	bool lastTargetValid = false;

	for( int i = 0; i < 20; i++ )
	{
		// error 3 - taking animated value at end of frame
		FloatTime target = FloatTime( 10.0f, dt.Time() + dt.Value() );

		// error 2 - computing accel from pos after pos update.
		FloatTime accel( 0.0f, 0.0f ); // default value of 0 - only valid on first frame!
		if( lastTargetValid )
		{
			accel = lastTarget - pos;
			accel *= MakeConstant( 4.0f );
		}

		// error 1 - integrating vel before pos
		pos.Integrate( vel, dt );

		printf( "%f\n", pos.Value() );

		vel.Integrate( accel, dt );

		lastTarget = target;
		lastTargetValid = true;

		AdvanceDt( dt, dt );
	}
}

int main()
{
	TestGame();

	//TestSimple();

    return 0;
}
