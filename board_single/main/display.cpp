#include "display.hpp"
extern "C"{
#include <driver/adc.h>
#include "esp_ota_ops.h"
}

#include "menu.hpp"



//==== display config ====
#define I2C_INTERFACE y
#define SCL_GPIO 22
#define SDA_GPIO 23
#define RESET_GPIO 15 // FIXME remove this
// the following options are set in menuconfig: (see sdkconfig)
//	#define CONFIG_OFFSETX 2 	//note: the larger display (actual 130x64) needs 2 pixel offset (prevents bugged column)
//	#define CONFIG_I2C_PORT_0 y

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
void display_init(){
	adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); //max voltage
	ESP_LOGW("display", "INTERFACE is i2c");
	ESP_LOGW("display", "SDA_GPIO=%d",SDA_GPIO);
	ESP_LOGW("display", "SCL_GPIO=%d",SCL_GPIO);
	ESP_LOGW("display", "RESET_GPIO=%d",RESET_GPIO);
	i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, RESET_GPIO);
#if FLIP
	dev._flip = true;
	ESP_LOGW("display", "Flip upside down");
#endif
	ESP_LOGI("display", "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
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
void showScreen1()
{
	char buf[20];
	char buf1[20];
	int len, len1;

	//-- battery percentage --
	// TODO update when no load (currentsensors = ~0A) only
	len1 = snprintf(buf1, sizeof(buf1), "B:%02.0f%%", getBatteryPercent());
	ssd1306_display_text_x3(&dev, 0, buf1, len1, false);

	//-- voltage and current --
	len = snprintf(buf, sizeof(buf), "%04.1fV %04.1f:%04.1fA",
				   getBatteryVoltage(),
				   fabs(motorLeft.getCurrentA()),
				   fabs(motorRight.getCurrentA()));
	ssd1306_display_text(&dev, 3, buf, len, false);

	//-- control state --
	len = snprintf(buf, sizeof(buf), "%s ", control.getCurrentModeStr());
	ssd1306_display_text_x3(&dev, 4, buf, len, false);

	//-- speed and RPM --
	len = snprintf(buf, sizeof(buf), "%3.1fkm/h %03.0f:%03.0fR",
				   fabs((speedLeft.getKmph() + speedRight.getKmph()) / 2),
				   speedLeft.getRpm(),
				   speedRight.getRpm());
	ssd1306_display_text(&dev, 7, buf, len, false);

	// debug speed sensors
	ESP_LOGD(TAG, "%s", buf);
}



//------------------------
//---- showStartupMsg ----
//------------------------
//shows welcome message and information about current version
void showStartupMsg(){
	char buf[20];
	int len;
	const esp_app_desc_t * desc = esp_ota_get_app_description();

	//show message
	len = snprintf(buf, 20, "START");
	ssd1306_display_text_x3(&dev, 0, buf, len, false);
	//show git-tag
	len = snprintf(buf, 20, "%s", desc->version);
	ssd1306_display_text(&dev, 4, buf, len, false);
	//show build-date (note: date,time of last clean build)
	len = snprintf(buf, 20, "%s", desc->date);
	ssd1306_display_text(&dev, 6, buf, len, false);
	//show build-time
	len = snprintf(buf, 20, "%s", desc->time);
	ssd1306_display_text(&dev, 7, buf, len, false);
}




//============================
//======= display task =======
//============================
#define VERY_SLOW_LOOP_INTERVAL 60000
#define SLOW_LOOP_INTERVAL 5000
#define FAST_LOOP_INTERVAL 200
//TODO: separate task for each loop?

void display_task( void * pvParameters ){
	//variables
	int countFastloop = 0;
	int countSlowLoop = 0;

	//initialize display
	display_init();
	//TODO check if successfully initialized

	//show startup message
	showStartupMsg();
	vTaskDelay(STARTUP_MSG_TIMEOUT / portTICK_PERIOD_MS);

	// repeatedly update display with content
	while (1)
	{

//currently only showing menu:
		handleMenu(&dev);


//status screen currently disabled:
	//	//--- fast loop ---
	//	showScreen1();

	//	if (countFastloop >= SLOW_LOOP_INTERVAL / FAST_LOOP_INTERVAL)
	//	{
	//		//--- slow loop ---

	//		if (countSlowLoop >= VERY_SLOW_LOOP_INTERVAL / SLOW_LOOP_INTERVAL)
	//		{
	//			//--- very slow loop ---
	//			// clear display - workaround for bugged line order after a few minutes
	//			countSlowLoop = 0;
	//			ssd1306_clear_screen(&dev, false);
	//		}
	//		countFastloop = 0;
	//		countSlowLoop++;
	//	}
	//	countFastloop++;
	//	vTaskDelay(FAST_LOOP_INTERVAL / portTICK_PERIOD_MS);
	//	// TODO add pages and menus
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