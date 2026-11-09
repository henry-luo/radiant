#include <gtest/gtest.h>
#include <string.h>

#define RADIANT_EVENT_CORE_ONLY
#include "radiant/input_batch.hpp"

struct PointerInputProbe {
    RdtEvent events[4];
    RdtPointerSamples samples[4];
    unsigned count;
    RdtInputBatch* reenter;
};

static void record_pointer_input(void* data, RdtEvent* event) {
    auto* probe = (PointerInputProbe*)data;
    ASSERT_LT(probe->count, 4u);
    unsigned index = probe->count++;
    probe->events[index] = *event;
    const RdtPointerSamples* borrowed = event->mouse_position.samples;
    ASSERT_NE(borrowed, nullptr);
    ASSERT_LE(borrowed->count, RDT_POINTER_SAMPLE_CAPACITY);
    probe->samples[index].count = borrowed->count;
    probe->samples[index].dropped = borrowed->dropped;
    memcpy(probe->samples[index].items, borrowed->items, borrowed->count * sizeof(RdtPointerSample));
    // inspect only owned snapshots after dispatch; native samples are borrowed for this call.
    probe->events[index].mouse_position.samples = nullptr;
    if (probe->reenter) {
        RdtInputBatch* batch = probe->reenter;
        probe->reenter = nullptr;
        RdtEvent next = *event;
        next.timestamp += 0.25;
        next.mouse_position.x += 1.0f;
        next.mouse_position.pressure = 0.75f;
        rdt_input_batch_submit(batch, &next, record_pointer_input, probe);
        EXPECT_EQ(borrowed->count, probe->samples[index].count);
        EXPECT_DOUBLE_EQ(borrowed->items[0].timestamp, probe->samples[index].items[0].timestamp);
        EXPECT_FLOAT_EQ(borrowed->items[0].x, probe->samples[index].items[0].x);
        EXPECT_FLOAT_EQ(borrowed->items[0].pressure, probe->samples[index].items[0].pressure);
    }
}

TEST(NativePointerInputTest, MouseConstructorsClearPreviousPenState) {
    RdtEvent event = {};
    RdtPointerSamples samples = {};
    for (unsigned kind = 0; kind < 3; kind++) {
        event.mouse_position.pointer_type = RDT_POINTER_ERASER;
        event.mouse_position.pointer_id = 7;
        event.mouse_position.pressure = 0.8f;
        event.mouse_position.tangential_pressure = -0.25f;
        event.mouse_position.tilt_x = 30.0f;
        event.mouse_position.tilt_y = -20.0f;
        event.mouse_position.twist = 90.0f;
        event.mouse_position.samples = &samples;
        if (kind == 0) rdt_event_set_mouse_position(&event, RDT_EVENT_MOUSE_MOVE, 12.5f, 25.25f, 3.0);
        else if (kind == 1) rdt_event_set_mouse_button(&event, RDT_EVENT_MOUSE_DOWN, 12.5f, 25.25f, 0, 1, RDT_MOD_SHIFT, 3.0);
        else rdt_event_set_scroll(&event, 12.5f, 25.25f, 0.5f, -1.25f, 3.0);
        const MousePositionEvent& mouse = event.mouse_position;
        EXPECT_EQ(mouse.pointer_type, RDT_POINTER_MOUSE);
        EXPECT_EQ(mouse.pointer_id, 0u);
        EXPECT_FLOAT_EQ(mouse.pressure, 0.0f);
        EXPECT_FLOAT_EQ(mouse.tangential_pressure, 0.0f);
        EXPECT_FLOAT_EQ(mouse.tilt_x, 0.0f);
        EXPECT_FLOAT_EQ(mouse.tilt_y, 0.0f);
        EXPECT_FLOAT_EQ(mouse.twist, 0.0f);
        EXPECT_EQ(mouse.samples, nullptr);
        EXPECT_FLOAT_EQ(mouse.x, 12.5f);
        EXPECT_FLOAT_EQ(mouse.y, 25.25f);
        EXPECT_DOUBLE_EQ(mouse.timestamp, 3.0);
    }
}

TEST(NativePointerInputTest, PenMotionPreservesAxesAndOrderedSamples) {
    RdtInputBatch batch = {};
    PointerInputProbe probe = {};
    RdtEvent event = {};
    event.mouse_position.type = RDT_EVENT_MOUSE_MOVE;
    event.mouse_position.pointer_type = RDT_POINTER_PEN;
    event.mouse_position.pointer_id = 7;
    event.mouse_position.tangential_pressure = -0.25f;
    event.mouse_position.tilt_x = 30.0f;
    event.mouse_position.tilt_y = -20.0f;
    event.mouse_position.twist = 90.0f;
    for (unsigned i = 0; i < 3; i++) {
        event.timestamp = 10.0 + i * 0.25;
        event.mouse_position.x = 12.5f + i;
        event.mouse_position.y = 25.25f + i;
        event.mouse_position.pressure = 0.25f + i * 0.125f;
        rdt_input_batch_submit(&batch, &event, record_pointer_input, &probe);
    }
    EXPECT_EQ(probe.count, 0u);
    ASSERT_TRUE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
    ASSERT_EQ(probe.count, 1u);
    const MousePositionEvent& delivered = probe.events[0].mouse_position;
    EXPECT_EQ(delivered.pointer_type, RDT_POINTER_PEN);
    EXPECT_EQ(delivered.pointer_id, 7u);
    EXPECT_FLOAT_EQ(delivered.x, 14.5f);
    EXPECT_FLOAT_EQ(delivered.y, 27.25f);
    EXPECT_FLOAT_EQ(delivered.pressure, 0.5f);
    EXPECT_DOUBLE_EQ(delivered.timestamp, 10.5);
    ASSERT_EQ(probe.samples[0].count, 3u);
    EXPECT_FALSE(probe.samples[0].dropped);
    for (unsigned i = 0; i < 3; i++) {
        const RdtPointerSample& sample = probe.samples[0].items[i];
        EXPECT_DOUBLE_EQ(sample.timestamp, 10.0 + i * 0.25);
        EXPECT_FLOAT_EQ(sample.x, 12.5f + i);
        EXPECT_FLOAT_EQ(sample.y, 25.25f + i);
        EXPECT_FLOAT_EQ(sample.pressure, 0.25f + i * 0.125f);
        EXPECT_FLOAT_EQ(sample.tangential_pressure, -0.25f);
        EXPECT_FLOAT_EQ(sample.tilt_x, 30.0f);
        EXPECT_FLOAT_EQ(sample.tilt_y, -20.0f);
        EXPECT_FLOAT_EQ(sample.twist, 90.0f);
    }
    EXPECT_FALSE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
}

TEST(NativePointerInputTest, DeviceAndPointerChangesSeparateSampleBatches) {
    RdtInputBatch batch = {};
    PointerInputProbe probe = {};
    const RdtPointerType types[] = {RDT_POINTER_MOUSE, RDT_POINTER_PEN, RDT_POINTER_ERASER, RDT_POINTER_ERASER};
    const uint32_t ids[] = {0, 1, 1, 2};
    RdtEvent event = {};
    event.type = RDT_EVENT_MOUSE_MOVE;
    for (unsigned i = 0; i < 4; i++) {
        event.mouse_position.pointer_type = types[i];
        event.mouse_position.pointer_id = ids[i];
        event.mouse_position.x = 10.5f + i;
        rdt_input_batch_submit(&batch, &event, record_pointer_input, &probe);
        EXPECT_EQ(probe.count, i);
    }
    ASSERT_TRUE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
    ASSERT_EQ(probe.count, 4u);
    for (unsigned i = 0; i < 4; i++) {
        EXPECT_EQ(probe.events[i].mouse_position.pointer_type, types[i]);
        EXPECT_EQ(probe.events[i].mouse_position.pointer_id, ids[i]);
        ASSERT_EQ(probe.samples[i].count, 1u);
        EXPECT_FLOAT_EQ(probe.samples[i].items[0].x, 10.5f + i);
    }
}

TEST(NativePointerInputTest, OverflowKeepsNewestSamplesAndResetsAfterFlush) {
    RdtInputBatch batch = {};
    PointerInputProbe probe = {};
    RdtEvent event = {};
    event.type = RDT_EVENT_MOUSE_MOVE;
    event.mouse_position.pointer_type = RDT_POINTER_PEN;
    for (unsigned i = 0; i < RDT_POINTER_SAMPLE_CAPACITY + 3; i++) {
        event.timestamp = i * 0.25;
        event.mouse_position.x = 0.5f + i;
        rdt_input_batch_submit(&batch, &event, record_pointer_input, &probe);
    }
    ASSERT_TRUE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
    ASSERT_EQ(probe.count, 1u);
    ASSERT_EQ(probe.samples[0].count, RDT_POINTER_SAMPLE_CAPACITY);
    EXPECT_TRUE(probe.samples[0].dropped);
    for (unsigned i = 0; i < RDT_POINTER_SAMPLE_CAPACITY; i++) {
        EXPECT_FLOAT_EQ(probe.samples[0].items[i].x, 3.5f + i);
        EXPECT_DOUBLE_EQ(probe.samples[0].items[i].timestamp, (i + 3) * 0.25);
    }
    rdt_input_batch_submit(&batch, &event, record_pointer_input, &probe);
    ASSERT_TRUE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
    ASSERT_EQ(probe.count, 2u);
    EXPECT_EQ(probe.samples[1].count, 1u);
    EXPECT_FALSE(probe.samples[1].dropped);
}

TEST(NativePointerInputTest, ReentrantSubmissionPreservesBorrowedSamples) {
    RdtInputBatch batch = {};
    PointerInputProbe probe = {};
    RdtEvent event = {};
    event.mouse_position.type = RDT_EVENT_MOUSE_MOVE;
    event.mouse_position.pointer_type = RDT_POINTER_PEN;
    event.mouse_position.x = 10.5f;
    event.mouse_position.pressure = 0.5f;
    event.timestamp = 1.0;
    probe.reenter = &batch;
    rdt_input_batch_submit(&batch, &event, record_pointer_input, &probe);
    ASSERT_TRUE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
    ASSERT_EQ(probe.count, 1u);
    EXPECT_TRUE(batch.pending_motion);
    ASSERT_TRUE(rdt_input_batch_flush(&batch, record_pointer_input, &probe));
    ASSERT_EQ(probe.count, 2u);
    ASSERT_EQ(probe.samples[1].count, 1u);
    EXPECT_DOUBLE_EQ(probe.samples[1].items[0].timestamp, 1.25);
    EXPECT_FLOAT_EQ(probe.samples[1].items[0].x, 11.5f);
    EXPECT_FLOAT_EQ(probe.samples[1].items[0].pressure, 0.75f);
    EXPECT_FALSE(batch.pending_motion);
}
