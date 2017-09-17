// This file is subject to the MIT License as seen in the root of this folder structure (LICENSE)
//

#include <stdio.h>
#include <assert.h>
#include <cmath>

#include "FloatTime.h"
#include "GameSimulation.h"

void TestGame()
{
	printf( "== TestGame ==\n" );

	GameSimulation game;
	
	FloatTime frameDt = FloatTime::SimStartValue( 1.0f / 30.0f );

	for( int i = 0; i < 10; i++ )
	{
		game.Update( frameDt );

		game.Render( frameDt );

		// modify dt each frame slightly so that it is not uniform
		FloatTime newDt = frameDt + FloatTime( 0.001f, frameDt );

		// advance time by current dt, and use new dt next frame
		AdvanceDt( frameDt, newDt );
	}
}

void TestSimple()
{
	printf( "== TestSimple ==\n" );

	FloatTime dt = FloatTime::SimStartValue( 1.0f / 32.0f );
	
	FloatTime pos = FloatTime::SimStartValue( 1.0f );
	FloatTime vel = FloatTime::SimStartValue( 2.0f );

	FloatTime lastTarget = FloatTime::SimStartValue( 0.0f );
	bool lastTargetValid = false;

	for( int i = 0; i < 10; i++ )
	{
		// test - taking animated value at end of frame
		FloatTime target = FloatTime( 10.0f, dt );
		// artificially push dt forwards - to simulate getting a value at end frame time
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
