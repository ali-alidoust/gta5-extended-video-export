struct Output
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

Output main(uint id: SV_VertexID)
{
	Output output;

	output.position = float4(
		((id == 0 | id == 1) * -1) + ((id == 2 | id == 3)) * 1,
		((id == 0 | id == 2) * -1) + ((id == 1 | id == 3)) * 1,
		0, 1);
	
	output.texcoord = float2(
		((id == 0 | id == 1) * 0) + ((id == 2 | id == 3)) * 1,
		((id == 0 | id == 2) * 0) + ((id == 1 | id == 3)) * 1);

	return output;
}