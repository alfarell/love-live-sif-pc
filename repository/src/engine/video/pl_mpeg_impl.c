// The pl_mpeg implementation (third_party/pl_mpeg, MIT), compiled once without project warnings, and the port's
// memory source for it (pl_mpeg_source.h), which needs the buffer struct only this file can see.
#include <stdio.h>  // the header names FILE before it includes stdio.h itself
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "engine/video/pl_mpeg_source.h"

static void sif_plm_load(plm_buffer_t* self, void* user) {
    sif_plm_memory* m = (sif_plm_memory*)user;
    if (self->discard_read_bytes) plm_buffer_discard_read_bytes(self);
    const size_t left = m->size - m->pos;
    if (left == 0) {
        self->has_ended = TRUE;  // as plm_buffer_load_file_callback does at the end of the file
        return;
    }
    // plm_buffer_has_start_code() scans ahead with discarding off: a full buffer must grow, not read as the end
    // (pl_mpeg's file callback reads 0 bytes then and marks the end early)
    if (self->capacity == self->length) {
        self->capacity *= 2;
        self->bytes = (uint8_t*)PLM_REALLOC(self->bytes, self->capacity);
    }
    size_t n = self->capacity - self->length;
    if (n > left) n = left;
    memcpy(self->bytes + self->length, m->data + m->pos, n);
    self->length += n;
    m->pos += n;
}

static void sif_plm_seek(plm_buffer_t* self, size_t offset, void* user) {
    PLM_UNUSED(self);
    sif_plm_memory* m = (sif_plm_memory*)user;
    m->pos = offset < m->size ? offset : m->size;
}

static size_t sif_plm_tell(plm_buffer_t* self, void* user) {
    PLM_UNUSED(self);
    return ((sif_plm_memory*)user)->pos;
}

plm_buffer_t* sif_plm_buffer_create(sif_plm_memory* source) {
    source->pos = 0;
    return plm_buffer_create_with_callbacks(sif_plm_load, sif_plm_seek, sif_plm_tell, source->size, source);
}

