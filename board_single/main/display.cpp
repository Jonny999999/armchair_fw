#include "display.hpp"
extern "C"{
#include <driver/adc.h>
}


//#
//# SSD1306 Configuration
//#
#define GPIO_RANGE_MAX 33
#define I2C_INTERFACE y
//# SPI_INTERFACE is not set
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





//--------------------------
//------- getVoltage -------
//--------------------------
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





void startDisplayTest(){

adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); //max voltage




	SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];

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
	ssd1306_display_text_x3(&dev, 0, "Hello", 5, false);
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	top = 2;
	center = 3;
	bottom = 8;
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

	vTaskDelay(1000 / portTICK_PERIOD_MS);


	while(1){
		float voltage = getVoltage1( ADC1_CHANNEL_6, 1000);
		float battVoltage = voltage * 11.9;
		char buf[20];
		char buf1[20];
		int len = snprintf(buf, sizeof(buf), "Batt: %.3fV", battVoltage);
		int len1 = snprintf(buf1, sizeof(buf1), "%.1fV", battVoltage);
		ESP_LOGD("display", "voltageAdc=%f, voltageConv=%f", voltage, battVoltage);
		ssd1306_display_text_x3(&dev, 0, buf1, len1, false);
		ssd1306_display_text(&dev, 3, buf, len, false);
		ssd1306_display_text(&dev, 4, buf, len, true);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	
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

