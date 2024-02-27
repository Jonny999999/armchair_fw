#include "display.hpp"
extern "C"{
#include <driver/adc.h>
#include "esp_ota_ops.h"
}

#include "menu.hpp"



//=== content config ===
#define STARTUP_MSG_TIMEOUT 2000
#define ADC_BATT_VOLTAGE ADC1_CHANNEL_6
#define BAT_CELL_COUNT 7



//--------------------------
//------- getVoltage -------
//--------------------------
//TODO duplicate code: getVoltage also defined in currentsensor.cpp -> outsource this
//local function to get average voltage from adc
int readAdc(adc1_channel_t adc, uint32_t samples){
	//measure voltage
	uint32_t measure = 0;
	for (int j=0; j<samples; j++){
		measure += adc1_get_raw(adc);
		ets_delay_us(50);
	}
	//return (float)measure / samples / 4096 * 3.3;
	return measure / samples;
}



//======================
//===== variables ======
//======================
//display
SSD1306_t dev;
//tag for logging
static const char * TAG = "display";
//define currently shown status page (continously displayed content when not in MENU mode)
static displayStatusPage_t selectedStatusPage = STATUS_SCREEN_OVERVIEW;



//======================
//==== display_init ====
//======================
void display_init(display_config_t config){
	adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); //max voltage
	ESP_LOGI(TAG, "Initializing Display with config: sda=%d, sdl=%d, reset=%d,  offset=%d, flip=%d, size: %dx%d", 
	config.gpio_sda, config.gpio_scl, config.gpio_reset, config.offsetX, config.flip, config.width, config.height);

	i2c_master_init(&dev, config.gpio_sda, config.gpio_scl, config.gpio_reset);
	if (config.flip) {
		dev._flip = true;
		ESP_LOGW(TAG, "Flip upside down");
	}
	ssd1306_init(&dev, config.width, config.height, config.offsetX);

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, config.contrast);
}



//===============================
//======= displayTextLine =======
//===============================
//abstracted function for printing one line on the display, using a format string directly
//and options: Large-font (3 lines, max 5 digits), or inverted color
void displayTextLine(SSD1306_t *display, int line, bool isLarge, bool inverted, const char *format, ...)
{
	char buf[17];
	int len;

	// format string + arguments to string
	va_list args;
	va_start(args, format);
	len = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	// show line on display
	if (isLarge)
		ssd1306_display_text_x3(display, line, buf, len, inverted);
	else
		ssd1306_display_text(display, line, buf, len, inverted);
}



//===================================
//===== displayTextLineCentered =====
//===================================
//abstracted function for printing a string CENTERED on the display, using a format string
//adds spaces left and right to fill the line (if not too long already)
#define MAX_LEN_NORMAL 16 //count of available digits on display (normal/large font)
#define MAX_LEN_LARGE 5
void displayTextLineCentered(SSD1306_t *display, int line, bool isLarge, bool inverted, const char *format, ...)
{
	// variables
	char buf[MAX_LEN_NORMAL*2 + 2];
	char tmp[MAX_LEN_NORMAL + 1];
	int len;

	// format string + arguments to string (-> tmp)
	va_list args;
	va_start(args, format);
	len = vsnprintf(tmp, sizeof(tmp), format, args);
	va_end(args);

	// define max available digits
	int maxLen = MAX_LEN_NORMAL;
	if (isLarge)
		maxLen = MAX_LEN_LARGE;

	// determine required spaces
	int numSpaces = (maxLen - len) / 2;
	if (numSpaces < 0) // limit to 0 in case string is too long already
		numSpaces = 0;

	// add certain spaces around string (-> buf)
	snprintf(buf, MAX_LEN_NORMAL*2, "%*s%s%*s", numSpaces, "", tmp, maxLen - numSpaces - len, "");
	ESP_LOGV(TAG, "print center - isLarge=%d, value='%s', needed-spaces=%d, resulted-string='%s'", isLarge, tmp, numSpaces, buf);

	// show line on display
	if (isLarge)
		ssd1306_display_text_x3(display, line, buf, maxLen, inverted);
	else
		ssd1306_display_text(display, line, buf, maxLen, inverted);
}



//=================================
//===== scaleUsingLookupTable =====
//=================================
//scale/inpolate an input value to output value between several known points (two arrays)
//notes: the lookup values must be in ascending order. If the input value is lower/larger than smalles/largest value, output is set to first/last element of output elements
float scaleUsingLookupTable(const float lookupInput[], const float lookupOutput[], int count, float input){
	// check limit case (set to min/max)
	if (input <= lookupInput[0]) {
		ESP_LOGV(TAG, "lookup: %.2f is lower than lowest value -> returning min", input);
		return lookupOutput[0];
	} else if (input >= lookupInput[count -1]) {
		ESP_LOGV(TAG, "lookup: %.2f is larger than largest value -> returning max", input);
		return lookupOutput[count -1];
	}

	// find best matching range and
	// scale input linear to output in matched range
	for (int i = 1; i < count; ++i)
	{
		if (input <= lookupInput[i]) //best match
		{
			float voltageRange = lookupInput[i] - lookupInput[i - 1];
			float voltageOffset = input - lookupInput[i - 1];
			float percentageRange = lookupOutput[i] - lookupOutput[i - 1];
			float percentageOffset = lookupOutput[i - 1];
			float output = percentageOffset + (voltageOffset / voltageRange) * percentageRange;
			ESP_LOGV(TAG, "lookup: - input=%.3f => output=%.3f", input, output);
			ESP_LOGV(TAG, "lookup - matched range: %.2fV-%.2fV  => %.1f-%.1f", lookupInput[i - 1], lookupInput[i], lookupOutput[i - 1], lookupOutput[i]);
			return output;
		}
	}
	ESP_LOGE(TAG, "lookup - unknown range");
	return 0.0; //unknown range
}


//==================================
//======= getBatteryVoltage ========
//==================================
// apparently the ADC in combination with the added filter and voltage 
// divider is slightly non-linear -> using lookup table
const float batteryAdcValues[] = {1732, 2418, 2509, 2600, 2753, 2853, 2889, 2909, 2936, 2951, 3005, 3068, 3090, 3122};
const float batteryVoltages[] = {14.01, 20, 21, 22, 24, 25.47, 26, 26.4, 26.84, 27, 28, 29.05, 29.4, 30};

float getBatteryVoltage(){
	// check if lookup table is configured correctly
	int countAdc = sizeof(batteryAdcValues) / sizeof(float);
	int countVoltages = sizeof(batteryVoltages) / sizeof(float);
	if (countAdc != countVoltages)
	{
		ESP_LOGE(TAG, "getBatteryVoltage - count of configured adc-values do not match count of voltages");
		return 0;
	}

	//read adc 
	int adcRead = readAdc(ADC_BATT_VOLTAGE, 1000);

	//convert adc to voltage using lookup table
	float battVoltage = scaleUsingLookupTable(batteryAdcValues, batteryVoltages, countAdc, adcRead);
	ESP_LOGD(TAG, "batteryVoltage - adcRaw=%d => voltage=%.3f, scaled using lookuptable with %d elements", adcRead, battVoltage, countAdc);
	return battVoltage;
}


//----------------------------------
//------- getBatteryPercent --------
//----------------------------------
// TODO find better/more accurate table?
// configure discharge curve of one cell with corresponding known voltage->chargePercent values
const float cellVoltageLevels[] = {3.00, 3.45, 3.68, 3.74, 3.77, 3.79, 3.82, 3.87, 3.92, 3.98, 4.06, 4.20};
const float cellPercentageLevels[] = {0.0, 5.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0};

float getBatteryPercent()
{
	// check if lookup table is configured correctly
	int sizeVoltage = sizeof(cellVoltageLevels) / sizeof(cellVoltageLevels[0]);
	int sizePer = sizeof(cellPercentageLevels) / sizeof(cellPercentageLevels[0]);
	if (sizeVoltage != sizePer)
	{
		ESP_LOGE(TAG, "getBatteryPercent - count of configured percentages do not match count of voltages");
		return 0;
	}

	//get current battery voltage
	float voltage = getBatteryVoltage();
	float cellVoltage = voltage / BAT_CELL_COUNT;
	
	//convert voltage to battery percentage using lookup table
	float percent = scaleUsingLookupTable(cellVoltageLevels, cellPercentageLevels, sizeVoltage, cellVoltage);
	ESP_LOGD(TAG, "batteryPercentage - Battery=%.3fV, Cell=%.3fV => percentage=%.3f, scaled using lookuptable with %d elements", voltage, cellVoltage, percent, sizePer);
	return percent;
}


//#############################
//#### showScreen Overview ####
//#############################
//shows overview on entire display:
//Battery percentage, voltage, current, mode, rpm, speed
#define STATUS_SCREEN_OVERVIEW_UPDATE_INTERVAL 500
void showStatusScreenOverview(display_task_parameters_t * objects)
{
	//-- battery percentage --
	// TODO update when no load (currentsensors = ~0A) only
	//-- large batt percent --
	displayTextLine(&dev, 0, true, false, "B:%02.0f%%", getBatteryPercent());

	//-- voltage and current --
	displayTextLine(&dev, 3, false, false, "%04.1fV %04.1f:%04.1fA",
				   getBatteryVoltage(),
				   fabs(objects->motorLeft->getCurrentA()),
				   fabs(objects->motorRight->getCurrentA()));

	//-- control state --
	//print large line
	displayTextLine(&dev, 4, true, false, "%s ", objects->control->getCurrentModeStr());

	//-- speed and RPM --
	displayTextLine(&dev, 7, false, false, "%3.1fkm/h %03.0f:%03.0fR",
				   fabs((objects->speedLeft->getKmph() + objects->speedRight->getKmph()) / 2),
				   objects->speedLeft->getRpm(),
				   objects->speedRight->getRpm());

	// debug speed sensors
	ESP_LOGD(TAG, "%3.1fkm/h %03.0f:%03.0fR",
				   fabs((objects->speedLeft->getKmph() + objects->speedRight->getKmph()) / 2),
				   objects->speedLeft->getRpm(),
				   objects->speedRight->getRpm());
	vTaskDelay(STATUS_SCREEN_OVERVIEW_UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


//############################
//##### showScreen Speed #####
//############################
// shows speed of each motor in km/h large in two lines and RPM in last line
#define STATUS_SCREEN_SPEED_UPDATE_INTERVAL 300
void showStatusScreenSpeed(display_task_parameters_t * objects)
{
	// title
	displayTextLine(&dev, 0, false, false, "Speed L,R - km/h");
	// show km/h large in two lines
	displayTextLine(&dev, 1, true, false, "%+.2f", objects->speedLeft->getKmph());
	displayTextLine(&dev, 4, true, false, "%+.2f", objects->speedRight->getKmph());
	// show both rotational speeds in one line
	displayTextLineCentered(&dev, 7, false, false, "%+04.0f:%+04.0f RPM",
				   objects->speedLeft->getRpm(),
				   objects->speedRight->getRpm());
	vTaskDelay(STATUS_SCREEN_SPEED_UPDATE_INTERVAL / portTICK_PERIOD_MS);
}



//#############################
//#### showScreen Joystick ####
//#############################
// shows speed of each motor in km/h large in two lines and RPM in last line
#define STATUS_SCREEN_JOYSTICK_UPDATE_INTERVAL 100
void showStatusScreenJoystick(display_task_parameters_t * objects)
{
        // print all joystick data
        joystickData_t data = objects->joystick->getData();
        displayTextLine(&dev, 0, false, false, "joystick status:");
        displayTextLine(&dev, 1, false, false, "x = %.3f     ", data.x);
        displayTextLine(&dev, 2, false, false, "y = %.3f     ", data.y);
        displayTextLine(&dev, 3, false, false, "radius = %.3f", data.radius);
        displayTextLine(&dev, 4, false, false, "angle = %-06.3f   ", data.angle);
        displayTextLine(&dev, 5, false, false, "pos=%-12s ", joystickPosStr[(int)data.position]);
        displayTextLine(&dev, 6, false, false, "adc: %d:%d ", objects->joystick->getRawX(), objects->joystick->getRawY());
		displayTextLine(&dev, 7, false, false, "mode=%s        ", objects->control->getCurrentModeStr());
		vTaskDelay(STATUS_SCREEN_JOYSTICK_UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


//#############################
//##### showScreen motors #####
//#############################
// shows speed of each motor in km/h large in two lines and RPM in last line
#define STATUS_SCREEN_MOTORS_UPDATE_INTERVAL 150
void showStatusScreenMotors(display_task_parameters_t *objects)
{
		// print all joystick data
		joystickData_t data = objects->joystick->getData();
		displayTextLine(&dev, 0, true, false, "%-4.0fW ", fabs(objects->motorLeft->getCurrentA()) * getBatteryVoltage());
		displayTextLine(&dev, 3, true, false, "%-4.0fW ", fabs(objects->motorRight->getCurrentA()) * getBatteryVoltage());
		//displayTextLine(&dev, 0, true, false, "L:%02.0f%%", objects->motorLeft->getStatus().duty);
		//displayTextLine(&dev, 3, true, false, "R:%02.0f%%", objects->motorRight->getStatus().duty);
		displayTextLineCentered(&dev, 6, false, false, "%+03.0f%% | %+03.0f%% DTY",
						objects->motorLeft->getStatus().duty,
						objects->motorRight->getStatus().duty);
		displayTextLineCentered(&dev, 7, false, false, "%+04.0f | %+04.0f RPM",
								objects->speedLeft->getRpm(),
								objects->speedRight->getRpm());
		vTaskDelay(STATUS_SCREEN_MOTORS_UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


//########################
//#### showStartupMsg ####
//########################
//shows welcome message and information about current version
void showStartupMsg(){
	const esp_app_desc_t * desc = esp_ota_get_app_description();

	//show message
	displayTextLine(&dev, 0, true, false, "START");
	//show git-tag
	displayTextLine(&dev, 4, false, false, "%s", desc->version);
	//show build-date (note: date,time of last clean build)
	displayTextLine(&dev, 6, false, false, "%s", desc->date);
	//show build-time
	displayTextLine(&dev, 7, false, false, "%s", desc->time);
}



//============================
//===== selectStatusPage =====
//============================
void display_selectStatusPage(displayStatusPage_t newStatusPage){
	ESP_LOGW(TAG, "switching statusPage from %d to %d", (int)selectedStatusPage, (int)newStatusPage);
	selectedStatusPage = newStatusPage;
}


//============================
//======= display task =======
//============================
// TODO: separate task for each loop?
void display_task(void *pvParameters)
{
	ESP_LOGW(TAG, "Initializing display and starting handle loop");
	//get struct with pointers to all needed global objects from task parameter
	display_task_parameters_t *objects = (display_task_parameters_t *)pvParameters;

	// initialize display
	display_init(objects->displayConfig);
	// TODO check if successfully initialized

	// show startup message
	showStartupMsg();
	vTaskDelay(STARTUP_MSG_TIMEOUT / portTICK_PERIOD_MS);
	ssd1306_clear_screen(&dev, false);

	// repeatedly update display with content
	while (1)
	{
		if (objects->control->getCurrentMode() == controlMode_t::MENU)
		{
			//uses encoder events to control menu and updates display
			handleMenu(objects, &dev);
		}
		else //show selected status screen in any other mode
		{
			switch (selectedStatusPage)
			{
			default:
			case STATUS_SCREEN_OVERVIEW:
				showStatusScreenOverview(objects);
				break;
			case STATUS_SCREEN_SPEED:
				showStatusScreenSpeed(objects);
				break;
			case STATUS_SCREEN_JOYSTICK:
				showStatusScreenJoystick(objects);
				break;
			case STATUS_SCREEN_MOTORS:
				showStatusScreenMotors(objects);
				break;
			}
		}
		// TODO add pages and menus
	}
}

	//-----------------------------------
	//---- text-related example code ----
	//-----------------------------------
	//ssd1306_display_text(&dev, 0, "SSD1306 128x64", 14, false);
	//ssd1306_display_text(&dev, 1, "ABCDEFGHIJKLMNOP", 16, false);
	//ssd1306_display_text(&dev, 2, "abcdefghijklmnop",16, false);
	//ssd1306_display_text(&dev, 3, "Hello World!!", 13, false);
	////ssd1306_clear_line(&dev, 4, true);
	////ssd1306_clear_line(&dev, 5, true);
	////ssd1306_clear_line(&dev, 6, true);
	////ssd1306_clear_line(&dev, 7, true);
	//ssd1306_display_text(&dev, 4, "SSD1306 128x64", 14, true);
	//ssd1306_display_text(&dev, 5, "ABCDEFGHIJKLMNOP", 16, true);
	//ssd1306_display_text(&dev, 6, "abcdefghijklmnop",16, true);
	//ssd1306_display_text(&dev, 7, "Hello World!!", 13, true);
	//
	//// Display Count Down
	//uint8_t image[24];
	//memset(image, 0, sizeof(image));
	//ssd1306_display_image(&dev, top, (6*8-1), image, sizeof(image));
	//ssd1306_display_image(&dev, top+1, (6*8-1), image, sizeof(image));
	//ssd1306_display_image(&dev, top+2, (6*8-1), image, sizeof(image));
	//for(int font=0x39;font>0x30;font--) {
	//	memset(image, 0, sizeof(image));
	//	ssd1306_display_image(&dev, top+1, (7*8-1), image, 8);
	//	memcpy(image, font8x8_basic_tr[font], 8);
	//	if (dev._flip) ssd1306_flip(image, 8);
	//	ssd1306_display_image(&dev, top+1, (7*8-1), image, 8);
	//	vTaskDelay(1000 / portTICK_PERIOD_MS);
	//}
	//
	//// Scroll Up
	//ssd1306_clear_screen(&dev, false);
	//ssd1306_contrast(&dev, 0xff);
	//ssd1306_display_text(&dev, 0, "---Scroll  UP---", 16, true);
	////ssd1306_software_scroll(&dev, 7, 1);
	//ssd1306_software_scroll(&dev, (dev._pages - 1), 1);
	//for (int line=0;line<bottom+10;line++) {
	//	lineChar[0] = 0x01;
	//	sprintf(&lineChar[1], " Line %02d", line);
	//	ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
	//	vTaskDelay(500 / portTICK_PERIOD_MS);
	//}
	//vTaskDelay(3000 / portTICK_PERIOD_MS);
	//
	//// Scroll Down
	//ssd1306_clear_screen(&dev, false);
	//ssd1306_contrast(&dev, 0xff);
	//ssd1306_display_text(&dev, 0, "--Scroll  DOWN--", 16, true);
	////ssd1306_software_scroll(&dev, 1, 7);
	//ssd1306_software_scroll(&dev, 1, (dev._pages - 1) );
	//for (int line=0;line<bottom+10;line++) {
	//	lineChar[0] = 0x02;
	//	sprintf(&lineChar[1], " Line %02d", line);
	//	ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
	//	vTaskDelay(500 / portTICK_PERIOD_MS);
	//}
	//vTaskDelay(3000 / portTICK_PERIOD_MS);

	//// Page Down
	//ssd1306_clear_screen(&dev, false);
	//ssd1306_contrast(&dev, 0xff);
	//ssd1306_display_text(&dev, 0, "---Page	DOWN---", 16, true);
	//ssd1306_software_scroll(&dev, 1, (dev._pages-1) );
	//for (int line=0;line<bottom+10;line++) {
	//	//if ( (line % 7) == 0) ssd1306_scroll_clear(&dev);
	//	if ( (line % (dev._pages-1)) == 0) ssd1306_scroll_clear(&dev);
	//	lineChar[0] = 0x02;
	//	sprintf(&lineChar[1], " Line %02d", line);
	//	ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
	//	vTaskDelay(500 / portTICK_PERIOD_MS);
	//}
	//vTaskDelay(3000 / portTICK_PERIOD_MS);

	//// Horizontal Scroll
	//ssd1306_clear_screen(&dev, false);
	//ssd1306_contrast(&dev, 0xff);
	//ssd1306_display_text(&dev, center, "Horizontal", 10, false);
	//ssd1306_hardware_scroll(&dev, SCROLL_RIGHT);
	//vTaskDelay(5000 / portTICK_PERIOD_MS);
	//ssd1306_hardware_scroll(&dev, SCROLL_LEFT);
	//vTaskDelay(5000 / portTICK_PERIOD_MS);
	//ssd1306_hardware_scroll(&dev, SCROLL_STOP);
	//
	//// Vertical Scroll
	//ssd1306_clear_screen(&dev, false);
	//ssd1306_contrast(&dev, 0xff);
	//ssd1306_display_text(&dev, center, "Vertical", 8, false);
	//ssd1306_hardware_scroll(&dev, SCROLL_DOWN);
	//vTaskDelay(5000 / portTICK_PERIOD_MS);
	//ssd1306_hardware_scroll(&dev, SCROLL_UP);
	//vTaskDelay(5000 / portTICK_PERIOD_MS);
	//ssd1306_hardware_scroll(&dev, SCROLL_STOP);
	//
	//// Invert
	//ssd1306_clear_screen(&dev, true);
	//ssd1306_contrast(&dev, 0xff);
	//ssd1306_display_text(&dev, center, "  Good Bye!!", 12, true);
	//vTaskDelay(5000 / portTICK_PERIOD_MS);


	//// Fade Out
	//ssd1306_fadeout(&dev);