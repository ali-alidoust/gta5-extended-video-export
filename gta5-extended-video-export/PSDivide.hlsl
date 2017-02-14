struct VSOutput
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

Texture2D<float4> MyTexture : register(t0);

cbuffer constantBuffer : register(b0) {
	float4 floats;
}

float4 main(VSOutput input) : SV_TARGET
{
	return MyTexture.Load(int3(input.position.xy, 0)) / floats.r;
}