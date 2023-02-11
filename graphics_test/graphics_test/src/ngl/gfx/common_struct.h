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



	template<int LEN>
	struct FixSizeName
	{
		template<int N>
		constexpr FixSizeName(const char (&s)[N])
			: str()
		{
			constexpr auto n = (LEN < N) ? LEN : N;
			str[n] = 0;
			for (auto i = 0; i < n; ++i)
				str[i] = s[i];
		}
		char str[LEN+1];
	};
	// セマンティクス名.
	using SemanticNameType = FixSizeName<32>;


	static constexpr int k_mesh_vertex_semantic_texcoord_max_count = 4;
	static constexpr int k_mesh_vertex_semantic_color_max_count = 4;
	// サポートしているセマンティクス種別.
	struct EMeshVertexSemanticKind
	{
		enum Type : int
		{
			POSITION,
			NORMAL,
			TANNGENT,
			BINORMAL,
			COLOR,
			TEXCOORD,

			_MAX,
		};
		static constexpr SemanticNameType Name[] =
		{
			"POSITION",
			"NORMAL",
			"TANNGENT",
			"BINORMAL",
			"COLOR",
			"TEXCOORD",
		};
		static constexpr int Count[] =
		{
			1,
			1,
			1,
			1,
			k_mesh_vertex_semantic_color_max_count,
			k_mesh_vertex_semantic_texcoord_max_count,
		};
	};

	// セマンティクス種別毎の数とオフセット等.
	struct MeshVertexSemanticSlotOffset
	{
		int offset[EMeshVertexSemanticKind::_MAX];
		int max_slot_count;

		constexpr MeshVertexSemanticSlotOffset()
			: offset()
			, max_slot_count()
		{
			// スロットオフセット計算.
			// メッシュ描画時の各セマンティクスに対応するスロットは固定(TEXCOORD1 -> 5).
			offset[0] = 0;
			for (auto i = 1; i < std::size(offset); ++i)
			{
				offset[i] = EMeshVertexSemanticKind::Count[i - 1] + offset[i - 1];
			}
			max_slot_count = offset[EMeshVertexSemanticKind::_MAX-1] + EMeshVertexSemanticKind::Count[EMeshVertexSemanticKind::_MAX - 1];
		}
	};

	// セマンティクス種別とインデックスからマッピングされたSlotインデックスの計算等を提供.
	struct MeshVertexSemantic
	{
		static constexpr MeshVertexSemanticSlotOffset count{};
		// MeshVertexSemanticSlotMaskで保持するため32以下チェック.
		static_assert(count.max_slot_count <= 32);
		
		// サポートしている最大スロット数.
		static constexpr int SemanticSlotMaxCount()
		{
			return count.max_slot_count;
		}

		// セマンティクスとそのインデックスからスロット番号を返す.
		//	TEXCOORD, 1 -> 5
		static constexpr int SemanticSlot(EMeshVertexSemanticKind::Type semantic, int semantic_index = 0)
		{
			assert(EMeshVertexSemanticKind::Count[semantic] > semantic_index);
			return count.offset[semantic] + semantic_index;
		}
		// 各セマンティクスの最大スロット数を返す.
		static constexpr int SemanticCount(EMeshVertexSemanticKind::Type semantic)
		{
			return EMeshVertexSemanticKind::Count[semantic];
		}

		// セマンティクス名.
		static const SemanticNameType& SemanticName(EMeshVertexSemanticKind::Type semantic)
		{
			return EMeshVertexSemanticKind::Name[semantic];
		}
		// セマンティクス名.
		static const char* SemanticNameStr(EMeshVertexSemanticKind::Type semantic)
		{
			return SemanticName(semantic).str;
		}
	};

	// Meshのセマンティクス有効BitMask. max 32.
	struct MeshVertexSemanticSlotMask
	{
		void Clear()
		{
			mask = 0;
		}
		void AddSlot(EMeshVertexSemanticKind::Type semantic, int semantic_index = 0)
		{
			mask |= (1 << MeshVertexSemantic::SemanticSlot(semantic, semantic_index));
		}
		void RemoveSlot(EMeshVertexSemanticKind::Type semantic, int semantic_index = 0)
		{
			mask &= ~(1 << MeshVertexSemantic::SemanticSlot(semantic, semantic_index));
		}

		const uint32_t& operator()()
		{
			return mask;
		}
		uint32_t mask = 0;
	};

}
}
