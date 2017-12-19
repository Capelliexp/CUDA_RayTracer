RWTexture2D<float4> output : register(u0);

struct triStruct {
	float4 p0;
	float4 p1;
	float4 p2;

	float4 edge0;
	float4 edge1;
	float4 edge2;

	float4 triNorm;
	float4 triColor;
};
StructuredBuffer<triStruct> StructBufferTriangle : register(t0);

struct lightStruct {
	float4 lightPos;
	float4 lightColor;
	float lightStrength;
	float padding1, padding2, padding3;
};
StructuredBuffer<lightStruct> StructBufferLight : register(t1);

cbuffer Globals : register(b0){
	float4 camPos;
	float4 camDir;
	int triAmount;
	int globalWidth;
	int globalHeight;
	int globalBounces;
	int lightAmount;
	float padding1, padding2, padding3;
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
	returnValue.range = 0;
	returnValue.hitOrigin = float3(0,0,0);
	float epsilon = 0.000001f;
	float t = 0.0f;

	float3 q = cross(direction, StructBufferTriangle[triIndex].edge1.xyz);
	float a = dot(q, StructBufferTriangle[triIndex].edge0.xyz);

	if (!((a > -epsilon) && (a < epsilon))){	//miss?
		float f = 1 / a;
		float3 s = origin - StructBufferTriangle[triIndex].p0;
		float u = f * dot(s, q);

		if (!(u < 0.0f)) {	//miss?
			float3 r = cross(s, StructBufferTriangle[triIndex].edge0.xyz);
			float v = f * dot(direction, r);

			if (!(v < 0.0f || u + v > 1.0f)) {	//miss?
				t = f * dot(StructBufferTriangle[triIndex].edge1.xyz, r);

				if (t > 0 && t < hitRange) {	//HIT
					returnValue.range = t;
					returnValue.hitOrigin = origin + direction*t;
				}
			}
		}
	}
	return returnValue;
}

float3 CalcReflectionVector(float3 D, float3 N) {
	return reflect(D, N);
}

float CalcLightPower(int triIndex, float3 intersectionPoint) {
	float lin = 0.0014;
	float quadratic = 0.000007;

	float3 triNorm = StructBufferTriangle[triIndex].triNorm.xyz;
	float power = 0;

	if (triNorm.z > 0) triNorm.z *= -1;

	for (int i = 0; i < lightAmount; ++i) {
		float3 lightDir = StructBufferLight[i].lightPos.xyz - intersectionPoint;
		float distance = length(lightDir);
		float attenuation = 1.0f / (1 + lin * distance + quadratic * (distance * distance)); //Attenuation
		lightDir = normalize(lightDir);
		float angle = dot(triNorm, lightDir);
		if (angle > 1.0f) angle = 1.0f;
		power += angle * attenuation;
	}
	return power;
}

struct bounce {
	int triIndex;
	float3 bounceOrig;
	float3 bounceDir;
	float calculatedPower;
};
bounce BounceCalc(float3 origin, float3 direction) {
	float hitRange = 99999999;
	int closestTriIndex = -1;
	float3 closestTriHitOrigin = origin;

	for (int i = 0; i < triAmount; ++i) {
		hit temp = TriangleTest(origin, direction, i, hitRange);
		if (temp.range != 0) {
			hitRange = temp.range;
			closestTriHitOrigin = temp.hitOrigin;
			closestTriIndex = i;
		}
	}

	bounce retValue;
	if (closestTriIndex != -1) {
		retValue.triIndex = closestTriIndex;
		retValue.bounceOrig = origin + (direction*hitRange);
		retValue.bounceDir = CalcReflectionVector(direction, StructBufferTriangle[closestTriIndex].triNorm);
		retValue.calculatedPower = CalcLightPower(closestTriIndex, retValue.bounceOrig);
	}
	else {
		retValue.bounceOrig = origin;
		retValue.bounceDir = direction;
		retValue.calculatedPower = -1;
	}

	return retValue;
}

[numthreads(32, 32, 1)]
void main( uint3 threadID : SV_DispatchThreadID ){
	float3 newDir = ComputeCameraRay(threadID.x, threadID.y, camPos.xyz, camDir.xyz);
	float4 finalColor = float4(0,0,0,0);

	float3 loopOrigin = camPos;
	float3 loopDir = newDir;
	for (int i = 0; i < globalBounces; ++i) {
		bounce thisBounce = BounceCalc(loopOrigin, loopDir);
		if (thisBounce.calculatedPower != -1) {
			finalColor += StructBufferTriangle[thisBounce.triIndex].triColor * thisBounce.calculatedPower;
		}
		loopOrigin = thisBounce.bounceOrig;
		loopDir = thisBounce.bounceDir;
	}
	output[threadID.xy] = finalColor;
}