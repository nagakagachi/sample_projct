#pragma once

#include "ngl/util/noncopyable.h"
#include "ngl/math/math.h"


namespace ngl
{
namespace gfx
{

	// Mesh Instance Buffer.
	struct InstanceInfo
	{
		ngl::math::Mat34 mtx;
	};

	struct CbSceneView
	{
		ngl::math::Mat34 cb_view_mtx;
		ngl::math::Mat34 cb_view_inv_mtx;
		ngl::math::Mat44 cb_proj_mtx;
		ngl::math::Mat44 cb_proj_inv_mtx;

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
		ngl::math::Vec4	cb_ndc_z_to_view_z_coef;
	};





	static constexpr int k_mesh_vertex_semantic_texcoord_max_count = 4;
	static constexpr int k_mesh_vertex_semantic_color_max_count = 4;
	// サポートしているセマンティクス種別.
	struct EMeshVertexSemanticSlotKind
	{
		enum Type : int
		{
			POSITION,
			NORMAL,
			TANNGENT,
			BINORMAL,
			TEXCOORD,
			COLOR,

			_MAX,
		};
	};
	// セマンティクス種別毎の数とオフセット等.
	struct MeshVertexSemanticSlotCount
	{
		static constexpr int count[] =
		{
			1,
			1,
			1,
			1,
			k_mesh_vertex_semantic_texcoord_max_count,
			k_mesh_vertex_semantic_color_max_count,
		};
		int offset[EMeshVertexSemanticSlotKind::_MAX];

		constexpr MeshVertexSemanticSlotCount()
			: offset()
		{
			offset[0] = 0;
			for (auto i = 1; i < std::size(offset); ++i)
			{
				offset[i] = count[i - 1] + offset[i - 1];
			}
		}
	};

	// セマンティクス種別とインデックスからマッピングされたSlotインデックスの計算等を提供.
	struct MeshVertexSemanticSlotInfo
	{
		static constexpr MeshVertexSemanticSlotCount count{};

		// セマンティクスとそのインデックスからスロット番号を返す.
		//	TEXCOORD, 1 -> 5
		static constexpr int SemanticSlot(EMeshVertexSemanticSlotKind::Type semantic, int semantic_index)
		{
			assert(count.count[semantic] > semantic_index);
			return count.offset[semantic] + semantic_index;
		}
		// 各セマンティクスの最大スロット数を返す.
		static constexpr int SemanticCount(EMeshVertexSemanticSlotKind::Type semantic)
		{
			return count.count[semantic];
		}

	};

}
}
