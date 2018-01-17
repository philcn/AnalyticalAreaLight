#version 150

uniform mat4 ciModelMatrix;
uniform mat4 ciModelViewProjection;

in vec4 ciPosition;
in vec3 ciNormal;

out vec3 WorldPosition;
out vec3 WorldNormal;

void main() {
	gl_Position = ciModelViewProjection * ciPosition;

	WorldPosition = vec3( ciModelMatrix * ciPosition );
	WorldNormal = normalize( mat3( inverse( transpose( ciModelMatrix ) ) ) * ciNormal );
}
