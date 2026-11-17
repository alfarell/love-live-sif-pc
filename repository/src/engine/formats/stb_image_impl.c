// The stb_image implementation (third_party/stb, public domain / MIT), compiled once without project warnings and
// with only its JPEG decoder (the NPC profile cards' pictures, proca_preset/*.jpg; M12 P7). No file access: the port
// decodes from memory read through the Vfs.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"
