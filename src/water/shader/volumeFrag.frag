#version 430 core
in vec3 vWorldPos;
uniform vec3 uCameraPos;
uniform vec3 uWaterDeep;
uniform vec3 uWaterShallow;
out vec4 FragColor;

void main() {
	vec3 V = normalize(uCameraPos - vWorldPos);
	
	vec3 N = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
	if (dot(N, V) < 0.0)
		N = -N;

	float NdotV = max(dot(N, V), 0.0);
	
	float fresnel = pow(1.0 - NdotV, 4.0);
	vec3 skyColor = vec3(0.3, 0.5, 0.9);

	vec3 color = mix(uWaterShallow, uWaterDeep, 0.5);
	color = mix(color, skyColor, fresnel * 0.7);
	
	float alpha = mix(0.4, 0.95, fresnel);
	
	FragColor = vec4(color, alpha);
}
