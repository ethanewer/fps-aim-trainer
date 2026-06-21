APP_NAME := Aim Trainer
BIN_NAME := aim-trainer
SRC_DIR := src
BUILD_DIR := build
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
DESKTOP_APP := $(HOME)/Desktop/$(APP_NAME).app

UNAME_S := $(shell uname -s)
WINDOWS_S := $(filter MSYS_NT% MINGW% CYGWIN_NT%,$(UNAME_S))
EXE_EXT :=

ifeq ($(UNAME_S),Darwin)
	SDL_PREFIX ?= /opt/homebrew
	CXX := clang++
	CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -DGL_SILENCE_DEPRECATION -I$(SDL_PREFIX)/include
	LDFLAGS := -L$(SDL_PREFIX)/lib -lSDL2 -framework OpenGL -framework Cocoa
else ifneq ($(WINDOWS_S),)
	EXE_EXT := .exe
	CXX ?= g++
	CXXFLAGS := -std=c++17 -O3 -Wall -Wextra $(shell sdl2-config --cflags)
	LDFLAGS := $(shell sdl2-config --libs) -lopengl32
else
	CXX ?= g++
	CXXFLAGS := -std=c++17 -O3 -Wall -Wextra $(shell sdl2-config --cflags)
	LDFLAGS := $(shell sdl2-config --libs) -lGL
endif

CXXFLAGS += -MMD -MP
BIN := $(BUILD_DIR)/$(BIN_NAME)$(EXE_EXT)

.PHONY: all clean app run

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

run: $(BIN)
	./$(BIN)

app: $(BIN)
	rm -rf "$(DESKTOP_APP)"
	mkdir -p "$(DESKTOP_APP)/Contents/MacOS" "$(DESKTOP_APP)/Contents/Frameworks"
	cp "$(BIN)" "$(DESKTOP_APP)/Contents/MacOS/$(BIN_NAME)"
	cp "$(SDL_PREFIX)/lib/libSDL2-2.0.0.dylib" "$(DESKTOP_APP)/Contents/Frameworks/"
	chmod u+w "$(DESKTOP_APP)/Contents/Frameworks/libSDL2-2.0.0.dylib"
	install_name_tool -change "$(SDL_PREFIX)/lib/libSDL2-2.0.0.dylib" "@executable_path/../Frameworks/libSDL2-2.0.0.dylib" "$(DESKTOP_APP)/Contents/MacOS/$(BIN_NAME)" || true
	install_name_tool -change "$(SDL_PREFIX)/opt/sdl2/lib/libSDL2-2.0.0.dylib" "@executable_path/../Frameworks/libSDL2-2.0.0.dylib" "$(DESKTOP_APP)/Contents/MacOS/$(BIN_NAME)" || true
	printf '%s\n' \
	'<?xml version="1.0" encoding="UTF-8"?>' \
	'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
	'<plist version="1.0">' \
	'<dict>' \
	'  <key>CFBundleExecutable</key>' \
	'  <string>$(BIN_NAME)</string>' \
	'  <key>CFBundleIdentifier</key>' \
	'  <string>local.aim-trainer</string>' \
	'  <key>CFBundleName</key>' \
	'  <string>$(APP_NAME)</string>' \
	'  <key>CFBundlePackageType</key>' \
	'  <string>APPL</string>' \
	'  <key>LSMinimumSystemVersion</key>' \
	'  <string>10.15</string>' \
	'</dict>' \
	'</plist>' > "$(DESKTOP_APP)/Contents/Info.plist"
	xattr -cr "$(DESKTOP_APP)"
	codesign --force --deep --sign - "$(DESKTOP_APP)"

clean:
	rm -rf $(BUILD_DIR)
