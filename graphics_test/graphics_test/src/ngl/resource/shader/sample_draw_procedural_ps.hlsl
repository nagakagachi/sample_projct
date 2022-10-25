
struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};



float2 rotate2(float2 p, float2 sc) {
    return float2(p.x * sc.y - p.y * sc.x, p.x * sc.x + p.y * sc.y);
}

float sd_box(float2 p, float2 b) {
    float2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float fractal_tree(float2 p, int iters, float a, float f, float w) {
    float r = f;
    p.y = p.y - r;
    float d = 1000.0;
    float2 sc = float2(sin(a), cos(a));
    for (int i = 0; i < iters; ++i) {
        float b = w * r;
        d = min(sd_box(p, float2(b, r + b * 0.5)), d);
        p = float2(abs(p.x), p.y - r);
        r = r * f;
        p = p - r * sc;
        p = rotate2(p, sc);
    }
    return d;
}

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	//return float4(input.uv.x, input.uv.y, 0.0, 1.0);

    const float k_pi = 3.141592653589793238;
    float2 mouse = float2(0.4, 0.7);
    float branch_angle = mouse.x * k_pi / 2.0;
    float branch_factor = mouse.y;
    float w = 0.1;

    float2 uv = input.uv * float2(1, -1) + float2(-0.5, +1.0);
    uv *= 5;

    float d = fractal_tree(uv, 9, branch_angle, branch_factor, w);
    float linewidth = 0.05;
    float3 col = smoothstep(0.0, linewidth, d);
    return float4(col, 1);
}
