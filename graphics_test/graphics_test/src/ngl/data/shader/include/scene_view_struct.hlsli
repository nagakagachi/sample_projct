#ifndef NGL_SHADER_SCENE_VIEW_STRUCT_H
#define NGL_SHADER_SCENE_VIEW_STRUCT_H

// nglのmatrix系ははrow-majorメモリレイアウトであるための指定.
#pragma pack_matrix( row_major )


struct SceneViewInfo
{
	float3x4 cb_view_mtx;
	float3x4 cb_view_inv_mtx;
	float4x4 cb_proj_mtx;
	float4x4 cb_proj_inv_mtx;

	// 正規化デバイス座標(NDC)のZ値からView空間Z値を計算するための係数. PerspectiveProjectionMatrixの方式によってCPU側で計算される値を変えることでシェーダ側は同一コード化.
	//	view_z = cb_ndc_z_to_view_z_coef.x / ( ndc_z * cb_ndc_z_to_view_z_coef.y + cb_ndc_z_to_view_z_coef.z )
	//
	//		cb_ndc_z_to_view_z_coef = 
	//			Standard RH: (-far_z * near_z, near_z - far_z, far_z, 0.0)
	//			Standard LH: ( far_z * near_z, near_z - far_z, far_z, 0.0)
	//			Reverse RH: (-far_z * near_z, far_z - near_z, near_z, 0.0)
	//			Reverse LH: ( far_z * near_z, far_z - near_z, near_z, 0.0)
	//			Infinite Far Reverse RH: (-near_z, 1.0, 0.0, 0.0)
	//			Infinite Far Reverse RH: ( near_z, 1.0, 0.0, 0.0)
	float4	cb_ndc_z_to_view_z_coef;

	float	cb_time_sec;
};


// ピクセルへのView空間レイ方向を, ピクセルのスクリーン上UVとProjection行列から計算する.
// ViewZを利用してView空間座標を復元する場合はzが1になるように修正して計算が必要な点に注意( pos_vs.z == ViewZ となるように).
//		pos_view = ViewSpaceRay.xyz/abs(ViewSpaceRay.z) * ViewZ
float3 CalcViewSpaceRay(float2 screen_uv, float4x4 proj_mtx)
{
	float2 ndc_xy = (screen_uv * 2.0 - 1.0) * float2(1.0, -1.0);
	// 逆行列を使わずにProj行列の要素からレイ方向計算.
	const float inv_tan_horizontal = proj_mtx._m00; // m00 = 1/tan(fov_x*0.5)
	const float inv_tan_vertical = proj_mtx._m11; // m11 = 1/tan(fov_y*0.5)
	
	const float3 ray_dir_view = normalize( float3(ndc_xy.x / inv_tan_horizontal, ndc_xy.y / inv_tan_vertical, 1.0) );
	// View空間でのRay方向. World空間へ変換する場合は InverseViewMatrix * ray_dir_view とすること.
	return ray_dir_view;
}


// DirectionalShadow定数バッファ構造定義.
struct SceneDirectionalShadowInfo
{
	float3x4 cb_shadow_view_mtx;
	float3x4 cb_shadow_view_inv_mtx;
	float4x4 cb_shadow_proj_mtx;
	float4x4 cb_shadow_proj_inv_mtx;
};


#endif // NGL_SHADER_SCENE_VIEW_STRUCT_H