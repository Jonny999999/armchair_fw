#include "display.hpp"
extern "C"{
#include <driver/adc.h>
}


//#
//# SSD1306 Configuration
//#
#define GPIO_RANGE_MAX 33
#define I2C_INTERFACE y
//# SSD1306_128x32 is not set
#define SSD1306_128x64 y
#define OFFSETX 0
//# FLIP is not set
#define SCL_GPIO 22
#define SDA_GPIO 23
#define RESET_GPIO 15 //FIXME remove this
#define I2C_PORT_0 y
//# I2C_PORT_1 is not set
//# end of SSD1306 Configuration

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



//==========================
//======= variables ========
//==========================
SSD1306_t dev;
//int center, top, bottom;
char lineChar[20];
//top = 2;
//center = 3;
//bottom = 8;
//tag for logging
static const char * TAG = "display";




//=================
//===== init ======
//=================
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

float getBatteryPercent(float voltage){
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

float getBatteryPercent(){
	float voltage = getBatteryVoltage();
	return getBatteryPercent(voltage);
}




//============================
//======= display task =======
//============================
#define VERY_SLOW_LOOP_INTERVAL 30000
#define SLOW_LOOP_INTERVAL 1000
#define FAST_LOOP_INTERVAL 200
//TODO: separate taks for each loop?

void display_task( void * pvParameters ){
	char buf[20];
	char buf1[20];
	int len, len1;
	int countFastloop = SLOW_LOOP_INTERVAL;
	int countSlowLoop = VERY_SLOW_LOOP_INTERVAL;

	display_init();
	//TODO check if successfully initialized

	//welcome msg
	strcpy(buf, "Hello");
	ssd1306_display_text_x3(&dev, 0, buf, 5, false);
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	//update stats
	while(1){

		if (countFastloop >= SLOW_LOOP_INTERVAL / FAST_LOOP_INTERVAL){
			//---- very slow loop ----
			if (countSlowLoop >= VERY_SLOW_LOOP_INTERVAL/SLOW_LOOP_INTERVAL){
				//clear display - workaround for bugged line order after a few minutes
				countSlowLoop = 0;
				ssd1306_clear_screen(&dev, false);
			}
			//---- slow loop ----
			countSlowLoop ++;
			countFastloop = 0;
			//--- battery stats ---
			//TODO update only when no load (currentsensors = ~0A)
			float battVoltage = getBatteryVoltage();
			float battPercent = getBatteryPercent(battVoltage);
			len = snprintf(buf, sizeof(buf), "Bat:%.1fV %.2fV", battVoltage, battVoltage/BAT_CELL_COUNT);
			len1 = snprintf(buf1, sizeof(buf1), "B:%02.0f%%", battPercent);
			ssd1306_display_text_x3(&dev, 0, buf1, len1, false);
			ssd1306_display_text(&dev, 3, buf, len, false);
			ssd1306_display_text(&dev, 4, buf, len, true);
		}

		//---- fast loop ----
		//update speed/rpm
		float sLeft = speedLeft.getKmph();
		float rLeft = speedLeft.getRpm();
		float sRight = speedRight.getKmph();
		float rRight = speedRight.getRpm();
		len = snprintf(buf, sizeof(buf), "L:%.1f R:%.1fkm/h", fabs(sLeft), fabs(sRight));
		ssd1306_display_text(&dev, 5, buf, len, false);
		len = snprintf(buf, sizeof(buf), "L:%4.0f R:%4.0fRPM", rLeft, rRight);
		ssd1306_display_text(&dev, 6, buf, len, false);
		//debug speed sensors
		ESP_LOGD(TAG, "%s", buf);
		//TODO show currentsensor values

		vTaskDelay(FAST_LOOP_INTERVAL / portTICK_PERIOD_MS);
		countFastloop++;
	}
	//TODO add pages and menus



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

#if 0
	// Fade Out
	for(int contrast=0xff;contrast>0;contrast=contrast-0x20) {
		ssd1306_contrast(&dev, contrast);
		vTaskDelay(40);
	}
#endif

}

