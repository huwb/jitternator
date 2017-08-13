// This file is subject to the MIT License as seen in the root of this folder structure (LICENSE)
//

#pragma once

class FloatTime;
// could this be automatic through a conversion constructor?
void CheckConsistency( const FloatTime& a, const FloatTime& b );


/**
* A float value with a timestamp attached
*/
class FloatTime
{
private:
	FloatTime()
	{
	}

public:
	// creates a float with the given value and timestamp 0
	explicit FloatTime( float value )
		: _value( value )
	{
	}

	// creates a float with timestamp, using the timestamp from an existing value. using the 
	// frame dt is a common pattern here
	explicit FloatTime( float value, const FloatTime& timeGiver )
	{
		_value = value;
		_time = timeGiver.Time();
	}

	FloatTime operator+( const FloatTime& other ) const
	{
		CheckConsistency( *this, other );
		FloatTime result;
		result._value = _value + other._value;
		result._time = _time;
		return result;
	}
	FloatTime operator-( const FloatTime& other ) const
	{
		CheckConsistency( *this, other );
		FloatTime result;
		result._value = _value - other._value;
		result._time = _time;
		return result;
	}
	FloatTime operator*( const FloatTime& other ) const
	{
		CheckConsistency( *this, other );
		FloatTime result;
		result._value = _value * other._value;
		result._time = _time;
		return result;
	}
	FloatTime operator/( const FloatTime& other ) const
	{
		CheckConsistency( *this, other );
		FloatTime result;
		result._value = _value / other._value;
		result._time = _time;
		return result;
	}

	FloatTime& operator+=( const FloatTime& other )
	{
		CheckConsistency( *this, other );
		_value += other.Value();
		return *this;
	}
	FloatTime& operator-=( const FloatTime& other )
	{
		CheckConsistency( *this, other );
		_value -= other.Value();
		return *this;
	}
	FloatTime& operator*=( const FloatTime& other )
	{
		CheckConsistency( *this, other );
		_value *= other._value;
		return *this;
	}

	void Integrate( const FloatTime& rateOfChange, const FloatTime& dt )
	{
		CheckConsistency( *this, rateOfChange );
		CheckConsistency( *this, dt );

		*this += rateOfChange * dt;

		_time = Time() + dt.Value();
	}

	void FinishedUpdate( const FloatTime& dt )
	{
		CheckConsistency( *this, dt );

		_time = Time() + dt.Value();
	}

	float Value() const
	{
		return _value;
	}

	float Time() const
	{
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
		FloatTime result;
		result._value = (1.0f - s) * a.Value() + s * b.Value();
		result._time = (1.0f - s) * a.Time() + s * b.Time();
		return result;
	}

private:
	friend void AdvanceDt( FloatTime& io_dt, const FloatTime& newDt );

	float _value = 0.0f;
	float _time = 0.0f;
};

bool ApproxEqual( float a, float b, float eps = 0.0001f )
{
	return abs( a - b ) < eps;
}

void CheckConsistency( const FloatTime& a, const FloatTime& b )
{
	bool consistent = ApproxEqual( a.Time(), b.Time() );
	if( !consistent )
		__debugbreak();
}

void AdvanceDt( FloatTime& io_dt, const FloatTime& newDt )
{
	// if newDt is non-constant (was computed from some time-varying things), then ensure consistency
	CheckConsistency( io_dt, newDt );

	// use new dt value, and advance time by the dt
	io_dt._value = newDt.Value();
	io_dt._time = io_dt._time + io_dt.Value();
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

	return FloatTime( vel, val_t1 );
}
