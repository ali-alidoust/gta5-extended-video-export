
cbuffer cbClippingPlanes : register(b0) {
	float4 planes;
};

Texture2D<float> logDepth : register(t0);

sampler samp = sampler_state {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct VSQuadOut {
	float4 position : SV_POSITION;
	float2 uv: TEXCOORD;
};

struct PSOut {
	float4 color : SV_Target;
	float  depth : SV_Depth;
};

PSOut main(VSQuadOut IN) {
	PSOut output;
	output.color = float4(0.1, 0.2, 0.3, 0.4);
	float n = 10;
	float f = 7800;
	float d = logDepth.Load(int3(IN.position.xy, 0)).r;
	output.depth = (pow(f+1,d)-1)/(f-n);
	//output.depth = (2 * planes.x) / (planes.y + planes.x - depth * (planes.y - planes.x));
	return output;
}