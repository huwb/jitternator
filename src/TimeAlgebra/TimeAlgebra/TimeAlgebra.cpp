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
	
	FloatTime frameDt = ConstructDt( 1.0f / 30.0f );
	game.Init( frameDt );

	for( int i = 0; i < 20; i++ )
	{
		game.Update( frameDt );

		AdvanceDt( frameDt );

		// modify dt each frame slightly so that it is not uniform
		frameDt += FloatTime( 0.001f, frameDt );
	}
}

void TestSimple()
{
	FloatTime dt = ConstructDt( 1.0f / 32.0f );
	
	FloatTime pos( 1.0f, dt );
	FloatTime vel( 2.0f, dt );

	FloatTime lastTarget( 0.0f, dt );
	bool lastTargetValid = false;

	for( int i = 0; i < 20; i++ )
	{
		// test - taking animated value at end of frame
		FloatTime target = FloatTime( 10.0f, dt );
		// artifically push dt forwards - to simulate getting a value at end frame time
		target.FinishedUpdate( dt );

		// compute accel before updating pos!
		// this default value has time=0 and would only work on the first frame. on subsequent
		// frames it is overwritten with the calculation in the branch.
		FloatTime accel = FloatTime::SimStartValue( 0.0f );
		if( lastTargetValid )
		{
			accel = lastTarget - pos;
			accel *= FloatTime( 4.0f, dt );
		}

		// integrate pos before vel!
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

	TestSimple();

    return 0;
}
