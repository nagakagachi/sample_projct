

RaytracingAccelerationStructure	rt_as;
RWTexture2D<float4>				out_uav;

[shader("raygeneration")]
void rayGen()
{
	uint3 ray_index = DispatchRaysIndex();
	
	float2 col = frac(ray_index.xy / 100.0);

	out_uav[ray_index.xy] = float4(col.x, col.y, 0.0, 0.0);
}

struct Payload
{
	uint hit;
};

[shader("miss")]
void miss(inout Payload payload)
{
	payload.hit = 0;
}

[shader("closesthit")]
void closestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.hit = 1;
}