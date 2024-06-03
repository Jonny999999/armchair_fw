#include <driver/adc.h>

//supported current sensor working method:
  //0V = -ratedCurrent  
  //centerVoltage = 0A
  //3.3V = ratedCurrent

class currentSensor{
	public:
		currentSensor (adc1_channel_t adcChannel_f, float ratedCurrent, float snapToZeroThreshold, bool inverted = false);
		void calibrateZeroAmpere(void); //set current voltage to voltage representing 0A
		float read(void); //get current ampere
	private:
		adc1_channel_t adcChannel;
		float ratedCurrent;
		bool isInverted;
		float snapToZeroThreshold;
		uint32_t measure;
		float voltage;
		float current;
		float centerVoltage = 3.3/2;
};
