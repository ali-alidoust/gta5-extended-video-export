//float4 main(uint id : SV_VertexID) : SV_POSITION
//{
//	float2 tex = float2((id << 1) & 2, id & 2);
//	return float4(tex * float2(2, -2) + float2(-1, 1), 0.1f, 1);
//}

struct VSQuadOut {
	float4 position : SV_POSITION;
	float2 uv: TEXCOORD;
};

// outputs a full screen triangle with screen-space coordinates
// input: three empty vertices
VSQuadOut main(uint vertexID : SV_VertexID) {
	VSQuadOut result;
	//uint isSecond = ((vertexID >> 2) & 1) | ((vertexID >> 1) & 1) & ((vertexID & 1));
	uint isSecond = vertexID > 2;

	float u = ((1 - isSecond)*((vertexID << 1) & 2) + ((isSecond)*((vertexID << 1) & 2)));
	float v = ((1 - isSecond)*(vertexID & 2) + (isSecond*2)*(1 - (((vertexID)& 1) + ((vertexID >> 2) & 1))));
	result.uv = float2(u, v);
	//result.uv = float2((vertexID << 1) & 2, vertexID & 2);
	result.position = float4(result.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.4f, 1.0f);
	return result;
}