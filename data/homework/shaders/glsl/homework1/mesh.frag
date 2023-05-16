#version 450

layout (set = 0, binding = 0) uniform UBOScene
{
	mat4 projection;
	mat4 view;
	vec4 lightPos;
	vec4 viewPos;
} uboScene;

layout (set = 1, binding = 0) uniform sampler2D colorTextureMap;
layout (set = 1, binding = 1) uniform sampler2D metallicRoughnessTextureMap;
layout (set = 1, binding = 2) uniform sampler2D normalTextureMap;
layout (set = 1, binding = 3) uniform sampler2D occlusionTextureMap;
layout (set = 1, binding = 4) uniform sampler2D emissiveTextureMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec4 inTangent;

layout (push_constant) uniform PbrMaterial {
	vec4 baseColorFactor;
	vec3 emissiveFactor;
	float metallicFactor;	
	float roughnessFactor;
} pbrMaterial;

layout (location = 0) out vec4 outFragColor;


const float PI = 3.14159265359;

vec3 baseColor()
{
	return texture(colorTextureMap, inUV).rgb * pbrMaterial.baseColorFactor.rgb * inColor;
}

vec3 Tonemap_ACES(const vec3 c) {
	// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
	// const float a = 2.51;
	// const float b = 0.03;
	// const float c = 2.43;
	// const float d = 0.59;
	// const float e = 0.14;
	// return saturate((x*(a*x+b))/(x*(c*x+d)+e));

	//ACES RRT/ODT curve fit courtesy of Stephen Hill
	vec3 a = c * (c + 0.0245786) - 0.000090537;
	vec3 b = c * (0.983729 * c + 0.4329510) + 0.238081;
	return a / b;
}

// Calculate final normal by TBN
vec3 getNormal()
{
	vec3 tangentNormal = texture(normalTextureMap, inUV).xyz * 2.0 - 1.0;

	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent.xyz);
	vec3 B = normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic)
{
	vec3 F0 = mix(vec3(0.04), baseColor(), metallic); // * material.specular
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		// float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{

		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);

		vec3 Ks = F;
		vec3 Kd = (vec3(1.0)-Ks) * (1.0 - metallic);

		color += (Kd * pow(baseColor(), vec3(2.2)) / PI + spec) * dotNL;
	}

	return color;
}

void main() 
{
	// Prepare basic vars
	vec3 N = getNormal(); 
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);

	float metallic = texture(metallicRoughnessTextureMap, inUV).r * pbrMaterial.metallicFactor;
	float roughness = texture(metallicRoughnessTextureMap, inUV).g * pbrMaterial.roughnessFactor;

	// Specular contribution
	vec3 Lo = vec3(0.0);
	Lo += BRDF(L, V, N, metallic, roughness);

	// Combine with ambient
	const float u_OcclusionStrength = 0.1;
	float ao = texture(occlusionTextureMap, inUV).r;
	vec3 color = Lo + (baseColor() * ao * u_OcclusionStrength) + vec3(0.02);

	// Combine with emissive
	color += texture(emissiveTextureMap, inUV).rgb * pbrMaterial.emissiveFactor;

	// TODO: move Tone mapping to new Render Pass
	color = Tonemap_ACES(color);

	// Gamma correct
	color = pow(color, vec3(0.4545));

	outFragColor = vec4(color, 1.0);	
}