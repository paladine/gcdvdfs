package(default_visibility = ["//visibility:public"])

cc_library(
  name = "core", 
  defines = [
    "_FILE_OFFSET_BITS=64",
    "FUSE_USE_VERSION=26",
  ],
  srcs = [
    "BinaryReader.cpp",
    "BinaryReader.h",
    "GamecubeFilesystemTable.cpp",
    "GamecubeFilesystemTable.h",
    "GamecubeIsoFilesystem.cpp",
    "GamecubeIsoFilesystem.h",
    "Tokenizer.cpp",
    "Tokenizer.h",
  ],
)

cc_binary(
  name = "gcdvdfs", 
  defines = [
    "_FILE_OFFSET_BITS=64",
    "FUSE_USE_VERSION=26",
  ],
  srcs = [
    "main.cpp",
  ],
  linkopts = [
    "-lfuse",
  ],
  deps = [
    ":core"
  ],
)
