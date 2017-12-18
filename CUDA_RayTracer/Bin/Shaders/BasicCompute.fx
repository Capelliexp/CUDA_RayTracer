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

cbuffer Globals : register(b0){
	float4 camPos;
	float4 camDir;
	int triAmount;
	int globalWidth;
	int globalHeight;
	int globalBounces;
};

float3 ComputeCameraRay(float pixelX, float pixelY, float3 CamPos, float3 CamLook) {
	float width = globalWidth;
	float height = globalHeight;

	float normalized_X = (pixelX / width) - 0.5;
	float normalized_Y = (pixelY / height) - 0.5;

	float3 camera_direction = normalize(CamLook - CamPos);

	float3 camera_right = cross(camera_direction, float3(0, 1, 0));
	float3 camera_up = cross(camera_right, camera_direction);

	float3 image_point = normalized_X * camera_right + normalized_Y * camera_up + camera_direction;

	return image_point;
}

struct hit {
	float range;
	float3 hitOrigin;
};

hit TriangleTest(float3 origin, float3 direction, int triIndex, float hitRange) {
	hit returnValue;
	returnValue.range = 0.f;
	returnValue.hitOrigin = float3(0,0,0);
	float epsilon = 0.000001f;
	float t = 0.0f;

	float3 q = cross(direction, StructBuffer[triIndex].edge1.xyz);
	float a = dot(q, StructBuffer[triIndex].edge0.xyz);

	if (!((a > -epsilon) && (a < epsilon))){	//miss?
		float f = 1 / a;
		float3 s = origin - StructBuffer[triIndex].p0;
		float u = f * dot(s, q);

		if (!(u < 0.0f)) {	//miss?
			float3 r = cross(s, StructBuffer[triIndex].edge0.xyz);
			float v = f * dot(direction, r);

			if (!(v < 0.0f || u + v > 1.0f)) {	//miss?
				t = f * dot(StructBuffer[triIndex].edge1.xyz, r);

				if (t > 0 && t < hitRange) {	//HIT
					returnValue.range = t;
					returnValue.hitOrigin = origin + direction*t;
				}
			}
		}
	}
	return returnValue;
}

float3 CalcReflection(float3 D, float3 N) {

	return float3(0, 0, 0);
}

[numthreads(32, 32, 1)]
void main( uint3 threadID : SV_DispatchThreadID ){
	float3 newDir = ComputeCameraRay(threadID.x, threadID.y, camPos.xyz, camDir.xyz);
	float hitRange = 99999999;
	int closestTriIndex = -1;

	for (int i = 0; i < triAmount; ++i) {
		hit temp = TriangleTest(camPos, newDir, i, hitRange);
		if (temp.range != 0) {
			hitRange = temp.range;
			closestTriIndex = i;
		}
	}

	//for (int i = 0; i < globalBounces; ++i) {	//bounce light around

	//}

	output[threadID.xy] = StructBuffer[closestTriIndex].color;
}