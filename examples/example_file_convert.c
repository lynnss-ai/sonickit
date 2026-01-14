/**
 * @file example_file_convert.c
 * @brief Audio file format conversion example
 * 
 * 演示音频文件格式转换和处理:
 * - 格式转换 (WAV, MP3, FLAC)
 * - 重采样
 * - 降噪处理
 */

#include "voice/voice.h"
#include "audio/file_io.h"
#include "dsp/resampler.h"
#include "dsp/denoiser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * 主程序
 * ============================================ */

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Audio File Converter\n");
        printf("====================\n");
        printf("Usage: %s <input> <output> [options]\n", argv[0]);
        printf("\nOptions:\n");
        printf("  -r <rate>      Output sample rate (e.g., 48000, 44100, 16000)\n");
        printf("  -n             Apply noise reduction\n");
        printf("  -q <0-10>      Resampler quality (default: 5)\n");
        printf("\nSupported formats: WAV, MP3, FLAC\n");
        printf("\nExamples:\n");
        printf("  %s input.mp3 output.wav\n", argv[0]);
        printf("  %s input.wav output.wav -r 16000 -n\n", argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    const char *output_file = argv[2];
    uint32_t output_rate = 0;
    bool apply_denoise = false;
    int resample_quality = 5;
    
    /* 解析选项 */
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            output_rate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0) {
            apply_denoise = true;
        } else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc) {
            resample_quality = atoi(argv[++i]);
            if (resample_quality < 0) resample_quality = 0;
            if (resample_quality > 10) resample_quality = 10;
        }
    }
    
    printf("Voice Library - File Converter\n");
    printf("==============================\n");
    printf("Input: %s\n", input_file);
    printf("Output: %s\n", output_file);
    
    voice_error_t err;
    audio_reader_t *reader = NULL;
    audio_writer_t *writer = NULL;
    voice_resampler_t *resampler = NULL;
    voice_denoiser_t *denoiser = NULL;
    int16_t *input_buffer = NULL;
    int16_t *output_buffer = NULL;
    int16_t *resample_buffer = NULL;
    
    /* 打开输入文件 */
    reader = audio_reader_open(input_file);
    if (!reader) {
        fprintf(stderr, "Failed to open input file: %s\n", input_file);
        return 1;
    }
    
    /* 获取输入信息 */
    uint32_t input_rate;
    uint8_t input_channels;
    uint64_t total_samples;
    audio_reader_get_info_ex(reader, &input_rate, &input_channels, &total_samples);
    
    printf("Input format: %u Hz, %u channels, %llu samples\n",
        input_rate, (unsigned)input_channels, (unsigned long long)total_samples);
    
    /* 确定输出采样率 */
    if (output_rate == 0) {
        output_rate = input_rate;
    }
    
    bool need_resample = (output_rate != input_rate);
    
    printf("Output format: %u Hz\n", output_rate);
    printf("Denoise: %s\n", apply_denoise ? "yes" : "no");
    if (need_resample) {
        printf("Resample quality: %d\n", resample_quality);
    }
    printf("\n");
    
    /* 创建输出文件 */
    writer = audio_writer_create_simple(output_file, output_rate, input_channels);
    if (!writer) {
        fprintf(stderr, "Failed to create output file: %s\n", output_file);
        goto cleanup;
    }
    
    /* 帧大小 (20ms) */
    const size_t input_frame_size = input_rate / 50;
    const size_t output_frame_size = output_rate / 50;
    
    /* 分配缓冲区 */
    input_buffer = (int16_t *)malloc(input_frame_size * sizeof(int16_t));
    output_buffer = (int16_t *)malloc(output_frame_size * sizeof(int16_t));
    
    if (!input_buffer || !output_buffer) {
        fprintf(stderr, "Failed to allocate buffers\n");
        goto cleanup;
    }
    
    /* 创建重采样器 */
    if (need_resample) {
        voice_resampler_config_t rs_config;
        voice_resampler_config_init(&rs_config);
        rs_config.input_rate = input_rate;
        rs_config.output_rate = output_rate;
        rs_config.channels = input_channels;
        rs_config.quality = resample_quality;
        
        resampler = voice_resampler_create(&rs_config);
        if (!resampler) {
            fprintf(stderr, "Failed to create resampler\n");
            goto cleanup;
        }
        
        resample_buffer = (int16_t *)malloc(output_frame_size * 2 * sizeof(int16_t));
    }
    
    /* 创建降噪器 (在输出采样率上操作) */
    if (apply_denoise) {
        voice_denoiser_config_t dn_config;
        voice_denoiser_config_init(&dn_config);
        dn_config.sample_rate = output_rate;
        dn_config.frame_size = output_frame_size;
        dn_config.engine = VOICE_DENOISE_SPEEX;
        dn_config.enable_vad = true;
        dn_config.enable_agc = false;
        
        denoiser = voice_denoiser_create(&dn_config);
        if (!denoiser) {
            fprintf(stderr, "Warning: Failed to create denoiser\n");
            apply_denoise = false;
        }
    }
    
    /* 处理循环 */
    uint64_t samples_processed = 0;
    uint64_t samples_written = 0;
    int last_progress = -1;
    
    printf("Processing...\n");
    
    while (1) {
        size_t samples_read = 0;
        err = audio_reader_read_s16(reader, input_buffer, input_frame_size, &samples_read);
        
        if (err != VOICE_OK || samples_read == 0) {
            break;
        }
        
        samples_processed += samples_read;
        
        int16_t *process_buffer = input_buffer;
        size_t process_samples = samples_read;
        
        /* 重采样 */
        if (need_resample && resampler) {
            size_t out_samples = output_frame_size * 2;
            err = voice_resampler_process(
                resampler,
                input_buffer, samples_read,
                resample_buffer, &out_samples
            );
            
            if (err == VOICE_OK) {
                process_buffer = resample_buffer;
                process_samples = out_samples;
            }
        }
        
        /* 降噪 */
        if (apply_denoise && denoiser) {
            /* 需要匹配帧大小进行降噪 */
            if (process_samples == output_frame_size) {
                memcpy(output_buffer, process_buffer, process_samples * sizeof(int16_t));
                voice_denoiser_process(denoiser, output_buffer, process_samples);
                process_buffer = output_buffer;
            }
        }
        
        /* 写入 */
        audio_writer_write_s16(writer, process_buffer, process_samples);
        samples_written += process_samples;
        
        /* 显示进度 */
        int progress = (int)((samples_processed * 100) / total_samples);
        if (progress != last_progress && progress % 10 == 0) {
            printf("\rProgress: %d%%", progress);
            fflush(stdout);
            last_progress = progress;
        }
    }
    
    printf("\rProgress: 100%%\n\n");
    
    /* 统计 */
    printf("Conversion complete!\n");
    printf("Samples processed: %llu\n", (unsigned long long)samples_processed);
    printf("Samples written: %llu\n", (unsigned long long)samples_written);
    printf("Duration: %.2f seconds\n", (double)samples_written / output_rate);
    
cleanup:
    if (reader) audio_reader_close(reader);
    if (writer) audio_writer_close(writer);
    if (resampler) voice_resampler_destroy(resampler);
    if (denoiser) voice_denoiser_destroy(denoiser);
    if (input_buffer) free(input_buffer);
    if (output_buffer) free(output_buffer);
    if (resample_buffer) free(resample_buffer);
    
    return 0;
}
