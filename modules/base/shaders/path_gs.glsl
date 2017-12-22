/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014 - 2017                                                             *
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

#include "PowerScaling/powerScalingMath.hglsl"
#include <${SHADERS_GENERATED}/constants.hglsl>:notrack

layout(points) in;
layout(location = 0) in vec4 vs_point_position[];
layout(location = 1) in flat int isHour[];
layout(location = 2) in vec4 vs_point_color[];

layout(points, max_vertices = 4) out;
layout(location = 0) out vec4 gs_point_position;
layout(location = 1) out vec4 gs_point_color;

uniform mat4 projection;
uniform mat4 ViewProjection;

const vec2 corners[4] = vec2[4](
    vec2(0.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);


void main() {
    gs_point_color = vs_point_color[0];
    gs_point_position = vs_point_position[0];
    if (isHour[0] == 1) {
        gl_Position =  gl_in[0].gl_Position;
        EmitVertex();
        EndPrimitive();
    }
    else {
        gl_Position =  gl_in[0].gl_Position;
        EmitVertex();
        EndPrimitive();
    return;
    }
}