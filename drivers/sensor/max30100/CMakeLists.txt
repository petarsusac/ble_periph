zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MAX30100 max30100.c)
