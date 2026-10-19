/*
 * Minimal Schwung plugin API v2 declaration used by s3g Thru.
 *
 * This is the stable ABI prefix required by the module, derived from
 * Schwung's MIT-licensed src/host/plugin_api_v1.h. Keep field order intact.
 */
#ifndef S3G_THRU_SCHWUNG_ABI_H
#define S3G_THRU_SCHWUNG_ABI_H

#include <stdint.h>

#define MOVE_PLUGIN_API_VERSION_2 2
#define MOVE_SAMPLE_RATE 44100
#define MOVE_FRAMES_PER_BLOCK 128

#define MOVE_MIDI_SOURCE_INTERNAL 0
#define MOVE_MIDI_SOURCE_EXTERNAL 2
#define MOVE_MIDI_SOURCE_HOST 3

typedef struct host_api_v1 {
    uint32_t api_version;
    int sample_rate;
    int frames_per_block;
    uint8_t *mapped_memory;
    int audio_out_offset;
    int audio_in_offset;
    void (*log)(const char *message);
} host_api_v1_t;

typedef struct plugin_api_v2 {
    uint32_t api_version;
    void *(*create_instance)(const char *module_dir, const char *json_defaults);
    void (*destroy_instance)(void *instance);
    void (*on_midi)(void *instance, const uint8_t *msg, int len, int source);
    void (*set_param)(void *instance, const char *key, const char *value);
    int (*get_param)(void *instance, const char *key, char *buffer, int buffer_len);
    int (*get_error)(void *instance, char *buffer, int buffer_len);
    void (*render_block)(void *instance, int16_t *out_interleaved_lr, int frames);
} plugin_api_v2_t;

#if defined(__GNUC__)
#define S3G_THRU_EXPORT __attribute__((visibility("default")))
#else
#define S3G_THRU_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
S3G_THRU_EXPORT plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
#ifdef __cplusplus
}
#endif

#endif

