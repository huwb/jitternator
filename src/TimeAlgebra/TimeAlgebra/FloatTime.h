// This file is subject to the MIT License as seen in the root of this folder structure (LICENSE)
//

#pragma once

class FloatTime;
// could this be automatic through a conversion constructor?
void CheckConsistency( const FloatTime& a, const FloatTime& b, float& o_consistentTime );
void CheckConsistency( const FloatTime& a, const FloatTime& b, const FloatTime& c, float& o_consistentTime );


/**
* A float value with a timestamp attached
*/
class FloatTime
{
public:
	FloatTime()
	{
	}

	explicit FloatTime( float value, float time )
	{
		_value = value;
		_time = time;
	}
	
	// given a frame dt, this uses the time stamp stored in the dt, which will be the frame start time. i thought
	// about making a global frame dt, but physics etc run on different timesteps
	explicit FloatTime( float value, const FloatTime& frameDt )
	{
		_value = value;
		_time = frameDt.Time();
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

	float Time() const
	{
		if( !HasTime() )
			__debugbreak();

		return _time;
	}

	static FloatTime Lerp( const FloatTime& a, const FloatTime& b, const FloatTime& s )
	{
		return (FloatTime( 1.0f, s ) - s) * a + s * b;
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

void AdvanceDt( FloatTime& io_dt )
{
	AdvanceDt( io_dt, io_dt );
}

// Vel is interesting as we have timestamps for the values. This eliminates an issue i've seen where a vel is
// computed through finite differences but with an incorrect dt.
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
