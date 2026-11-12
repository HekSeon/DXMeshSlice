#pragma once

inline constexpr const char* kShaderSrc = R"(
cbuffer CB : register(b0)
{
	float4x4	world;
	float4x4	view;
	float4x4	proj;
};

struct	VSIn	{float3 pos : POSITION; float3 nrm : NORMAL; };
struct	PSIn	{float4 pos : SV_POSITION; float3 nrm : NORMAL;	};

PSIn VSMain(VSIn i)
{
	PSIn o;
	float4	w = mul(float4(i.pos, 1.0), world);
	o.pos	= mul(mul(w, view),proj);
	o.nrm	= mul(i.nrm,(float3x3)world);
	return o;
}

float4 PSMain(PSIn i) : SV_TARGET
{
	float3 n = normalize(i.nrm);
	float3 l = normalize(float3(-0.4,0.8,-0.6));
	float  d = saturate(dot(n, l));
	float3 col = float3(0.9, 0.6, 0.3) * (0.2 + 0.8 * d);
	return float4(col, 1.0);
}
)";
