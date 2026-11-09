# Native archive build during dependency packaging.
PYTHON ?= python3
INPUT_ROOT ?= ../lambda-input
DEPS_ROOT ?= $(INPUT_ROOT)
PROFILE ?= debug
RADIANT_CPPFLAGS = -I. -I$(INPUT_ROOT) -I$(INPUT_ROOT)/lib/re2 -I$(INPUT_ROOT)/lib/font/woff2/include \
  -I$(INPUT_ROOT)/build_temp/image-config/include -I$(INPUT_ROOT)/build/deps/utf8proc/include \
  -I$(INPUT_ROOT)/build/deps/curl/include -I$(INPUT_ROOT)/build/deps/thorvg/include -I/opt/homebrew/include -I/usr/local/include \
  -I$(INPUT_ROOT)/lib/image/libpng -I$(INPUT_ROOT)/lib/image/libjpeg-turbo/src -I$(INPUT_ROOT)/lib/image/libwebp/src \
  -I$(INPUT_ROOT)/lib/image/libavif/include -I$(INPUT_ROOT)/lib/image/libtiff/libtiff -I$(INPUT_ROOT)/lib/image/giflib \
  -I$(INPUT_ROOT)/lib/image/libnsbmp/include $(shell pkg-config --cflags fribidi libopenjp2 2>/dev/null) $(RADIANT_EXTRA_CPPFLAGS)
ifeq ($(shell uname -s),Darwin)
PLATFORM_FLAGS ?= -mmacosx-version-min=15.7
endif
ifeq ($(PROFILE),debug)
PROFILE_FLAGS = -O3 -g -fno-omit-frame-pointer
PROFILE_DEFINES = -DDEBUG -DLAMBDA_JS_EXEC_PROFILE
else ifeq ($(PROFILE),debug_asan)
PROFILE_FLAGS = -Og -g -fno-omit-frame-pointer -fsanitize=address
PROFILE_DEFINES = -DDEBUG
else ifeq ($(PROFILE),release)
PROFILE_FLAGS = -O3
PROFILE_DEFINES = -DNDEBUG
else ifeq ($(PROFILE),release_profile)
PROFILE_FLAGS = -O3 -g -fno-omit-frame-pointer
PROFILE_DEFINES = -DNDEBUG -DLAMBDA_JS_EXEC_PROFILE
else ifeq ($(PROFILE),coverage)
PROFILE_FLAGS = -O0 -g -fprofile-instr-generate -fcoverage-mapping
PROFILE_DEFINES = -DDEBUG
else
$(error unsupported PROFILE $(PROFILE))
endif
CXXFLAGS ?= $(PROFILE_FLAGS) -Wno-return-type-c-linkage
DEFINES = $(PROFILE_DEFINES) -DGL_SILENCE_DEPRECATION
NATIVE_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant .)
PROPERTY_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant-properties .)
OBJCXX_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant . --objcxx)
ifeq ($(strip $(NATIVE_SOURCES)),)
$(error Radiant source list is empty)
endif
ifeq ($(strip $(PROPERTY_SOURCES)),)
$(error Radiant property source list is empty)
endif
NATIVE_OBJECTS := $(addprefix build/obj/native/$(PROFILE)/,$(addsuffix .o,$(NATIVE_SOURCES)))
PROPERTY_OBJECTS := $(addprefix build/obj/properties/$(PROFILE)/,$(addsuffix .o,$(PROPERTY_SOURCES)))

.PHONY: all clean test-native check-input-facade input-deps force-build-signature
all: check-input-facade input-deps build/lib/$(PROFILE)/libradiant.a build/lib/$(PROFILE)/libradiant-properties.a

check-input-facade:
	$(PYTHON) scripts/check_input_facade.py

# Flags, dependency paths and source membership are part of archive identity.
export PROFILE CXX AR CPPFLAGS CXXFLAGS PLATFORM_FLAGS RADIANT_CPPFLAGS DEFINES NATIVE_SOURCES PROPERTY_SOURCES OBJCXX_SOURCES
force-build-signature:

build/radiant-$(PROFILE)-build.json: force-build-signature $(INPUT_ROOT)/scripts/update_build_signature.py
	$(PYTHON) $(INPUT_ROOT)/scripts/update_build_signature.py radiant

$(NATIVE_OBJECTS) $(PROPERTY_OBJECTS): build/radiant-$(PROFILE)-build.json | input-deps

build/lib/$(PROFILE)/libradiant.a: $(NATIVE_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

build/lib/$(PROFILE)/libradiant-properties.a: $(PROPERTY_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

build/obj/native/$(PROFILE)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 -fms-extensions $(if $(filter $<,$(OBJCXX_SOURCES)),-x objective-c++) -MMD -MP -MF $@.d -c $< -o $@

build/obj/properties/$(PROFILE)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 -fms-extensions -MMD -MP -MF $@.d -c $< -o $@

# These native host tests link the real archive and do not start a scripting runtime.
NATIVE_TEST_NAMES = test_radiant_event_sim_controller_host_gtest test_radiant_event_reconcile_host_gtest test_radiant_event_navigation_host_gtest test_radiant_event_host_gtest test_radiant_resource_observer_gtest test_radiant_scroll_observer_gtest \
  test_scene3d_physics_event_host_gtest test_scene3d_navigation_event_host_gtest \
  test_radiant_media_backend_gtest
NATIVE_TEST_BINS = $(addprefix build/test/,$(NATIVE_TEST_NAMES))
GTEST_FLAGS ?= $(shell pkg-config --cflags --libs gtest_main)

build/test/%: test/%.cpp build/lib/$(PROFILE)/libradiant.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< build/lib/$(PROFILE)/libradiant.a $(GTEST_FLAGS) -o $@

# Always ask the owner to refresh its archives before consuming them.
input-deps:
	$(MAKE) -C "$(INPUT_ROOT)" PROFILE="$(PROFILE)"

$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a: | input-deps
	@test -f $@

ifeq ($(shell uname -s),Darwin)
MEDIA_TEST_LIBS = -Wl,-dead_strip -framework Foundation -framework CoreFoundation -framework CoreText \
  -framework CoreGraphics -framework Security -framework CoreServices -framework AVFoundation \
  -framework CoreMedia -framework CoreVideo -framework AudioToolbox -lpthread
else
MEDIA_TEST_LIBS = -lpthread
endif

# Link the real archive so a platform stub cannot silently replace the provider.
build/test/test_radiant_media_backend_gtest: test/test_radiant_media_backend_gtest.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(GTEST_FLAGS) $(MEDIA_TEST_LIBS) -o $@

test-native: check-input-facade $(NATIVE_TEST_BINS)
	@set -e; for test_bin in $(NATIVE_TEST_BINS); do ./$$test_bin; done

# Force every native archive member into a client without a Lambda runtime.
# The current macOS gate uses the same sibling-built vendor artifacts as the split build.
ifeq ($(shell uname -s),Darwin)
RADIANT_BREW_PREFIX ?= $(shell brew --prefix)
NATIVE_CLOSURE_LIBS ?= $(INPUT_ROOT)/build/deps/utf8proc/lib/libutf8proc.a \
  $(INPUT_ROOT)/build/lib/native/$(PROFILE)/libre2.a $(INPUT_ROOT)/build/deps/curl/lib/libcurl.a \
  $(RADIANT_BREW_PREFIX)/lib/libnghttp2.a $(RADIANT_BREW_PREFIX)/lib/libmbedtls.a \
  $(RADIANT_BREW_PREFIX)/lib/libmbedx509.a $(RADIANT_BREW_PREFIX)/lib/libmbedcrypto.a \
  $(RADIANT_BREW_PREFIX)/lib/libuv.a $(INPUT_ROOT)/build/deps/brotli/lib/libbrotlidec.a \
  $(INPUT_ROOT)/build/deps/brotli/lib/libbrotlicommon.a $(INPUT_ROOT)/build/deps/thorvg/lib/libthorvg.a \
  $(INPUT_ROOT)/build/lib/native/$(PROFILE)/liblambda-image-codecs.a $(RADIANT_BREW_PREFIX)/opt/zlib/lib/libz.a \
  $(RADIANT_BREW_PREFIX)/opt/openssl@3/lib/libcrypto.a $(RADIANT_BREW_PREFIX)/lib/libmpdec.a \
  $(RADIANT_BREW_PREFIX)/opt/zstd/lib/libzstd.a $(INPUT_ROOT)/build/lib/native/$(PROFILE)/liblambda-archive-readers.a \
  $(RADIANT_BREW_PREFIX)/opt/xz/lib/liblzma.a $(RADIANT_BREW_PREFIX)/opt/bzip2/lib/libbz2.a \
  $(RADIANT_BREW_PREFIX)/opt/openjpeg/lib/libopenjp2.a $(RADIANT_BREW_PREFIX)/lib/libfribidi.a \
  $(RADIANT_BREW_PREFIX)/lib/libglfw3.a -framework CoreFoundation -framework CoreServices \
  -framework SystemConfiguration -framework Security -framework Cocoa -framework IOKit \
  -framework CoreVideo -framework AVFoundation -framework AudioToolbox -framework CoreMedia \
  -framework OpenGL -framework Foundation -framework CoreGraphics -framework Accelerate \
  -framework CoreText -framework AppKit -framework Carbon -framework WebKit -liconv -lpthread

.PHONY: test-native-closure
test-native-closure: check-input-facade build/lib/$(PROFILE)/libradiant.a build/lib/$(PROFILE)/libradiant-properties.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p build/test
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(CXXFLAGS) -std=c++17 \
	  test/test_native_archive_closure.cpp -o build/test/test_native_archive_closure \
	  -Wl,-force_load,build/lib/$(PROFILE)/libradiant.a -Wl,-force_load,build/lib/$(PROFILE)/libradiant-properties.a \
	  -Wl,-force_load,$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a \
	  -Wl,-force_load,$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(NATIVE_CLOSURE_LIBS)
	./build/test/test_native_archive_closure
endif

clean:
	rm -rf build

-include $(addsuffix .d,$(NATIVE_OBJECTS) $(PROPERTY_OBJECTS))
