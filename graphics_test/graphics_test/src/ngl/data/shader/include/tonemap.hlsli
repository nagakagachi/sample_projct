#ifndef NGL_SHADER_TONEMAP_H
#define NGL_SHADER_TONEMAP_H

// nglのmatrix系ははrow-majorメモリレイアウトであるための指定.
#pragma pack_matrix( row_major )


#define ngl_PI 3.14159265358979323846
#define ngl_EPSILON 1e-6


// Clamp (Simplest).
float3 Tonempa_Clamp(float3 input)
{
    //return clamp(input, 0.0, 1.0);
    return saturate(input);
}

// Reinhard. https://www-old.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf .
float3 Tonempa_Reinhard(float3 input)
{
    return input / (1.0 + input);
}


#endif // NGL_SHADER_TONEMAP_H