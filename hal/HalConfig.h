#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

#define HAL_UART_INVALED_PORT 0xff //无效串口定义
#define HAL_GPIO_INVALED_PIN  0xff

/*异常码定义 范围1~64*/
#define HAL_ERROR_CODE_WS    1 //风速异常码
#define HAL_ERROR_CODE_WD    2 //风向异常码
#define HAL_ERROR_CODE_TH    3 //温湿度异常码
#define HAL_ERROR_CODE_CO    4 //CO异常码
#define HAL_ERROR_CODE_NO2   5 //NO2异常码
#define HAL_ERROR_CODE_SO2   6 //SO2异常码
#define HAL_ERROR_CODE_O3    7 //O3异常码
#define HAL_ERROR_CODE_NOISE 8 //噪声异常码
#define HAL_ERROR_CODE_VOC   9 //VOC异常码
#define HAL_ERROR_CODE_PM    10 //PM异常码
#define HAL_ERROR_CODE_SDCARD 11 //SD卡异常码

#define HAL_ERROR_CODE_MPS   12
#define HAL_ERROR_CODE_COD   13
#define HAL_ERROR_CODE_FLOW  14
#define HAL_ERROR_CODE_NH4   15
#define HAL_ERROR_CODE_SS    16
#define HAL_ERROR_CODE_CHRM  17
#define HAL_ERROR_CODE_BOD   18

#define HAL_ERROR_CODE_GPS   63 //GPS异常码
#define HAL_ERROR_CODE_RTC   64

#define HAL_LEDPANEL_USING   0


#if defined(HAL_BOARD_NAME_LITE)
#define HAL_ERROR_CODE_WP    12
#endif

#if defined(HAL_BOARD_NAME_BASIC)

/****************************** Basic *********************************/

/*GPRS控制管脚*/
#define HAL_GPRS_POWER_PIN 0x00  //GPRS电源控制管脚
#define HAL_GPRS_POWER_ACTIVE_LEVEL   0
#define HAL_GPRS_POWER_DEACTIVE_LEVEL 1

#define HAL_GPRS_SLEEP_PIN 0x01  //休眠管脚
#define HAL_GPRS_SLEEP_ENTRY_LEVEL 1
#define HAL_GPRS_SLEEP_EXIT_LEVEL  0

/*指示灯管脚*/
#define HAL_STATUS_LED_PIN 0x24  //状态指示灯管脚
#define HAL_STATUS_LED_ENABLE_LEVEL  0
#define HAL_STATUS_LED_DISABLE_LEVEL 1

/*电源控制管脚*/
#define HAL_DB_POWER_PIN 0x4e  //DB POWER管脚
#define HAL_DB_POWER_ON_LEVEL  1
#define HAL_DB_POWER_OFF_LEVEL 0

/*485芯片控制管脚*/
#define HAL_485_DISPLAY_CONTRL_PIN 0x4d  //显示485芯片控制管脚
#define HAL_485_SENSOR_CONTRL_PIN  0x4f  //传感器485芯片控制管脚
#define HAL_485_CONTRL_ENABLE_LEVEL  1
#define HAL_485_CONTRL_DISABLE_LEVEL 0

/*串口定义*/
#define HAL_UART_GPRS_PORT    0  //uart1
#define HAL_UART_SENSOR_PORT  1  //uart2
#define HAL_UART_DISPLAY_PORT 2  //uart3
#define HAL_UART_LOG_PORT     3
#define HAL_UART_PM_PORT      HAL_UART_INVALED_PORT

/*模拟串口*/
#define HAL_IOUART_TX_PIN 0x48
#define HAL_IOUART_RX_PIN 0x49

#define MANUFACTURE_ENABLE 0
#elif defined(HAL_BOARD_NAME_LITE) 

/****************************** Lite *********************************/

/*GPRS控制管脚*/
#define HAL_GPRS_POWER_PIN 0x07  //GPRS电源控制管脚
#define HAL_GPRS_POWER_ACTIVE_LEVEL   0
#define HAL_GPRS_POWER_DEACTIVE_LEVEL 1

#define HAL_GPRS_SLEEP_PIN 0x06  //休眠管脚
#define HAL_GPRS_SLEEP_ENTRY_LEVEL 1
#define HAL_GPRS_SLEEP_EXIT_LEVEL  0

/*指示灯管脚*/
#define HAL_STATUS_LED_PIN 0x34  //状态指示灯管脚
#define HAL_STATUS_LED_ENABLE_LEVEL  0
#define HAL_STATUS_LED_DISABLE_LEVEL 1

/*电源控制管脚*/
#define HAL_DB_POWER_PIN 0x4e  //DB POWER管脚
#define HAL_DB_POWER_ON_LEVEL  1
#define HAL_DB_POWER_OFF_LEVEL 0

/*485芯片控制管脚*/
#define HAL_485_DISPLAY_CONTRL_PIN 0x4f  //显示485芯片控制管脚
#define HAL_485_SENSOR_CONTRL_PIN  HAL_GPIO_INVALED_PIN  //传感器485芯片控制管脚
#define HAL_485_CONTRL_ENABLE_LEVEL  1
#define HAL_485_CONTRL_DISABLE_LEVEL 0

/*串口定义*/
#define HAL_UART_GPRS_PORT    0  //uart1
#define HAL_UART_SENSOR_PORT  HAL_UART_INVALED_PORT //not used
#define HAL_UART_PM_PORT      1
#define HAL_UART_DISPLAY_PORT 2  //uart3
#define HAL_UART_LOG_PORT     3

/*模拟串口*/
#define HAL_IOUART_TX_PIN 0x48
#define HAL_IOUART_RX_PIN 0x49

/*PM传感器控制引脚*/
#define HAL_PM_SET_PIN    0x00
#define HAL_PM_RESET_PIN  0x01

#define MANUFACTURE_ENABLE 1
#endif

/*gprs缓存队列长度*/
#define HAL_GPRS_RECV_QUEUE_LEN  1024

/*flash分区
* 0 ~ 5k     :boot
* 5k ~ 125k  :app
* 125k ~ 245k:ota
* 245k ~ 256k:args
*/
#define KB(x) ((x)*1024)
#define DW(x) ((x)*4)

#define HAL_FLASH_SIZE (KB(256))  //256KB 120k(ota), 4k(boot) 4+240+12
#define HAL_FLASH_PAGE_SIZE (KB(2))
#define HAL_FLASH_OTA_SIZE (KB(120))

#define HAL_FLASH_BASE_ADDR      0x8000000
#define HAL_BOOT_FLASH_ADDR      (HAL_FLASH_BASE_ADDR + 0)
#define HAL_APP_FLASH_ADDR       (HAL_FLASH_BASE_ADDR + KB(6))
#define HAL_OTA_FLASH_ADDR       (HAL_FLASH_BASE_ADDR + KB(106))
#define HAL_DEVICE_OTA_INFO_ADDR (HAL_FLASH_BASE_ADDR + KB(206))
#define HAL_ARGS_FLASH_ADDR      (HAL_FLASH_BASE_ADDR + KB(248))

#define HAL_DEVICE_ID_FLASH_ADDR (HAL_ARGS_FLASH_ADDR + 0) //设备ID和pwd
#define HAL_DEVICE_SYS_ARGS_ADDR (HAL_ARGS_FLASH_ADDR + KB(2))
#define HAL_DEVICE_TEST_ADDR     (HAL_ARGS_FLASH_ADDR + KB(4))
#define HAL_DEVICE_RESERVE_ADDR  (HAL_ARGS_FLASH_ADDR + KB(14)) //reserve

#define HAL_ARGS_INTERVAL_ADDR         (HAL_DEVICE_SYS_ARGS_ADDR + 0)//0x0803e800
#define HAL_ARGS_PM25_SENSITIVITY_ADDR (HAL_DEVICE_SYS_ARGS_ADDR + DW(1))
#define HAL_ARGS_PM25_OFFSET_ADDR      (HAL_DEVICE_SYS_ARGS_ADDR + DW(2))
#define HAL_ARGS_PM10_SENSITIVITY_ADDR (HAL_DEVICE_SYS_ARGS_ADDR + DW(3))
#define HAL_ARGS_PM10_OFFSET_ADDR      (HAL_DEVICE_SYS_ARGS_ADDR + DW(4))
#define HAL_ARGS_SLEEP_MODE_ADDR	   (HAL_DEVICE_SYS_ARGS_ADDR + DW(64))


#if defined(HAL_BOARD_NAME_BASIC)

#define HAL_ARGS_WT_TP_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(1))
#define HAL_ARGS_WT_TP_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(2))
#define HAL_ARGS_WT_EC_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(3))
#define HAL_ARGS_WT_EC_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(4))
#define HAL_ARGS_WT_PH_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(5))
#define HAL_ARGS_WT_PH_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(6))
#define HAL_ARGS_WT_O2_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(7))
#define HAL_ARGS_WT_O2_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(8))
#define HAL_ARGS_WT_TB_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(9))
#define HAL_ARGS_WT_TB_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(10))
#define HAL_ARGS_WT_COD_SENSITIVITY_ADDR  (HAL_ARGS_PM10_OFFSET_ADDR + DW(11))
#define HAL_ARGS_WT_COD_OFFSET_ADDR       (HAL_ARGS_PM10_OFFSET_ADDR + DW(12))
#define HAL_ARGS_WT_DEP_SENSITIVITY_ADDR  (HAL_ARGS_PM10_OFFSET_ADDR + DW(13))
#define HAL_ARGS_WT_DEP_OFFSET_ADDR       (HAL_ARGS_PM10_OFFSET_ADDR + DW(14))
#define HAL_ARGS_WT_FLOW_SENSITIVITY_ADDR (HAL_ARGS_PM10_OFFSET_ADDR + DW(15))
#define HAL_ARGS_WT_FLOW_OFFSET_ADDR      (HAL_ARGS_PM10_OFFSET_ADDR + DW(16))
#define HAL_ARGS_WT_NH4_SENSITIVITY_ADDR  (HAL_ARGS_PM10_OFFSET_ADDR + DW(17))
#define HAL_ARGS_WT_NH4_OFFSET_ADDR       (HAL_ARGS_PM10_OFFSET_ADDR + DW(18))
#define HAL_ARGS_WT_SS_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(19))
#define HAL_ARGS_WT_SS_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(20))
#define HAL_ARGS_WT_CHRM_SENSITIVITY_ADDR   (HAL_ARGS_PM10_OFFSET_ADDR + DW(21))
#define HAL_ARGS_WT_CHRM_OFFSET_ADDR        (HAL_ARGS_PM10_OFFSET_ADDR + DW(22))
#define HAL_ARGS_WT_BOD_SENSITIVITY_ADDR  (HAL_ARGS_PM10_OFFSET_ADDR + DW(23))
#define HAL_ARGS_WT_BOD_OFFSET_ADDR       (HAL_ARGS_PM10_OFFSET_ADDR + DW(24))

#endif

#if defined(HAL_BOARD_NAME_LITE)

#define HAL_ARGS_VOC_SENSITIVITY_ADDR (HAL_ARGS_PM10_OFFSET_ADDR + DW(1))
#define HAL_ARGS_VOC_OFFSET_ADDR      (HAL_ARGS_VOC_SENSITIVITY_ADDR + DW(1))

#define HAL_ARGS_CO_SENSITIVITY_ADDR  (HAL_ARGS_VOC_OFFSET_ADDR + DW(1))
#define HAL_ARGS_CO_OFFSET_ADDR       (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(1))
#define HAL_ARGS_CO_N1_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(2))
#define HAL_ARGS_CO_N2_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(3))
#define HAL_ARGS_CO_N3_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(4))
#define HAL_ARGS_CO_N4_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(5))
#define HAL_ARGS_CO_N5_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(6))
#define HAL_ARGS_CO_N6_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(7))
#define HAL_ARGS_CO_N7_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(8))
#define HAL_ARGS_CO_N8_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(9))
#define HAL_ARGS_CO_N9_ADDR           (HAL_ARGS_CO_SENSITIVITY_ADDR + DW(10))

#define HAL_ARGS_NO2_SENSITIVITY_ADDR (HAL_ARGS_CO_N9_ADDR + DW(1))
#define HAL_ARGS_NO2_OFFSET_ADDR      (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(1))
#define HAL_ARGS_NO2_N1_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(2))
#define HAL_ARGS_NO2_N2_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(3))
#define HAL_ARGS_NO2_N3_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(4))
#define HAL_ARGS_NO2_N4_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(5))
#define HAL_ARGS_NO2_N5_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(6))
#define HAL_ARGS_NO2_N6_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(7))
#define HAL_ARGS_NO2_N7_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(8))
#define HAL_ARGS_NO2_N8_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(9))
#define HAL_ARGS_NO2_N9_ADDR          (HAL_ARGS_NO2_SENSITIVITY_ADDR + DW(10))

#define HAL_ARGS_SO2_SENSITIVITY_ADDR (HAL_ARGS_NO2_N9_ADDR + DW(1))
#define HAL_ARGS_SO2_OFFSET_ADDR      (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(1))
#define HAL_ARGS_SO2_N1_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(2))
#define HAL_ARGS_SO2_N2_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(3))
#define HAL_ARGS_SO2_N3_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(4))
#define HAL_ARGS_SO2_N4_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(5))
#define HAL_ARGS_SO2_N5_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(6))
#define HAL_ARGS_SO2_N6_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(7))
#define HAL_ARGS_SO2_N7_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(8))
#define HAL_ARGS_SO2_N8_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(9))
#define HAL_ARGS_SO2_N9_ADDR          (HAL_ARGS_SO2_SENSITIVITY_ADDR + DW(10))

#define HAL_ARGS_O3_SENSITIVITY_ADDR  (HAL_ARGS_SO2_N9_ADDR + DW(1))
#define HAL_ARGS_O3_OFFSET_ADDR       (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(1))
#define HAL_ARGS_O3_N1_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(2))
#define HAL_ARGS_O3_N2_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(3))
#define HAL_ARGS_O3_N3_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(4))
#define HAL_ARGS_O3_N4_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(5))
#define HAL_ARGS_O3_N5_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(6))
#define HAL_ARGS_O3_N6_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(7))
#define HAL_ARGS_O3_N7_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(8))
#define HAL_ARGS_O3_N8_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(9))
#define HAL_ARGS_O3_N9_ADDR           (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(10))
#define HAL_ARGS_O3CAL_R              (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(11))
#define HAL_ARGS_O3CAL_A              (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(12))
#define HAL_ARGS_O3CAL_B              (HAL_ARGS_O3_SENSITIVITY_ADDR + DW(13))

#define HAL_ARGS_CO_WEOFFSET_ADDR     (HAL_ARGS_O3CAL_B + DW(1))
#define HAL_ARGS_NO2_WEOFFSET_ADDR    (HAL_ARGS_CO_WEOFFSET_ADDR + DW(1))
#define HAL_ARGS_SO2_WEOFFSET_ADDR    (HAL_ARGS_CO_WEOFFSET_ADDR + DW(2))
#define HAL_ARGS_O3_WEOFFSET_ADDR     (HAL_ARGS_CO_WEOFFSET_ADDR + DW(3))

#define HAL_ARGS_CO_AEOFFSET_ADDR     (HAL_ARGS_O3_WEOFFSET_ADDR + DW(1))
#define HAL_ARGS_NO2_AEOFFSET_ADDR    (HAL_ARGS_CO_AEOFFSET_ADDR + DW(1))
#define HAL_ARGS_SO2_AEOFFSET_ADDR    (HAL_ARGS_CO_AEOFFSET_ADDR + DW(2))
#define HAL_ARGS_O3_AEOFFSET_ADDR     (HAL_ARGS_CO_AEOFFSET_ADDR + DW(3))

#define HAL_ARGS_NOISE_SENSITIVITY_ADDR (HAL_ARGS_O3_AEOFFSET_ADDR + DW(1))
#define HAL_ARGS_NOISE_OFFSET_ADDR      (HAL_ARGS_NOISE_SENSITIVITY_ADDR + DW(1))

#endif

#endif

