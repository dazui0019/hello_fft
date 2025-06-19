#include "main.h"   // IWYU pragma: keep
#include "adc.h"
#include "tim.h"
#include "arm_math.h"
#include "printf.h" // IWYU pragma: keep
#include "ws28xx.h"

#define FFT_SIZE 1024

uint16_t ADC_Value[FFT_SIZE];
__IO uint8_t FFT_Flag = 0;

float32_t fft_input_buffer[FFT_SIZE * 2];  // 实部和虚部交错存储
float32_t fft_output_buffer[FFT_SIZE/2];   // FFT幅度输出(只需要前一半)
arm_cfft_radix4_instance_f32 fft_instance; // FFT实例
float32_t sampling_frequency = 10000.0f;   // 采样频率，需要根据您的定时器配置调整

WS28XX_HandleTypeDef hLed;

uint32_t DMA_Buffer[8] = {50, 50, 50, 50, 50, 50, 50, 50};

void setup(void)
{
    // 初始化FFT实例
    arm_cfft_radix4_init_f32(&fft_instance, FFT_SIZE, 0, 1);

    __HAL_DMA_CLEAR_FLAG(hadc1.DMA_Handle, DMA_FLAG_TCIF0_4|DMA_FLAG_HTIF0_4);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Value, FFT_SIZE);
    HAL_TIM_Base_Start(&htim2);
    printf("Hello, FFT!\r\n");
    WS28XX_Init(&hLed, &htim3, 90, TIM_CHANNEL_2, 64);
    WS28XX_SetPixel_RGB_888(&hLed, 0, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 8, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 16, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 24, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 32, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 40, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 48, COLOR_RGB888_PURPLE);
    WS28XX_SetPixel_RGB_888(&hLed, 56, COLOR_RGB888_PURPLE);
  
    WS28XX_Update(&hLed);
}

void loop(void)
{
    WS28XX_Update(&hLed);
    if(FFT_Flag == 0){
        return;
    }

    FFT_Flag = 1;

    // 将ADC数据转换为复数格式（实部为ADC值，虚部为0）
    for(int i = 0; i < FFT_SIZE; i++){
        fft_input_buffer[2*i] = (float32_t)ADC_Value[i];     // 实部
        fft_input_buffer[2*i + 1] = 0.0f;                    // 虚部
    }

    // 执行FFT变换
    arm_cfft_radix4_f32(&fft_instance, fft_input_buffer);

    // 只计算小于奈奎斯特频率的幅度谱
    arm_cmplx_mag_f32(fft_input_buffer, fft_output_buffer, FFT_SIZE/2);

    // 重新启动ADC采集
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Value, FFT_SIZE);
    HAL_TIM_Base_Start(&htim2);
}

// dma 中断回调函数
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop_DMA(hadc);
    FFT_Flag = 1;
}
