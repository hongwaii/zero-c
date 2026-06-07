# ============================================================
#  midware — Application-level middleware modules
# ============================================================
# Each subdirectory is an independent, self-contained module.
# Modules can depend on lib/ and third_party/ but should be
# decoupled from each other when possible.
#
# HOW TO ADD A NEW MODULE:
#   1. Create a subdirectory: midware/<module_name>/
#   2. Add CMakeLists.txt inside it (copy the template below)
#   3. add_subdirectory(<module_name>) here
# ============================================================

# ---- HTTP client/server middleware ----
add_subdirectory(http)

# ---- Serial (COM port) channel middleware ----
add_subdirectory(serial)

# TODO: Add new modules here, e.g.:
# add_subdirectory(mqtt)
# add_subdirectory(websocket)
# add_subdirectory(modbus)
