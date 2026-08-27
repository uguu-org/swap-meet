# Build images, sounds and other supporting data.  Takes ~2 minutes to build
# and costs ~20M of disk space.
#
# Requires gcc, perl, ruby, netpbm, ImageMagick, Inkscape, and ffmpeg.
# gcc also needs libpng, and perl needs libxml.
#
# See svg_to_png.sh for more details on Inkscape.
#
# The reason why we have this separate "data" directory is due to all the
# extra software needed to build it.  The usual workflow is to refresh data
# only once in a while and keep the built output in the "source" directory.
# This means code changes only need commonly available compiler tools.


BUILD_DIR = build
TARGETS = \
	$(BUILD_DIR)/pd_player.png \
	$(BUILD_DIR)/pd_goal.png \
	$(BUILD_DIR)/pd_items.png \
	$(BUILD_DIR)/pd_small_items.png \
	$(BUILD_DIR)/pd_floor.png \
	$(BUILD_DIR)/pd_landmarks.png \
	$(BUILD_DIR)/pd_fireworks.png \
	$(BUILD_DIR)/pd_swap.png \
	$(BUILD_DIR)/pd_direction.png \
	$(BUILD_DIR)/pd_title.png \
	$(BUILD_DIR)/pd_card.png \
	$(BUILD_DIR)/pd_icon.png \
	$(BUILD_DIR)/pd_icon_highlighted.png \
	$(BUILD_DIR)/pd_gnossienne.c \
	$(BUILD_DIR)/itch_cover.png \
	$(BUILD_DIR)/guitar1.wav \
	$(BUILD_DIR)/guitar2.wav \
	$(BUILD_DIR)/guitar3.wav \
	$(BUILD_DIR)/guitar4.wav \
	$(BUILD_DIR)/guitar5.wav \
	$(BUILD_DIR)/guitar6.wav

# Compiler settings.
CC = gcc
CXX = g++
CFLAGS = -march=native -O3 -Wall -Wextra -pedantic

# Sound encoding options.
FFMPEG = ffmpeg -loglevel fatal
SOUND_OPTIONS = -acodec pcm_s16le -ar 44100

# Render resolution for tree leaves.
#
# Trees were recycled from Old Pond project:
# https://github.com/uguu-org/old-pond/blob/master/data/world.svg
#
# But the size of the trees didn't match the size of sprites for this
# project, so they are rendered at a higher resolution (instead of the
# default 96dpi) to make them a bit bigger.
TREE_DPI=144

# ......................................................................
# {{{ Primary build artifacts.

all: $(TARGETS)

# Images.
$(BUILD_DIR)/pd_player.png: $(BUILD_DIR)/pd_t_player.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_goal.png: $(BUILD_DIR)/pd_t_goal.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_items.png: $(BUILD_DIR)/pd_t_items.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_small_items.png: $(BUILD_DIR)/pd_t_small_items.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_floor.png: $(BUILD_DIR)/pd_t_floor.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_landmarks.png: $(BUILD_DIR)/pd_t_landmarks.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_fireworks.png: $(BUILD_DIR)/pd_t_fireworks.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_swap.png: $(BUILD_DIR)/pd_t_swap.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_direction.png: $(BUILD_DIR)/pd_t_direction.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_title.png: $(BUILD_DIR)/pd_t_title.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_card.png: $(BUILD_DIR)/pd_t_card.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_icon.png: $(BUILD_DIR)/pd_t_icon.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/pd_icon_highlighted.png: $(BUILD_DIR)/pd_t_icon_highlighted.png optimize_png.pl
	perl optimize_png.pl $< > $@

$(BUILD_DIR)/itch_cover.png: $(BUILD_DIR)/t_itch_cover.png optimize_png.pl
	perl optimize_png.pl $< > $@

# Song data.
$(BUILD_DIR)/pd_gnossienne.c: gnossienne.m4 convert_tab_to_c.pl force_max_velocity.pl | make_build_dir
	m4 $< | perl convert_tab_to_c.pl g_gnossienne | perl force_max_velocity.pl > $@

# Sounds.
$(BUILD_DIR)/guitar1.wav: sounds/guitar.wav | make_build_dir
	$(FFMPEG) -ss 0:20 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar2.wav: sounds/guitar.wav | make_build_dir
	$(FFMPEG) -ss 0:16 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar3.wav: sounds/guitar.wav | make_build_dir
	$(FFMPEG) -ss 0:12 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar4.wav: sounds/guitar.wav | make_build_dir
	$(FFMPEG) -ss 0:08 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar5.wav: sounds/guitar.wav | make_build_dir
	$(FFMPEG) -ss 0:04 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

$(BUILD_DIR)/guitar6.wav: sounds/guitar.wav | make_build_dir
	$(FFMPEG) -ss 0:00 -i $< -t 0:02.10 $(SOUND_OPTIONS) -y $@

# }}}

# ......................................................................
# {{{ Intermediate files.

# Sprites.
$(BUILD_DIR)/pd_t_player.png: $(BUILD_DIR)/pd_t_world.png $(BUILD_DIR)/crop_table.exe
	magick $< +repage -crop "80x256+256+1344" png:- | $(BUILD_DIR)/crop_table.exe 80 64 80 48 0 0 > $@

$(BUILD_DIR)/pd_t_goal.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '96x96+720+544' $@

$(BUILD_DIR)/pd_t_items.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '192x1008+512+592' $@

$(BUILD_DIR)/pd_t_small_items.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '16x336+768+1280' $@

$(BUILD_DIR)/pd_t_floor.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '256x160+768+928' $@

$(BUILD_DIR)/pd_t_landmarks.png: $(BUILD_DIR)/pd_t_trees_gray.png $(BUILD_DIR)/pd_t_extra_landmarks_gray.png $(BUILD_DIR)/tile_dither.exe
	magick $(BUILD_DIR)/pd_t_trees_gray.png $(BUILD_DIR)/pd_t_extra_landmarks_gray.png +append png:- | $(BUILD_DIR)/tile_dither.exe - $@ 192 192

$(BUILD_DIR)/pd_t_swap.png: $(BUILD_DIR)/pd_t_world_ordered.png $(BUILD_DIR)/crop_table.exe
	magick $< +repage -crop '256x256+768+656' png:- | $(BUILD_DIR)/crop_table.exe 256 128 256 112 0 0 > $@

$(BUILD_DIR)/pd_t_direction.png: $(BUILD_DIR)/pd_t_world_ordered.png $(BUILD_DIR)/crop_table.exe
	magick $< +repage -crop '160x160+824+1288' png:- | $(BUILD_DIR)/crop_table.exe 80 80 72 72 4 4 > $@

$(BUILD_DIR)/pd_t_title.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '400x240+0+0' $@

$(BUILD_DIR)/pd_t_card.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '350x155+25+43' $@

$(BUILD_DIR)/pd_t_icon.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '32x32+1024+592' $@

$(BUILD_DIR)/pd_t_icon_highlighted.png: $(BUILD_DIR)/pd_t_world.png
	magick $< +repage -crop '32x32+1056+592' $@

$(BUILD_DIR)/pd_t_world.png: $(BUILD_DIR)/pd_t_world_gray.png $(BUILD_DIR)/fs_dither.exe
	$(BUILD_DIR)/fs_dither.exe $< $@

$(BUILD_DIR)/pd_t_world_ordered.png: $(BUILD_DIR)/pd_t_world_gray.png $(BUILD_DIR)/bayer_dither.exe
	$(BUILD_DIR)/bayer_dither.exe $< $@

$(BUILD_DIR)/pd_t_world_gray.png: $(BUILD_DIR)/pd_t_world.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/pd_t_world.svg: world.svg select_layers.pl remove_unused_defs.pl | make_build_dir
	perl select_layers.pl '^world.*' pd_t_world_gray.png $< | perl remove_unused_defs.pl > $@

$(BUILD_DIR)/pd_t_trees.svg: trees.svg select_layers.pl remove_unused_defs.pl | make_build_dir
	perl select_layers.pl '^world.*' t_trees.png $< | perl remove_unused_defs.pl > $@

$(BUILD_DIR)/pd_t_trees0.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 0 $< > $@

$(BUILD_DIR)/pd_t_trees1.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 1 $< > $@

$(BUILD_DIR)/pd_t_trees2.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 2 $< > $@

$(BUILD_DIR)/pd_t_trees3.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 3 $< > $@

$(BUILD_DIR)/pd_t_trees4.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 4 $< > $@

$(BUILD_DIR)/pd_t_trees5.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 5 $< > $@

$(BUILD_DIR)/pd_t_trees6.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 6 $< > $@

$(BUILD_DIR)/pd_t_trees7.svg: $(BUILD_DIR)/pd_t_trees.svg perturb_leaves.pl
	perl perturb_leaves.pl 7 $< > $@

$(BUILD_DIR)/pd_t_trees0_gray.png: $(BUILD_DIR)/pd_t_trees0.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees1_gray.png: $(BUILD_DIR)/pd_t_trees1.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees2_gray.png: $(BUILD_DIR)/pd_t_trees2.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees3_gray.png: $(BUILD_DIR)/pd_t_trees3.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees4_gray.png: $(BUILD_DIR)/pd_t_trees4.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees5_gray.png: $(BUILD_DIR)/pd_t_trees5.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees6_gray.png: $(BUILD_DIR)/pd_t_trees6.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees7_gray.png: $(BUILD_DIR)/pd_t_trees7.svg svg_to_png.sh
	bash svg_to_png.sh $< $@ 0 256 1024 384 $(TREE_DPI)

$(BUILD_DIR)/pd_t_trees_gray.png: $(BUILD_DIR)/pd_t_trees0_gray.png $(BUILD_DIR)/pd_t_trees1_gray.png $(BUILD_DIR)/pd_t_trees2_gray.png $(BUILD_DIR)/pd_t_trees3_gray.png $(BUILD_DIR)/pd_t_trees4_gray.png $(BUILD_DIR)/pd_t_trees5_gray.png $(BUILD_DIR)/pd_t_trees6_gray.png $(BUILD_DIR)/pd_t_trees7_gray.png
	magick $^ -append $@

$(BUILD_DIR)/pd_t_landmarks.svg: landmarks.svg select_layers.pl remove_unused_defs.pl | make_build_dir
	perl select_layers.pl '^world.*' t_landmarks.png $< | perl remove_unused_defs.pl > $@

$(BUILD_DIR)/pd_t_extra_landmarks_gray.png: $(BUILD_DIR)/pd_t_landmarks.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/pd_t_fireworks.svg: generate_fireworks.pl | make_build_dir
	perl $< > $@

$(BUILD_DIR)/pd_t_fireworks_gray.png: $(BUILD_DIR)/pd_t_fireworks.svg svg_to_png.sh
	bash svg_to_png.sh $< $@

$(BUILD_DIR)/pd_t_fireworks.png: $(BUILD_DIR)/pd_t_fireworks_gray.png $(BUILD_DIR)/fs_dither.exe
	$(BUILD_DIR)/fs_dither.exe $< $@

# Itch cover is generated by selecting a subset of layers and rasterizing
# that with anti-aliasing enabled.
$(BUILD_DIR)/t_itch_cover.png: $(BUILD_DIR)/t_itch_cover.svg $(BUILD_DIR)/aa_svg_to_png.sh
	bash $(BUILD_DIR)/aa_svg_to_png.sh $< $@ 512 0 1142 500

$(BUILD_DIR)/t_itch_cover.svg: world.svg select_layers.pl remove_unused_defs.pl | make_build_dir
	perl select_layers.pl '^world.*' t_itch_cover_gray.png $< | perl remove_unused_defs.pl > $@

# }}}

# ......................................................................
# {{{ Tools.

# There are various tools here that we don't use.  Basically we just
# copy whole directory whenever we need to create a new project.

# Convert grayscale PNG to black and white.
$(BUILD_DIR)/bayer_dither.exe: bayer_dither.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/fs_dither.exe: fs_dither.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/random_dither.exe: random_dither.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/tile_dither.exe: tile_dither.c $(BUILD_DIR)/dither_array_pixel.c $(BUILD_DIR)/dither_array_alpha.c
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/dither_array_pixel.c: $(BUILD_DIR)/generate_symmetric_dither_array.exe
	./$< pixel_pattern 1 > $@

$(BUILD_DIR)/dither_array_alpha.c: $(BUILD_DIR)/generate_symmetric_dither_array.exe
	./$< alpha_pattern 2 > $@

$(BUILD_DIR)/generate_symmetric_dither_array.exe: generate_symmetric_dither_array.cc | make_build_dir
	$(CXX) $(CFLAGS) -DTILE_SIZE=192 $< -o $@

# Reduce the sizes of image tables.
$(BUILD_DIR)/crop_table.exe: crop_table.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/shrink_tiles.exe: shrink_tiles.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Mirror inner tiles in image tables.
$(BUILD_DIR)/h_mirror_table.exe: h_mirror_table.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

$(BUILD_DIR)/v_mirror_table.exe: v_mirror_table.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Composite a series of black and white PNGs together.
$(BUILD_DIR)/stack_bw.exe: stack_bw.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Replace black pixels with transparency.
$(BUILD_DIR)/transparent_black.exe: transparent_black.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Replace black pixels with transparency, preserving edges.
$(BUILD_DIR)/transparent_black_edge.exe: transparent_black_edge.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Generate image table from grayscale image.
$(BUILD_DIR)/generate_table_from_grayscale_image.exe: generate_table_from_grayscale_image.c | make_build_dir
	$(CC) $(CFLAGS) $< -lpng -o $@

# Find unique tiles to reconstruct the input bitmap.
$(BUILD_DIR)/generate_unique_tiles.exe: generate_unique_tiles.cc | make_build_dir
	$(CXX) $(CFLAGS) $< -lpng -o $@

# SVG rasterizer script with anti-aliasing enabled (i.e. without the flag
# to disable anti-aliasing).
$(BUILD_DIR)/aa_svg_to_png.sh: svg_to_png.sh | make_build_dir
	grep -vF "export-png-antialias=0" $< > $@

# }}}

# ......................................................................
# {{{ Tests and maintenance commands.

test: \
	$(BUILD_DIR)/test_passed.bayer_dither \
	$(BUILD_DIR)/test_passed.brighten \
	$(BUILD_DIR)/test_passed.cleanup_styles \
	$(BUILD_DIR)/test_passed.convert_tab_to_c \
	$(BUILD_DIR)/test_passed.crop_table \
	$(BUILD_DIR)/test_passed.element_count \
	$(BUILD_DIR)/test_passed.fs_dither \
	$(BUILD_DIR)/test_passed.generate_table_from_grayscale_image \
	$(BUILD_DIR)/test_passed.generate_unique_tiles \
	$(BUILD_DIR)/test_passed.h_mirror_table \
	$(BUILD_DIR)/test_passed.no_text \
	$(BUILD_DIR)/test_passed.random_dither \
	$(BUILD_DIR)/test_passed.select_layers \
	$(BUILD_DIR)/test_passed.shrink_tiles \
	$(BUILD_DIR)/test_passed.stack_bw \
	$(BUILD_DIR)/test_passed.tile_dither \
	$(BUILD_DIR)/test_passed.transparent_black \
	$(BUILD_DIR)/test_passed.transparent_black_edge \
	$(BUILD_DIR)/test_passed.v_mirror_table

$(BUILD_DIR)/test_passed.remove_unused_defs: remove_unused_defs.pl remove_unused_defs_test.sh | make_build_dir
	bash remove_unused_defs_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.select_layers: select_layers.pl select_layers_test.sh | make_build_dir
	bash select_layers_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.element_count: element_count.pl element_count_test.sh | make_build_dir
	bash element_count_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.cleanup_styles: cleanup_styles.pl cleanup_styles_test.sh | make_build_dir
	bash cleanup_styles_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.brighten: brighten.pl brighten_test.sh | make_build_dir
	bash brighten_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.no_text: $(BUILD_DIR)/pd_t_world.svg element_count.pl
	! ( perl element_count.pl $< | grep '^text' ) && touch $@

$(BUILD_DIR)/test_passed.bayer_dither: $(BUILD_DIR)/bayer_dither.exe bayer_dither_test.sh
	bash bayer_dither_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.fs_dither: $(BUILD_DIR)/fs_dither.exe dither_test.sh
	bash dither_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.random_dither: $(BUILD_DIR)/random_dither.exe dither_test.sh
	bash dither_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.tile_dither: $(BUILD_DIR)/tile_dither.exe tile_dither_test.sh
	bash tile_dither_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.crop_table: $(BUILD_DIR)/crop_table.exe crop_table_test.sh
	bash crop_table_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.shrink_tiles: $(BUILD_DIR)/shrink_tiles.exe shrink_tiles_test.sh
	bash shrink_tiles_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.h_mirror_table: $(BUILD_DIR)/h_mirror_table.exe h_mirror_table_test.sh
	bash h_mirror_table_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.v_mirror_table: $(BUILD_DIR)/v_mirror_table.exe v_mirror_table_test.sh
	bash v_mirror_table_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.stack_bw: $(BUILD_DIR)/stack_bw.exe stack_bw_test.sh
	bash stack_bw_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.transparent_black: $(BUILD_DIR)/transparent_black.exe transparent_black_test.sh
	bash transparent_black_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.transparent_black_edge: $(BUILD_DIR)/transparent_black_edge.exe transparent_black_edge_test.sh
	bash transparent_black_edge_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.generate_table_from_grayscale_image: $(BUILD_DIR)/generate_table_from_grayscale_image.exe generate_table_from_grayscale_image_test.sh
	bash generate_table_from_grayscale_image_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.generate_unique_tiles: $(BUILD_DIR)/generate_unique_tiles.exe generate_unique_tiles_test.sh
	bash generate_unique_tiles_test.sh $< && touch $@

$(BUILD_DIR)/test_passed.convert_tab_to_c: convert_tab_to_c.pl convert_tab_to_c_test.sh | make_build_dir
	bash convert_tab_to_c_test.sh $< && touch $@

make_build_dir: $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -rf $(BUILD_DIR)

# }}}
