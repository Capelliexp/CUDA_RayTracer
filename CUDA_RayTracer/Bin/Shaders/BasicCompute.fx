RWTexture2D<float4> output : register(u0);

struct triStruct {
	float4 p0;
	float4 p1;
	float4 p2;

	float4 edge0;
	float4 edge1;
	float4 edge2;

	float4 norm;
	float4 color;
};
StructuredBuffer<triStruct> StructBuffer : register(t0); 

cbuffer Cam : register(b0){
	float4 Pos;
	float4 Dir;
};

float3 ComputeCameraRay(float pixelX, float pixelY, float3 CamPos, float3 CamLook) {
	float width = 800.0;
	float height = 800.0;

	float normalized_X = (pixelX / width) - 0.5;
	float normalized_Y = (pixelY / height) - 0.5;

	float3 camera_direction = normalize(CamLook - CamPos);

	float3 camera_right = cross(camera_direction, float3(0, 1, 0));
	float3 camera_up = cross(camera_right, camera_direction);

	float3 image_point = normalized_X * camera_right + normalized_Y * camera_up + camera_direction;

	return image_point;
}


int TriangleTest(float3 dir, int triIndex) {
	int returnValue = 0;
	float epsilon = 0.000001f;
	float t = 0.0f;

	float3 q = cross(dir, StructBuffer[triIndex].edge1.xyz);
	float a = dot(q, StructBuffer[triIndex].edge0.xyz);

	if (!((a > -epsilon) && (a < epsilon))){	//miss?
		float f = 1 / a;
		float3 s = Pos - StructBuffer[triIndex].p0;
		float u = f * dot(s, q);

		if (!(u < 0.0f)) {	//miss?
			float3 r = cross(s, StructBuffer[triIndex].edge0.xyz);
			float v = f * dot(dir, r);

			if (!(v < 0.0f || u + v > 1.0f)) {	//miss?
				t = f * dot(StructBuffer[triIndex].edge1.xyz, r);

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
	float3 newDir = ComputeCameraRay(threadID.x, threadID.y, Pos.xyz, Dir.xyz);
	
	for (int i = 0; i < 2; i++) {
		if(TriangleTest(newDir, i) == 1)
			output[threadID.xy] = StructBuffer[i].color;
	}

	
}