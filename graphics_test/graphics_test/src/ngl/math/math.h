#pragma once

#include <cmath>
#include <memory>

#include <assert.h>

#include "detail/math_vector.h"
#include "detail/math_matrix.h"

namespace ngl
{
	namespace math
	{

		// -------------------------------------------------------------------------------------------
		// Matrix Vector 演算系
		
		// Mat * Vector(column)
		inline constexpr Vec2 operator*(const Mat22& m, const Vec2& v)
		{
			return Vec2(Vec2::Dot(m.r0, v), Vec2::Dot(m.r1, v));
		}
		// Vector(row) * Mat
		inline constexpr Vec2 operator*(Vec2& v, const Mat22& m)
		{
			return m.r0 * v.x + m.r1 * v.y;
		}

		// Mat * Vector(column)
		inline constexpr Vec3 operator*(const Mat33& m, const Vec3& v)
		{
			return Vec3(Vec3::Dot(m.r0, v), Vec3::Dot(m.r1, v), Vec3::Dot(m.r2, v));
		}
		// Vector(row) * Mat
		inline constexpr Vec3 operator*(Vec3& v, const Mat33& m)
		{
			return m.r0 * v.x + m.r1 * v.y + m.r2 * v.z;
		}

		// Mat * Vector(column)
		inline constexpr Vec4 operator*(const Mat44& m, const Vec4& v)
		{
			return Vec4(Vec4::Dot(m.r0, v), Vec4::Dot(m.r1, v), Vec4::Dot(m.r2, v), Vec4::Dot(m.r3, v));
		}
		// Vector(row) * Mat
		inline constexpr Vec4 operator*(Vec4& v, const Mat44& m)
		{
			return m.r0 * v.x + m.r1 * v.y + m.r2 * v.z + m.r3 * v.w;
		}

		// Mat * Vector(column)
		inline constexpr Vec3 operator*(const Mat34& m, const Vec3& v)
		{
			const Vec4 w1(v, 1);
			return Vec3(Vec4::Dot(m.r0, w1), Vec4::Dot(m.r1, w1), Vec4::Dot(m.r2, w1));
		}




		// ------------------------------------------------------------------------------------------------
		// Utility
		
		// View Matrix (LeftHand).
		inline Mat34 CalcViewMatrixLH(const Vec3& camera_location, const Vec3& forward, const Vec3& up)
		{
			assert(!(forward == Vec3::Zero()));
			assert(!(up == Vec3::Zero()));

			Vec3 r2 = Vec3::Normalize(forward);

			Vec3 r0 = Vec3::Cross(up, r2);
			r0 = Vec3::Normalize(r0);

			Vec3 r1 = Vec3::Cross(r2, r0);

			Vec3 neg_camera_location = -camera_location;
			float d0 = Vec3::Dot(r0, neg_camera_location);
			float d1 = Vec3::Dot(r1, neg_camera_location);
			float d2 = Vec3::Dot(r2, neg_camera_location);

			Mat34 M(
				Vec4(r0, d0),
				Vec4(r1, d1),
				Vec4(r2, d2)
			);

			return M;
		}
		// View Matrix (RightHand).
		inline Mat34 CalcViewMatrixRH(const Vec3& camera_location, const Vec3& forward, const Vec3& up)
		{
			return CalcViewMatrixLH(camera_location, -forward, up);
		}
		


		// ------------------------------------------------------------------------------------------------
		inline void funcAA()
		{
			static constexpr auto  v3_0 = Vec3();
			static constexpr auto  v4_0 = Vec4();

			static constexpr auto  v3_1 = Vec3(1.0f);
			static constexpr auto  v4_1 = Vec4(1.0f);

			static constexpr auto  v3_2 = Vec3(0.0f, 1.0f, 2.0f);
			static constexpr auto  v4_2 = Vec4(0.0f, 1.0f, 2.0f, 3.0f);

			static constexpr Vec2  v2_ux = -Vec2::UnitX();
			static constexpr Vec3  v3_ux = -Vec3::UnitX();
			static constexpr Vec4  v4_ux = -Vec4::UnitX();

			auto v2_ux_ = v2_ux;
			auto v3_ux_ = v3_ux;
			auto v4_ux_ = v4_ux;


			constexpr auto v4_negative = -v4_2;


			ngl::math::Vec3 v3_3(0, 1, 2);
			v3_3 += v3_2;

			ngl::math::Vec4 v4_3(0, 1, 2, 3);
			v4_3 += v4_2;

			v4_3 *= v4_2;
			v4_3 /= v4_1;


			constexpr auto v4_4 = v4_1 + v4_2 - v4_2;

			constexpr auto v_mul0 = ngl::math::Vec4(1.0f) * 2.0f;
			constexpr auto v_mul1 = 0.5f * ngl::math::Vec4(1.0f);

			constexpr auto v_div0 = ngl::math::Vec2(1.0f) / 2.0f;
			constexpr auto v_div1 = 5.0f / ngl::math::Vec4(1.0f, 2, 3, 4);



			ngl::math::Vec3 v3_4(1, 2, 3);
			v3_4 /= ngl::math::Vec3(3, 2, 1) / ngl::math::Vec3(2);

			constexpr auto v3_5 = ngl::math::Vec3(2) / ngl::math::Vec3(4);
			constexpr auto v3_6 = ngl::math::Vec3(2) * ngl::math::Vec3(4);

			constexpr auto v3_dot0 = ngl::math::Vec3::Dot(ngl::math::Vec3::UnitX(), ngl::math::Vec3(2));



			constexpr auto v3_cross_xy = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitX(), ngl::math::Vec3::UnitY());
			constexpr auto v3_cross_yz = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitY(), ngl::math::Vec3::UnitZ());
			constexpr auto v3_cross_zx = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitZ(), ngl::math::Vec3::UnitX());

			constexpr auto v3_cross_yx = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitY(), ngl::math::Vec3::UnitX());
			constexpr auto v3_cross_zy = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitZ(), ngl::math::Vec3::UnitY());
			constexpr auto v3_cross_xz = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitX(), ngl::math::Vec3::UnitZ());

			const auto v3_cross_yx_len = ngl::math::Vec3::Length(v3_cross_yx);
			const auto v3_dot0_len = v3_6.Length();
		}

	}
}