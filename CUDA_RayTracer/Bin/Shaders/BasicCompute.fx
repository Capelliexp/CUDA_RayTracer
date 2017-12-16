RWTexture2D<float4> output : register(u0);

cbuffer Cam : register(b0){
	float4 Pos;
	float4 Dir;
};

cbuffer Tri : register(b1) {
	float4 p0;
	float4 p1;
	float4 p2;
	float4 edge0;
	float4 edge1;
	float4 edge2;
	float4 norm;
};

int TriangleTest(float3 dir) {
	///u and v, barycentric coordinates
	///w = (1 - u - v)
	///r(t) = f(u, v)
	///o + td = (1 - u - v)p1 + up2 + vp3

	int returnValue = 0;
	float epsilon = 0.000001f;
	float t = 0.0f;

	float3 q = cross(dir, edge1.xyz);
	float a = dot(q, edge0.xyz);

	if (!((a > -epsilon) && (a < epsilon))) {	//miss?
		float f = 1 / a;
		float3 s = Pos - p0;
		float u = f * dot(s, q);

		if (!(u < 0.0f)) {	//miss?
			float3 r = cross(s, edge0.xyz);
			float v = f * dot(dir, r);

			if (!(v < 0.0f || u + v > 1.0f)) {	//miss?
				t = f * dot(edge1.xyz, r);

				if (t > 0) {	//HIT
					//hit.t = t;
					//hit.lastShape = this;
					//hit.color = c;
					//hit.lastNormal = normal(ray.o + ray.d*t); //Save normal
					returnValue = 1;
				}
			}
			else
				returnValue = -3;
		}
		else
			returnValue = -2;
	}
	else
		returnValue = -1;

	return returnValue;
}

[numthreads(32, 32, 1)]
void main( uint3 threadID : SV_DispatchThreadID ){
	float4 color = { 1, 0, 0, 1 };
	float3 newDir;
	newDir.x = (threadID.x - 400) / 400;
	newDir.y = (threadID.y - 400) / 400;
	newDir.z = 1.f;

	int triTest1 = TriangleTest(Dir);

	if (triTest1 == 1)	//hit
		output[threadID.xy] = float4(0, 1, 0, 1);
	else if (triTest1 == -1)	//miss
		output[threadID.xy] = float4(1, 1, 0, 1);
	else if (triTest1 == -2)	//miss
		output[threadID.xy] = float4(0, 1, 1, 1);
	else if (triTest1 == -3)	//miss
		output[threadID.xy] = float4(1, 0, 1, 1);
	else	//error
		output[threadID.xy] = float4(1, 0, 0, 1);
		

	//output[threadID.xy] = (color + float4(0, triTest1, b, 0));

	//if (newDir.x > 1) {
	//	output[threadID.xy] = float4(1, 0, 0, 1);
	//}
	//else if (newDir.x < 1) {
	//	output[threadID.xy] = float4(0, 1, 0, 1);
	//}
	//else
	//	output[threadID.xy] = float4(float3(1,0,1) * (1 - length(threadID.xy - float2(400, 400)) / 400.0f), 1);
}