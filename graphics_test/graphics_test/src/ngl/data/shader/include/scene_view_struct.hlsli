

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
};

