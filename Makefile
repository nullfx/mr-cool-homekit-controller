# MrCool HomeKit controller - build / flash helpers around arduino-cli.
#
#   make setup                     install the ESP32 core + HomeSpan (once)
#   make build                     compile the firmware into build/
#   make flash [PORT=...]          compile and flash over USB
#   make monitor [PORT=...]        open the serial console (115200)
#   make flash SETUP_CODE=31415926 bake a fixed HomeKit setup code into the build
#   make flash CHIP=esp32           build for an original ESP32 (NodeMCU-32S, DevKit)
#
# Run `make help` for everything.

SKETCH      := firmware/mrcool_homekit
BUILD       := build
BIN         := $(BUILD)/mrcool_homekit.ino.bin

# Target chip: esp32c3 (default), esp32 (NodeMCU-32S / DevKit), esp32c5,
# esp32c6, esp32s3. See docs/wiring.md for per-chip pins.
# Native-USB chips get "USB CDC On Boot" so the USB socket is the console
# when plugged into a computer (and can be the AC link otherwise).
CHIP        ?= esp32c3
BOARD       ?= esp32:esp32:$(CHIP)
ifeq ($(CHIP),esp32)
BOARD_OPTS  ?= PartitionScheme=min_spiffs,UploadSpeed=921600
else ifeq ($(CHIP),esp32s3)
BOARD_OPTS  ?= PartitionScheme=min_spiffs,UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc
else
BOARD_OPTS  ?= PartitionScheme=min_spiffs,UploadSpeed=921600,CDCOnBoot=cdc
endif
FQBN        := $(BOARD):$(BOARD_OPTS)

ESP32_CORE_VERSION ?= 3.3.6
HOMESPAN_VERSION   ?= 2.1.8
ESP32_INDEX_URL    := https://espressif.github.io/arduino-esp32/package_esp32_index.json

# arduino-cli from PATH (brew install arduino-cli), else the one bundled with Arduino IDE 2.
ARDUINO_CLI ?= $(shell command -v arduino-cli 2>/dev/null || \
  ls "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" 2>/dev/null)

# The ESP32 core, toolchains and HomeSpan are installed inside the project
# (.arduino/), so builds don't depend on (or change) your Arduino IDE setup.
# Point ARDUINO_DATA_DIR at ~/Library/Arduino15 to reuse an existing core.
ARDUINO_DATA_DIR ?= $(CURDIR)/.arduino/data
CLI = ARDUINO_DIRECTORIES_USER="$(CURDIR)/.arduino" \
  ARDUINO_DIRECTORIES_DATA="$(ARDUINO_DATA_DIR)" \
  ARDUINO_DIRECTORIES_DOWNLOADS="$(ARDUINO_DATA_DIR)/staging" \
  ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS="$(ESP32_INDEX_URL)" \
  "$(ARDUINO_CLI)"

# First USB-serial port found: native USB (C3/C6/S3) shows up as usbmodem,
# CP210x / CH34x / FTDI adapters (NodeMCU-32S, DevKit) as usbserial.
ifeq ($(CHIP),esp32)
PORT ?= $(shell ls /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial* \
  /dev/ttyUSB* 2>/dev/null | head -n 1)
else
PORT ?= $(shell ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null | head -n 1)
endif

# Optional fixed HomeKit setup code (8 digits, dashes allowed).
SETUP_CODE ?=
SETUP_DIGITS := $(subst -,,$(SETUP_CODE))
ifneq ($(SETUP_DIGITS),)
EXTRA_PROPS := --build-property "compiler.cpp.extra_flags=-DHOMEKIT_SETUP_CODE_NUM=$(SETUP_DIGITS)"
endif

# OTA updates
HOST         ?=
OTA_PASSWORD ?= homespan-ota
ESPOTA        = $(ARDUINO_DATA_DIR)/packages/esp32/hardware/esp32/$(ESP32_CORE_VERSION)/tools/espota.py
ESPTOOL       = $(lastword $(sort $(wildcard $(ARDUINO_DATA_DIR)/packages/esp32/tools/esptool_py/*/esptool)))

SETUP_ID ?= MRCL
CODE     ?=

.PHONY: help setup build flash upload monitor flash-monitor ports erase ota qr clean check-cli check-port

help:
	@echo "MrCool HomeKit controller"
	@echo ""
	@echo "  make setup                      install ESP32 core $(ESP32_CORE_VERSION) + HomeSpan $(HOMESPAN_VERSION)"
	@echo "  make build                      compile to $(BIN)"
	@echo "  make flash   [PORT=/dev/cu.X]   compile and flash over USB"
	@echo "  make upload  [PORT=/dev/cu.X]   flash the last build without recompiling"
	@echo "  make monitor [PORT=/dev/cu.X]   serial console at 115200 (Ctrl-C to quit)"
	@echo "  make flash-monitor              flash, then open the console"
	@echo "  make ports                      list connected boards"
	@echo "  make erase   [PORT=/dev/cu.X]   wipe flash: Wi-Fi, pairings and setup code"
	@echo "  make ota HOST=<ip|name.local>   update over Wi-Fi [OTA_PASSWORD=...]"
	@echo "  make qr CODE=<8 digits>         draw the HomeKit pairing QR code [SETUP_ID=$(SETUP_ID)]"
	@echo ""
	@echo "  Options: CHIP=$(CHIP) (esp32 for NodeMCU-32S)  SETUP_CODE=31415926 (fixed setup code)"
	@echo "  Detected port: $(if $(PORT),$(PORT),none)"

check-cli:
	@test -n "$(ARDUINO_CLI)" || { echo "arduino-cli not found. Install it: brew install arduino-cli"; exit 1; }

check-port:
	@test -n "$(PORT)" || { echo "No serial port found. Plug the ESP32 in, run 'make ports', then pass PORT=/dev/cu.xxx"; exit 1; }

setup: check-cli
	$(CLI) core update-index
	$(CLI) core install esp32:esp32@$(ESP32_CORE_VERSION)
	$(CLI) lib install HomeSpan@$(HOMESPAN_VERSION)
	rm -rf "$(ARDUINO_DATA_DIR)/staging"   # downloaded archives, ~1.6 GB
	@echo "Setup complete. Next: make flash"

build: check-cli
	@test -d .arduino/libraries/HomeSpan -a -d "$(ARDUINO_DATA_DIR)/packages/esp32" || \
	  { echo "ESP32 core or HomeSpan missing - run 'make setup' first"; exit 1; }
	$(if $(SETUP_DIGITS),@echo "Baking in fixed HomeKit setup code $(SETUP_CODE)")
	$(CLI) compile --fqbn $(FQBN) --library "$(CURDIR)/lib/MideaUART" \
	  --build-path "$(CURDIR)/$(BUILD)/work" --output-dir "$(CURDIR)/$(BUILD)" \
	  --warnings default $(EXTRA_PROPS) $(SKETCH)

# Boards enter the bootloader automatically (DTR/RTS, or the C3's built-in
# USB). If an upload hangs at "Connecting...", hold BOOT, tap RST (or replug
# USB while holding BOOT), release BOOT, then run `make upload`.
upload: check-cli check-port
	$(CLI) upload --fqbn $(FQBN) --port $(PORT) --input-dir "$(CURDIR)/$(BUILD)" $(SKETCH)

flash: build upload

monitor: check-cli check-port
	$(CLI) monitor --port $(PORT) --config baudrate=115200

flash-monitor: flash monitor

ports: check-cli
	$(CLI) board list

erase: check-cli check-port
	@test -n "$(ESPTOOL)" || { echo "esptool not found - run 'make setup'"; exit 1; }
	"$(ESPTOOL)" --port $(PORT) erase-flash

ota: check-cli
	@test -n "$(HOST)" || { echo "Usage: make ota HOST=192.168.1.50 (or MrCool-AC-xxxx.local) [OTA_PASSWORD=...]"; exit 1; }
	@test -f "$(BIN)" || { echo "No firmware built yet - run 'make build' first"; exit 1; }
	python3 "$(ESPOTA)" -r -i $(HOST) -p 3232 --auth="$(OTA_PASSWORD)" -f "$(BIN)"

qr:
	@test -n "$(CODE)" || { echo "Usage: make qr CODE=31415926 [SETUP_ID=$(SETUP_ID)]"; exit 1; }
	@python3 -I tools/homekit_qr.py $(CODE) $(SETUP_ID)

clean:
	rm -rf $(BUILD)
