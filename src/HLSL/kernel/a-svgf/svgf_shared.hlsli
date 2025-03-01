/*
Copyright (c) 2018, Christoph Schied
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Karlsruhe Institute of Technology nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define gGradientDownsample 3

#ifdef __HLSL_VERSION

#include "../../scene.hlsli"

inline bool test_reprojected_normal(const float3 n1, const float3 n2) {
	return dot(n1, n2) > 0.99619469809; // cos(5 degrees)
}

inline bool test_inside_screen(const int2 p, ViewData view) {
	return all(p >= view.image_min) && all(p < view.image_max);
}

inline bool test_reprojected_depth(const float z1, const float z2, const float2 offset, const float dz_dx, const float dz_dy) {
	return abs(z1 - z2) < 2*(length(float2(dz_dx*offset.x, dz_dy*offset.y)) + 1e-2);
}

#define TILE_OFFSET_SHIFT 3u
#define TILE_OFFSET_MASK ((1u << TILE_OFFSET_SHIFT) - 1u)

inline uint2 get_gradient_tile_pos(const uint idx) {
	/* didn't store a gradient sample in the previous frame, this creates
	   a new sample in the center of the tile */
	if (idx < (1u<<31)) return gGradientDownsample / 2;
	return uint2((idx & TILE_OFFSET_MASK), (idx >> TILE_OFFSET_SHIFT) & TILE_OFFSET_MASK);
}

inline uint get_gradient_idx_from_tile_pos(const uint2 pos) {
	return (1 << 31) | pos.x | (pos.y << TILE_OFFSET_SHIFT);
}

inline bool is_gradient_sample(Texture2D<float> tex_gradient, const uint2 ipos) {
	uint2 ipos_grad = ipos / gGradientDownsample;
	uint u = tex_gradient.Load(int3(ipos_grad,0)).r;
	uint2 tile_pos = uint2((u & TILE_OFFSET_MASK), (u >> TILE_OFFSET_SHIFT) & TILE_OFFSET_MASK);
	return u >= (1u << 31) && all(ipos == ipos_grad * gGradientDownsample + tile_pos);
 }

#endif