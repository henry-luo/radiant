#include <gtest/gtest.h>
#include <stdlib.h>
#include "../radiant/source_path_value.hpp"
#include "../radiant/source_position_host.hpp"
#include "test_native_provider.hpp"
#include "../lambda/input/input.hpp"
#include "../lambda/io/mark_builder.hpp"
#include "../lambda/core/mark_reader.hpp"
#include "../lambda/core/lambda-decimal.hpp"

static uint64_t recorded_source;
static int recorded_depth, reset_count;
static bool publish_reverse(Item result, RenderMapLookup*, SourcePathC* path) {
    if (path) source_path_init(path);
    return result.item == 42;
}
static bool publish_position(Item result, Item* source, SourcePathC* path) {
    if (result.item != 42 || !source || !path) return false;
    *source = Item{.item = 17};
    source_path_init(path);
    return true;
}
static void record_path(Item source, const char*, const int*, int depth) {
    recorded_source = source.item;
    recorded_depth = depth;
}
static void reset_paths() { reset_count++; }

TEST(RadiantSourcePositionHostTest, MissingHostAndRegisteredLookup) {
    Item source = ItemNull;
    SourcePathC path = {reinterpret_cast<int*>(1), 1};
    render_map_record_path(Item{.item = 9}, "template", nullptr, 0);
    source_pos_bridge_reset();
    EXPECT_EQ(recorded_source, 0u);
    EXPECT_EQ(reset_count, 0);
    EXPECT_FALSE(render_map_reverse_lookup_with_path(Item{.item = 42}, nullptr, &path));
    EXPECT_EQ(path.indices, nullptr);
    EXPECT_EQ(path.depth, 0);
    EXPECT_FALSE(radiant_source_position_lookup(Item{.item = 42}, &source, &path));
    static const RadiantSourcePositionHostOps incomplete = {record_path, reset_paths, publish_reverse, nullptr};
    static const RadiantSourcePositionHostOps host = {record_path, reset_paths, publish_reverse, publish_position};
    static const RadiantSourcePositionHostOps conflict = {record_path, reset_paths, publish_reverse, publish_position};
    ASSERT_TRUE(expect_native_provider_registration(radiant_source_position_host_register,
                                                     &incomplete, &host, &conflict));
    render_map_record_path(Item{.item = 9}, "template", nullptr, 3);
    source_pos_bridge_reset();
    EXPECT_EQ(recorded_source, 9u);
    EXPECT_EQ(recorded_depth, 3);
    EXPECT_EQ(reset_count, 1);
    EXPECT_TRUE(render_map_reverse_lookup_with_path(Item{.item = 42}, nullptr, &path));
    EXPECT_TRUE(radiant_source_position_lookup(Item{.item = 42}, &source, &path));
    EXPECT_EQ(source.item, 17u);
    EXPECT_FALSE(radiant_source_position_lookup(Item{.item = 0}, &source, &path));
}

TEST(SourcePosBridgeC, PathInitFreeIsSafe) {
    SourcePathC p;
    source_path_init(&p);
    EXPECT_EQ(p.indices, nullptr);
    EXPECT_EQ(p.depth, 0);
    source_path_free(&p);  // double-free of init'd state is safe
    source_path_free(&p);
}

TEST(SourcePosBridgeC, PathCloneAndEqual) {
    SourcePathC src;
    source_path_init(&src);
    src.depth = 3;
    src.indices = (int*)malloc(sizeof(int) * 3);
    src.indices[0] = 4;
    src.indices[1] = 0;
    src.indices[2] = 7;

    SourcePathC dup = source_path_clone(&src);
    EXPECT_EQ(dup.depth, 3);
    ASSERT_NE(dup.indices, nullptr);
    EXPECT_NE(dup.indices, src.indices);
    EXPECT_EQ(dup.indices[0], 4);
    EXPECT_EQ(dup.indices[2], 7);
    EXPECT_TRUE(source_path_equal(&src, &dup));

    dup.indices[1] = 1;
    EXPECT_FALSE(source_path_equal(&src, &dup));

    source_path_free(&src);
    source_path_free(&dup);
}

TEST(SourcePosBridgeC, EmptyPathsAreEqual) {
    SourcePathC a, b;
    source_path_init(&a);
    source_path_init(&b);
    EXPECT_TRUE(source_path_equal(&a, &b));
    SourcePathC c = source_path_clone(&a);
    EXPECT_EQ(c.depth, 0);
    EXPECT_EQ(c.indices, nullptr);
    EXPECT_TRUE(source_path_equal(&a, &c));
}

TEST(SourcePosBridgeC, SourcePosFreeReleasesPath) {
    SourcePosC p;
    source_path_init(&p.path);
    p.path.depth = 2;
    p.path.indices = (int*)malloc(sizeof(int) * 2);
    p.path.indices[0] = 0;
    p.path.indices[1] = 1;
    p.offset = 5;
    p.kind = SOURCE_POS_ELEMENT;
    source_pos_free(&p);
    EXPECT_EQ(p.path.indices, nullptr);
    EXPECT_EQ(p.path.depth, 0);
    EXPECT_EQ(p.offset, 0u);
    EXPECT_EQ(p.kind, SOURCE_POS_TEXT);
}

namespace {

SourcePosC make_pos(const int* indices, int count, uint32_t offset, SourcePosKind kind) {
    SourcePosC p;
    source_path_init(&p.path);
    p.offset = offset;
    p.kind = kind;
    if (count > 0) {
        p.path.indices = (int*)malloc(sizeof(int) * (size_t)count);
        for (int i = 0; i < count; i++) p.path.indices[i] = indices[i];
        p.path.depth = count;
    }
    return p;
}

} // namespace

TEST(SourcePosBridgeMarkBuilder, PosToItemBuildsMap) {
    Input* input = InputManager::create_input(nullptr);
    ASSERT_NE(input, nullptr);
    MarkBuilder mb(input);

    const int indices[] = {0, 2, 1};
    SourcePosC p = make_pos(indices, 3, 7, SOURCE_POS_TEXT);
    Item item = source_pos_to_item(mb, &p);

    MapReader m = MapReader::fromItem(item);
    ItemReader path = m.get("path");
    ItemReader off  = m.get("offset");
    EXPECT_TRUE(path.isArray());
    EXPECT_TRUE(off.isInt());
    EXPECT_EQ(off.asInt(), 7);
    ArrayReader arr = path.asArray();
    EXPECT_EQ(arr.length(), 3);

    source_pos_free(&p);
}

TEST(SourcePosBridgeMarkBuilder, PosToItemEmptyPath) {
    Input* input = InputManager::create_input(nullptr);
    MarkBuilder mb(input);

    SourcePosC p = make_pos(nullptr, 0, 0, SOURCE_POS_ELEMENT);
    Item item = source_pos_to_item(mb, &p);
    MapReader m = MapReader::fromItem(item);
    ItemReader path = m.get("path");
    EXPECT_TRUE(path.isArray());
    EXPECT_EQ(path.asArray().length(), 0);

    source_pos_free(&p);
}

TEST(SourcePosBridgeMarkBuilder, ExactDecimalCoordinatesPreserveSourcePosition) {
    Input* input = InputManager::create_input(nullptr);
    ASSERT_NE(input, nullptr);
    MarkBuilder mb(input);

    // Template editor arithmetic promotes path/offset coordinates to Decimal.
    // The C bridge must retain exact integral coordinates instead of treating
    // them as absent and rebinding a later edit to offset zero.
    ArrayBuilder path = mb.array();
    path.append(decimal_from_int64_arena(1, mb.arena()));
    path.append(decimal_from_int64_arena(0, mb.arena()));
    MapBuilder pos_map = mb.map();
    pos_map.put("path", path.final());
    pos_map.put("offset", decimal_from_int64_arena(47, mb.arena()));

    SourcePosC pos;
    ASSERT_TRUE(source_pos_from_item(pos_map.final(), &pos));
    EXPECT_EQ(pos.kind, SOURCE_POS_TEXT);
    EXPECT_EQ(pos.offset, 47u);
    ASSERT_EQ(pos.path.depth, 2);
    EXPECT_EQ(pos.path.indices[0], 1);
    EXPECT_EQ(pos.path.indices[1], 0);
    source_pos_free(&pos);
}

TEST(SourcePosBridgeMarkBuilder, RejectsFractionalDecimalCoordinate) {
    Input* input = InputManager::create_input(nullptr);
    ASSERT_NE(input, nullptr);
    MarkBuilder mb(input);

    ArrayBuilder path = mb.array();
    path.append(decimal_from_string_arena("0.5", mb.arena()));
    MapBuilder pos_map = mb.map();
    pos_map.put("path", path.final());
    pos_map.put("offset", decimal_from_int64_arena(0, mb.arena()));

    SourcePosC pos;
    EXPECT_FALSE(source_pos_from_item(pos_map.final(), &pos));
}

TEST(SourcePosBridgeMarkBuilder, TextSelectionShape) {
    Input* input = InputManager::create_input(nullptr);
    MarkBuilder mb(input);

    const int indices[] = {0, 0};
    SourcePosC anchor = make_pos(indices, 2, 1, SOURCE_POS_TEXT);
    SourcePosC head   = make_pos(indices, 2, 4, SOURCE_POS_TEXT);
    Item sel = source_text_selection_to_item(mb, &anchor, &head);

    MapReader m = MapReader::fromItem(sel);
    ItemReader kind = m.get("kind");
    EXPECT_TRUE(kind.isSymbol());
    EXPECT_STREQ(kind.asSymbol()->chars, "text");
    EXPECT_TRUE(m.get("anchor").isMap());
    EXPECT_TRUE(m.get("head").isMap());

    source_pos_free(&anchor);
    source_pos_free(&head);
}

TEST(SourcePosBridgeMarkBuilder, NodeSelectionShape) {
    Input* input = InputManager::create_input(nullptr);
    MarkBuilder mb(input);

    SourcePathC path;
    source_path_init(&path);
    path.indices = (int*)malloc(sizeof(int) * 2);
    path.indices[0] = 0;
    path.indices[1] = 3;
    path.depth = 2;

    Item sel = source_node_selection_to_item(mb, &path);
    MapReader m = MapReader::fromItem(sel);
    ItemReader kind = m.get("kind");
    EXPECT_TRUE(kind.isSymbol());
    EXPECT_STREQ(kind.asSymbol()->chars, "node");
    ItemReader p = m.get("path");
    EXPECT_TRUE(p.isArray());
    EXPECT_EQ(p.asArray().length(), 2);

    source_path_free(&path);
}

