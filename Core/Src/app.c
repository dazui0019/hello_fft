#include "main.h"   // IWYU pragma: keep
#include "stm32f4xx_hal_i2s.h"
#include "tim.h"
#include "arm_math.h"
#include "printf.h" // IWYU pragma: keep
#include "ws28xx.h"
#include "i2s.h" // IWYU pragma: keep
#include <stdint.h>

#define FFT_SIZE 1024

void vol2led(float32_t vol);

// i2s传输完成标志
__IO uint8_t i2s_cplt_flag = 0;
__IO uint8_t i2s_half_flag = 0;

float32_t fft_input_buffer[FFT_SIZE * 2];  // 实部和虚部交错存储
float32_t fft_output_buffer[FFT_SIZE/2];   // FFT幅度输出(只需要前一半)
arm_cfft_radix4_instance_f32 fft_instance; // FFT实例
float32_t sampling_frequency = 48000.0f;   // 采样频率，需要根据您的定时器配置调整

WS28XX_HandleTypeDef hLed;

uint16_t I2S_Buffer[2*4*FFT_SIZE];    // 一个周期的数据需要4个元素来存储, 包含左右声道. 然后使用了一个乒乓缓冲区

int32_t val_24bit = 0;

void setup(void)
{
    // 初始化FFT实例
    arm_cfft_radix4_init_f32(&fft_instance, FFT_SIZE, 0, 1);

    // __HAL_DMA_CLEAR_FLAG(hadc1.DMA_Handle, DMA_FLAG_TCIF0_4|DMA_FLAG_HTIF0_4);
    // HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Value, FFT_SIZE);
    // HAL_TIM_Base_Start(&htim2);
    // printf("Hello, FFT!\r\n");
    WS28XX_Init(&hLed, &htim3, 90, TIM_CHANNEL_2, 64);
    // HAL_I2S_Receive(&hi2s1, I2S_Buffer, FFT_SIZE, 100);
    // HAL_I2S_Receive_DMA(&hi2s1, I2S_Buffer, 2*2*FFT_SIZE);
    HAL_I2S_Receive_DMA(&hi2s1, I2S_Buffer, 4);
}

void loop(void)
{
    // if(FFT_Flag == 1){
    //     printf("%d\n", I2S_Buffer[2]);
    //     FFT_Flag = 0;
    // }

    HAL_Delay(100);
    return;

    if((i2s_half_flag|i2s_cplt_flag) == 0){
        return;
    }

    if(i2s_half_flag == 1){
        uint16_t* raw_buff = I2S_Buffer;
        i2s_half_flag = 0;
        for(int i = 0; i < FFT_SIZE; i++){
            val_24bit = ((raw_buff[(4*i)]<<8)|(raw_buff[(4*i)+1]>>8));
            //将24位有符号整型扩展到32位
            if(val_24bit & 0x800000){//negative
                val_24bit |= 0xff000000;
            }
            fft_input_buffer[2*i] = (float32_t)val_24bit;   // 实部
            fft_input_buffer[2*i + 1] = 0.0f;                   // 虚部
            // printf("[%d]:%d ", i, val_24bit);
        }
    }else if(i2s_cplt_flag == 1){
        uint16_t* raw_buff = &I2S_Buffer[4*FFT_SIZE];
        i2s_cplt_flag = 0;
        for(int i = 0; i < FFT_SIZE; i++){
            val_24bit = ((raw_buff[(4*i)]<<8)|(raw_buff[(4*i)+1]>>8));
            //将24位有符号整型扩展到32位
            if(val_24bit & 0x800000){//negative
                val_24bit |= 0xff000000;
            }
            fft_input_buffer[2*i] = (float32_t)val_24bit;   // 实部
            fft_input_buffer[2*i + 1] = 0.0f;                   // 虚部
            printf("[%d]:%d ", i, val_24bit);
        }
    }

    printf("\r\n\r\n");

    // arm_cfft_radix4_f32(&fft_instance, fft_input_buffer);

    // // 只计算小于奈奎斯特频率的幅度谱
    // arm_cmplx_mag_f32(fft_input_buffer, fft_output_buffer, FFT_SIZE/2);

    // // 计算主要频率
    // int max_bin = 0;
    // for(int i = 1; i < FFT_SIZE/2; i++){
    //     if(fft_output_buffer[i] > fft_output_buffer[max_bin]){
    //         max_bin = i;
    //     }
    // }
    // float32_t freq = max_bin * sampling_frequency / FFT_SIZE;
    // printf("Freq: %f Hz\n", freq);
}

// void loop(void)
// {
//     WS28XX_Update(&hLed);
//     if(FFT_Flag == 0){
//         return;
//     }

//     FFT_Flag = 1;

//     // 将ADC数据转换为复数格式（实部为ADC值，虚部为0）
//     for(int i = 0; i < FFT_SIZE; i++){
//         fft_input_buffer[2*i] = (float32_t)ADC_Value[i];     // 实部
//         fft_input_buffer[2*i + 1] = 0.0f;                    // 虚部
//     }

//     // 执行FFT变换
//     arm_cfft_radix4_f32(&fft_instance, fft_input_buffer);

//     // 只计算小于奈奎斯特频率的幅度谱
//     arm_cmplx_mag_f32(fft_input_buffer, fft_output_buffer, FFT_SIZE/2);

//     // 计算1K-1.5KHz的FFT幅度
//     float32_t freq_resolution = sampling_frequency / FFT_SIZE;  // 频率分辨率
//     int start_bin = (int)(1000.0f / freq_resolution);          // 1KHz对应的频率bin
//     int end_bin = (int)(1500.0f / freq_resolution);            // 1.5KHz对应的频率bin
    
//     // 计算指定频率范围内的平均幅度
//     float32_t total_magnitude = 0.0f;
//     for(int i = start_bin; i <= end_bin && i < FFT_SIZE/2; i++){
//         total_magnitude += fft_output_buffer[i];
//     }
//     float32_t avg_magnitude = total_magnitude / (end_bin - start_bin + 1);
    
//     // 根据音频幅值控制LED
//     vol2led(avg_magnitude);

//     // 重新启动ADC采集
//     HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Value, FFT_SIZE);
//     HAL_TIM_Base_Start(&htim2);
// }

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

    // 自适应对数映射
    static float32_t max_vol = 15000.0f;
    static float32_t min_vol = 500.0f;

    // 更新动态范围
    if(vol_last > max_vol) {
        max_vol = vol_last * 0.8f + max_vol * 0.2f;
    } else {
        // max_vol的最小值为15000.0f
        if(max_vol > 15000.0f){
            // 当音量低于最大值时，让最大值缓慢衰减
            max_vol = max_vol * 0.999f; // 每次衰减0.1%
        }
    }

    printf("vol: %f, max_vol: %f, min_vol: %f\n", vol_last, max_vol, min_vol);

    // 对数映射
    float32_t log_vol = log10f((vol_last - min_vol) / (max_vol - min_vol) + 1.0f);
    float32_t led_count_float = log_vol * 8.0f / log10f(2.0f);  // 映射到0-8

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

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    // 半满
    i2s_half_flag = 1;
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    // 全满
    i2s_cplt_flag = 1;
}
