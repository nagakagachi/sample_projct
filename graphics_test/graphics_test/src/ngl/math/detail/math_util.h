#pragma once

#include <cmath>
#include <memory>

#include "math_vector.h"

namespace ngl
{
	namespace math
	{

		struct Plane
		{
			math::Vec3 normal_ = math::Vec3::UnitZ();
			float distance_ = 0.0f;

			Plane() = default;
			Plane(const math::Vec3& plane_pos/*平面の通る点*/, const math::Vec3& normal/*平面の法線*/)
			{
				normal_ = math::Vec3::Normalize(normal);
				distance_ = math::Vec3::Dot(normal_, plane_pos);
			}
		};
		struct Frustum
		{
			struct EPlaneIndex
			{
				enum : int
				{
					TOP_PLANE,
					BOTTOM_PLANE,
					LEFT_PLANE,
					RIGHT_PLANE,
					NEAR_PLANE,
					FAR_PLANE,
						
					_MAX
				};
			};
			Plane planes_[EPlaneIndex::_MAX];
		};
				
		// --
		inline void CreateFrustum(Frustum& out_frustum,
			math::Vec3 view_pos, math::Vec3 view_dir, math::Vec3 view_up, math::Vec3 view_right,
			float near_z, float far_z, float fov_y, float aspect_ratio)
		{
			Frustum     frustum;
			const float half_v_side = far_z * tanf(fov_y * 0.5f);
			const float half_h_side = half_v_side * aspect_ratio;
			const math::Vec3 front_mult_far = far_z * view_dir;

			frustum.planes_[Frustum::EPlaneIndex::NEAR_PLANE] = { view_pos + near_z * view_dir, view_dir };
			frustum.planes_[Frustum::EPlaneIndex::FAR_PLANE] = { view_pos + front_mult_far, -view_dir };
			frustum.planes_[Frustum::EPlaneIndex::RIGHT_PLANE] = { view_pos, math::Vec3::Cross(front_mult_far - view_right * half_h_side, view_up) };
			frustum.planes_[Frustum::EPlaneIndex::LEFT_PLANE] = { view_pos, math::Vec3::Cross(view_up,front_mult_far + view_right * half_h_side) };
			frustum.planes_[Frustum::EPlaneIndex::TOP_PLANE] = { view_pos, math::Vec3::Cross(view_right, front_mult_far - view_up * half_v_side) };
			frustum.planes_[Frustum::EPlaneIndex::BOTTOM_PLANE] = { view_pos, math::Vec3::Cross(front_mult_far + view_up * half_v_side, view_right) };

			out_frustum = frustum;
		}

	}
}
