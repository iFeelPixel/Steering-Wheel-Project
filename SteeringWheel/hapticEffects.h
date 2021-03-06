#pragma once
#include <map>
#include <SDL.h>
#include <string>
#include <iostream>
#include <climits>
#include <ctime>
#include <iostream>
#include <list>


namespace HE
{
	// Define EFFECT MAP constants
	constexpr auto CONSTANT_LEFT = 0;
	constexpr auto CONSTANT_RIGHT = 1;

	// General constants
	constexpr auto LEFT = 1;
	constexpr auto RIGHT = -1;
	constexpr auto BAD_DIRECTION = 99;
	constexpr auto BAD_PROFILE_INDEX = -1;
	constexpr auto MIN_DURATION = 10;
	constexpr auto MIN_LEVEL = 0;
	constexpr auto MAX_POWER_LEVEL = 32;
	constexpr auto MIN_POWER_LEVEL = 0;

	// General settings can be tinkered with
	constexpr auto SAFE_POWER_LEVEL = 10000;
	constexpr auto TOO_CLOSE_TO_END_STOP = 31000;
	constexpr auto HIT_END_STOP = 32500;
	constexpr auto PROFILE_UNITS = 200;
	constexpr auto PROFILE_TIME_PERIOD = 10;

	// Object holds profiling details
	class Profile
	{
		// TODO make private
	public:
		Sint8 direction; // 1 means right, -1 means left
		int reading; // reading number
		double delta; // rate of change
		Sint16 from; // postion
		Sint16 to; // position
		Uint16 power; // level of effect
		Sint16 distance; // how far moves
		clock_t timeStamp; // Time reading was taken
		clock_t duration;
		bool freeWheel;
		bool timedOut;
		clock_t timeToMove; // time from command to 1st movement


	public:
		// TODO add other setters
		// TODO calculate delta and average power
		Profile();

		// Getters and Setters
		void sDirection(Sint8 dir);
		Sint8 gDirection();

		void sReading(Sint8 read);
		Sint8 gReading();

	};


	// Define Object of effects
	class hapticEffects
	{
	private:
		SDL_HapticEffect effect;
		SDL_Haptic* haptic = nullptr;
		
		//std::map<int, int> effectsMap = {
		//	{CONSTANT_LEFT, -1},
		//	{CONSTANT_RIGHT, -1}
		//};

		int32_t maxPosition, minPosition; // stores max / min wheel position that was found
		
	public:
		int setConstantForce(Uint32 mS, Uint16 level, int direction);
		int runEffect(int effect, Uint32 iterations);
		void setMaxPosition(int32_t position);
		void setMinPosition(int32_t position);
		void findMaxWheelPosition();
		void findMinWheelPosition();

		int32_t getMaxPosition();
		int32_t getMinPosition();
		SDL_Haptic* setHaptic(int device);

		// Stores profile of power levels in 10 mS intervals
		Profile profileLeft[MAX_POWER_LEVEL + 1][PROFILE_UNITS + 1];
		Profile profileRight[MAX_POWER_LEVEL + 1][PROFILE_UNITS + 1];
		
		std::map<int, int> effectsMap = {
			{CONSTANT_LEFT, -1},
			{CONSTANT_RIGHT, -1}
		};

		// Constructor
		hapticEffects();
	};



}

