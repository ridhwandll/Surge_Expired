add_library(basisu
    "transcoder/basisu_transcoder.cpp"
    "transcoder/basisu_transcoder.h"
    "transcoder/basisu_containers.h"
    "transcoder/basisu_file_headers.h"
    "transcoder/basisu_transcoder_internal.h"

    "zstd/zstd.c"
    "zstd/zstddeclib.c"
)

target_compile_definitions(basisu PRIVATE
    BASISD_SUPPORT_KTX2_ZSTD=1
    BASISD_SUPPORT_PVRTC1=0
    BASISD_SUPPORT_PVRTC2=0
    BASISD_SUPPORT_ATC=0
    BASISD_SUPPORT_FXT1=0
)

set_target_properties(basisu PROPERTIES FOLDER Dependencies)
