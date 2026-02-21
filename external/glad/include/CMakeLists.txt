target_sources(
    glad

    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "."
    FILES
        "KHR/khrplatform.h"
        "glad/glad.h"
)
