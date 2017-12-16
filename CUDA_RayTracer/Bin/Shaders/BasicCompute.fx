cbuffer Cam : register(b0){
	float4 Pos;
	float4 Dir;
};

cbuffer Tri : register(b1) {
	float4 p1;
	float4 p2;
	float4 p3;
	float4 norm;
};

RWTexture2D<float4> output : register(u0);

[numthreads(32, 32, 1)]
void main( uint3 threadID : SV_DispatchThreadID ){

	if (Dir.z == 1.f) {
		output[threadID.xy] = float4(1, 0, 0, 1);
	}
	else
		output[threadID.xy] = float4(float3(1,0,1) * (1 - length(threadID.xy - float2(400, 400)) / 400.0f), 1);
}