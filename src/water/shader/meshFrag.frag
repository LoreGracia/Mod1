#version 430 core
#include "waterShading.glsl"

/**
* @file meshFrag.frag
* @brief Fragment shader for rendering the water surface with realistic lighting.
* 
* Implements procedural micro-waves (sum of directional sines), detailed normal 
* perturbation, Schlick's Fresnel approximation, Blinn-Phong specular highlights, 
* and depth-based color blending to simulate water transparency and sky reflection.
* It also synthesizes procedural foam from surface curvature, wave-field flow and
* fractal gradient noise, blended as an opaque white layer on the water surface.
* Sky reflections are sampled from a procedural horizon/zenith gradient with a
* sun halo, using the per-fragment reflection vector and the Fresnel term.
*/

in vec3 vWorldPos; ///< Interpolated world position from the vertex shader.
in vec3 vNormal;   ///< Interpolated smooth normal from the vertex shader.

uniform vec3  uCameraPos;     ///< Position of the camera in world space.
uniform vec3  uSunDir;        ///< Direction towards the sun (normalized).
uniform vec3  uSunColor;      ///< Color of the sunlight.
uniform vec3  uWaterDeep;     ///< Color of deep water.
uniform vec3  uWaterShallow;  ///< Color of shallow water.
uniform vec3  uBoundsMin;     ///< Minimum bounds of the water volume.
uniform vec3  uBoundsMax;     ///< Maximum bounds of the water volume.
uniform float uTime;          ///< Current time for wave animation.
uniform float uWaterEnergy;   ///< Mean squared particle speed; gates the wave amplitude.

uniform vec3  uFoamColor;      ///< Base color of the procedural foam.
uniform float uFoamIntensity;  ///< Global strength of the foam layer.
uniform float uFoamThreshold;  ///< Foam cutoff: lower values expand the patches.
uniform float uFoamScale;      ///< Spatial frequency of the foam noise.
uniform float uFoamSpeed;      ///< Weight of the derived flow/velocity term.

uniform vec3  uSkyHorizon;    ///< Warm color of the procedural sky at the horizon.
uniform vec3  uSkyZenith;     ///< Deep color of the procedural sky at the zenith.
uniform vec3  uSunGlowColor;  ///< Color of the sun halo in the reflected sky.

out vec4 FragColor;

// ─────────────────────────────────────────────────────────────────────
// Phase 5: micro-waves. Sum of directional sines; returns height.
// ─────────────────────────────────────────────────────────────────────
/**
* @brief Calculates the height of the procedural micro-waves at a given 2D position.
* @param p The 2D position (usually world x and z coordinates).
* @param t The current time.
* @return The combined wave height.
*/
float waveH(vec2 p, float t) {
	float h = 0.0;
	h += 0.0040 * sin(dot(p, vec2( 18.0,  7.0)) + t * 2.1);
	h += 0.0025 * sin(dot(p, vec2( -9.0, 27.0)) + t * 2.9);
	h += 0.0015 * sin(dot(p, vec2( 41.0,-16.0)) + t * 3.7);
	return h;
}

/**
* @brief Computes the gradient of the wave height using finite differences.
* 
* Used to perturb the surface normal and create the illusion of ripples.
* 
* @param p The 2D position.
* @param t The current time.
* @return A 2D gradient vector (dx, dz).
*/
vec2 waveGrad(vec2 p, float t) {
	const float e = 0.02;
	float hx = waveH(p + vec2(e, 0.0), t) - waveH(p - vec2(e, 0.0), t);
	float hz = waveH(p + vec2(0.0, e), t) - waveH(p - vec2(0.0, e), t);
	return vec2(hx, hz) / (2.0 * e);
}

/**
* @brief Hash function returning a pseudo-random gradient for a 3D lattice cell.
* @param p Lattice cell coordinate.
* @return A pseudo-random vector in [-1, 1]^3.
*/
vec3 foamHash33(vec3 p) {
	p = vec3(dot(p, vec3(127.1, 311.7,  74.7)),
			dot(p, vec3(269.5, 183.3, 246.1)),
			dot(p, vec3(113.5, 271.9, 124.6)));
	return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

/**
* @brief Gradient (Perlin-style) noise evaluated on a 3D lattice.
* @param p Sample position.
* @return Noise value in approximately [-1, 1].
*/
float foamNoise(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);

	float n000 = dot(foamHash33(i + vec3(0.0, 0.0, 0.0)), f - vec3(0.0, 0.0, 0.0));
	float n100 = dot(foamHash33(i + vec3(1.0, 0.0, 0.0)), f - vec3(1.0, 0.0, 0.0));
	float n010 = dot(foamHash33(i + vec3(0.0, 1.0, 0.0)), f - vec3(0.0, 1.0, 0.0));
	float n110 = dot(foamHash33(i + vec3(1.0, 1.0, 0.0)), f - vec3(1.0, 1.0, 0.0));
	float n001 = dot(foamHash33(i + vec3(0.0, 0.0, 1.0)), f - vec3(0.0, 0.0, 1.0));
	float n101 = dot(foamHash33(i + vec3(1.0, 0.0, 1.0)), f - vec3(1.0, 0.0, 1.0));
	float n011 = dot(foamHash33(i + vec3(0.0, 1.0, 1.0)), f - vec3(0.0, 1.0, 1.0));
	float n111 = dot(foamHash33(i + vec3(1.0, 1.0, 1.0)), f - vec3(1.0, 1.0, 1.0));

	float nx00 = mix(n000, n100, u.x);
	float nx10 = mix(n010, n110, u.x);
	float nx01 = mix(n001, n101, u.x);
	float nx11 = mix(n011, n111, u.x);
	return mix(mix(nx00, nx10, u.y), mix(nx01, nx11, u.y), u.z);
}

/**
* @brief Fractal Brownian motion built from three gradient-noise octaves.
* @param p Sample position.
* @return Fractal noise in approximately [-1, 1].
*/
float foamFbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int octave = 0; octave < 3; ++octave) {
		value += amplitude * foamNoise(p);
		p = p * 2.03 + vec3(17.1, 9.7, 23.3);
		amplitude *= 0.5;
	}
	return value;
}

/**
* @brief Evaluates a procedural sky color for a reflection direction.
*
* Blends a warm horizon color into a deep zenith color according to the
* vertical component of the direction and adds a sharp sun halo around the
* sun direction. Below the horizon the color fades back to the horizon tone
* so grazing reflections never sample a pure black region.
*
* @param dir Direction used to sample the sky (usually the reflection vector).
* @param sunDir Normalized direction towards the sun.
* @return The procedural sky color along the given direction.
*/
vec3 proceduralSky(vec3 dir, vec3 sunDir) {
	return waterSky(dir, sunDir, uSkyHorizon, uSkyZenith, uSunGlowColor);
}

/**
* @brief Main fragment shading logic for the water surface.
*/
void main() {
	vec3 N = normalize(vNormal);
	vec3 V = normalize(uCameraPos - vWorldPos);
	vec3 L = normalize(uSunDir);

	// Distance-based detail attenuation to reduce aliasing in the distance.
	float dist   = length(uCameraPos - vWorldPos);
	float detail = 1.0 / (1.0 + 0.15 * dist);

	// Wave attenuation driven by the fluid energy: a settled pool (low mean
	// squared speed) fades the micro-waves out until the surface is flat,
	// while an active body of water restores them at full strength.
	float waveAmount = smoothstep(0.4, 1.2, uWaterEnergy);

	// Geometric normal before the wave perturbation: its screen-space
	// derivatives approximate the curvature of the Marching Cubes surface
	// without the high-frequency micro-wave detail dominating the mask.
	vec3 Ngeom = N;

	// Normal perturbation by micro-waves.
	vec2 g = waveGrad(vWorldPos.xz, uTime);
	N = normalize(N + vec3(-g.x, 0.0, -g.y) * 1.5 * detail * waveAmount);

	// Extra fine noise to break the periodicity of the sine waves. It scales
	// with the same energy factor so a resting surface stays perfectly flat.
	float micro = sin(vWorldPos.x * 41.0 + uTime * 4.1)
				* sin(vWorldPos.z * 37.0 - uTime * 3.3);
	N = normalize(N + vec3(micro, 0.0, micro) * 0.025 * detail * waveAmount);

	// Perceptual mirror trick: with no wave activity, bias the shading normal
	// towards world-up so a settled pool reflects the sky as a perfect plane.
	N = normalize(mix(N, vec3(0.0, 1.0, 0.0), 1.0 - waveAmount));

	// ── Procedural foam signal ───────────────────────────────────────
	// Curvature proxy: variation of the geometric normal across the screen.
	// Dividing by the detail attenuation keeps distant ripples contributing.
	float curvature = (length(dFdx(Ngeom)) + length(dFdy(Ngeom))) / max(detail, 0.05);

	// Flow/velocity proxy derived from the wave field. The temporal change
	// of the wave height behaves like a vertical velocity, while the
	// gradient magnitude grows on the slopes that feed foam.
	const float velDt = 0.05;
	float waveVel = abs(waveH(vWorldPos.xz, uTime + velDt) -
						waveH(vWorldPos.xz, uTime - velDt)) / (2.0 * velDt);
	float flow = (length(g) + waveVel) * waveAmount;

	// Steep surfaces (crests and troughs) accumulate more foam.
	float slope = 1.0 - clamp(N.y, 0.0, 1.0);

	// Fractal noise breaks the mask into irregular patches.
	float noise   = foamFbm(vec3(vWorldPos.xz * uFoamScale, uTime * 0.35));
	float patches = smoothstep(-0.2, 0.9, noise);

	// Combined foam mask in [0, 1]. The curvature and flow terms are boosted
	// so crests, troughs and high-velocity slopes produce foam over a wider
	// area, and the wider smoothstep transition softens the patch edges.
	float foamSignal = curvature * 8.0 + slope * 1.5 + flow * uFoamSpeed;
	float foam = smoothstep(uFoamThreshold, uFoamThreshold + 0.5, foamSignal * patches);
	foam = clamp(foam * uFoamIntensity, 0.0, 1.0);

	float NdotV = max(dot(N, V), 0.0);

	// Aggressive Fresnel (Schlick): transparent center, reflective edges.
	float fresnel = waterFresnel(NdotV);

	// Approximate depth-based color within the water AABB.
	float heightT = clamp((vWorldPos.y - uBoundsMin.y) /
						max(uBoundsMax.y - uBoundsMin.y, 0.001), 0.0, 1.0);
	vec3 base = waterBaseColor(vWorldPos.y, uBoundsMin, uBoundsMax, uWaterDeep, uWaterShallow);

	// Environment reflection: sample the procedural sky with the reflection
	// vector around the perturbed normal. Fresnel drives the mixing, so the
	// reflection dominates at grazing angles.
	vec3 R = reflect(-V, N);
	vec3 skyColor = proceduralSky(R, L);

	// Shared shading: ambient, diffuse, subsurface, Fresnel-driven sky
	// reflection, Blinn-Phong sun glint, wave-crest highlight and foam.
	vec3 color = waterShade(N, V, base, L, uSunColor, fresnel, skyColor,
							foam, uFoamColor, g);

	// Transparency: the center lets the background through, edges are opaque.
	float alpha = waterAlpha(fresnel, heightT, foam);

	FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
