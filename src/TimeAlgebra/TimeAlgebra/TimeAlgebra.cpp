// This file is subject to the MIT License as seen in the root of this folder structure (LICENSE)
//

#include <stdio.h>
#include <assert.h>
#include <cmath>

#include "FloatTime.h"
#include "GameSimulation.h"

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
