/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014                                                                    *
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

#include <${SHADERS_GENERATED}/version.hglsl>:notrack

const vec2 corners[4] = { 
    vec2(0.0, 1.0), 
	vec2(0.0, 0.0), 
	vec2(1.0, 1.0), 
	vec2(1.0, 0.0) 
};

#include "PowerScaling/powerScalingMath.hglsl"

in vec4 psc_position[];
in vec4 campos[];

layout(points) in;
//layout(points, max_vertices = 1) out;
layout(location = 2) in  vec3 vs_brightness[];
layout(location = 2) out vec3 ge_brightness[];
layout(triangle_strip, max_vertices = 4) out;

out vec2 texCoord;
out vec4 vs_position;

uniform mat4 projection; // we do this after distance computation. 

float spriteSize = 0.0000005f; // set here for now.

void main(){
	ge_brightness[0] = vs_brightness[0];                           // pass on to fragment shader. 
	
	/// --- distance modulus --- NOT OPTIMIZED YET.
 	
//	float M  = vs_brightness[0][0];                                 // get ABSOLUTE magnitude (x param)
	float M  = vs_brightness[0][2]; // if NOT running test-target.
	vec4 cam = vec4(-campos[0].xyz, campos[0].w);                  // get negative camera position   
	vec4 pos = psc_position[0];                                    // get OK star position
	
	vec4 result = psc_addition(pos, cam);                          // compute vec from camera to position
	float x, y, z, w;
	x = result[0];
	y = result[1];
	z = result[2];
	w = result[3];
	                                                               // I dont trust the legnth function at this point
	vec2 pc = vec2(sqrt(x*x +y*y + z*z), result[3]);               // form vec2 
	 
	pc[0] *= 0.324077929f;                                          // convert meters -> parsecs
	pc[1] += -18f;
	
	float pc_psc   = pc[0] * pow(10, pc[1]);                       // psc scale out
	float apparent = -(M - 5.0f * (1.f - log10(pc_psc)));          // get APPARENT magnitude. 
     
	vec4 P = gl_in[0].gl_Position;
	 
	float weight = 0.00001f; 										    // otherwise this takes over.
	float depth  = pc[0] * pow(10, pc[1]);
	depth       *= pow(apparent,3);
	//if(round(apparent) > 10)
	spriteSize  += (depth*weight); 
	
	// EMIT QUAD
	for(int i = 0; i < 4; i++){
		vec4 p1     = P;                 
		p1.xy      += spriteSize *(corners[i] - vec2(0.5)); 
		vs_position = p1;
		gl_Position = projection * p1;  
		// gl_Position = z_normalization(projection * p1);                                 // projection here
		texCoord    = corners[i];                           
	  EmitVertex();
	}
	EndPrimitive();
}