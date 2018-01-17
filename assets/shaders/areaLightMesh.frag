#version 150

uniform vec3 uLightColor;
uniform float uIntensity;

out vec4 oColor;

void main() {
	oColor = vec4( uLightColor * uIntensity, 1.0 );
}
