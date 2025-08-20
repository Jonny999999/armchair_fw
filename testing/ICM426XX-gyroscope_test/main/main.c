#include <stdio.h>
#include <stdint.h>
#include "ICM426XX.h"
#include "malloc_local.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"

/* INCLUDE for serial interface */
#include "I2Cdev.h"

#define TAG "ICM426XX-Example"

#define SERIF_TYPE ICM426XX_UI_I2C

#ifdef COMFIG_ICM426XX_USE_SPI4
#define SERIF_TYPE ICM426XX_UI_SPI4
#endif

// --- Bank 0 data register block (not in the lib headers) ---
#define ICM426XX_REG_TEMP_DATA1     0x1D
#define ICM426XX_REG_TEMP_DATA0     0x1E
#define ICM426XX_REG_ACCEL_DATA_X1  0x1F  // start here for a 14-byte burst
#define ICM426XX_REG_ACCEL_DATA_X0  0x20
#define ICM426XX_REG_ACCEL_DATA_Y1  0x21
#define ICM426XX_REG_ACCEL_DATA_Y0  0x22
#define ICM426XX_REG_ACCEL_DATA_Z1  0x23
#define ICM426XX_REG_ACCEL_DATA_Z0  0x24
#define ICM426XX_REG_GYRO_DATA_X1   0x25
#define ICM426XX_REG_GYRO_DATA_X0   0x26
#define ICM426XX_REG_GYRO_DATA_Y1   0x27
#define ICM426XX_REG_GYRO_DATA_Y0   0x28
#define ICM426XX_REG_GYRO_DATA_Z1   0x29
#define ICM426XX_REG_GYRO_DATA_Z0   0x2A


#define ECHK(val) do{int v = (val); if(v != 0) {  \
    ESP_LOGE(TAG,"TRACE %s(%d): %d", __FUNCTION__, __LINE__, v);  \
	for (int i = 5; i >= 0; i--) {\
	ESP_LOGE(TAG, "Restarting in %d seconds...\n", i); osSleep(1000);} \
	ESP_LOGE(TAG, "Restarting now.\n"); \
	fflush(stdout); \
	abort();} \
	}while(0)

#define ELOG(val) do{int v = (val); if(v != 0) {ESP_LOGE(TAG, "Error in %s(%d): %d", __FUNCTION__, __LINE__, v);}} while(0)
#define ELOG_RET(val) do{int v = (val); if(v != 0) {ESP_LOGE(TAG, "Error in %s(%d): %d", __FUNCTION__, __LINE__, v); return v;}} while(0)

static int ESP32_HAL_read_reg(struct inv_icm426xx_serif *serif, uint8_t reg, uint8_t *buf, uint32_t len);
static int ESP32_HAL_write_reg(struct inv_icm426xx_serif *serif, uint8_t reg, uint8_t *buf, uint32_t len);
static int ESP32_icm_serif_init(struct inv_icm426xx_serif *serif);
/*
 * --------------------------------------------------------------------------------------
 *  ESP32 SPECIFIC HAL Functions 
 * --------------------------------------------------------------------------------------
 *
 * --------------------------------------------------------------------------------------
 *  INSTRUCTIONS
 *
 * Insert the functions you want to use below, as per the example
 * --------------------------------------------------------------------------------------
 */

/** @brief Below are the I2C HAL functions compatible with the "Icm426xxTransport.h"
struct inv_icm426xx_serif {
	void *     context;
	int      (*read_reg)(struct inv_icm426xx_serif * serif, uint8_t reg, uint8_t * buf, uint32_t len);
	int      (*write_reg)(struct inv_icm426xx_serif * serif, uint8_t reg, const uint8_t * buf, uint32_t len);
	int      (*configure)(struct inv_icm426xx_serif * serif);
	uint32_t   max_read;
	uint32_t   max_write;
	ICM426XX_SERIAL_IF_TYPE_t serif_type;
};
*/
/**
 * @brief This is to connect in ESP32 HW functions for reading registers from the ICM426XX device
 * @param serif:  We only use ICM426XX_SERIAL_IF_TYPE_t serif_type to brach for the properly
 * @param reg:    address of register to be read
 * @param buf:    buffer pointer for data to be stored
 * @param len:    read length
 * 
 * @return: 0: OK
 *          Everything return that is not 0, means an error
 */
static int ESP32_HAL_read_reg(struct inv_icm426xx_serif *serif, uint8_t reg, uint8_t *buf, uint32_t len)
{
	switch (serif->serif_type)
	{
	case ICM426XX_AUX1_SPI3:
	case ICM426XX_AUX1_SPI4:
	case ICM426XX_AUX2_SPI3:
	case ICM426XX_UI_SPI4:
	//Add your SPI read function here
		return -10;
	case ICM426XX_UI_I2C:
		return (I2Cdev_readBytes(ICM426XX_devAddr, reg, len, buf));
	default:
		return -1;
	}
	return 1;
}

/**
 * @brief This is to connect in ESP32 HW functions for writing to registers of ICM426XX device
 * @param serif:  We only use ICM426XX_SERIAL_IF_TYPE_t serif_type to brach for the properly
 * @param reg:    address of register to be read
 * @param buf:    buffer pointer for data to be stored
 * @param len:    read length
 * 
 * @return: 0: OK
 *          Everything return that is not 0, means an error
 */
static int ESP32_HAL_write_reg(struct inv_icm426xx_serif *serif, uint8_t reg, uint8_t *buf, uint32_t len)
{
	switch (serif->serif_type)
	{
	case ICM426XX_AUX1_SPI3:
	case ICM426XX_AUX1_SPI4:
	case ICM426XX_AUX2_SPI3:
	case ICM426XX_UI_SPI4:
	//Add your SPI write function here
		return -10;
	case ICM426XX_UI_I2C:
		return I2Cdev_writeBytes(ICM426XX_devAddr, reg, len, buf);
	default:
		return -1;
	}
	return 1;
}

static int ESP32_icm_serif_init(struct inv_icm426xx_serif *serif)
{
	int rc;
	switch (serif->serif_type)
	{
	case ICM426XX_AUX1_SPI3:
	case ICM426XX_AUX1_SPI4:
	case ICM426XX_AUX2_SPI3:
	case ICM426XX_UI_SPI4:
		//TODO: Add SPI later
		return -10;
	case ICM426XX_UI_I2C:
		rc = I2Cdev_init(-1, -1);
		if (rc != ESP_OK)
		{
			ESP_LOGE(LOGNAME, "Failed to init I2Cdev: %s", esp_err_to_name(rc));
			return -1;
		}
		else
		{
			ESP_LOGI(LOGNAME, "I2C driver initialized!");
			return 0;
		}
	default:
		return -1;
	}
	return 1;
}


static void ICM426XX_processData(void *pvParams)
{
	ag_buffer_t *recv = NULL;
	ag_sensor_data_t *dp;
	while (1)
	{
		//ESP_LOGW(TAG, "Data waiting on Queue: %d", uxQueueMessagesWaiting(xICMeventQ));
		xQueueReceive(xICMeventQ, &recv, portMAX_DELAY);
		dp = recv->data;
		for(int i = 0; i < recv->pcnt; i++)
		{
		ESP_LOGI(TAG, "RAW AG DATA: <%d>ms\t temp:%3.1fC\t[x]-%d\t[y]-%d\t[z]-%d\t-\t[Rx]-%d\t[Ry]-%d\t[Rz]-%d", dp->tmstp/10000, (float)(dp->temp/10), dp->accel[0], dp->accel[1], dp->accel[2], dp->gyro[0], dp->gyro[1], dp->gyro[2]);
		dp++;
		}
		ICM426XX_delete_ag_buffer(recv);
		continue;
	}
	vTaskDelete(NULL);
}

static void printTaskList(void)
{
	char *info = malloc_local(800);
	vTaskList(info);
	ESP_LOGW(TAG, "\n%s", info);
	free_local(info);
}





// helper: read raw accel+gyro (bypass/FIFO off)
static esp_err_t poll_accel_gyro(int16_t acc[3], int16_t gyr[3], int16_t *temp10)
{
    // Register addresses are family-dependent; the 426xx has banked regs.
    // The driver exposes helpers so prefer those if available:
    //   inv_icm426xx_get_accel_raw(), inv_icm426xx_get_gyro_raw(), inv_icm426xx_get_temperature()
    // If not, do one burst read of the contiguous data block:
    //   ACCEL_DATA_X1 ... GYRO_DATA_Z0 (12 bytes) + TEMP_DATA (2 bytes)
    uint8_t buf[14];
    // Replace ICM426XX_REG_ACCEL_DATA_X1 with the correct symbol in your driver.
    int rc = I2Cdev_readBytes(ICM426XX_devAddr, ICM426XX_REG_ACCEL_DATA_X1, sizeof(buf), buf);
    if (rc != 0) return ESP_FAIL;

    acc[0] = (int16_t)((buf[0] << 8) | buf[1]);
    acc[1] = (int16_t)((buf[2] << 8) | buf[3]);
    acc[2] = (int16_t)((buf[4] << 8) | buf[5]);
    gyr[0] = (int16_t)((buf[6] << 8) | buf[7]);
    gyr[1] = (int16_t)((buf[8] << 8) | buf[9]);
    gyr[2] = (int16_t)((buf[10] << 8) | buf[11]);
    *temp10 = (int16_t)((buf[12] << 8) | buf[13]); // driver has temp scaling helper
    return ESP_OK;
}



// helper: one burst read of accel+gyro+temp (14 bytes starting at ACCEL_X1)
static esp_err_t poll_accel_gyro_raw(int16_t acc[3], int16_t gyr[3], int16_t *temp_raw)
{
    uint8_t buf[14];

    // make sure we’re on Bank 0 (data regs live here)
    ESP_ERROR_CHECK(ICM426XX_set_reg_bank(0));

    int rc = I2Cdev_readBytes(ICM426XX_devAddr, ICM426XX_REG_ACCEL_DATA_X1, sizeof(buf), buf);
    if (rc != 0) return ESP_FAIL;

    acc[0] = (int16_t)((buf[0]  << 8) | buf[1]);   // AX
    acc[1] = (int16_t)((buf[2]  << 8) | buf[3]);   // AY
    acc[2] = (int16_t)((buf[4]  << 8) | buf[5]);   // AZ
    gyr[0] = (int16_t)((buf[6]  << 8) | buf[7]);   // GX
    gyr[1] = (int16_t)((buf[8]  << 8) | buf[9]);   // GY
    gyr[2] = (int16_t)((buf[10] << 8) | buf[11]);  // GZ
    *temp_raw = (int16_t)((buf[12] << 8) | buf[13]); // TEMP

    return ESP_OK;
}



void app_main(void)
{
	{
		esp_chip_info_t chip_info;
		esp_chip_info(&chip_info);
		printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
			   chip_info.cores,
			   (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			   (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
		printf("silicon revision %d\n", chip_info.revision);
	}
	uint8_t devFound = 0;
	struct inv_icm426xx_serif serif = {0};
	serif.context = 0; /* no need */
	serif.read_reg = ESP32_HAL_read_reg;
	serif.write_reg = ESP32_HAL_write_reg;
	serif.max_read = 1024 * 32;	 /* maximum number of bytes allowed per serial read */
	serif.max_write = 1024 * 32; /* maximum number of bytes allowed per serial write */
	serif.serif_type = SERIF_TYPE;
	ESP_ERROR_CHECK(ESP32_icm_serif_init(&serif));
	ESP_LOGI(TAG, "Scanning I2C bus for ICM DEVICE");
	uint8_t found[10];
	I2Cdev_scan(&found[0], 10);

	for (int i = 0; i < 10; i++)
	{
		if (found[i] == ICM426XX_ADDR_AD0_LO)
		{
			ESP_LOGI(TAG, "Found ICM device at 0x%.2x", found[i]);
			devFound = found[i];
			break;
		}
		if (found[i] == ICM426XX_ADDR_AD0_HI)
		{
			ESP_LOGI(TAG, "Found ICM device at 0x%.2x", found[i]);
			devFound = found[i];
			break;
		}
	}
	if (devFound == 0) ESP_LOGE(TAG, "Device not found on I2C bus");
	osSleep(100);
	char icmName[9] = {0};
	ICM426XX_set_I2C_addr(devFound);
	ECHK(ICM426XX_driver_init(&serif, icmName));
	// ECHK(ICM426XX_set_fifo_threshold(10));
	// ECHK(ICM426XX_ConfigureUseFifo((uint8_t)IS_LOW_NOISE_MODE,
	// 						(uint8_t)IS_HIGH_RES_MODE,
	// 						ICM426XX_ACCEL_CONFIG0_FS_SEL_4g,
	// 						ICM426XX_GYRO_CONFIG0_FS_SEL_2000dps,
	// 						ICM426XX_ACCEL_CONFIG0_ODR_50_HZ,
	// 						ICM426XX_GYRO_CONFIG0_ODR_50_HZ,
	// 						(uint8_t)USE_CLK_IN));

	ECHK(ICM426XX_sensor_en_all());

	// 1) Bank 0
	ESP_ERROR_CHECK(ICM426XX_set_reg_bank(0));

	// 2) Power up accel+gyro in LN mode
	// PWR_MGMT0 (0x4E): [7:6]=0, [5]=TEMP_DIS(0=enable temp), [4]=IDLE
	//                   [3:2]=GYRO_MODE, [1:0]=ACCEL_MODE
	// LN mode = 0b11 for each -> value 0x0F
	uint8_t pwr = 0x0F;
	ESP_ERROR_CHECK(ICM426XX_writereg(0x4E, 1, &pwr));

	// Give sensors time to start (>= 20–30 ms or a few ODR periods)
	vTaskDelay(pdMS_TO_TICKS(30));


	// CUSTOM CONFIGURATION using direct driver access
	// Gyro ±250 dps, Accel ±4 g; ODR 200 Hz (pick what you want)
	// set full scale range
	ECHK(inv_icm426xx_set_gyro_fsr(&icm_driver, ICM426XX_GYRO_CONFIG0_FS_SEL_250dps));
	ECHK(inv_icm426xx_set_accel_fsr(&icm_driver, ICM426XX_ACCEL_CONFIG0_FS_SEL_4g));

	// fifo sample speed
	ECHK(inv_icm426xx_set_gyro_frequency(&icm_driver, ICM426XX_GYRO_CONFIG0_ODR_200_HZ));
	ECHK(inv_icm426xx_set_accel_frequency(&icm_driver, ICM426XX_ACCEL_CONFIG0_ODR_200_HZ));

	// 1) Enable low-noise paths (continuous, lowest noise)
	ECHK(inv_icm426xx_enable_accel_low_noise_mode(&icm_driver));
	ECHK(inv_icm426xx_enable_gyro_low_noise_mode(&icm_driver));

	// 4) Set the **low-noise bandwidths** (the UI LPF)
	//    Pick the narrowest BW that still tracks your motion well.
	//    The exact enum names depend on your Icm426xxDefs.h, but look for
	//    ICM426XX_GYRO_ACCEL_CONFIG0_GYRO_FILT_BW_*  and
	//    ICM426XX_GYRO_ACCEL_CONFIG0_ACCEL_FILT_BW_*
	//
	//    Examples you’re likely to find (choose one of the "lower" bandwidths):
	//      *_BW_AVG_5     (tight)
	//      *_BW_AVG_10    (tighter)
	//      *_BW_WIDE      (looser)
	//    or explicit bands like *_BW_ODR_DIV_10, *_BW_20HZ, etc.
	//
	//    Start with a *low* bandwidth for gyro, slightly higher for accel.

	ECHK(inv_icm426xx_set_gyro_ln_bw(&icm_driver, /* e.g. */ ICM426XX_GYRO_ACCEL_CONFIG0_GYRO_FILT_BW_2));
	ECHK(inv_icm426xx_set_accel_ln_bw(&icm_driver, /* e.g. */ ICM426XX_GYRO_ACCEL_CONFIG0_ACCEL_FILT_BW_2));




	// --- Main polling loop ---
	uint8_t count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));

        int16_t acc[3], gyr[3], t_raw;
        if (poll_accel_gyro_raw(acc, gyr, &t_raw) == ESP_OK) {
            printf("ACC raw: %6d %6d %6d | GYRO raw: %6d %6d %6d | TEMP raw: %6d\n",
                   acc[0], acc[1], acc[2], gyr[0], gyr[1], gyr[2], t_raw);
        } else {
            ESP_LOGW(TAG, "I2C read failed");
        }


		// run self test every 10s
		//count ++;
		//if (count > 100){
		//	count = 0;
		//	int res;
		//	printf("\n running selftest... \n");
		//	inv_icm426xx_run_selftest(&icm_driver, &res);
		//}
    }


}