new_local_repository(
    name = "homebrew_opencv",
    path = "/opt/homebrew/opt/opencv@4",
    build_file_content = """
load("@rules_cc//cc:defs.bzl", "cc_library")
cc_library(
    name = "opencv",
    hdrs = glob(["include/opencv4/**"]),
    includes = ["include/opencv4"],
    linkopts = [
        "-L/opt/homebrew/opt/opencv@4/lib",
        "-lopencv_core",
        "-lopencv_imgproc",
        "-lopencv_imgcodecs",
        "-Wl,-rpath,/opt/homebrew/opt/opencv@4/lib",
    ],
    visibility = ["//visibility:public"],
)
""",
)

new_local_repository(
    name = "homebrew_tesseract",
    path = "/opt/homebrew/opt/tesseract",
    build_file_content = """
load("@rules_cc//cc:defs.bzl", "cc_library")
cc_library(
    name = "tesseract",
    hdrs = glob(["include/**"]),
    includes = ["include"],
    linkopts = [
        "-L/opt/homebrew/opt/tesseract/lib",
        "-ltesseract",
        "-Wl,-rpath,/opt/homebrew/opt/tesseract/lib",
    ],
    visibility = ["//visibility:public"],
)
""",
)
