
cbuffer UnityFFVertex {
	float4x4	ff_matrix_mvp; // 0
	float4x4	ff_matrix_mv; // 4
	float4		ff_vec_color; // 8
	float4		ff_vec_ambient; // 9
	float4		ff_light_color[8]; // 10
	float4		ff_light_pos[8]; // 18
	float4		ff_light_atten[8]; // 26
	float4		ff_light_spot[8]; // 34
	float4		ff_mat_diffuse; // 42
	float4		ff_mat_ambient; // 43
	float4		ff_mat_spec; // 44
	float4		ff_mat_emission; // 45
	float4x4	ff_matrix_tex[4]; // 46
	float4		ff_fog_vs; // 62
}; // 62

cbuffer UnityFFPixel {
	float4		ff_vec_colors[8]; // 0
	float		ff_alpha_ref; // 8
	float4		ff_fog_ps; // 9
};


export float4 LoadVertexColor(float4 vc) { return vc; }
export float4 LoadVertexColorUniform() { return ff_vec_color; }
export float3 LoadEyePos(float4 vertex) { return mul (ff_matrix_mv, vertex).xyz; }
export float3 LoadEyeNormal(float3 normal) { return normalize (mul ((float3x3)ff_matrix_mv, normal).xyz); } //@TODO: proper normal matrix
export float3 LoadZero() { return 0.0; }
export float3 LoadViewDir(float3 eyePos) { return -normalize(eyePos); }
export float3 LoadEyeRefl(float3 viewDir, float3 eyeNormal) { return 2.0f * dot (viewDir, eyeNormal) * eyeNormal - viewDir; }
export float4 LoadAmbientColor() { return ff_mat_ambient; }
export float4 LoadDiffuseColor() { return ff_mat_diffuse; }
export float4 LoadEmissionColor() { return ff_mat_emission; }

export float3 InitLightColor(float4 emission, float4 ambient) { return emission.rgb + ambient.rgb * ff_vec_ambient.rgb; }

float3 ComputeLighting (int idx, float3 dirToLight, float3 eyeNormal, float3 viewDir, float4 diffuseColor, float atten, inout float3 specColor) {
  float NdotL = max(dot(eyeNormal, dirToLight), 0.0);
  float3 color = NdotL * diffuseColor.rgb * ff_light_color[idx].rgb;
  return color * atten;
}
float3 ComputeLightingSpec (int idx, float3 dirToLight, float3 eyeNormal, float3 viewDir, float4 diffuseColor, float atten, inout float3 specColor) {
  float NdotL = max(dot(eyeNormal, dirToLight), 0.0);
  float3 color = NdotL * diffuseColor.rgb * ff_light_color[idx].rgb;
  if (NdotL > 0.0) {
    float3 h = normalize(dirToLight + viewDir);
    float HdotN = max(dot(eyeNormal, h), 0.0);
    float sp = saturate(pow(HdotN, ff_mat_spec.w));
    specColor += atten * sp * ff_light_color[idx].rgb;
  }
  return color * atten;
}
float3 ComputeSpotLight(int idx, float3 eyePosition, float3 eyeNormal, float3 viewDir, float4 diffuseColor, inout float3 specColor) {
  float3 dirToLight = ff_light_pos[idx].xyz - eyePosition * ff_light_pos[idx].w;
  float distSqr = dot(dirToLight, dirToLight);
  float att = 1.0 / (1.0 + ff_light_atten[idx].z * distSqr);
  if (ff_light_pos[idx].w != 0 && distSqr > ff_light_atten[idx].w) att = 0.0; // set to 0 if outside of range
  dirToLight *= rsqrt(distSqr);
  float rho = max(dot(dirToLight, ff_light_spot[idx].xyz), 0.0);
  float spotAtt = (rho - ff_light_atten[idx].x) * ff_light_atten[idx].y;
  spotAtt = saturate(spotAtt);
  return min (ComputeLighting (idx, dirToLight, eyeNormal, viewDir, diffuseColor, att*spotAtt, specColor), 1.0);
}
float3 ComputeSpotLightSpec(int idx, float3 eyePosition, float3 eyeNormal, float3 viewDir, float4 diffuseColor, inout float3 specColor) {
  float3 dirToLight = ff_light_pos[idx].xyz - eyePosition * ff_light_pos[idx].w;
  float distSqr = dot(dirToLight, dirToLight);
  float att = 1.0 / (1.0 + ff_light_atten[idx].z * distSqr);
  if (ff_light_pos[idx].w != 0 && distSqr > ff_light_atten[idx].w) att = 0.0; // set to 0 if outside of range
  dirToLight *= rsqrt(distSqr);
  float rho = max(dot(dirToLight, ff_light_spot[idx].xyz), 0.0);
  float spotAtt = (rho - ff_light_atten[idx].x) * ff_light_atten[idx].y;
  spotAtt = saturate(spotAtt);
  return min (ComputeLightingSpec (idx, dirToLight, eyeNormal, viewDir, diffuseColor, att*spotAtt, specColor), 1.0);
}
#define SPOT_LIGHT(n) \
export float3 ComputeSpotLight##n(float3 eyePosition, float3 eyeNormal, float3 viewDir, float4 diffuseColor, inout float3 specColor, float3 amb) { \
	float3 l = amb; \
	for (int i = 0; i < n; ++i) \
		l += ComputeSpotLight(i, eyePosition, eyeNormal, viewDir, diffuseColor, specColor); \
	return l; \
} \
export float3 ComputeSpotLightSpec##n(float3 eyePosition, float3 eyeNormal, float3 viewDir, float4 diffuseColor, inout float3 specColor, float3 amb) { \
	float3 l = amb; \
	for (int i = 0; i < n; ++i) \
		l += ComputeSpotLightSpec(i, eyePosition, eyeNormal, viewDir, diffuseColor, specColor); \
	return l; \
}
SPOT_LIGHT(0)
SPOT_LIGHT(1)
SPOT_LIGHT(2)
SPOT_LIGHT(3)
SPOT_LIGHT(4)
SPOT_LIGHT(5)
SPOT_LIGHT(6)
SPOT_LIGHT(7)
SPOT_LIGHT(8)

export float4 LoadLightingColor(float3 lcolor, float4 diffcolor) { return float4(lcolor.rgb, diffcolor.a); }

export float4 TransformVertex(float4 vertex) { return mul (ff_matrix_mvp, vertex); }

export float4 Saturate4(float4 c) { return saturate(c); }
export float3 Saturate3(float3 c) { return saturate(c); }
export float3 Load3(float3 c) { return c; }
export float3 ModulateSpec(float3 c) { return c * ff_mat_spec.rgb; }

export float4 MultiplyUV0(float4 uv) { return mul(ff_matrix_tex[0], uv); }
export float4 MultiplyUV1(float4 uv) { return mul(ff_matrix_tex[1], uv); }
export float4 MultiplyUV2(float4 uv) { return mul(ff_matrix_tex[2], uv); }
export float4 MultiplyUV3(float4 uv) { return mul(ff_matrix_tex[3], uv); }
export float4 MultiplyUV4(float4 uv) { return uv; }
export float4 MultiplyUV5(float4 uv) { return uv; }
export float4 MultiplyUV6(float4 uv) { return uv; }
export float4 MultiplyUV7(float4 uv) { return uv; }

export float4 UVSphereMap(float3 eyeRefl) { return float4(eyeRefl.xy / (2.0*sqrt(eyeRefl.x*eyeRefl.x + eyeRefl.y*eyeRefl.y + (eyeRefl.z+1)*(eyeRefl.z+1))) + 0.5, 0, 1); }
export float4 Float3to4(float3 v) { return float4(v.xyz,1); }

export float4 LoadConstantColor0() { return ff_vec_colors[0]; }
export float4 LoadConstantColor1() { return ff_vec_colors[1]; }
export float4 LoadConstantColor2() { return ff_vec_colors[2]; }
export float4 LoadConstantColor3() { return ff_vec_colors[3]; }
export float4 LoadConstantColor4() { return ff_vec_colors[4]; }
export float4 LoadConstantColor5() { return ff_vec_colors[5]; }
export float4 LoadConstantColor6() { return ff_vec_colors[6]; }
export float4 LoadConstantColor7() { return ff_vec_colors[7]; }

export float OneMinus1(float v) { return 1.0-v; }
export float3 OneMinus3(float3 v) { return 1.0-v; }
export float4 OneMinus4(float4 v) { return 1.0-v; }

export float4 CombReplace	(float4 a) { return a; }
export float4 CombModulate	(float4 a, float4 b) { return a * b; }
export float4 CombAdd 		(float4 a, float4 b) { return a + b; }
export float4 CombAddSigned(float4 a, float4 b) { return a + b - 0.5; }
export float4 CombSubtract	(float4 a, float4 b) { return a - b; }
export float4 CombLerp		(float4 a, float4 b, float4 c) { return lerp(b, a, c.a); }
export float4 CombDot3		(float4 a, float4 b) { float3 r = 4.0 * dot(a.rgb-0.5, b.rgb-0.5); return float4(r, a.a); }
export float4 CombDot3rgba	(float4 a, float4 b) { return 4.0 * dot(a.rgb-0.5, b.rgb-0.5); }
export float4 CombMulAdd	(float4 a, float4 b, float4 c) { return a * c.a + b; }
export float4 CombMulSub	(float4 a, float4 b, float4 c) { return a * c.a - b; }
export float4 CombMulAddSigned(float4 a, float4 b, float4 c) { return a * c.a + b - 0.5; }

export float4 Scale2(float4 a) { return a + a; }
export float4 Scale4(float4 a) { return a * 4; }

export float4 AddSpec(float4 col, float3 spec) { col.rgb += spec; return col; }
export float4 CombineAlpha(float4 c, float4 a) { return float4(c.rgb, a.a); }

export float FogLinear(float3 eyePos) {
	return saturate(length(eyePos) * ff_fog_vs.z + ff_fog_vs.w);
}
export float FogExp(float3 eyePos) {
	return saturate(exp2(-(length(eyePos) * ff_fog_vs.y)));
}
export float FogExp2(float3 eyePos) {
	float f = length(eyePos) * ff_fog_vs.y;
	return saturate(exp2(-f * f));
}
export float4 ApplyFog(float4 col, float ifog) {
	return float4(lerp(ff_fog_ps.rgb, col.rgb, ifog), col.a);
}

export float4 AlphaTestNever(float4 col) { discard; return col; }
export float4 AlphaTestLess(float4 col) { if (!(col.a < ff_alpha_ref)) discard; return col; }
export float4 AlphaTestEqual(float4 col) { if (!(col.a == ff_alpha_ref)) discard; return col; }
export float4 AlphaTestLEqual(float4 col) { if (!(col.a <= ff_alpha_ref)) discard; return col; }
export float4 AlphaTestGreater(float4 col) { if (!(col.a > ff_alpha_ref)) discard; return col; }
export float4 AlphaTestNotEqual(float4 col) { if (!(col.a != ff_alpha_ref)) discard; return col; }
export float4 AlphaTestGEqual(float4 col) { if (!(col.a >= ff_alpha_ref)) discard; return col; }
