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

		frameDt += FloatTime( 0.001f, frameDt );
	}
}

void TestSimple()
{
	FloatTime dt = FloatTime( 1.0f / 32.0f, 0.0f );

	FloatTime pos( 1.0f, dt );
	FloatTime vel( 2.0f, dt );

	FloatTime lastTarget;
	bool lastTargetValid = false;

	for( int i = 0; i < 20; i++ )
	{
		// fixed error 3 - taking animated value at end of frame
		FloatTime target = FloatTime( 10.0f, dt.Time() + dt.Value() );

		// fixed error 2 - computing accel from pos after pos update.
		// this default value has time=0 and would only work on the first frame. on subsequent
		// frames it is overwritten with the calculation in the branch.
		FloatTime accel( 0.0f, 0.0f );
		if( lastTargetValid )
		{
			accel = lastTarget - pos;
			accel *= FloatTime( 4.0f, dt );
		}

		// fixed error 1 - integrating vel before pos
		pos.Integrate( vel, dt );

		printf( "%f\n", pos.Value() );

		vel.Integrate( accel, dt );

		lastTarget = target;
		lastTargetValid = true;

		AdvanceDt( dt );
	}
}

int main()
{
	TestGame();

	//TestSimple();

    return 0;
}
