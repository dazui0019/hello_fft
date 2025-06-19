#include "main.h"   // IWYU pragma: keep
#include "adc.h"
#include "tim.h"
#include "arm_math.h"
#include "printf.h" // IWYU pragma: keep
#include "ws28xx.h"

#define FFT_SIZE 1024

void vol2led(float32_t vol);

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

    // 计算1K-1.5KHz的FFT幅度
    float32_t freq_resolution = sampling_frequency / FFT_SIZE;  // 频率分辨率
    int start_bin = (int)(1000.0f / freq_resolution);          // 1KHz对应的频率bin
    int end_bin = (int)(1500.0f / freq_resolution);            // 1.5KHz对应的频率bin
    
    // 计算指定频率范围内的平均幅度
    float32_t total_magnitude = 0.0f;
    for(int i = start_bin; i <= end_bin && i < FFT_SIZE/2; i++){
        total_magnitude += fft_output_buffer[i];
    }
    float32_t avg_magnitude = total_magnitude / (end_bin - start_bin + 1);
    
    // 根据音频幅值控制LED
    vol2led(avg_magnitude);

    // 重新启动ADC采集
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Value, FFT_SIZE);
    HAL_TIM_Base_Start(&htim2);
}

void vol2led(float32_t vol)
{
    // 给vol添加一阶滞后滤波
    static float32_t vol_last = 0.0f;
    vol = 0.4f * vol + 0.6f * vol_last;
    vol_last = vol;
    
    // 先清除所有LED
    for(int i = 0; i < 8; i++){
        WS28XX_SetPixel_RGB_888(&hLed, (i*8), COLOR_RGB888_BLACK);
        WS28XX_SetPixel_RGB_888(&hLed, (i*8)+7, COLOR_RGB888_BLACK);
    }
    
    // 将平滑后的音频幅值映射到0-8范围（LED数量）
    float32_t scale_factor = 0.001f;  // 根据实际情况调整
    float32_t led_count_float = vol_last * scale_factor;
    
    // 在转换成int之前做滞后处理
    static float32_t prev_led_count_float = 0.0f;
    float32_t hysteresis_threshold = 0.8f;  // 滞后阈值
    
    if(led_count_float > prev_led_count_float + hysteresis_threshold) {
        prev_led_count_float = led_count_float;
    } else if(led_count_float < prev_led_count_float - hysteresis_threshold) {
        prev_led_count_float = led_count_float;
    } else {
        led_count_float = prev_led_count_float;  // 保持上一次的值
    }
    
    int led_count = (int)led_count_float;
    
    // 限制LED数量在合理范围内
    if(led_count > 8) led_count = 8;
    if(led_count < 0) led_count = 0;
    
    // 点亮对应数量的LED
    for(int i = 0; i < led_count; i++){
        WS28XX_SetPixel_RGBW_888(&hLed, (i*8), COLOR_RGB888_PURPLE, 150);
        WS28XX_SetPixel_RGBW_888(&hLed, (i*8)+7, COLOR_RGB888_PURPLE, 150);
    }
}

// dma 中断回调函数
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop_DMA(hadc);
    FFT_Flag = 1;
}
