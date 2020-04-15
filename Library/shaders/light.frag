/*    
    Copyright (c) 2020 Patryk Cieslak. All rights reserved.

    This file is a part of Stonefish.

    Stonefish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Stonefish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#version 330

in float logz;
in vec4 fragPos;
out vec4 fragColor;

uniform vec3 eyePos;
uniform vec3 color;
uniform float FC;

const vec3 waterSurfaceN = vec3(0.0, 0.0, -1.0);

vec3 BeerLambert(float d);

void main()
{
    //Logarithmic z-buffer correction
	gl_FragDepth = log2(logz) * FC;

    vec3 P = fragPos.xyz/fragPos.w;
	vec3 toEye = eyePos - P;
	float d = length(toEye);
	vec3 V = toEye/d;
	float dw = d;

	if(eyePos.z < 0.0)
		dw = max(P.z, 0.0)/dot(V, waterSurfaceN);
    
    if(P.z > 0.0)
        fragColor = vec4(BeerLambert(dw), 1.0);
    else
        fragColor = vec4(color, 1.0);
}
