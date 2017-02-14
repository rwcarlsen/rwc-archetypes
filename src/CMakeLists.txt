### DO NOT DELETE THIS COMMENT: INSERT_ARCHETYPES_HERE ###
CONFIGURE_FILE(rwc_version.h.in "${CMAKE_CURRENT_SOURCE_DIR}/rwc_version.h" @ONLY)
SET(CYCLUS_CUSTOM_HEADERS "rwc_version.h")

USE_CYCLUS("rwc" "storage")
USE_CYCLUS("rwc" "fleet_reactor")
USE_CYCLUS("rwc" "pattern_sink")

INSTALL_CYCLUS_MODULE("rwc" "./")

# install header files
FILE(GLOB h_files "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${h_files} DESTINATION include/rwc COMPONENT rwc)
