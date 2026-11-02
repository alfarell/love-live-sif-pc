#pragma once
// SIF `.fad` 2D animations (ui/ui_ingame.pac/anim.pac: judgement texts, ring break, LIVE CLEAR; spec in
// docs/formats/fad.md). A Flash-like timeline on a 400 x 400 canvas: textures, one-frame sprites that each place one
// textured rectangle, and a main timeline that places sprites at depths and changes their matrix and colour per frame.

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace sif::fad {

// 2D affine, y down (Flash order): x' = a x + c y + tx, y' = b x + d y + ty
struct Matrix {
    float a = 1, d = 1, b = 0, c = 0, tx = 0, ty = 0;
};
Matrix operator*(const Matrix& outer, const Matrix& inner);  // inner first

struct Colour {
    std::array<float, 4> mul{1, 1, 1, 1};  // RGBA multipliers
    std::array<float, 4> add{0, 0, 0, 0};  // RGBA added after the multiply, 0..1
};

// A sprite: the textured rectangle (0,0)-(width,height) of `texture` under `matrix` (the sprite's own placement,
// e.g. centring it on its origin)
struct Sprite {
    std::string texture;
    float width = 0, height = 0;
    Matrix matrix;
};

struct Placed {
    int sprite = 0;
    Matrix matrix;
    Colour colour;
};

struct Animation {
    float width = 0, height = 0;  // canvas
    int fps = 60;
    std::map<int, Sprite> sprites;                 // by character id
    std::vector<std::map<int, Placed>> frames;     // main timeline: per frame, depth -> what is shown
};

bool Parse(std::span<const uint8_t> file, Animation& out, std::string* error = nullptr);

}
