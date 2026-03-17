IMAGE_NAME  := pico2-ili9341-build
CONTAINER   := pico2-build-tmp
BUILD_DIR   := build
TARGET      := pico2_ili9341

.PHONY: all docker-image build clean flash

all: build

# --- Docker image (cached unless Dockerfile changes) ---
docker-image:
	docker build -t $(IMAGE_NAME) .

# --- Build inside container, copy artefacts out ---
build: docker-image
	docker run --rm --name $(CONTAINER) \
		-v "$(CURDIR)":/project \
		$(IMAGE_NAME) \
		bash -c "\
			mkdir -p $(BUILD_DIR) && \
			cd $(BUILD_DIR) && \
			cmake -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s .. && \
			make -j$$(nproc) \
		"
	@echo ""
	@echo ">>> UF2 ready: $(BUILD_DIR)/src/$(TARGET).uf2"

# --- Wipe build artefacts ---
clean:
	rm -rf $(BUILD_DIR)

# --- Copy UF2 to mounted Pico (macOS default mount point) ---
#     Override PICO_MOUNT for Linux: make flash PICO_MOUNT=/media/$(USER)/RPI-RP2
PICO_MOUNT ?= /Volumes/RP2350
flash: build
	@test -d "$(PICO_MOUNT)" || { echo "Pico not mounted at $(PICO_MOUNT)"; exit 1; }
	cp $(BUILD_DIR)/src/$(TARGET).uf2 "$(PICO_MOUNT)/"
	@echo ">>> Flashed to $(PICO_MOUNT)"
