.PHONY: help keyboard check rootfs image

ARCH ?= arm64
FBKEYBOARD ?= $(CURDIR)/fbkeyboard/fbkeyboard

help:
	@echo 'make keyboard                   Build fbkeyboard with the selected toolchain'
	@echo 'make check                      Run keyboard and console tests'
	@echo 'make rootfs ROOTFS_DIR=/mnt/root Install factory console into a staging rootfs'
	@echo 'sudo make image BASE_ROOTFS=/path/rootfs.img BOOT_IMAGE=/path/boot.img OUTPUT_DIR=/path/new-image'

keyboard:
	$(MAKE) -C fbkeyboard

check: keyboard
	$(MAKE) -C fbkeyboard check
	python3 scripts/test_console.py -v
	python3 scripts/test_factory.py -v

rootfs:
	@test -n "$(ROOTFS_DIR)" || { echo 'Set ROOTFS_DIR to a staging rootfs'; exit 1; }
	python3 scripts/install_console_rootfs.py --root "$(ROOTFS_DIR)" --keyboard "$(FBKEYBOARD)" --architecture "$(ARCH)"

image:
	bash scripts/build-console-image.sh --base "$(BASE_ROOTFS)" --boot "$(BOOT_IMAGE)" --output "$(OUTPUT_DIR)"
