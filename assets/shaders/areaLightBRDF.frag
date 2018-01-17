// References:
// https://eheitzresearch.wordpress.com/415-2/
// https://github.com/mosra/magnum-examples/blob/master/src/arealights/AreaLights.frag
// http://blog.selfshadow.com/sandbox/ltc.html

#version 150

uniform vec3 uCameraPos;

uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform vec3 uLightVertices[4];
uniform bool uDoubleSided;

uniform vec3 uBaseColor;
uniform float uMetalness;
uniform float uRoughness;
uniform float uF0;

uniform sampler2D uTexLTCMat;
uniform sampler2D uTexLTCAmp;

in vec3 WorldPosition;
in vec3 WorldNormal;

out vec4 oColor;

#define M_PI 3.14159265359
#define LUT_SIZE 64.0

// Get uv coordinates into LTC lookup texture
vec2 ltcCoords( float cosTheta, float roughness ) {
	float theta = acos( cosTheta );
	vec2 coords = vec2( roughness, theta / ( 0.5 * M_PI ) );
	
	/* Scale and bias coordinates, for correct filtered lookup */
	coords = coords * ( LUT_SIZE - 1.0 ) / LUT_SIZE + 0.5 / LUT_SIZE;
	return coords;
}

// Get inverse matrix from LTC lookup texture
mat3 ltcMatrix( sampler2D tex, vec2 coord ) {
	vec4 t = texture( tex, coord );
	mat3 Minv = mat3(vec3(  1,   0, t.y),
					 vec3(  0, t.z,   0),
					 vec3(t.w,   0, t.x));
	return Minv;
}

// Integrate between two edges on a clamped cosine distribution
float integrateEdge( vec3 v1, vec3 v2 ) {
	float cosTheta = dot( v1, v2 );
	cosTheta = clamp( cosTheta, -0.9999, 0.9999 );
	
	float theta = acos( cosTheta );
	// For theta <= 0.001 `theta/sin(theta)` is approximated as 1.0
	float res = cross( v1, v2 ).z * ( ( theta > 0.001 ) ? theta / sin( theta ) : 1.0);
	return res;
}

int clipQuadToHorizon( inout vec3 L[5] ) {
	// Detect clipping config
	int config = 0;
	if( L[0].z > 0.0 ) config += 1;
	if( L[1].z > 0.0 ) config += 2;
	if( L[2].z > 0.0 ) config += 4;
	if( L[3].z > 0.0 ) config += 8;
	
	int n = 0;
	
	if( config == 0 ) {
		// clip all
	} else if( config == 1 ) { // V1 clip V2 V3 V4
		n = 3;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[3].z * L[0] + L[0].z * L[3];
	} else if( config == 2 ) { // V2 clip V1 V3 V4
		n = 3;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	} else if( config == 3 ) { // V1 V2 clip V3 V4
		n = 4;
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
		L[3] = -L[3].z * L[0] + L[0].z * L[3];
	} else if( config == 4 ) { // V3 clip V1 V2 V4
		n = 3;
		L[0] = -L[3].z * L[2] + L[2].z * L[3];
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
	} else if( config == 5 ) { // V1 V3 clip V2 V4, impossible
		n = 0;
	} else if( config == 6 ) { // V2 V3 clip V1 V4
		n = 4;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	} else if( config == 7 ) { // V1 V2 V3 clip V4
		n = 5;
		L[4] = -L[3].z * L[0] + L[0].z * L[3];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	} else if( config == 8 ) { // V4 clip V1 V2 V3
		n = 3;
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
		L[1] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] =  L[3];
	} else if( config == 9 ) { // V1 V4 clip V2 V3
		n = 4;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[2].z * L[3] + L[3].z * L[2];
	} else if( config == 10 ) { // V2 V4 clip V1 V3, impossible
		n = 0;
	} else if( config == 11 ) { // V1 V2 V4 clip V3
		n = 5;
		L[4] = L[3];
		L[3] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	} else if( config == 12 ) { // V3 V4 clip V1 V2
		n = 4;
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
	} else if( config == 13 ) { // V1 V3 V4 clip V2
		n = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].z * L[2] + L[2].z * L[1];
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
	} else if( config == 14 ) { // V2 V3 V4 clip V1
		n = 5;
		L[4] = -L[0].z * L[3] + L[3].z * L[0];
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
	} else if( config == 15 ) { // V1 V2 V3 V4
		n = 4;
	}
	
	if( n == 3 )
		L[3] = L[0];
	if( n == 4 )
		L[4] = L[0];
	
	return n;
}

/*
 * Get intensity of light from the arealight given by `points` at the point `P`
 * with normal `N` when viewed from direction `V`.
 * @param N World space normal
 * @param V World space view direction
 * @param P World space vertex position
 * @param Minv Matrix to transform from BRDF distribution to clamped cosine distribution
 * @param points Light quad vertices
 * @param twoSided Whether the light is two sided
 */
float ltcEvaluate( vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided ) {
	// Construct orthonormal basis around N
	vec3 T1 = normalize( V - N * dot( V, N ) );
	vec3 T2 = cross( N, T1 );
	
	// Rotate area light in (T1, T2, R) basis
	Minv = Minv * transpose( mat3( T1, T2, N ) );
	
	// Allocate 5 vertices for polygon (one additional which may result from clipping)
	vec3 L[5];
	L[0] = Minv * ( points[0] - P );
	L[1] = Minv * ( points[1] - P );
	L[2] = Minv * ( points[2] - P );
	L[3] = Minv * ( points[3] - P );
	
	// Clip light quad so that the part behind the surface does not affect the lighting of the point
	int n = clipQuadToHorizon( L );
	if( n == 0 )
		return 0.0;
	
	// project onto sphere
	L[0] = normalize( L[0] );
	L[1] = normalize( L[1] );
	L[2] = normalize( L[2] );
	L[3] = normalize( L[3] );
	L[4] = normalize( L[4] );
	
	// Integrate over the clamped cosine distribution in the domain of the transformed light polygon
	float sum = integrateEdge( L[0], L[1] ) + integrateEdge( L[1], L[2] ) + integrateEdge( L[2], L[3] );
	if( n >= 4 )
		sum += integrateEdge( L[3], L[4] );
	if( n == 5 )
		sum += integrateEdge( L[4], L[0] );
	
	// Negated due to winding order
	sum = twoSided ? abs( sum ) : max( 0.0, -sum );
	
	return sum;
}

void main() {
	vec3 diffuseColor = uBaseColor * ( 1.0 - uMetalness );
	vec3 specularColor = mix( vec3( uF0 ), uBaseColor, uMetalness );
	
	vec3 N = normalize( WorldNormal );
	vec3 V = normalize( uCameraPos - WorldPosition );
	vec3 P = WorldPosition;
	
	vec2 coords = ltcCoords( dot( V, N ), uRoughness );
	mat3 invMat = ltcMatrix( uTexLTCMat, coords );
	vec2 schlick = texture( uTexLTCAmp, coords ).xy;
	
	float diffuse = ltcEvaluate( N, V, P, mat3( 1.0 ), uLightVertices, uDoubleSided );
	float specular = ltcEvaluate( N, V, P, invMat, uLightVertices, uDoubleSided ) * schlick.x;
	
	vec3 color = uLightIntensity * uLightColor * ( specularColor * specular + diffuseColor * diffuse ) / ( 2.0 * M_PI );

	oColor = vec4( pow( color, vec3( 1.0 / 2.2 ) ), 1.0 );
}
