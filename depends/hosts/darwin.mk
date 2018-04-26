LD64_VERSION=253.9

OSX_MIN_VERSION=10.8
OSX_SDK_VERSION=10.11
OSX_SDK=$(SDK_PATH)/MacOSX$(OSX_SDK_VERSION).sdk

IOS_MIN_VERSION=7.0
IOS_SDK_VERSION=9.3
IOS_SDK=$(SDK_PATH)/iPhoneOS$(IOS_SDK_VERSION).sdk

darwin_CC=clang -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION)
darwin_CXX=clang++ -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION) -stdlib=libc++

armv7_darwin_CC=clang -target $(host) -arch armv7 -miphoneos-version-min=$(IOS_MIN_VERSION) --sysroot $(IOS_SDK) -mlinker-version=$(LD64_VERSION)
armv7_darwin_CXX=clang++ -target $(host) -arch armv7 -miphoneos-version-min=$(IOS_MIN_VERSION) --sysroot $(IOS_SDK) -mlinker-version=$(LD64_VERSION) -stdlib=libc++

aarch64_darwin_CC=clang -target $(host) -arch arm64 -miphoneos-version-min=$(IOS_MIN_VERSION) --sysroot $(IOS_SDK) -mlinker-version=$(LD64_VERSION)
aarch64_darwin_CXX=clang++ -target $(host) -arch arm64 -miphoneos-version-min=$(IOS_MIN_VERSION) --sysroot $(IOS_SDK) -mlinker-version=$(LD64_VERSION) -stdlib=libc++

darwin_CFLAGS=-pipe
darwin_CXXFLAGS=$(darwin_CFLAGS)

darwin_release_CFLAGS=-O2
darwin_release_CXXFLAGS=$(darwin_release_CFLAGS)

darwin_debug_CFLAGS=-O1
darwin_debug_CXXFLAGS=$(darwin_debug_CFLAGS)

darwin_native_toolchain=native_cctools
