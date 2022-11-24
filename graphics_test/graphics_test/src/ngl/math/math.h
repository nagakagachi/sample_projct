#pragma once

#include <memory>

namespace ngl
{
	namespace math
	{
		struct Vec3
		{
			union
			{
				struct { float x, y, z; };

				float data[3];
			};


			Vec3() = default;
			constexpr Vec3(const Vec3& v) = default;
			constexpr Vec3(float v)
				: x(v), y(v), z(v)
			{
			}
			constexpr Vec3(float _x, float _y, float _z)
				: x(_x), y(_y), z(_z)
			{
			}

			static constexpr Vec3 Zero()
			{
				return { 0.0f };
			}
			static constexpr Vec3 One()
			{
				return { 1.0f };
			}
			static constexpr Vec3 UnitX()
			{
				return { 1.0f, 0.0f, 0.0f };
			}
			static constexpr Vec3 UnitY()
			{
				return { 0.0f, 1.0f, 0.0f };
			}
			static constexpr Vec3 UnitZ()
			{
				return { 0.0f, 0.0f, 1.0f };
			}
		};

		struct Vec4
		{
			union
			{
				struct { float x, y, z, w; };

				float data[4];
			};


			Vec4() = default;
			constexpr Vec4(const Vec4& v) = default;
			constexpr Vec4(float v)
				: x(v), y(v), z(v), w(v)
			{
			}
			constexpr Vec4(float _x, float _y, float _z)
				: x(_x), y(_y), z(_z), w(0.0f)
			{
			}
			constexpr Vec4(float _x, float _y, float _z, float _w)
				: x(_x), y(_y), z(_z), w(_w)
			{
			}
			constexpr Vec4(const Vec3& v3)
				: x(v3.x), y(v3.y), z(v3.z), w(0.0f)
			{
			}
			constexpr Vec4(const Vec3& v3, float w)
				: x(v3.x), y(v3.y), z(v3.z), w(w)
			{
			}

			static constexpr Vec4 Zero()
			{
				return { 0.0f };
			}
			static constexpr Vec4 One()
			{
				return { 1.0f };
			}
			static constexpr Vec4 UnitX()
			{
				return { 1.0f, 0.0f, 0.0f, 0.0f };
			}
			static constexpr Vec4 UnitY()
			{
				return { 0.0f, 1.0f, 0.0f, 0.0f };
			}
			static constexpr Vec4 UnitZ()
			{
				return { 0.0f, 0.0f, 1.0f, 0.0f };
			}
			static constexpr Vec4 UnitW()
			{
				return { 0.0f, 0.0f, 0.0f, 1.0f };
			}
		};




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
			constexpr Mat34(float v)
				: r0(v), r1(v), r2(v)
			{
			}
			constexpr Mat34(float _x, float _y, float _z, float _w)
				: r0(_x,_y,_z,_w), r1(_x, _y, _z, _w), r2(_x, _y, _z, _w)
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


			static constexpr Mat34 Zero()
			{
				return {0.0f};
			}
			static constexpr Mat34 Identity()
			{
				return {
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
				};
			}
		};
	}
}