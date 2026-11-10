# Native archive build during dependency packaging.
PYTHON ?= python3
NODE ?= node
INPUT_ROOT ?= ../lambda-input
DEPS_ROOT ?= $(INPUT_ROOT)
PROFILE ?= debug
RADIANT_CPPFLAGS = -I. -I$(INPUT_ROOT) -I$(INPUT_ROOT)/lib/re2 -I$(INPUT_ROOT)/lib/font/woff2/include -I$(INPUT_ROOT)/test/css \
  -I$(INPUT_ROOT)/build_temp/image-config/include -I$(INPUT_ROOT)/build/deps/utf8proc/include \
  -I$(INPUT_ROOT)/build/deps/curl/include -I$(INPUT_ROOT)/build/deps/thorvg/include -I/opt/homebrew/include -I/usr/local/include \
  -I$(INPUT_ROOT)/lib/image/libpng -I$(INPUT_ROOT)/lib/image/libjpeg-turbo/src -I$(INPUT_ROOT)/lib/image/libwebp/src \
  -I$(INPUT_ROOT)/lib/image/libavif/include -I$(INPUT_ROOT)/lib/image/libtiff/libtiff -I$(INPUT_ROOT)/lib/image/giflib \
  -I$(INPUT_ROOT)/lib/image/libnsbmp/include $(shell pkg-config --cflags fribidi libopenjp2 2>/dev/null) $(RADIANT_EXTRA_CPPFLAGS)
ifeq ($(shell uname -s),Darwin)
RADIANT_CPPFLAGS += -I$(shell brew --prefix)/opt/mbedtls@3/include
PLATFORM_FLAGS ?= -mmacosx-version-min=15.7
else ifeq ($(shell uname -s),Linux)
RADIANT_CPPFLAGS += -I$(INPUT_ROOT)/build/deps/linux-support/include
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
OBJCXX_SOURCES := $(shell $(PYTHON) $(INPUT_ROOT)/scripts/build_sources.py radiant . --objcxx)
ifeq ($(strip $(NATIVE_SOURCES)),)
$(error Radiant source list is empty)
endif
NATIVE_OBJECTS := $(addprefix build/obj/native/$(PROFILE)/,$(addsuffix .o,$(NATIVE_SOURCES)))

.PHONY: all clean test-native run-radiant-baseline check-input-facade input-deps force-build-signature test-layout-baseline test-layout-smoke
all: check-input-facade input-deps build/lib/$(PROFILE)/libradiant.a

check-input-facade:
	$(PYTHON) scripts/check_input_facade.py

# Flags, dependency paths and source membership are part of archive identity.
export PROFILE CXX AR CPPFLAGS CXXFLAGS PLATFORM_FLAGS RADIANT_CPPFLAGS DEFINES NATIVE_SOURCES OBJCXX_SOURCES
force-build-signature:

build/radiant-$(PROFILE)-build.json: force-build-signature $(INPUT_ROOT)/scripts/update_build_signature.py
	$(PYTHON) $(INPUT_ROOT)/scripts/update_build_signature.py radiant

$(NATIVE_OBJECTS): build/radiant-$(PROFILE)-build.json | input-deps

build/lib/$(PROFILE)/libradiant.a: $(NATIVE_OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

build/obj/native/$(PROFILE)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 -fms-extensions $(if $(filter $<,$(OBJCXX_SOURCES)),-x objective-c++) -MMD -MP -MF $@.d -c $< -o $@

# These native host tests link the real archive and do not start a scripting runtime.
NATIVE_TEST_NAMES = $(patsubst test/%.cpp,%,$(wildcard test/test_*_gtest.cpp test/css/*.cpp)) lib/test_own_tagged
NATIVE_TEST_BINS = $(addprefix build/test/,$(NATIVE_TEST_NAMES))
GTEST_FLAGS ?= $(shell pkg-config --cflags --libs gtest_main)

build/test/%: test/%.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-test-utils.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

build/test/test_layout_baseline: test/test_layout_baseline.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

# Some isolated native suites supply their own host and renderer functions.
build/test/test_layout_custom_gtest: test/test_layout_custom_gtest.cpp test/test_unlinked_layout_stubs.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

build/test/test_gif_player_gtest build/test/test_lottie_player_gtest: build/test/%: test/%.cpp test/test_animation_host_stubs.cpp test/test_media_player_stubs.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

# Always ask the owner to refresh its archives before consuming them.
input-deps:
	$(MAKE) -C "$(INPUT_ROOT)" PROFILE="$(PROFILE)"

$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a: | input-deps
	@test -f $@

# These recorder tests provide a controlled vector backend for native archives.
build/test/test_display_list_gtest: test/test_display_list_gtest.cpp test/test_display_list_stubs.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

build/test/test_retained_display_list_gtest: test/test_retained_display_list_gtest.cpp test/test_retained_display_list_stubs.cpp test/test_unlinked_layout_stubs.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

# The state-store slice uses input-owned test utility providers.
$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-test-utils.a: | input-deps
	$(MAKE) -C "$(INPUT_ROOT)" PROFILE="$(PROFILE)" build/lib/$(PROFILE)/liblambda-test-utils.a

build/test/test_state_store_gtest: test/test_state_store_gtest.cpp test/test_state_store_stubs.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-test-utils.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) $(NATIVE_CLOSURE_LIBS) -o $@

# The scheduler test supplies its isolated visibility host.
build/test/test_animation_gtest: test/test_animation_gtest.cpp test/test_animation_host_stubs.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $^ $(GTEST_FLAGS) -lpthread -o $@

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

run-radiant-baseline:
	@radiant_test_rc=0; \
	$(MAKE) --no-print-directory test-native || radiant_test_rc=1; \
	$(MAKE) --no-print-directory test-layout-baseline || radiant_test_rc=1; \
	exit $$radiant_test_rc


# The executable owns the optional scripting host; the Radiant archive remains runtime-free.
LAMBDA_ROOT ?= ../Lambda
LAMBDA_HOST ?= $(LAMBDA_ROOT)/lambda.exe
RADIANT_EXE ?= ./radiant.exe
ifeq ($(abspath $(RADIANT_EXE)),$(abspath ./radiant.exe))
LAYOUT_EXE_DEP = ./radiant.exe
endif
LAMBDA_RUNTIME_LIB = $(LAMBDA_ROOT)/build/lib/native/$(PROFILE)/liblambda-rt-cpp.a
LAMBDA_ADAPTER_LIB = $(LAMBDA_ROOT)/build/lib/native/$(PROFILE)/liblambda-radiant-adapter-cpp.a
LAMBDA_MIR_LIB = $(LAMBDA_ROOT)/lambda/mir/libmir.a
LAMBDA_RUNTIME_MAKE_CONFIG ?= $(PROFILE)_native
LAYOUT_BASELINE_SUITES ?= baseline baseline-js
LAYOUT_BASELINE_CONCURRENCY ?= 4
LAYOUT_BASELINE_TOLERANCE ?= 5
.PHONY: radiant-host-deps
radiant-host-deps:
	@test -f "$(LAMBDA_ROOT)/build/premake/lambda-rt-cpp.make" && test -f "$(LAMBDA_ROOT)/build/premake/lambda-radiant-adapter-cpp.make" || { echo "Configure the Lambda split build under $(LAMBDA_ROOT)/build/premake first"; exit 1; }
	$(MAKE) -C "$(LAMBDA_ROOT)/build/premake" -f lambda-rt-cpp.make config="$(LAMBDA_RUNTIME_MAKE_CONFIG)" LDDEPS=
	$(MAKE) -C "$(LAMBDA_ROOT)/build/premake" -f lambda-radiant-adapter-cpp.make config="$(LAMBDA_RUNTIME_MAKE_CONFIG)" LDDEPS=

$(LAMBDA_RUNTIME_LIB) $(LAMBDA_ADAPTER_LIB): | radiant-host-deps
	@test -f "$@"

$(LAMBDA_MIR_LIB):
	$(MAKE) -C "$(LAMBDA_ROOT)" lambda/mir/libmir.a

# Relink on every explicit host build: PROFILE can change while the public
# executable path stays the same, and host archive refreshes must be consumed.
./radiant.exe: radiant/main.cpp build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(LAMBDA_RUNTIME_LIB) $(LAMBDA_ADAPTER_LIB) $(LAMBDA_MIR_LIB) radiant-host-deps
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(DEFINES) $(CXXFLAGS) -std=c++17 $< \
	  $(LAMBDA_RUNTIME_LIB) $(LAMBDA_ADAPTER_LIB) build/lib/$(PROFILE)/libradiant.a \
	  $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a \
	  $(LAMBDA_RUNTIME_LIB) $(LAMBDA_ADAPTER_LIB) $(LAMBDA_MIR_LIB) $(NATIVE_CLOSURE_LIBS) -o $@
.PHONY: test-navigation-cli
test-navigation-cli: $(LAYOUT_EXE_DEP)
	$(PYTHON) test/test_navigation_cli.py --host "$(LAMBDA_HOST)" --layout-host "$(RADIANT_EXE)" --lambda-root "$(LAMBDA_ROOT)"

test-native: check-input-facade $(NATIVE_TEST_BINS)
	@mkdir -p temp
	@rc=0; for test_bin in $(NATIVE_TEST_BINS); do ./$$test_bin || rc=1; done; exit $$rc

# Exercise the shared non-dynamic HTML corpus without a Lambda script host.
test-layout-smoke: check-input-facade build/test/test_layout_baseline
	@./build/test/test_layout_baseline

# Reuse the existing corpus runner for geometry, reference scoring and scheduling.
test-layout-baseline: $(LAYOUT_EXE_DEP)
	@test -x "$(abspath $(RADIANT_EXE))" || { echo "Layout executable missing: $(abspath $(RADIANT_EXE))"; exit 1; }
	@layout_test_rc=0; \
	LAMBDA_HOME="$(abspath $(LAMBDA_ROOT))/lmd" RADIANT_TEST_EXE="$(abspath $(RADIANT_EXE))" \
	$(NODE) --test test/test_layout_runner.test.js || layout_test_rc=1; \
	$(MAKE) --no-print-directory test-layout-smoke || layout_test_rc=1; \
	mkdir -p temp/layout-baseline; \
	LAMBDA_HOME="$(abspath $(LAMBDA_ROOT))/lmd" \
	LAMBDA_TEMP_DIR="$(abspath temp/layout-baseline)" \
	LAMBDA_CACHE_DIR="$(abspath temp/layout-baseline)" \
	LAMBDA_LOG_DIR="$(abspath temp/layout-baseline)" \
	$(NODE) test/layout/test_radiant_layout.js --engine lambda-css \
	  --radiant-exe "$(abspath $(RADIANT_EXE))" --host-cwd "$(abspath $(LAMBDA_ROOT))" \
	  --categories "$(LAYOUT_BASELINE_SUITES)" --baseline-only \
	  --tolerance $(LAYOUT_BASELINE_TOLERANCE) \
	  --concurrency $(LAYOUT_BASELINE_CONCURRENCY) --global-concurrency $(LAYOUT_BASELINE_CONCURRENCY) \
	  --batch-size 100 $(ARGS) > temp/layout-baseline/results.log 2>&1 || layout_test_rc=1; \
	cat temp/layout-baseline/results.log; \
	exit $$layout_test_rc

# Force every native archive member into a client without a Lambda runtime.
# The current macOS gate uses the same sibling-built vendor artifacts as the split build.
ifeq ($(shell uname -s),Darwin)
RADIANT_BREW_PREFIX ?= $(shell brew --prefix)
NATIVE_CLOSURE_LIBS ?= $(INPUT_ROOT)/build/deps/utf8proc/lib/libutf8proc.a \
  $(INPUT_ROOT)/build/lib/native/$(PROFILE)/libre2.a $(INPUT_ROOT)/build/deps/curl/lib/libcurl.a \
  $(RADIANT_BREW_PREFIX)/lib/libnghttp2.a $(RADIANT_BREW_PREFIX)/opt/mbedtls@3/lib/libmbedtls.a \
  $(RADIANT_BREW_PREFIX)/opt/mbedtls@3/lib/libmbedx509.a $(RADIANT_BREW_PREFIX)/opt/mbedtls@3/lib/libmbedcrypto.a \
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
  -framework CoreText -framework AppKit -framework Carbon -framework WebKit -liconv -lpthread \
  $(shell PKG_CONFIG_PATH="$(INPUT_ROOT)/build/deps/thorvg/lib/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --libs --static thorvg 2>/dev/null | tr ' ' '\n' | sed -n 's/\.dylib$$/.a/p')

.PHONY: test-native-closure
test-native-closure: check-input-facade build/lib/$(PROFILE)/libradiant.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a $(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a
	@mkdir -p build/test
	$(CXX) $(CPPFLAGS) $(PLATFORM_FLAGS) $(RADIANT_CPPFLAGS) $(CXXFLAGS) -std=c++17 \
	  test/test_native_archive_closure.cpp -o build/test/test_native_archive_closure \
	  -Wl,-force_load,build/lib/$(PROFILE)/libradiant.a \
	  -Wl,-force_load,$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-input.a \
	  -Wl,-force_load,$(INPUT_ROOT)/build/lib/$(PROFILE)/liblambda-lib.a $(NATIVE_CLOSURE_LIBS)
	./build/test/test_native_archive_closure
endif

clean:
	rm -rf build radiant.exe radiant.exe.dSYM

-include $(addsuffix .d,$(NATIVE_OBJECTS))
