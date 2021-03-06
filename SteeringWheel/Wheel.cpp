#include "Wheel.h"

///////////////////////////////////////////////////////////////////////////////////////////
//            //
// profiler() //
//            //
////////////////

// Creates a profile array of motor forces
// Stores profile in hapticEffects object
void Wheel::profiler()
{
	////////////////////////////////////////////////////
	// We like comments just like the WHO likes tests //
	//                                                //
	// Some days it makes us feel like we have        //
	// done a lot of programming and bug fixing when  //
	// really all we have done is explain this crap   //
	// so that future me thinks I did a really good   //
	// job :)                                         //
	////////////////////////////////////////////////////

	// TODO - encapsulate some of this into functions !!!

	if (WHEEL_DEBUG_OUTPUT)
	{
		std::cout << "Profiling wheel motor" << std::endl;
		std::cout << "---------------------" << std::endl;
	}

	// Some defaults
	Uint32 duration = 1000;			// 1000 milli Seconds for effect to run (left movement)
	Uint32 offsetDuration = 200;	// 200 mS for offset effect (right movement)

	bool useRight = false;	// Shall we use an offset?
	int i = 0;				// iterations of right offset

	// set gain to 100 - just so we know
	SDL_HapticSetGain(haptic, 100);

	// We will be using SDL_JOYAXISMOTION type
	SDL_Event e;

	// Loop through all power levels
	for (Uint16 powerLevel = HE::MIN_POWER_LEVEL; powerLevel <= HE::MAX_POWER_LEVEL; ++powerLevel)
	{
		// Just easier to see whats going on - slow down
		SDL_Delay(100);

		if (WHEEL_DEBUG_OUTPUT) std::cout << "Power Level : " << (powerLevel * 1000) << std::endl;
		
		// centre wheel so we have a known start point for comparisons
		centre();
		
		bool done = false;
		Sint16 min;

		while (!done)
		{
			// Set to sensible defaults 
			min = SDL_MAX_SINT16; // wheel is centred - can't be here

			// flush queue
			flushEventQueue(e, 1);

			// TODO Minor bug on first use, ie don't use this for the 1st useRight
			// We are centred - if the last powerLevel required a right offset then use it
			if (useRight) moveRightByOffset(e, offsetDuration, i);

			// Setup constant force for 1 second
			int effect_left = setConstantForce(duration, (powerLevel * 1000), HE::LEFT);

			// Start to move the wheel - SDL_Events should now be generated
			runEffect(HE::CONSTANT_LEFT, 1);

			// Seems without this delay we check for movement too early
			// Motor needs to build up power before it can move
			SDL_Delay(50); // TODO arbitary time delay? Too much / little? 

			// Min holds the left most position when the wheel stopped
			leftMostPositionWhenStopped(e, min);
			
			// Take a breath!
			SDL_Delay(100);

			// TODO need to deal with both end stops + and - HIT_END_STOP

			// check movement doesn't hit end stop
			if (min > -HE::TOO_CLOSE_TO_END_STOP)
			{
				// Exit while loop
				done = true;
			}
			// We went too close to end stop or actually hit it, next time
			// start further away
			else
			{
				moveRightIncreaseOffset(offsetDuration, i, min, useRight);
			}
			
		} // end while loop
		
		// We now know that the right offset will allow 1 second of wheel travel
		// without hitting or being too close to end stops
		// we can now re-run the movement but this time record it

		if (WHEEL_DEBUG_OUTPUT)
		{
			std::cout << "Power level (" << (powerLevel * 1000) << ") start position known ";
			if (useRight) std::cout << "with right start of (200 mS * " << i << ")";
			std::cout << std::endl;
		}

		// Profile and record
		profilePowerLevel(i, powerLevel, duration, offsetDuration, useRight);

		// Before next power level, just pause, helps to see whats going on
		// visually - part of diagnostic testing and bug finding. It does no harm
		SDL_Delay(100);

	} // end for loop

	centre();

	// The world moves too fast, just chill allow any movement to finish for certain
	SDL_Delay(1000);

	if (WHEEL_DEBUG_OUTPUT)
	{
		std::cout << "Profiling wheel motor end" << std::endl;
		std::cout << "-------------------------" << std::endl;
	}

	saveProfile(HE::LEFT); // could be right
}

///////////////////////////////////////////////////////////////////////////////////////////
//                     //
// profilePowerLevel() //
//                     //
/////////////////////////

// Profile this power level
void Wheel::profilePowerLevel(const int i, const Uint16 powerLevel, const Uint32 duration,
	const Uint32 offsetDuration, const bool useRight)
{

	// TODO Works but needs rewriting
	// TODO is clock() the right measurement?

	if (WHEEL_DEBUG_OUTPUT)
	{
		std::cout << "Profiling power level: " << (powerLevel * 1000) << " ";
		if (useRight)
		{
			std::cout << "with right offset of " << (offsetDuration * i) << " mS ";
			std::cout << "at power level: " << HE::SAFE_POWER_LEVEL;
		}
		std::cout << std::endl;
	}

	SDL_Event e;
	int index = 0;

	// Get to known starting position
	centre();

	// We are centred - if the last powerLevel required a right offset then use it
	if (useRight) moveRightByOffset(e, offsetDuration, i);

	// Setup constant force for 1 second duration
	int effect_left = setConstantForce(duration, (powerLevel * 1000), HE::LEFT);

	// flush queue
	flushEventQueue(e, 1);
	
	// Get initial reading
	SDL_JoystickUpdate();
	Sint16 from = readWheelPosition();

	
	// Start to move the wheel - SDL_Events should now be generated
	runEffect(HE::CONSTANT_LEFT, 1);

	clock_t start = clock();
	bool timedOut = false;

	// Start Storing Profile Details
	profileLeft[powerLevel][index].power = powerLevel;
	profileLeft[powerLevel][index].reading = index;
	profileLeft[powerLevel][index].direction = HE::LEFT; // TODO needs to be also for right
	profileLeft[powerLevel][index].timeStamp = start;
	profileLeft[powerLevel][index].from = from;

	// Timeout or a position is obtained
	Sint16 to = from;
	while (to == from && !timedOut)
	{
		if ((clock() - start) >= 100) timedOut = true;
		SDL_JoystickUpdate();
		to = readWheelPosition();
	}

	if (timedOut)
	{
		profileLeft[powerLevel][index].timedOut = true;
		if (WHEEL_DEBUG_OUTPUT) std::cout << "Timedout! No Movement" << std::endl;
		SDL_Delay(500);
		return;
	}

	profileLeft[powerLevel][index].timeToMove = clock() - start;
	profileLeft[powerLevel][index].to = to;
	profileLeft[powerLevel][index].distance = std::abs(from - to);
	profileLeft[powerLevel][index].delta = std::abs(from - to);

	if (WHEEL_DEBUG_OUTPUT)
	{
		std::cout << "From: " << profileLeft[powerLevel][index].from << " To: " << profileLeft[powerLevel][index].to
			<< " Ttfm: " << profileLeft[powerLevel][index].timeToMove << " mS" << std::endl;
	}

	from = to;
	bool stationary = false;
	bool running = true;
	clock_t timePeriodStart = clock();
	clock_t now;

	// Get movement readings
	while (!stationary)
	{
		// Are we at the end of a measurement time period?
		now = clock();
		if ((now - timePeriodStart) >= HE::PROFILE_TIME_PERIOD)
		{
			// Increase the profile index
			index++; 

			profileLeft[powerLevel][index].from = to;
			profileLeft[powerLevel][index].duration = now - timePeriodStart;
			profileLeft[powerLevel][index].timeStamp = now;
			profileLeft[powerLevel][index].power = powerLevel;
			profileLeft[powerLevel][index].reading = index;
			profileLeft[powerLevel][index].direction = HE::LEFT;

			// Force event update
			SDL_JoystickUpdate(); 
			to = readWheelPosition();
			profileLeft[powerLevel][index].to = to;
			profileLeft[powerLevel][index].distance = std::abs(from - to);

			// At some stage we stop - breakout
			if (from == to) stationary = true;
			from = to;
			
			// Are we freewheeling?
			running = SDL_HapticGetEffectStatus(haptic, effectsMap[HE::CONSTANT_LEFT]);
			profileLeft[powerLevel][index].freeWheel = (running == 0) ? true : false;

			// Restart timer
			timePeriodStart = clock();
		}

		SDL_Delay(1); // TODO It is possible we don't need this delay
	}

	if (WHEEL_DEBUG_OUTPUT)
	{
		std::cout << "Total time of movement: " << (profileLeft[powerLevel][index].timeStamp
			- profileLeft[powerLevel][0].timeStamp) << " mS" << std::endl;
	}

	SDL_Delay(500);
}

///////////////////////////////////////////////////////////////////////////////////////////
//               //
// saveProfile() //
//               //
///////////////////

// Writes profileLeft / Right to files
void Wheel::saveProfile(int direction)
{
	// TODO expand to save left and right profile
	// TODO check folder, create folder
	struct stat info;

	// TODO get path properly
	std::string exeName = "SteeringWheel.exe";
	
	// Get the name string
	std::string n = SDL_JoystickName(wheel);
	std::replace(n.begin(), n.end(), ' ', '_');

	// Find path
	std::string exePath = getExePath();
	int pos = exePath.find(exeName);
	exePath.erase(pos, exeName.size());
	
	for (int p = HE::MIN_POWER_LEVEL; p <= HE::MAX_POWER_LEVEL; ++p)
	{
		std::ofstream myFile;
		
		// Shouldnt build paths like this
		std::string name = exePath + "Profile/" + n + "_" + std::to_string(p) + ".txt";
		
		if (WHEEL_DEBUG_OUTPUT) std::cout << "saving " << name << std::endl;
		
		myFile.open(name);
		myFile << "Reading,Direction,To,From,Duration,Disatance,TimeStamp,TimeToMove,TimedOut,Delta,FreeWheel" << std::endl;
		for (int i = 0; i <= HE::PROFILE_UNITS; ++i)
		{
			myFile << profileLeft[p][i].reading << ","
				<< (int)profileLeft[p][i].direction << ","
				<< profileLeft[p][i].to << ","
				<< profileLeft[p][i].from << ","
				<< profileLeft[p][i].duration << ","
				<< profileLeft[p][i].distance << ","
				<< profileLeft[p][i].timeStamp << ","
				<< profileLeft[p][i].timeToMove << ","
				<< profileLeft[p][i].timedOut << ","
				<< profileLeft[p][i].delta << ","
				<< profileLeft[p][i].freeWheel << std::endl;
		}
		myFile.close();
	}

//	char* path = new char[n.size() + 1];
//	std::copy(n.begin(), n.end(), path);
//	path[n.size()] = '\0';
	


//	std::cout << path << std::endl;

// https://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from
// path needs to be full path and not relative
//#include <string>
//#include <windows.h>
//
//	std::string getexepath()
//	{
//		char result[MAX_PATH];
//		return std::string(result, GetModuleFileName(NULL, result, MAX_PATH));
//	}

// https://stackoverflow.com/questions/18100097/portable-way-to-check-if-directory-exists-windows-linux-c
//	if (stat(path, &info) != 0)
//		printf("cannot access %s\n", path);
//	else if (info.st_mode & S_IFDIR)   
//		printf("%s is a directory\n", path);
//	else
//		printf("%s is no directory\n", path);
//
//	delete[] path;
}

///////////////////////////////////////////////////////////////////////////////////////////
//                     //
// moveRightByOffset() //
//                     //
/////////////////////////

// Move right by offset duration * i
void Wheel::moveRightByOffset(SDL_Event& e, const Uint32 offsetDuration, const int i)
{
	if (WHEEL_DEBUG_OUTPUT) std::cout << "Using right offset" << std::endl;

	// Setup the force
	int effect_right = setConstantForce(offsetDuration, HE::SAFE_POWER_LEVEL, HE::RIGHT);
	
	// Run "i" times
	runEffect(HE::CONSTANT_RIGHT, i);

	// Delay by longer than "i" times
	SDL_Delay(offsetDuration * (i + 1));

	// flush queue
	flushEventQueue(e, 1);
}

// Move right by offset duration * i
void Wheel::moveRightByOffset(const Uint32 offsetDuration, const int i)
{
	if (WHEEL_DEBUG_OUTPUT) std::cout << "Using right offset" << std::endl;

	// Setup the force
	int effect_right = setConstantForce(offsetDuration, HE::SAFE_POWER_LEVEL, HE::RIGHT);

	// Run "i" times
	runEffect(HE::CONSTANT_RIGHT, i);

	// Delay by longer than "i" times
	SDL_Delay(offsetDuration * (i + 1));
}

///////////////////////////////////////////////////////////////////////////////////////////
//                           //
// moveRightIncreaseOffset() //
//                           //
///////////////////////////////

// Increase offset duration and move right
// modifies i, useRight
void Wheel::moveRightIncreaseOffset(const Uint32 offsetDuration, int &i, const Sint16 min, bool &useRight)
{
	if (WHEEL_DEBUG_OUTPUT)
	{
		std::cout << "Too close to end" << std::endl;
		if (min < -HE::HIT_END_STOP)
		{
			std::cout << "Hit End Stop!!!" << std::endl;
		}
	}

	// Move a little more right than before
	i += 1; // maybe +2?

	// Get to a known position
	centre();

	// A little info is helpful
	if (WHEEL_DEBUG_OUTPUT) std::cout << "Move right: " << i << std::endl;

	// Physically move right
	if (!useRight) moveRightByOffset(offsetDuration, i); 

	// From now on use a right offset
	useRight = true;
}

///////////////////////////////////////////////////////////////////////////////////////////
//                   //
// flushQueueEvent() //
//                   //
///////////////////////

// Flush Event Queue
// TODO flush a particular type of event
void Wheel::flushEventQueue(SDL_Event& e, const int eventType)
{
	while (SDL_PollEvent(&e)) SDL_Delay(1);
}

///////////////////////////////////////////////////////////////////////////////////////////
//                               //
// leftMostPositionWhenStopped() //
//                               //
///////////////////////////////////

// Keep polling xaxis and find the left most position (min)
// before it stopped. Min is NOT the actual position of wheel
// because an end stop bounce may have ocurred
// Pass in event and min variable - both could be altered
void Wheel::leftMostPositionWhenStopped(SDL_Event &e, Sint16 &min )
{
	bool stationary = false;

	// Set to sensible defaults
	Sint16 current = SDL_MIN_SINT16;	// wheel is centred - can't be here
	Sint16 last = SDL_MAX_SINT16;		// Ditto

	while (!stationary)
	{
		// Find how far it travelled
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_JOYAXISMOTION)
			{
				// Where are we?
				current = e.jaxis.value;

				// Have we been this far before?
				if (current < min) min = current;
			}

			// Lets not rush, may need to wait for next event
			SDL_Delay(5);
		}

		// Let it settle, we should be stopped
		SDL_Delay(10); //  TODO intermittent hitting end stop without this, but why?

		// Have we stopped?
		if (current == last) stationary = true;
		last = current;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////



// Simply display on console
void Wheel::toConsole(const std::string msg)
{
	std::cout << "Wheel: " << msg;
}


///////////////////////////////////////////////////////////////////////////////////////////



// Constructor just zeros SDL_HapticEffect
Wheel::Wheel()
{
	memset(&effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
}


///////////////////////////////////////////////////////////////////////////////////////////



// Gets x axis and returns position
int Wheel::readWheelPosition()
{
	// TODO need to add joystick update test to see if events are pending
	int pos = SDL_JoystickGetAxis(wheel, 0);
	return pos;
}


///////////////////////////////////////////////////////////////////////////////////////////



// Joysticks and wheels may or may not have buttons
int Wheel::getNumberOfButtons()
{
	return SDL_JoystickNumButtons(wheel);
}


///////////////////////////////////////////////////////////////////////////////////////////



// For diagnostic purposes
void Wheel::displayWheelAbilities()
{
	toConsole("Haptic Abilities\n");
	toConsole("----------------\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_SINE)  toConsole("has sine effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_CONSTANT) toConsole("has constant effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_LEFTRIGHT) toConsole("has leftright effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_TRIANGLE) toConsole("has triangle wave effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_SAWTOOTHUP) toConsole("has saw tooth up effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_SAWTOOTHDOWN) toConsole("has saw tooth down effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_RAMP) toConsole("has ramp effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_SPRING) toConsole("has spring effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_DAMPER) toConsole("has damper effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_INERTIA) toConsole("has inertia effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_FRICTION) toConsole("has friction effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_CUSTOM) toConsole("has custom effect\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_GAIN) toConsole("can set gain\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_AUTOCENTER) toConsole("has auto centre effect\n ");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_PAUSE)  toConsole("can be paused\n");
	if (SDL_HapticQuery(haptic) & SDL_HAPTIC_STATUS)  toConsole("can have its status queried\n");
	if (SDL_HapticRumbleSupported(haptic)) toConsole("support haptic rumble\n");
}


///////////////////////////////////////////////////////////////////////////////////////////



// See SDL_Haptic.h
//   __      __      __      __
//  /  \    /  \    /  \    /
// /    \__/    \__/    \__/
//
void Wheel::hapticSine()
{
	effect.type = SDL_HAPTIC_SINE;
	// TODO why do we need to reset these?
	effect.periodic.period = 100; // 100 ms
	effect.periodic.magnitude = 32000; // 32000 of 32767 strength
	effect.periodic.offset = 0;
	effect.periodic.phase = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////



// See SDL_Haptic.h
//   /\    /\    /\    /\    /\
//  /  \  /  \  /  \  /  \  /
// /    \/    \/    \/    \/
void Wheel::hapticTriangle()
{
	effect.type = SDL_HAPTIC_TRIANGLE;
	// TODO why do we need to reset these?
	effect.periodic.period = 100; // 100 ms
	effect.periodic.magnitude = 32000; // 32000 of 32767 strength
	effect.periodic.offset = 0;
	effect.periodic.phase = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////



// See SDL_Haptic.h
//   /|  /|  /|  /|  /|  /|  /|
//  / | / | / | / | / | / | / |
// /  |/  |/  |/  |/  |/  |/  |
void Wheel::hapticSawToothUp()
{
	effect.type = SDL_HAPTIC_SAWTOOTHUP;
	// TODO why do we need to reset these?
	effect.periodic.period = 100; // 100 ms
	effect.periodic.magnitude = 32000; // 32000 of 32767 strength
	effect.periodic.offset = 0;
	effect.periodic.phase = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////



void Wheel::spring()
{
	effect.type = SDL_HAPTIC_SPRING;
	effect.condition.length = 5000;
	effect.condition.right_sat[0] = 0xFFFF;
	effect.condition.left_sat[0] = 0xFFFF;
	effect.condition.right_coeff[0] = 0x2000;
	effect.condition.left_coeff[0] = 0x2000;
	effect.condition.deadband[0] = 0x100;
	effect.condition.center[0] = 0x1000;     /* Displace the center for it to move. */
}


///////////////////////////////////////////////////////////////////////////////////////////



// Set default initial effect
void Wheel::initEffect()
{
	

	// SDL_HAPTIC_TRIANGLE SDL_HAPTIC_SINE SDL_HAPTIC_SAWTOOTHUP
	effect.periodic.direction.type = SDL_HAPTIC_CARTESIAN;
	effect.periodic.direction.dir[0] = 0;
	effect.periodic.direction.dir[1] = -1; // Force comes from south
	effect.periodic.direction.dir[2] = 0;

	effect.periodic.length = 3000; // 5 seconds long
	effect.periodic.delay = 0;

	effect.periodic.period = 100; // 100 ms
	effect.periodic.magnitude = 32000; // 32000 of 32767 strength
	effect.periodic.offset = 0;
	effect.periodic.phase = 0;

	effect.periodic.attack_length = 50; // Takes 50ms to get max strength
	effect.periodic.attack_level = 32000;
	effect.periodic.fade_length = 50; // Takes 50ms to fade away
	effect.periodic.fade_level = 0;

}


///////////////////////////////////////////////////////////////////////////////////////////



// Wait till wheel stops moving
bool Wheel::waitForNoMovement()
{
	// TODO check this function - seems to work sometimes but not all

	// Start timeout timer
	std::clock_t start = std::clock();
	int last = 1;
	int current = 0;

	// Wait till wheel stops moving
	while (last != current)
	{
		SDL_JoystickUpdate();
		current = readWheelPosition();
		SDL_Delay(8);

		last = readWheelPosition();

		// Timed out return
		if ((std::clock() - start) >= GENERAL_TIMEOUT) return false;
	}

	// return wheel stopped
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////


// Centre the wheel
void Wheel::centre()
{
	// TODO need to use variables for absolute positions so that we can deal with different power
	// levels and different grees of roation

	// Time of effect
	Uint32 duration = 10;

	// Strength level
	Uint16 level = 10000;

	// Make effects
	int effect_left = setConstantForce(duration, level, HE::LEFT);
	int effect_right = setConstantForce(duration, level, HE::RIGHT);

	std::cout << "Centering...";

	// Force update as there is no event
	SDL_JoystickUpdate();
	int pos = readWheelPosition();

	// Find the centre as best we can
	while ( pos > 20 || pos < -20 )
	{	
		// If we are to the right of centre
		if (pos > 0)
		{
			runEffect(HE::CONSTANT_LEFT, 1);

			// For large movements let effect run but not to the end to avoid vibration
			if (pos > 500) SDL_Delay(duration - 2); else waitForNoMovement();
		}
		// If we are left of centre
		else if (pos < 0)
		{
			runEffect(HE::CONSTANT_RIGHT, 1);

			// For large movements let effect run but not to the end to avoid vibration
			if (pos < -500) SDL_Delay(duration - 2); else waitForNoMovement();
		}

		// If close to middle, slow down
		if (pos < 500 && pos > -500 ) SDL_Delay(100);

		// Force update so we can read position
		SDL_JoystickUpdate();
		pos = readWheelPosition();
	}

	// Let it settle
	SDL_Delay(1000);

	// Output position 
	SDL_JoystickUpdate();
	std::cout << "done (pos=" << readWheelPosition() << ")" << std::endl;
}


///////////////////////////////////////////////////////////////////////////////////////////



// Helper function to send effect to wheel controller
int Wheel::uploadExecuteEffect()
{

	// Upload the effect
	int effect_id = SDL_HapticNewEffect(haptic, &effect);

	// error?
	if (effect_id < 0)
	{
		std::string msg = SDL_GetError();
		toConsole("Error: " + msg + "\n");
	}

	// Test the effect
	effect_id = SDL_HapticRunEffect(haptic, effect_id, 1);
	
	// Error?
	if (effect_id < 0)
	{
		std::string msg = SDL_GetError();
		toConsole("Error: " + msg + "\n");
	}

	// Return it (negative is failure)
	return effect_id;
}


///////////////////////////////////////////////////////////////////////////////////////////



// For testing purposes only
void Wheel::hapticTest()
{

	int effect_id;
	
	// TODO force first device to be used
	wheel = SDL_JoystickOpen(0);
	haptic = SDL_HapticOpenFromJoystick(wheel);

	if (haptic == NULL) return ; // Most likely joystick isn't haptic
	
	// See if it can do sine waves
	if ((SDL_HapticQuery(haptic) & SDL_HAPTIC_SINE) == 0) 
	{
		SDL_HapticClose(haptic); // No sine effect
		return ;
	}
	

	//// test 0
	spring();
	toConsole("Trying Haptic Spring...\n");
	effect_id = uploadExecuteEffect();
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(5000);

	SDL_HapticSetGain(haptic, 100);
	centre();
	SDL_Delay(5000);

	// test 1
	toConsole("Trying Constant Force Right...\n");
	effect_id = runEffect(HE::CONSTANT_RIGHT, 100);
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(5000);

	// test 2
	toConsole("Trying Constant Force Left...\n");
	effect_id = runEffect(HE::CONSTANT_LEFT, 100);
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(5000); 

	// test 3
	hapticSine();
	toConsole("Trying Haptic Sine...\n");
	effect_id = uploadExecuteEffect();
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(5000);

	// test 4
	hapticTriangle();
	toConsole("Trying Haptic Triangle...\n");
	effect_id = uploadExecuteEffect();
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(5000);



	// test 5
	hapticSawToothUp();
	toConsole("Trying Haptic Sawtooth Up...\n");
	effect_id = uploadExecuteEffect();
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(5000);

	// test 6
	// Initialize simple rumble
	toConsole("Trying Haptic Rumble...\n");
	effect_id = SDL_HapticRumbleInit(haptic);	
	// Play effect at 50% strength for 2 seconds
	if (effect_id == 0) SDL_HapticRumblePlay(haptic, 0.5, 2000);
	if (effect_id == 0) toConsole("OK\n"); else toConsole("FAILED\n");
	SDL_Delay(2000);
	
	// Destroy this effect
	SDL_HapticDestroyEffect(haptic, effect_id);
	
	// Close the device
	SDL_HapticClose(haptic);
}


///////////////////////////////////////////////////////////////////////////////////////////



// Init SDL and find joysticks / wheels with the right abilities
void Wheel::init()
{
	// TODO we cant read keyboard events until a screen is initialised !!!
	// remember this rather than spending an hour working out why keybrd
	// isnt working you muppet!

	//Initialize SDL
	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) < 0)
	{
		std::string e = SDL_GetError();
		toConsole("SDL could not initialize! SDL Error:\n"  + e + "\n");
		error = true;
	}
	else
	{
		//Check for joysticks
		int num_wheels = SDL_NumJoysticks();
		
		if (num_wheels < 1)
		{
			toConsole("Error: No joysticks / wheels connected!\n");
		}
		else
		{
			// We have wheels
			int devicePointer = 0;

			// test each wheel upto MAX_WHEEL number
			for (int i = 0; i < num_wheels && i < MAX_WHEELS; ++i)
			{
				SDL_JoystickEventState(SDL_ENABLE);
				wheel = SDL_JoystickOpen(i);

				// Open wheel
				if (wheel != NULL)
				{
					// Display info 
					std::string name = SDL_JoystickName(wheel);
					std::string num = std::to_string(SDL_JoystickNumAxes(wheel));
					haptic = setHaptic(i); ; // SDL_HapticOpen(i);
					
					int nEffects = SDL_HapticNumEffects(haptic);
					toConsole("Found <" + name + "> with axis [" + num + "]  Device# [" + std::to_string(i) + "] effects [" + std::to_string(nEffects) + "]\n");

					// Haptic wheel?
					if (haptic == NULL)
					{
						deviceIndex[devicePointer] = NON_HAPTIC;
						toConsole("has no haptic ability\n");
					}
					else
					{
						deviceIndex[devicePointer] = HAPTIC;
						toConsole("has haptic ability\n");

						// Try Auto centre (G27 doesn't have auto centre - relies on Logitech driver)
						if ((SDL_HapticQuery(haptic) & SDL_HAPTIC_AUTOCENTER))
						{
							if (SDL_HapticSetAutocenter(haptic, 50) == 0)
							{
								toConsole("wheel was centred with force 50\n");
							}
							else
							{
								toConsole("Error: wheel centreing failed\n");
							}
						}

						// display abilities
						displayWheelAbilities();
					}
				}
				else
				{
					// Wheel could not be opened - ODD? Something was there!
					deviceIndex[devicePointer] = ERROR;
				}

				// Next device
				++devicePointer;

			} // each wheel

		} // end we have wheels

	} // end SDL init tests
	
	initEffect();

} // end init

///////////////////////////////////////////////////////////////////////////////////////////

// Returns full path with executable name
std::string Wheel::getExePath()
{
	// MAX_PATH is in windows.h
	char result[MAX_PATH];
	
	// Put path into result
	GetModuleFileNameA(NULL, result, MAX_PATH);
	
	return std::string(result);
}
