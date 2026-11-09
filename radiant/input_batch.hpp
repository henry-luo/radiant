#pragma once

#include "event.hpp"
#include <string.h>

typedef void (*RdtInputDispatch)(void* data, RdtEvent* event);

struct RdtInputBatch {
    RdtEvent motion;
    bool pending_motion;
    // PE6: every move folded into `motion`, oldest first, so a pen's high-rate samples survive
    // the once-per-turn dispatch.
    RdtPointerSamples samples;
};

// PE6: append one sample; a full buffer drops its oldest sample and says so.
inline void rdt_pointer_samples_push(RdtPointerSamples* samples, const RdtPointerSample& sample) {
    if (samples->count == RDT_POINTER_SAMPLE_CAPACITY) {
        memmove(samples->items, samples->items + 1,
                (RDT_POINTER_SAMPLE_CAPACITY - 1) * sizeof(RdtPointerSample));
        samples->count--;
        samples->dropped = true;
    }
    samples->items[samples->count++] = sample;
}

inline bool rdt_input_batch_flush(RdtInputBatch* batch, RdtInputDispatch dispatch, void* data) {
    if (!batch->pending_motion) return false;
    RdtEvent motion = batch->motion;
    // dispatch may reenter the host and submit more motion, so it reads its own copy of the
    // samples (PE6) while the batch starts the next run.
    RdtPointerSamples samples;
    samples.count = batch->samples.count;
    samples.dropped = batch->samples.dropped;
    memcpy(samples.items, batch->samples.items, samples.count * sizeof(RdtPointerSample));
    motion.mouse_position.samples = &samples;
    batch->samples.count = 0;
    batch->samples.dropped = false;
    // consume this sample before calling user code.
    batch->pending_motion = false;
    dispatch(data, &motion);
    return true;
}

inline void rdt_input_batch_submit(RdtInputBatch* batch, RdtEvent* event,
                                   RdtInputDispatch dispatch, void* data) {
    if (event->type == RDT_EVENT_MOUSE_MOVE) {
        // samples of one pointer only: a different device first dispatches what it moved.
        const MousePositionEvent* pending = &batch->motion.mouse_position;
        if (batch->pending_motion && (pending->pointer_type != event->mouse_position.pointer_type ||
                                      pending->pointer_id != event->mouse_position.pointer_id)) {
            rdt_input_batch_flush(batch, dispatch, data);
        }
        rdt_pointer_samples_push(&batch->samples, rdt_pointer_sample_of(&event->mouse_position));
        // platform polling must stay cheap enough to reach paint under continuous input.
        batch->motion = *event;
        batch->pending_motion = true;
    } else {
        rdt_input_batch_flush(batch, dispatch, data);
        dispatch(data, event);
    }
}
