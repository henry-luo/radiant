// Coalesced pointer samples in the window input batch (Radiant_Design_Paint_Engine PE6).
#include <gtest/gtest.h>
#include <string.h>
#include "radiant/input_batch.hpp"

namespace {

// one dispatched event, with a copy of the samples it carried during dispatch.
struct Dispatched {
    RdtEvent event;
    RdtPointerSamples samples;
    bool has_samples;
};

struct Recorder {
    Dispatched seen[8];
    int count = 0;
};

void record(void* data, RdtEvent* event) {
    Recorder* recorder = (Recorder*)data;
    ASSERT_LT(recorder->count, 8);
    Dispatched& d = recorder->seen[recorder->count++];
    d.event = *event;
    d.has_samples = event->type == RDT_EVENT_MOUSE_MOVE && event->mouse_position.samples;
    if (d.has_samples) d.samples = *event->mouse_position.samples;
    d.event.mouse_position.samples = nullptr;   // valid only during dispatch
}

RdtEvent pen_move(float x, float y, float pressure, double time) {
    RdtEvent event;
    rdt_event_set_mouse_position(&event, RDT_EVENT_MOUSE_MOVE, x, y, time);
    event.mouse_position.pointer_type = RDT_POINTER_PEN;
    event.mouse_position.pointer_id = 1;
    event.mouse_position.pressure = pressure;
    event.mouse_position.tilt_x = x / 10.0f;
    return event;
}

TEST(PointerSamples, MovesBetweenDispatchesKeepEverySample) {
    static RdtInputBatch batch = {};
    Recorder recorder;
    for (int i = 0; i < 5; i++) {
        RdtEvent move = pen_move(10.0f * i, 2.0f * i, 0.1f * (i + 1), 1.0 + 0.004 * i);
        rdt_input_batch_submit(&batch, &move, record, &recorder);
    }
    EXPECT_EQ(recorder.count, 0);   // moves wait for the turn's flush
    EXPECT_TRUE(rdt_input_batch_flush(&batch, record, &recorder));
    ASSERT_EQ(recorder.count, 1);
    const Dispatched& d = recorder.seen[0];
    ASSERT_TRUE(d.has_samples);
    ASSERT_EQ(d.samples.count, 5);
    EXPECT_FALSE(d.samples.dropped);
    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(d.samples.items[i].x, 10.0f * i);
        EXPECT_FLOAT_EQ(d.samples.items[i].pressure, 0.1f * (i + 1));
        EXPECT_FLOAT_EQ(d.samples.items[i].tilt_x, (float)i);
        EXPECT_DOUBLE_EQ(d.samples.items[i].timestamp, 1.0 + 0.004 * i);
    }
    // the dispatched move is the last sample
    EXPECT_FLOAT_EQ(d.event.mouse_position.x, 40.0f);
    EXPECT_FLOAT_EQ(d.event.mouse_position.pressure, 0.5f);
    // the next run starts empty
    EXPECT_FALSE(rdt_input_batch_flush(&batch, record, &recorder));
    RdtEvent move = pen_move(1.0f, 1.0f, 0.3f, 2.0);
    rdt_input_batch_submit(&batch, &move, record, &recorder);
    rdt_input_batch_flush(&batch, record, &recorder);
    ASSERT_EQ(recorder.count, 2);
    EXPECT_EQ(recorder.seen[1].samples.count, 1);
}

TEST(PointerSamples, OtherEventsAndDevicesDispatchPendingMotionFirst) {
    static RdtInputBatch batch = {};
    Recorder recorder;
    RdtEvent a = pen_move(1.0f, 1.0f, 0.2f, 1.0), b = pen_move(2.0f, 1.0f, 0.4f, 1.01);
    rdt_input_batch_submit(&batch, &a, record, &recorder);
    rdt_input_batch_submit(&batch, &b, record, &recorder);
    // a mouse move after the pen: the pen's run dispatches alone
    RdtEvent mouse;
    rdt_event_set_mouse_position(&mouse, RDT_EVENT_MOUSE_MOVE, 5.0f, 5.0f, 1.02);
    rdt_input_batch_submit(&batch, &mouse, record, &recorder);
    ASSERT_EQ(recorder.count, 1);
    EXPECT_EQ(recorder.seen[0].event.mouse_position.pointer_type, RDT_POINTER_PEN);
    EXPECT_EQ(recorder.seen[0].samples.count, 2);
    // a button event dispatches the pending mouse move, then itself
    RdtEvent down;
    rdt_event_set_mouse_button(&down, RDT_EVENT_MOUSE_DOWN, 5.0f, 5.0f, 0, 1, 0, 1.03);
    rdt_input_batch_submit(&batch, &down, record, &recorder);
    ASSERT_EQ(recorder.count, 3);
    EXPECT_EQ(recorder.seen[1].event.mouse_position.pointer_type, RDT_POINTER_MOUSE);
    EXPECT_EQ(recorder.seen[1].samples.count, 1);
    EXPECT_EQ(recorder.seen[2].event.type, RDT_EVENT_MOUSE_DOWN);
    EXPECT_FALSE(recorder.seen[2].has_samples);
}

TEST(PointerSamples, OverflowDropsTheOldestSamples) {
    static RdtInputBatch batch = {};
    Recorder recorder;
    const int total = RDT_POINTER_SAMPLE_CAPACITY + 10;
    for (int i = 0; i < total; i++) {
        RdtEvent move = pen_move((float)i, 0.0f, 0.5f, 1.0 + 0.001 * i);
        rdt_input_batch_submit(&batch, &move, record, &recorder);
    }
    rdt_input_batch_flush(&batch, record, &recorder);
    ASSERT_EQ(recorder.count, 1);
    const RdtPointerSamples& s = recorder.seen[0].samples;
    EXPECT_EQ(s.count, RDT_POINTER_SAMPLE_CAPACITY);
    EXPECT_TRUE(s.dropped);
    EXPECT_FLOAT_EQ(s.items[0].x, 10.0f);
    EXPECT_FLOAT_EQ(s.items[RDT_POINTER_SAMPLE_CAPACITY - 1].x, (float)(total - 1));
}

}  // namespace
