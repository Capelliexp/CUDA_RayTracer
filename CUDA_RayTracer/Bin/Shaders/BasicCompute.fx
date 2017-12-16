//--------------------------------------------------------------------------------------
// BasicCompute.fx
// Direct3D 11 Shader Model 5.0 Demo
// Copyright (c) Stefan Petersson, 2013
//--------------------------------------------------------------------------------------

RWStructuredBuffer<int> output : register(u0);

cbuffer cbComputationData : register(b0)
{
	uint numBlocksX;
	uint numBlocksY;
};


// For info, see http://msdn.microsoft.com/en-us/library/windows/desktop/ff476405(v=vs.85).aspx

[numthreads(NUM_THREADS_X, NUM_THREADS_Y, 1)]
void main(
	uint3 DispatchThreadID	: SV_DispatchThreadID,
	uint3 GroupThreadID		: SV_GroupThreadID,
	uint3 GroupID			: SV_GroupID,
	uint  GroupIndex		: SV_GroupIndex
	)
{
	//calculate 1D output index
	int outputIndex = (GroupID.y * numBlocksX + GroupID.x) * (NUM_THREADS_X * NUM_THREADS_Y) + GroupIndex;

	output[outputIndex] = pow(outputIndex, 2);
}