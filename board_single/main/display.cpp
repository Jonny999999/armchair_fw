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
float getVoltage1(adc1_channel_t adc, uint32_t samples){
	//measure voltage
	int measure = 0;
	for (int j=0; j<samples; j++){
		measure += adc1_get_raw(adc);
		ets_delay_us(50);
	}
	return (float)measure / samples / 4096 * 3.3;
}



//======================
//===== variables ======
//======================
//display
SSD1306_t dev;
//tag for logging
static const char * TAG = "display";



//======================
//==== display_init ====
//======================
//note CONFIG_OFFSETX is used (from menuconfig)
void display_init(display_config_t config){
	adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); //max voltage
	ESP_LOGW(TAG, "Initializing Display...");
	ESP_LOGI(TAG, "config: sda=%d, sdl=%d, reset=%d,  offset=%d, flip=%d, size: %dx%d", 
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
	ESP_LOGD(TAG, "print center - isLarge=%d, value='%s', needed-spaces=%d, resulted-string='%s'", isLarge, tmp, numSpaces, buf);

	// show line on display
	if (isLarge)
		ssd1306_display_text_x3(display, line, buf, maxLen, inverted);
	else
		ssd1306_display_text(display, line, buf, maxLen, inverted);
}



//----------------------------------
//------- getBatteryVoltage --------
//----------------------------------
float getBatteryVoltage(){
#define BAT_VOLTAGE_CONVERSION_FACTOR 11.9
	float voltageRead = getVoltage1(ADC_BATT_VOLTAGE, 1000);
	float battVoltage = voltageRead * 11.9; //note: factor comes from simple test with voltmeter
	ESP_LOGD(TAG, "batteryVoltage - voltageAdc=%f, voltageConv=%f, factor=%.2f", voltageRead, battVoltage,  BAT_VOLTAGE_CONVERSION_FACTOR);
	return battVoltage;
}



//----------------------------------
//------- getBatteryPercent --------
//----------------------------------
//TODO find better/more accurate table?
//configure discharge curve of one cell with corresponding known voltage->chargePercent values
const float voltageLevels[] = {3.00, 3.45, 3.68, 3.74, 3.77, 3.79, 3.82, 3.87, 3.92, 3.98, 4.06, 4.20};
const float percentageLevels[] = {0.0, 5.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0};

float getBatteryPercent(){
	float voltage = getBatteryVoltage();
	float cellVoltage = voltage/BAT_CELL_COUNT;
	int size = sizeof(voltageLevels) / sizeof(voltageLevels[0]);
	int sizePer = sizeof(percentageLevels) / sizeof(percentageLevels[0]);
	//check if configured correctly
	if (size != sizePer) {
		ESP_LOGE(TAG, "getBatteryPercent - count of configured percentages do not match count of voltages");
		return 0;
	}
	if (cellVoltage <= voltageLevels[0]) {
		return 0.0;
	} else if (cellVoltage >= voltageLevels[size - 1]) {
		return 100.0;
	}

	//scale voltage linear to percent in matched range
	for (int i = 1; i < size; ++i) {
		if (cellVoltage <= voltageLevels[i]) {
			float voltageRange = voltageLevels[i] - voltageLevels[i - 1];
			float voltageOffset = cellVoltage - voltageLevels[i - 1];
			float percentageRange = percentageLevels[i] - percentageLevels[i - 1];
			float percentageOffset = percentageLevels[i - 1];
			float percent = percentageOffset + (voltageOffset / voltageRange) * percentageRange;
			ESP_LOGD(TAG, "getBatPercent - cellVoltage=%.3f => percentage=%.3f", cellVoltage, percent);
			ESP_LOGD(TAG, "getBatPercent - matched range: %.2fV-%.2fV  => %.1f%%-%.1f%%", voltageLevels[i-1], voltageLevels[i], percentageLevels[i-1], percentageLevels[i]);
			return percent;
		}
	}
	ESP_LOGE(TAG, "getBatteryPercent - unknown voltage range");
	return 0.0; //unknown range
}



//-----------------------
//----- showScreen1 -----
//-----------------------
//shows overview on entire display:
//percentage, voltage, current, mode, rpm, speed
void showScreen1(display_task_parameters_t * objects)
{
	//-- battery percentage --
	// TODO update when no load (currentsensors = ~0A) only
	//large
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
}



//------------------------
//---- showStartupMsg ----
//------------------------
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
//======= display task =======
//============================
#define STATUS_SCREEN_UPDATE_INTERVAL 500
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
		else //show status screen in any other mode
		{
			showScreen1(objects);
			vTaskDelay(STATUS_SCREEN_UPDATE_INTERVAL / portTICK_PERIOD_MS);
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