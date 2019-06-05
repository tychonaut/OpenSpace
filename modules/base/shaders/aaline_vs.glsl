/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2019                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#version __CONTEXT__

#include "PowerScaling/powerScaling_vs.hglsl"

layout(location = 0) in vec3 pos0;
layout(location = 1) in vec3 pos1;
layout(location = 2) in vec4 weights;
layout(location = 3) in float radius;

out vec4 textureUV;
out float vs_screenSpaceDepth;
//out vec4 vs_positionViewSpace;

uniform float aspectRatio;
uniform mat4 modelViewProjection;   // World * View * Projection matrix

//uniform dmat4 modelViewMatrix;
//uniform dmat4 projectionMatrix;

void main() {
    // Transform the input points.
    vec4 p0 = modelViewProjection * vec4( pos0, 1.0f );
    vec4 p1 = modelViewProjection * vec4( pos1, 1.0f );

    // Warp transformed points by aspectRatio ratio.
    vec4 w0 = p0;
    vec4 w1 = p1;
    w0.y /= aspectRatio;
    w1.y /= aspectRatio;

    // Calc vectors between points in screen space.
    vec2 delta2 = w1.xy / w1.z - w0.xy / w0.z;
    vec3 delta_p;

    delta_p.xy = delta2;
    delta_p.z = w1.z - w0.z;

    //
    // Calc UV basis vectors.
    //
    // Calc U
    float len = length( delta2 );
    vec3 U = delta_p / len;

    // Create V orthogonal to U.
    vec3 V;
    V.x = U.y;
    V.y = -U.x;
    V.z = 0;

    // Calculate output position based on this
    // vertex's weights.
    vec4 position = p0 * weights.x + p1 * weights.y;

    // Calc offset part of postion.
    vec3 offset = U * weights.z + V * weights.w;

    // Apply line thickness.
    offset.xy *= radius/1000;

    // Unwarp by inverse of aspectRatio ratio.
    offset.y *= aspectRatio;

    // Undo perspective divide since the hardware will do it.
    position.xy += offset.xy * position.z;

    // Set up UVs.  We have to use projected sampling rather
    // than regular sampling because we don't want to get
    // perspective correction.
    textureUV.x = weights.z;
    textureUV.y = weights.w;
    textureUV.z = 0.0f;
    textureUV.w = 1.0f;

    vec4 positionClipSpaceZNorm = z_normalization(position);
    vs_screenSpaceDepth  = positionClipSpaceZNorm.w;

    gl_Position = positionClipSpaceZNorm;
}
