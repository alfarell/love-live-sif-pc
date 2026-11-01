# Third-party code

## pt-pc (MIT)

Source: https://github.com/LoreanXavier/pt-pc, commit `8966e89`. Copied files carry a header naming their source path.

- `src/engine/core/log.*`, `crash_report.*`, `parallel_for.*`, `src/engine/platform/os.*` (renamed: namespace `pt` → `sif`, P.T. strings replaced)
- `src/engine/platform/settings.cpp`: the INI helpers `Trim`, `Parse`, `Read` only (the settings struct and keys are SIF's own)
- `tools/self2elf.py`, `tools/dynlib.py` (extended), `tools/gcn.py`

```
MIT License

Copyright (c) 2026 LoreanXavier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## clHCA (ISC-style, from vgmstream)

Source: https://github.com/vgmstream/vgmstream `src/coding/libs/clhca.{c,h}`, `clhca_data.h`; latest commit touching `clhca.c` is `8ebeacb2fc38c8141e230c4ae39a1506998b1b36` (2026-08-28), fetched 2026-10-31. Copied unchanged to `third_party/clhca/` with vgmstream's `COPYING` (the license text and copyright holders). HCA decoder for the CRI audio path (M2 `sif_hca2wav`, M3 engine). Key support in the library is not used: SIF's HCA streams are unencrypted.

## LZMA SDK decoder (public domain)

Source: https://github.com/ip7z/7zip (Igor Pavlov's 7-Zip/LZMA SDK mirror) `C/LzmaDec.{c,h}`, `C/7zTypes.h`, `C/Precomp.h`, `C/Compiler.h`; latest commit touching `LzmaDec.c` is `0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17` (2026-09-04), fetched 2026-10-31. Copied unchanged to `third_party/lzma/`. Each file states "Igor Pavlov : Public domain". Decodes the two `cmp\0lzma` archives (`common/common.pac`, `common2.pac`).

## stb_truetype (public domain or MIT)

Source: https://github.com/nothings/stb `stb_truetype.h` v1.26; latest commit touching it is `6e9f34d5429cf16790ec43c9bac3f1ee4ad1f760` (2024-07-15), fetched 2026-11-01 (git blob `90a5c2e2`). Copied unchanged to `third_party/stb/`. The file ends with its dual licence (MIT or Unlicense), Sean Barrett / RAD Game Tools. Rasterizes the game's TrueType UI fonts (`common/common.pac/font.pac`; chosen by the user over FreeType and SDL3_ttf, M6).

## Fetched at build time

| Library | Version | License | Use |
|---|---|---|---|
| SDL3 | `release-3.4.16` (CMake FetchContent) | zlib | Window, events, later input and audio |
| zlib | `v1.3.2` (CMake FetchContent, same pin as pt-pc) | zlib | `cmp\0zlib` archives |

## Read at tool run time only (nothing copied)

- shadPS4 (GPL-2.0): `src/core/aerolib/aerolib.inl` (NID names, `tools/dynlib.py --nid-db`), `src/shader_recompiler/frontend/opcodes.h` (`tools/gcn.py --opcodes`), `LIB_FUNCTION` registrations (`tools/nid_coverage.py`). No shadPS4 code is part of this repository.
