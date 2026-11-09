#pragma once
// A pl_mpeg buffer reading a caller-owned stream in memory. pl_mpeg's own memory buffer (plm_buffer_create_with_memory)
// does not work with the bare video decoder: plm_video_decode() discards read bytes in place, which moves the
// caller's data and shrinks the length, so the buffer never reports its end (the last pictures are lost) and
// plm_video_rewind() reads moved data (found in M8 V1). This source copies the stream into pl_mpeg's own buffer
// through its load/seek/tell callbacks, like its file source, and marks the end the same way.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plm_buffer_t plm_buffer_t;

typedef struct sif_plm_memory {
    const uint8_t* data;
    size_t size;
    size_t pos;  // next byte to hand to pl_mpeg
} sif_plm_memory;

// `source` (and its data) must outlive the buffer.
plm_buffer_t* sif_plm_buffer_create(sif_plm_memory* source);

#ifdef __cplusplus
}
#endif
