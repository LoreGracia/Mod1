/**
 * @file waterShading.glsl
 * @brief Shared analytic shading model for every water representation.
 *
 * Both the Marching Cubes surface and the fake water volume include this file
 * so the base color, Fresnel reflectance, sky reflection and final alpha are
 * computed with exactly the same formulas and uniforms.
 */

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
 * @param horizon Warm sky color at the horizon.
 * @param zenith Deep sky color at the zenith.
 * @param sunGlow Color of the sun halo.
 * @return The procedural sky color along the given direction.
 */
vec3 waterSky(vec3 dir, vec3 sunDir, vec3 horizon, vec3 zenith, vec3 sunGlow) {
	float up = max(dir.y, 0.0);
	vec3 sky = mix(horizon, zenith, up);

	float glow = pow(max(dot(dir, sunDir), 0.0), 128.0);
	sky += sunGlow * glow;

	sky = mix(sky, horizon * 0.55, 1.0 - smoothstep(-0.35, 0.0, dir.y));
	return sky;
}

/**
 * @brief Computes the Schlick Fresnel reflectance of water.
 * @param NdotV Cosine between the shading normal and the view direction.
 * @return Reflectance in [0, 1].
 */
float waterFresnel(float NdotV) {
	const float F0 = 0.015;
	float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
	return clamp(fresnel, 0.0, 1.0);
}

/**
 * @brief Returns the depth-dependent water base color inside the container.
 * @param worldY World-space height of the fragment.
 * @param boundsMin Minimum corner of the water volume.
 * @param boundsMax Maximum corner of the water volume.
 * @param deep Color of deep water.
 * @param shallow Color of shallow water.
 * @return The interpolated base color.
 */
vec3 waterBaseColor(float worldY, vec3 boundsMin, vec3 boundsMax, vec3 deep, vec3 shallow) {
	float heightT = clamp((worldY - boundsMin.y) /
						max(boundsMax.y - boundsMin.y, 0.001), 0.0, 1.0);
	return mix(deep, shallow, heightT);
}

/**
 * @brief Shades a water fragment with the shared material model.
 *
 * Combines ambient, diffuse and subsurface terms, a Fresnel-driven sky
 * reflection, a Blinn-Phong sun glint, an optional wave-crest highlight and
 * an optional foam layer.
 *
 * @param N Shading normal in world space.
 * @param V Normalized view direction.
 * @param base Base water color.
 * @param sunDir Normalized direction towards the sun.
 * @param sunColor Sun color.
 * @param fresnel Fresnel reflectance.
 * @param skyColor Reflected sky color.
 * @param foam Foam mask in [0, 1].
 * @param foamColor Foam color.
 * @param waveGrad Wave-height gradient used for the crest highlight.
 * @return Lit color (RGB).
 */
vec3 waterShade(vec3 N, vec3 V, vec3 base, vec3 sunDir, vec3 sunColor,
				float fresnel, vec3 skyColor, float foam, vec3 foamColor,
				vec2 waveGrad) {
	vec3 L = normalize(sunDir);
	vec3 H = normalize(L + V);

	float NdotL = max(dot(N, L), 0.0);

	vec3 ambient = base * 0.50;
	vec3 diffuse = base * sunColor * NdotL * 0.50;

	float shininess = mix(120.0, 600.0, fresnel);
	float spec = pow(max(dot(N, H), 0.0), shininess);
	vec3 specular = sunColor * spec * (0.50 + 2.0 * fresnel);

	float sss = pow(max(dot(N, V), 0.0), 2.0) * 0.3;
	vec3 subSurface = vec3(0.1, 0.4, 0.6) * sss;

	vec3 color = ambient + diffuse + subSurface;
	color = mix(color, skyColor, fresnel);
	color += specular;

	color += sunColor * max(waveGrad.x + waveGrad.y, 0.0) * 0.15 * fresnel;

	vec3 foamLit = foamColor * (0.55 + 0.45 * NdotL);
	color = mix(color, foamLit, foam);
	return color;
}

/**
 * @brief Returns the final alpha of a water fragment.
 * @param fresnel Fresnel reflectance.
 * @param heightT Normalized height inside the container.
 * @param foam Foam mask in [0, 1].
 * @return Alpha in [0, 1].
 */
float waterAlpha(float fresnel, float heightT, float foam) {
	float alpha = mix(0.65, 0.98, fresnel);
	alpha = mix(alpha, 0.85, heightT * 0.25);
	alpha = mix(alpha, 1.0, foam);
	return clamp(alpha, 0.0, 1.0);
}
