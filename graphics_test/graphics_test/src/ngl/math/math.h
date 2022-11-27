#pragma once

#include <cmath>
#include <memory>

#include "detail/math_vector.h"
#include "detail/math_matrix.h"

namespace ngl
{
	namespace math
	{

		// -------------------------------------------------------------------------------------------
		// Matrix Vector 演算系
		// 
		// Mat * Vector(column)
		inline constexpr Vec2 operator*(const Mat22& m, const Vec2& v)
		{
			return Vec2(Vec2::dot(m.r0, v), Vec2::dot(m.r1, v));
		}
		// Vector(row) * Mat
		inline constexpr Vec2 operator*(Vec2& v, const Mat22& m)
		{
			return m.r0 * v.x + m.r1 * v.y;
		}

		// Mat * Vector(column)
		inline constexpr Vec3 operator*(const Mat33& m, const Vec3& v)
		{
			return Vec3(Vec3::dot(m.r0, v), Vec3::dot(m.r1, v), Vec3::dot(m.r2, v));
		}
		// Vector(row) * Mat
		inline constexpr Vec3 operator*(Vec3& v, const Mat33& m)
		{
			return m.r0 * v.x + m.r1 * v.y + m.r2 * v.z;
		}

		// Mat * Vector(column)
		inline constexpr Vec4 operator*(const Mat44& m, const Vec4& v)
		{
			return Vec4(Vec4::dot(m.r0, v), Vec4::dot(m.r1, v), Vec4::dot(m.r2, v), Vec4::dot(m.r3, v));
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
			return Vec3(Vec4::dot(m.r0, w1), Vec4::dot(m.r1, w1), Vec4::dot(m.r2, w1));
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

			constexpr auto v3_dot0 = ngl::math::Vec3::dot(ngl::math::Vec3::UnitX(), ngl::math::Vec3(2));



			constexpr auto v3_cross_xy = ngl::math::Vec3::cross(ngl::math::Vec3::UnitX(), ngl::math::Vec3::UnitY());
			constexpr auto v3_cross_yz = ngl::math::Vec3::cross(ngl::math::Vec3::UnitY(), ngl::math::Vec3::UnitZ());
			constexpr auto v3_cross_zx = ngl::math::Vec3::cross(ngl::math::Vec3::UnitZ(), ngl::math::Vec3::UnitX());

			constexpr auto v3_cross_yx = ngl::math::Vec3::cross(ngl::math::Vec3::UnitY(), ngl::math::Vec3::UnitX());
			constexpr auto v3_cross_zy = ngl::math::Vec3::cross(ngl::math::Vec3::UnitZ(), ngl::math::Vec3::UnitY());
			constexpr auto v3_cross_xz = ngl::math::Vec3::cross(ngl::math::Vec3::UnitX(), ngl::math::Vec3::UnitZ());

			const auto v3_cross_yx_len = ngl::math::Vec3::length(v3_cross_yx);
			const auto v3_dot0_len = v3_6.length();
		}

	}
}