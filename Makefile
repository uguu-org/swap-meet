# Toplevel Makefile for Swap Meet project.
#
# This Makefile only needs common tools that are available through Cygwin.
#
# See source/Makefile and data/Makefile for additional tools needed to build
# code and data.
#
# To build release packages:
#
#   make clean && make -j
#
# To rebuild data files and copy them to source directory:
#
#   make -j refresh_data

ifeq ($(PLAYDATE_SDK_PATH),)
$(error need to set PLAYDATE_SDK_PATH environment)
endif

PACKAGE_NAME = swap_meet
DEVICE_SOURCE = device_build_source

all: $(PACKAGE_NAME).zip

# Build rules for hybrid device+simulator package.
$(PACKAGE_NAME).zip: $(PACKAGE_NAME).pdx
	rm -f $@
	zip -9 -r $@ $<

$(PACKAGE_NAME).pdx: device_source device_launcher_source $(DEVICE_SOURCE)/main.lua
	"$(PLAYDATE_SDK_PATH)/bin/pdc" -s $(DEVICE_SOURCE) $@

$(DEVICE_SOURCE)/main.lua: source/main.lua source/inline_constants.pl source/strip_lua.pl | make_device_dir
	perl source/inline_constants.pl $< | perl source/strip_lua.pl > $@

device_source: source/device_build/pdex.elf source/sim_build/pdex.dll source/pdxinfo source/images/* source/sounds/* | make_device_dir
	cp $^ $(DEVICE_SOURCE)/

device_launcher_source: source/launcher/* | make_device_dir
	cp -R $^ $(DEVICE_SOURCE)/launcher/
	cp source/images/title.png $(DEVICE_SOURCE)/launcher/launchImage.png
	cp source/images/title.png $(DEVICE_SOURCE)/launcher/launchImages/1.png

make_device_dir:
	mkdir -p $(DEVICE_SOURCE)/launcher/launchImages

# Binary build rules.
source/sim_build/pdex.dll: | build_source

source/device_build/pdex.elf: | build_source

build_source:
	$(MAKE) -C source

# Refresh data files.
refresh_data:
	$(MAKE) -C data
	cp data/build/pd_player.png source/images/player-table-80-48.png
	cp data/build/pd_goal.png source/images/goal.png
	cp data/build/pd_items.png source/images/items-table-48-48.png
	cp data/build/pd_small_items.png source/images/small_items-table-16-16.png
	cp data/build/pd_floor.png source/images/floor-table-32-32.png
	cp data/build/pd_landmarks.png source/images/landmarks-table-192-192.png
	cp data/build/pd_fireworks.png source/images/fireworks-table-100-100.png
	cp data/build/pd_swap.png source/images/swap-table-256-112.png
	cp data/build/pd_direction.png source/images/direction-table-72-72.png
	cp data/build/pd_title.png source/images/title.png
	cp data/build/pd_card.png source/launcher/card.png
	cp data/build/pd_icon.png source/launcher/icon.png
	cp data/build/pd_icon_highlighted.png source/launcher/icon-highlighted/1.png
	cp data/build/pd_gnossienne.c source/gnossienne.c
	cp data/build/guitar1.wav source/sounds/m11.wav
	cp data/build/guitar2.wav source/sounds/m12.wav
	cp data/build/guitar3.wav source/sounds/m13.wav
	cp data/build/guitar4.wav source/sounds/m14.wav
	cp data/build/guitar5.wav source/sounds/m15.wav
	cp data/build/guitar6.wav source/sounds/m16.wav
	cp data/build/itch_cover.png doc/itch_cover.png

# Maintenance rules.
clean:
	$(MAKE) -C data clean
	$(MAKE) -C source clean
	rm -rf $(DEVICE_SOURCE)
	rm -rf $(PACKAGE_NAME).{pdx,zip}
