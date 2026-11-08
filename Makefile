# Native archive build during migration; runtime-dependent sources remain visible.
PYTHON ?= python3
INPUT_ROOT ?= ../lambda-input
DEPS_ROOT ?= ../Lambda
RADIANT_CPPFLAGS = -I. -I$(INPUT_ROOT) -Ilib/re2 -Ilib/font/woff2/include -Ilambda/mir \
  -I$(DEPS_ROOT)/build_temp/image-config/include -I$(DEPS_ROOT)/build_temp/utf8proc \
  -I$(DEPS_ROOT)/mac-deps/curl-8.10.1/include -I$(DEPS_ROOT)/mac-deps/thorvg/inc \
  -I$(DEPS_ROOT)/mac-deps/thorvg/src/bindings/capi -I/opt/homebrew/include -I/usr/local/include \
  -Ilib/image/libpng -Ilib/image/libjpeg-turbo/src -Ilib/image/libwebp/src \
  -Ilib/image/libavif/include -Ilib/image/libtiff/libtiff -Ilib/image/giflib \
  -Ilib/image/libnsbmp/include $(shell pkg-config --cflags fribidi libopenjp2 2>/dev/null) $(RADIANT_EXTRA_CPPFLAGS)
ifeq ($(shell uname -s),Darwin)
PLATFORM_FLAGS ?= -mmacosx-version-min=15.7
endif
CXXFLAGS ?= -O2 -g -Wno-return-type-c-linkage
DEFINES = -DDEBUG -DLAMBDA_JS_EXEC_PROFILE -DGL_SILENCE_DEPRECATION
NATIVE_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant .)
PROPERTY_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant-properties .)
OBJCXX_SOURCES := $(shell $(PYTHON) -c 'import json; print(" ".join(next(t for t in json.load(open("build_targets.json"))["targets"] if t["name"] == "radiant")["objcxx_source_files"]))')
ifeq ($(strip $(NATIVE_SOURCES)),)
$(error Radiant source list is empty)
endif
ifeq ($(strip $(PROPERTY_SOURCES)),)
$(error Radiant property source list is empty)
endif
NATIVE_OBJECTS := $(addprefix build/obj/native/,$(addsuffix .o,$(NATIVE_SOURCES)))
PROPERTY_OBJECTS := $(addprefix build/obj/properties/,$(addsuffix .o,$(PROPERTY_SOURCES)))

.PHONY: all clean test-native force-build-signature
all: build/lib/libradiant.a build/lib/libradiant-properties.a

# Flags, dependency paths and source membership are part of archive identity.
export CXX AR CPPFLAGS CXXFLAGS PLATFORM_FLAGS RADIANT_CPPFLAGS DEFINES NATIVE_SOURCES PROPERTY_SOURCES OBJCXX_SOURCES
force-build-signature:

build/radiant-build.json: force-build-signature $(INPUT_ROOT)/scripts/update_build_signature.py
	$(PYTHON) $(INPUT_ROOT)/scripts/update_build_signature.py radiant

$(NATIVE_OBJECTS) $(PROPERTY_OBJECTS): build/radiant-build.json

build/lib/libradiant.a: $(NATIVE_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

build/lib/libradiant-properties.a: $(PROPERTY_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

build/obj/native/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 -fms-extensions $(if $(filter $<,$(OBJCXX_SOURCES)),-x objective-c++) -MMD -MP -MF $@.d -c $< -o $@

build/obj/properties/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 -fms-extensions -MMD -MP -MF $@.d -c $< -o $@

# These native host tests link the real archive and do not start a scripting runtime.
NATIVE_TEST_NAMES = test_radiant_resource_observer_gtest test_radiant_scroll_observer_gtest \
  test_scene3d_physics_event_host_gtest test_scene3d_navigation_event_host_gtest
NATIVE_TEST_BINS = $(addprefix build/test/,$(NATIVE_TEST_NAMES))
GTEST_FLAGS ?= $(shell pkg-config --cflags --libs gtest_main)

build/test/%: test/%.cpp build/lib/libradiant.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< build/lib/libradiant.a $(GTEST_FLAGS) -o $@

test-native: $(NATIVE_TEST_BINS)
	@set -e; for test_bin in $(NATIVE_TEST_BINS); do ./$$test_bin; done

clean:
	rm -rf build

-include $(addsuffix .d,$(NATIVE_OBJECTS) $(PROPERTY_OBJECTS))
