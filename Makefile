# Native archive build during dependency packaging.
PYTHON ?= python3
INPUT_ROOT ?= ../lambda-input
DEPS_ROOT ?= ../Lambda
RADIANT_CPPFLAGS = -I. -I$(INPUT_ROOT) -I$(INPUT_ROOT)/lib/re2 -I$(INPUT_ROOT)/lib/font/woff2/include \
  -I$(DEPS_ROOT)/build_temp/image-config/include -I$(DEPS_ROOT)/build_temp/utf8proc \
  -I$(DEPS_ROOT)/mac-deps/curl-8.10.1/include -I$(DEPS_ROOT)/mac-deps/thorvg/inc \
  -I$(DEPS_ROOT)/mac-deps/thorvg/src/bindings/capi -I/opt/homebrew/include -I/usr/local/include \
  -I$(INPUT_ROOT)/lib/image/libpng -I$(INPUT_ROOT)/lib/image/libjpeg-turbo/src -I$(INPUT_ROOT)/lib/image/libwebp/src \
  -I$(INPUT_ROOT)/lib/image/libavif/include -I$(INPUT_ROOT)/lib/image/libtiff/libtiff -I$(INPUT_ROOT)/lib/image/giflib \
  -I$(INPUT_ROOT)/lib/image/libnsbmp/include $(shell pkg-config --cflags fribidi libopenjp2 2>/dev/null) $(RADIANT_EXTRA_CPPFLAGS)
ifeq ($(shell uname -s),Darwin)
PLATFORM_FLAGS ?= -mmacosx-version-min=15.7
endif
CXXFLAGS ?= -O2 -g -Wno-return-type-c-linkage
DEFINES = -DDEBUG -DLAMBDA_JS_EXEC_PROFILE -DGL_SILENCE_DEPRECATION
NATIVE_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant .)
PROPERTY_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant-properties .)
OBJCXX_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant . --objcxx)
ifeq ($(strip $(NATIVE_SOURCES)),)
$(error Radiant source list is empty)
endif
ifeq ($(strip $(PROPERTY_SOURCES)),)
$(error Radiant property source list is empty)
endif
NATIVE_OBJECTS := $(addprefix build/obj/native/,$(addsuffix .o,$(NATIVE_SOURCES)))
PROPERTY_OBJECTS := $(addprefix build/obj/properties/,$(addsuffix .o,$(PROPERTY_SOURCES)))

.PHONY: all clean test-native check-input-facade force-build-signature
all: check-input-facade build/lib/libradiant.a build/lib/libradiant-properties.a

check-input-facade:
	$(PYTHON) scripts/check_input_facade.py

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
NATIVE_TEST_NAMES = test_radiant_event_sim_controller_host_gtest test_radiant_event_reconcile_host_gtest test_radiant_event_navigation_host_gtest test_radiant_event_host_gtest test_radiant_resource_observer_gtest test_radiant_scroll_observer_gtest \
  test_scene3d_physics_event_host_gtest test_scene3d_navigation_event_host_gtest \
  test_radiant_media_backend_gtest
NATIVE_TEST_BINS = $(addprefix build/test/,$(NATIVE_TEST_NAMES))
GTEST_FLAGS ?= $(shell pkg-config --cflags --libs gtest_main)

build/test/%: test/%.cpp build/lib/libradiant.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< build/lib/libradiant.a $(GTEST_FLAGS) -o $@

$(INPUT_ROOT)/build/lib/liblambda-lib.a:
	$(MAKE) -C "$(INPUT_ROOT)" INPUT_DEPS_ROOT="$(abspath $(DEPS_ROOT))" CURL_PREFIX="$(abspath $(INPUT_ROOT)/build/deps/curl)"

$(INPUT_ROOT)/build/lib/liblambda-input.a: | $(INPUT_ROOT)/build/lib/liblambda-lib.a
	$(MAKE) -C "$(INPUT_ROOT)" INPUT_DEPS_ROOT="$(abspath $(DEPS_ROOT))" CURL_PREFIX="$(abspath $(INPUT_ROOT)/build/deps/curl)"

ifeq ($(shell uname -s),Darwin)
MEDIA_TEST_LIBS = -Wl,-dead_strip -framework Foundation -framework CoreFoundation -framework CoreText \
  -framework CoreGraphics -framework Security -framework CoreServices -framework AVFoundation \
  -framework CoreMedia -framework CoreVideo -framework AudioToolbox -lpthread
else
MEDIA_TEST_LIBS = -lpthread
endif

# Link the real archive so a platform stub cannot silently replace the provider.
build/test/test_radiant_media_backend_gtest: test/test_radiant_media_backend_gtest.cpp build/lib/libradiant.a $(INPUT_ROOT)/build/lib/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< build/lib/libradiant.a $(INPUT_ROOT)/build/lib/liblambda-lib.a $(GTEST_FLAGS) $(MEDIA_TEST_LIBS) -o $@

test-native: check-input-facade $(NATIVE_TEST_BINS)
	@set -e; for test_bin in $(NATIVE_TEST_BINS); do ./$$test_bin; done

# Force every native archive member into a client without a Lambda runtime.
# The current macOS gate uses the same sibling-built vendor artifacts as the split build.
ifeq ($(shell uname -s),Darwin)
RADIANT_BREW_PREFIX ?= $(shell brew --prefix)
NATIVE_CLOSURE_LIBS ?= $(DEPS_ROOT)/build_temp/utf8proc/build/libutf8proc.a \
  $(DEPS_ROOT)/build/lib/native/debug/libre2.a $(INPUT_ROOT)/build/deps/curl/lib/libcurl.a \
  $(RADIANT_BREW_PREFIX)/lib/libnghttp2.a $(RADIANT_BREW_PREFIX)/lib/libmbedtls.a \
  $(RADIANT_BREW_PREFIX)/lib/libmbedx509.a $(RADIANT_BREW_PREFIX)/lib/libmbedcrypto.a \
  $(RADIANT_BREW_PREFIX)/lib/libuv.a $(DEPS_ROOT)/mac-deps/brotli/out/libbrotlidec.a \
  $(DEPS_ROOT)/mac-deps/brotli/out/libbrotlicommon.a $(DEPS_ROOT)/mac-deps/thorvg/build-mac/src/libthorvg.a \
  $(DEPS_ROOT)/build/lib/native/debug/liblambda-image-codecs.a $(RADIANT_BREW_PREFIX)/opt/zlib/lib/libz.a \
  $(RADIANT_BREW_PREFIX)/opt/openssl@3/lib/libcrypto.a $(RADIANT_BREW_PREFIX)/lib/libmpdec.a \
  $(RADIANT_BREW_PREFIX)/opt/zstd/lib/libzstd.a $(DEPS_ROOT)/build/lib/native/debug/liblambda-archive-readers.a \
  $(RADIANT_BREW_PREFIX)/opt/xz/lib/liblzma.a $(RADIANT_BREW_PREFIX)/opt/bzip2/lib/libbz2.a \
  $(RADIANT_BREW_PREFIX)/opt/openjpeg/lib/libopenjp2.a $(RADIANT_BREW_PREFIX)/lib/libfribidi.a \
  $(RADIANT_BREW_PREFIX)/lib/libglfw3.a -framework CoreFoundation -framework CoreServices \
  -framework SystemConfiguration -framework Security -framework Cocoa -framework IOKit \
  -framework CoreVideo -framework AVFoundation -framework AudioToolbox -framework CoreMedia \
  -framework OpenGL -framework Foundation -framework CoreGraphics -framework Accelerate \
  -framework CoreText -framework AppKit -framework Carbon -framework WebKit -liconv -lpthread

.PHONY: test-native-closure
test-native-closure: check-input-facade build/lib/libradiant.a build/lib/libradiant-properties.a $(INPUT_ROOT)/build/lib/liblambda-input.a $(INPUT_ROOT)/build/lib/liblambda-lib.a
	@mkdir -p build/test
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(CXXFLAGS) -std=c++17 \
	  test/test_native_archive_closure.cpp -o build/test/test_native_archive_closure \
	  -Wl,-force_load,build/lib/libradiant.a -Wl,-force_load,build/lib/libradiant-properties.a \
	  -Wl,-force_load,$(INPUT_ROOT)/build/lib/liblambda-input.a \
	  -Wl,-force_load,$(INPUT_ROOT)/build/lib/liblambda-lib.a $(NATIVE_CLOSURE_LIBS)
	./build/test/test_native_archive_closure
endif

clean:
	rm -rf build

-include $(addsuffix .d,$(NATIVE_OBJECTS) $(PROPERTY_OBJECTS))
