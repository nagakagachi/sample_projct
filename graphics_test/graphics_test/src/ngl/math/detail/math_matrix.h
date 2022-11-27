#pragma once

#include <cmath>
#include <memory>

#include "math_vector.h"

namespace ngl
{
	namespace math
	{

		// -----------------------------------------------------------------------------------------
		struct Mat22
		{
			union
			{
				float	m[2][2];
				struct
				{
					Vec2	r0, r1;
				};
			};

			Mat22() = default;
			constexpr Mat22(const Mat22& v) = default;
			explicit constexpr Mat22(float v)
				: r0(v), r1(v)
			{
			}
			constexpr Mat22(
				float _m00, float _m01,
				float _m10, float _m11)
				: r0(_m00, _m01)
				, r1(_m10, _m11)
			{
			}
			constexpr Mat22(const Vec2& row0, const Vec2& row1)
				: r0(row0)
				, r1(row1)
			{
			}

			static constexpr Mat22 Zero()
			{
				return Mat22(0.0f);
			}
			static constexpr Mat22 Identity()
			{
				return {
					Vec2::UnitX(),
					Vec2::UnitY(),
				};
			}

			// 転置
			static constexpr Mat22 Transpose(const Mat22& m)
			{
				return Mat22(
					m.r0.x, m.r1.x,
					m.r0.y, m.r1.y
				);
			}
			// 行列式.
			static constexpr float Determinant(const Mat22& m)
			{
				return m.r0.x * m.r1.y - m.r0.y * m.r1.x;
			}
			// 逆行列.
			static constexpr Mat22 Inverse(const Mat22& m)
			{
				const float c00 = m.r1.y;
				const float c01 = -m.r1.x;

				const float c10 = -m.r0.y;
				const float c11 = m.r0.x;

				const float det = m.r0.x * c00 + m.r0.y * c01;
				const float inv_det = 1.0f / det;

				return Mat22(
					c00 * inv_det, c10 * inv_det,
					c01 * inv_det, c11 * inv_det
				);
			}
		};
		// Mat + Mat
		inline constexpr Mat22 operator+(const Mat22& m0, const Mat22& m1)
		{
			return Mat22(
				m0.r0 + m1.r0,
				m0.r1 + m1.r1
			);
		}
		// Mat - Mat
		inline constexpr Mat22 operator-(const Mat22& m0, const Mat22& m1)
		{
			return Mat22(
				m0.r0 - m1.r0,
				m0.r1 - m1.r1
			);
		}
		// Mat * Mat
		inline constexpr Mat22 operator*(const Mat22& m0, const Mat22& m1)
		{
			return Mat22(
				m0.r0.x * m1.r0 + m0.r0.y * m1.r1,
				m0.r1.x * m1.r0 + m0.r1.y * m1.r1
			);
		}
		// Mat * scalar
		inline constexpr Mat22 operator*(const Mat22& m, float s)
		{
			return Mat22(
				m.r0 * s,
				m.r1 * s
			);
		}
		// scalar * Mat
		inline constexpr Mat22 operator*(float s, const Mat22& m)
		{
			return m * s;
		}
		// Mat / scalar
		inline constexpr Mat22 operator/(const Mat22& m, float s)
		{
			const auto s_inv = 1.0f / s;
			return m * s_inv;
		}
		// -----------------------------------------------------------------------------------------

		// -----------------------------------------------------------------------------------------
		struct Mat33
		{
			union
			{
				float	m[3][3];
				struct
				{
					Vec3	r0, r1, r2;
				};
			};

			Mat33() = default;
			constexpr Mat33(const Mat33& v) = default;
			explicit constexpr Mat33(float v)
				: r0(v), r1(v), r2(v)
			{
			}
			constexpr Mat33(
				float _m00, float _m01, float _m02,
				float _m10, float _m11, float _m12,
				float _m20, float _m21, float _m22)
				: r0(_m00, _m01, _m02)
				, r1(_m10, _m11, _m12)
				, r2(_m20, _m21, _m22)
			{
			}
			constexpr Mat33(const Vec3& row0, const Vec3& row1, const Vec3& row2)
				: r0(row0)
				, r1(row1)
				, r2(row2)
			{
			}

			static constexpr Mat33 Zero()
			{
				return Mat33(0.0f);
			}
			static constexpr Mat33 Identity()
			{
				return {
					Vec3::UnitX(),
					Vec3::UnitY(),
					Vec3::UnitZ()
				};
			}

			// 転置
			static constexpr Mat33 Transpose(const Mat33& m)
			{
				return Mat33(
					m.r0.x, m.r1.x, m.r2.x,
					m.r0.y, m.r1.y, m.r2.y,
					m.r0.z, m.r1.z, m.r2.z
				);
			}
			// 行列式.
			static constexpr float Determinant(const Mat33& m)
			{
				const float c00 = Mat22::Determinant(Mat22(m.r1.y, m.r1.z, m.r2.y, m.r2.z));
				const float c01 = -Mat22::Determinant(Mat22(m.r1.x, m.r1.z, m.r2.x, m.r2.z));
				const float c02 = Mat22::Determinant(Mat22(m.r1.x, m.r1.y, m.r2.x, m.r2.y));
				return m.r0.x * c00 + m.r0.y * c01 + m.r0.z * c02;
			}
			// 逆行列.
			static constexpr Mat33 Inverse(const Mat33& m)
			{
				const float c00 = Mat22::Determinant(Mat22(m.r1.y, m.r1.z, m.r2.y, m.r2.z));
				const float c01 = -Mat22::Determinant(Mat22(m.r1.x, m.r1.z, m.r2.x, m.r2.z));
				const float c02 = Mat22::Determinant(Mat22(m.r1.x, m.r1.y, m.r2.x, m.r2.y));

				const float c10 = -Mat22::Determinant(Mat22(m.r0.y, m.r0.z, m.r2.y, m.r2.z));
				const float c11 = Mat22::Determinant(Mat22(m.r0.x, m.r0.z, m.r2.x, m.r2.z));
				const float c12 = -Mat22::Determinant(Mat22(m.r0.x, m.r0.y, m.r2.x, m.r2.y));

				const float c20 = Mat22::Determinant(Mat22(m.r0.y, m.r0.z, m.r1.y, m.r1.z));
				const float c21 = -Mat22::Determinant(Mat22(m.r0.x, m.r0.z, m.r1.x, m.r1.z));
				const float c22 = Mat22::Determinant(Mat22(m.r0.x, m.r0.y, m.r1.x, m.r1.y));

				const float det = m.r0.x * c00 + m.r0.y * c01 + m.r0.z * c02;
				const float inv_det = 1.0f / det;

				return Mat33(
					c00 * inv_det, c10 * inv_det, c20 * inv_det,
					c01 * inv_det, c11 * inv_det, c21 * inv_det,
					c02 * inv_det, c12 * inv_det, c22 * inv_det
				);
			}
		};
		// Mat + Mat
		inline constexpr Mat33 operator+(const Mat33& m0, const Mat33& m1)
		{
			return Mat33(
				m0.r0 + m1.r0,
				m0.r1 + m1.r1,
				m0.r2 + m1.r2
			);
		}
		// Mat - Mat
		inline constexpr Mat33 operator-(const Mat33& m0, const Mat33& m1)
		{
			return Mat33(
				m0.r0 - m1.r0,
				m0.r1 - m1.r1,
				m0.r2 - m1.r2
			);
		}
		// Mat * Mat
		inline constexpr Mat33 operator*(const Mat33& m0, const Mat33& m1)
		{
			return Mat33(
				m0.r0.x * m1.r0 + m0.r0.y * m1.r1 + m0.r0.z * m1.r2,
				m0.r1.x * m1.r0 + m0.r1.y * m1.r1 + m0.r1.z * m1.r2,
				m0.r2.x * m1.r0 + m0.r2.y * m1.r1 + m0.r2.z * m1.r2
			);
		}
		// Mat * scalar
		inline constexpr Mat33 operator*(const Mat33& m, float s)
		{
			return Mat33(
				m.r0 * s,
				m.r1 * s,
				m.r2 * s
			);
		}
		// scalar * Mat
		inline constexpr Mat33 operator*(float s, const Mat33& m)
		{
			return m * s;
		}
		// Mat / scalar
		inline constexpr Mat33 operator/(const Mat33& m, float s)
		{
			const auto s_inv = 1.0f / s;
			return m * s_inv;
		}
		// -----------------------------------------------------------------------------------------


		// -----------------------------------------------------------------------------------------
		struct Mat44
		{
			union
			{
				float	m[4][4];
				struct
				{
					Vec4	r0, r1, r2, r3;
				};
			};

			Mat44() = default;
			constexpr Mat44(const Mat44& v) = default;
			explicit constexpr Mat44(float v)
				: r0(v), r1(v), r2(v), r3(v)
			{
			}
			constexpr Mat44(float _x, float _y, float _z, float _w)
				: r0(_x, _y, _z, _w)
				, r1(_x, _y, _z, _w)
				, r2(_x, _y, _z, _w)
				, r3(_x, _y, _z, _w)
			{
			}
			constexpr Mat44(
				float _m00, float _m01, float _m02, float _m03,
				float _m10, float _m11, float _m12, float _m13,
				float _m20, float _m21, float _m22, float _m23,
				float _m30, float _m31, float _m32, float _m33)
				: r0(_m00, _m01, _m02, _m03)
				, r1(_m10, _m11, _m12, _m13)
				, r2(_m20, _m21, _m22, _m23)
				, r3(_m30, _m31, _m32, _m33)
			{
			}
			constexpr Mat44(const Vec4& row0, const Vec4& row1, const Vec4& row2, const Vec4& row3)
				: r0(row0)
				, r1(row1)
				, r2(row2)
				, r3(row3)
			{
			}


			static constexpr Mat44 Zero()
			{
				return Mat44(0.0f);
			}
			static constexpr Mat44 Identity()
			{
				return {
					Vec4::UnitX(),
					Vec4::UnitY(),
					Vec4::UnitZ(),
					Vec4::UnitW(),
				};
			}

			// 転置
			static constexpr Mat44 Transpose(const Mat44& m)
			{
				return Mat44(
					m.r0.x, m.r1.x, m.r2.x, m.r3.x,
					m.r0.y, m.r1.y, m.r2.y, m.r3.y,
					m.r0.z, m.r1.z, m.r2.z, m.r3.z,
					m.r0.w, m.r1.w, m.r2.w, m.r3.w
				);
			}

			// 行列式.
			static constexpr float Determinant(const Mat44& m)
			{
				const float c00 = Mat33::Determinant(Mat33(m.r1.y, m.r1.z, m.r1.w, m.r2.y, m.r2.z, m.r2.w, m.r3.y, m.r3.z, m.r3.w));
				const float c01 = -Mat33::Determinant(Mat33(m.r1.x, m.r1.z, m.r1.w, m.r2.x, m.r2.z, m.r2.w, m.r3.x, m.r3.z, m.r3.w));
				const float c02 = Mat33::Determinant(Mat33(m.r1.x, m.r1.y, m.r1.w, m.r2.x, m.r2.y, m.r2.w, m.r3.x, m.r3.y, m.r3.w));
				const float c03 = -Mat33::Determinant(Mat33(m.r1.x, m.r1.y, m.r1.z, m.r2.x, m.r2.y, m.r2.z, m.r3.x, m.r3.y, m.r3.z));
				const float det = m.r0.x * c00 + m.r0.y * c01 + m.r0.z * c02 + m.r0.w * c03;
				return det;
			}
			// 逆行列.
			static constexpr Mat44 Inverse(const Mat44& m)
			{
				const float c00 = Mat33::Determinant(Mat33(m.r1.y, m.r1.z, m.r1.w, m.r2.y, m.r2.z, m.r2.w, m.r3.y, m.r3.z, m.r3.w));
				const float c01 = -Mat33::Determinant(Mat33(m.r1.x, m.r1.z, m.r1.w, m.r2.x, m.r2.z, m.r2.w, m.r3.x, m.r3.z, m.r3.w));
				const float c02 = Mat33::Determinant(Mat33(m.r1.x, m.r1.y, m.r1.w, m.r2.x, m.r2.y, m.r2.w, m.r3.x, m.r3.y, m.r3.w));
				const float c03 = -Mat33::Determinant(Mat33(m.r1.x, m.r1.y, m.r1.z, m.r2.x, m.r2.y, m.r2.z, m.r3.x, m.r3.y, m.r3.z));

				const float c10 = -Mat33::Determinant(Mat33(m.r0.y, m.r0.z, m.r0.w, m.r2.y, m.r2.z, m.r2.w, m.r3.y, m.r3.z, m.r3.w));
				const float c11 = Mat33::Determinant(Mat33(m.r0.x, m.r0.z, m.r0.w, m.r2.x, m.r2.z, m.r2.w, m.r3.x, m.r3.z, m.r3.w));
				const float c12 = -Mat33::Determinant(Mat33(m.r0.x, m.r0.y, m.r0.w, m.r2.x, m.r2.y, m.r2.w, m.r3.x, m.r3.y, m.r3.w));
				const float c13 = Mat33::Determinant(Mat33(m.r0.x, m.r0.y, m.r0.z, m.r2.x, m.r2.y, m.r2.z, m.r3.x, m.r3.y, m.r3.z));

				const float c20 = Mat33::Determinant(Mat33(m.r0.y, m.r0.z, m.r0.w, m.r1.y, m.r1.z, m.r1.w, m.r3.y, m.r3.z, m.r3.w));
				const float c21 = -Mat33::Determinant(Mat33(m.r0.x, m.r0.z, m.r0.w, m.r1.x, m.r1.z, m.r1.w, m.r3.x, m.r3.z, m.r3.w));
				const float c22 = Mat33::Determinant(Mat33(m.r0.x, m.r0.y, m.r0.w, m.r1.x, m.r1.y, m.r1.w, m.r3.x, m.r3.y, m.r3.w));
				const float c23 = -Mat33::Determinant(Mat33(m.r0.x, m.r0.y, m.r0.z, m.r1.x, m.r1.y, m.r1.z, m.r3.x, m.r3.y, m.r3.z));

				const float c30 = -Mat33::Determinant(Mat33(m.r0.y, m.r0.z, m.r0.w, m.r1.y, m.r1.z, m.r1.w, m.r2.y, m.r2.z, m.r2.w));
				const float c31 = Mat33::Determinant(Mat33(m.r0.x, m.r0.z, m.r0.w, m.r1.x, m.r1.z, m.r1.w, m.r2.x, m.r2.z, m.r2.w));
				const float c32 = -Mat33::Determinant(Mat33(m.r0.x, m.r0.y, m.r0.w, m.r1.x, m.r1.y, m.r1.w, m.r2.x, m.r2.y, m.r2.w));
				const float c33 = Mat33::Determinant(Mat33(m.r0.x, m.r0.y, m.r0.z, m.r1.x, m.r1.y, m.r1.z, m.r2.x, m.r2.y, m.r2.z));

				const float det = m.r0.x * c00 + m.r0.y * c01 + m.r0.z * c02 + m.r0.w * c03;
				const float inv_det = 1.0f / det;

				return Mat44(
					c00 * inv_det, c10 * inv_det, c20 * inv_det, c30 * inv_det,
					c01 * inv_det, c11 * inv_det, c21 * inv_det, c31 * inv_det,
					c02 * inv_det, c12 * inv_det, c22 * inv_det, c32 * inv_det,
					c03 * inv_det, c13 * inv_det, c23 * inv_det, c33 * inv_det
				);
			}
		};

		// Mat + Mat
		inline constexpr Mat44 operator+(const Mat44& m0, const Mat44& m1)
		{
			return Mat44(
				m0.r0 + m1.r0,
				m0.r1 + m1.r1,
				m0.r2 + m1.r2,
				m0.r3 + m1.r3
			);
		}
		// Mat - Mat
		inline constexpr Mat44 operator-(const Mat44& m0, const Mat44& m1)
		{
			return Mat44(
				m0.r0 - m1.r0,
				m0.r1 - m1.r1,
				m0.r2 - m1.r2,
				m0.r3 - m1.r3
			);
		}
		// Mat * Mat
		inline constexpr Mat44 operator*(const Mat44& m0, const Mat44& m1)
		{
			return Mat44(
				m0.r0.x * m1.r0 + m0.r0.y * m1.r1 + m0.r0.z * m1.r2 + m0.r0.w * m1.r3,
				m0.r1.x * m1.r0 + m0.r1.y * m1.r1 + m0.r1.z * m1.r2 + m0.r1.w * m1.r3,
				m0.r2.x * m1.r0 + m0.r2.y * m1.r1 + m0.r2.z * m1.r2 + m0.r2.w * m1.r3,
				m0.r3.x * m1.r0 + m0.r3.y * m1.r1 + m0.r3.z * m1.r2 + m0.r3.w * m1.r3
			);
		}
		// Mat * scalar
		inline constexpr Mat44 operator*(const Mat44& m, float s)
		{
			return Mat44(
				m.r0 * s,
				m.r1 * s,
				m.r2 * s,
				m.r3 * s
			);
		}
		// scalar * Mat
		inline constexpr Mat44 operator*(float s, const Mat44& m)
		{
			return m * s;
		}
		// Mat / scalar
		inline constexpr Mat44 operator/(const Mat44& m, float s)
		{
			const auto s_inv = 1.0f / s;
			return m * s_inv;
		}
		// -----------------------------------------------------------------------------------------


		// row-major Transform Matrix34 が必要な場合の Mat44->Mat34 コンバート用.
		struct Mat34
		{
			union
			{
				float	m[3][4];
				struct
				{
					Vec4	r0, r1, r2;
				};
			};

			Mat34() = default;
			constexpr Mat34(const Mat34& v) = default;
			explicit constexpr Mat34(float v)
				: r0(v), r1(v), r2(v)
			{
			}
			constexpr Mat34(float _x, float _y, float _z, float _w)
				: r0(_x, _y, _z, _w), r1(_x, _y, _z, _w), r2(_x, _y, _z, _w)
			{
			}
			constexpr Mat34(
				float _m00, float _m01, float _m02, float _m03,
				float _m10, float _m11, float _m12, float _m13,
				float _m20, float _m21, float _m22, float _m23)
				: r0(_m00, _m01, _m02, _m03), r1(_m10, _m11, _m12, _m13), r2(_m20, _m21, _m22, _m23)
			{
			}
			constexpr Mat34(const Vec4& row0, const Vec4& row1, const Vec4& row2)
				: r0(row0), r1(row1), r2(row2)
			{
			}
			// from Mat44.
			constexpr Mat34(const Mat44& m)
				: r0(m.r0), r1(m.r1), r2(m.r2)
			{
			}
			// from Mat33.
			constexpr Mat34(const Mat33& m)
				: r0(m.r0.x, m.r0.y, m.r0.z, 0.0f), r1(m.r1.x, m.r1.y, m.r1.z, 0.0f), r2(m.r2.x, m.r2.y, m.r2.z)
			{
			}

			static constexpr Mat34 Zero()
			{
				return Mat34(0.0f);
			}
			static constexpr Mat34 Identity()
			{
				return {
					Vec4::UnitX(),
					Vec4::UnitY(),
					Vec4::UnitZ(),
				};
			}
		};
	}
}