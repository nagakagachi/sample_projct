#pragma once

#include <cmath>
#include <memory>

namespace ngl
{
	namespace math
	{

		template<int N>
		struct VecComponentT
		{
		};

		// Vec2 Data.
		template<>
		struct VecComponentT<2>
		{
			// data.
			union
			{
				struct
				{
					float x, y;
				};
				float data[2];
			};

			VecComponentT() = default;
			constexpr VecComponentT(const VecComponentT& v) = default;
			explicit constexpr VecComponentT(float v)
				: x(v) , y(v)
			{}
			constexpr VecComponentT(float _x, float _y, float _dummy0, float _dummy1)
				: x(_x) , y(_y)
			{}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecComponentT(float _x, float _y, float _dummy0, float _dummy1, ComponentModifyFunc func)
				: x(func(_x)) , y(func(_y))
			{}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecComponentT(const float* _data, ComponentModifyFunc func)
				: x(func(_data[0])) , y(func(_data[1]))
			{}
		};

		// Vec3 Data.
		template<>
		struct VecComponentT<3>
		{
			// data.
			union
			{
				struct
				{
					float x, y, z;
				};
				float data[3];
			};

			VecComponentT() = default;
			constexpr VecComponentT(const VecComponentT& v) = default;
			explicit constexpr VecComponentT(float v)
				: x(v) , y(v) , z(v)
			{}
			constexpr VecComponentT(float _x, float _y, float _z, float _dummy0)
				: x(_x) , y(_y) , z(_z)
			{}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecComponentT(float _x, float _y, float _z, float _dummy0, ComponentModifyFunc func)
				: x(func(_x)) , y(func(_y)), z(func(_z))
			{}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecComponentT(const float* _data, ComponentModifyFunc func)
				: x(func(_data[0])) , y(func(_data[1])) , z(func(_data[2]))
			{}
		};

		// Vec4 Data.
		template<>
		struct VecComponentT<4>
		{
			// data.
			union
			{
				struct
				{
					float x, y, z, w;
				};
				float data[4];
			};

			VecComponentT() = default;
			constexpr VecComponentT(const VecComponentT& v) = default;
			explicit constexpr VecComponentT(float v)
				: x(v) , y(v) , z(v) , w(v)
			{}
			constexpr VecComponentT(float _x, float _y, float _z, float _w)
				: x(_x) , y(_y) , z(_z) , w(_w)
			{}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecComponentT(float _x, float _y, float _z, float _w, ComponentModifyFunc func)
				: x(func(_x)) , y(func(_y)) , z(func(_z)) , w(func(_w))
			{}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecComponentT(const float* _data, ComponentModifyFunc func)
				: x(func(_data[0])) , y(func(_data[1])) , z(func(_data[2])) , w(func(_data[3]))
			{}
		};

		// VectorN Template. N-> 2,3,4.
		template<int N>
		struct VecN : public VecComponentT<N>
		{
			static constexpr int DIMENSION = N;

			constexpr VecN() = default;
			constexpr VecN(const VecN& v) = default;

			explicit constexpr VecN(float v)
				: VecComponentT<N>(v)
			{
			}
			constexpr VecN(float _x, float _y)
				: VecComponentT<N>(_x, _y, 0, 0)
			{
			}
			constexpr VecN(float _x, float _y, float _z)
				: VecComponentT<N>(_x, _y, _z, 0)
			{
			}
			constexpr VecN(float _x, float _y, float _z, float _w)
				: VecComponentT<N>(_x, _y, _z, _w)
			{
			}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecN(float _x, float _y, float _z, float _w, ComponentModifyFunc func)
				: VecComponentT<N>(_x, _y, _z, _w, func)
			{
			}
			// 要素修正付きコンストラクタ.
			template<typename ComponentModifyFunc>
			constexpr VecN(const float* _data, ComponentModifyFunc func)
				: VecComponentT<N>(_data, func)
			{
			}
			

			// += operator Broadcast.
			VecN& operator +=(const VecN& v)
			{
				for(auto i = 0; i < N; ++i)
					this->data[i] += v.data[i];
				return *this;
			}
			// -= operator Broadcast.
			VecN& operator -=(const VecN& v)
			{
				for (auto i = 0; i < N; ++i)
					this->data[i] -= v.data[i];
				return *this;
			}
			// *= operator Broadcast.
			VecN& operator *=(const VecN& v)
			{
				for (auto i = 0; i < N; ++i)
					this->data[i] *= v.data[i];
				return *this;
			}
			// /= operator Broadcast.
			VecN& operator /=(const VecN& v)
			{
				for (auto i = 0; i < N; ++i)
					this->data[i] /= v.data[i];
				return *this;
			}
			// *= operator Broadcast.
			VecN& operator *=(const float v)
			{
				for (auto i = 0; i < N; ++i)
					this->data[i] *= v;
				return *this;
			}
			// /= operator Broadcast.
			VecN& operator /=(const float v)
			{
				for (auto i = 0; i < N; ++i)
					this->data[i] /= v;
				return *this;
			}

			float length() const
			{
				return length(*this);
			}

			static constexpr float dot(const VecN& v0, const VecN& v1)
			{
				if constexpr (2 == DIMENSION)
					return v0.x * v1.x + v0.y * v1.y;
				else if constexpr (3 == DIMENSION)
					return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
				else if constexpr (4 == DIMENSION)
					return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z + v0.w * v1.w;
			}
			static constexpr VecN cross(const VecN& v0, const VecN& v1)
			{
				if constexpr (3 == DIMENSION)
				{
					return VecN(
						v0.y * v1.z - v1.y * v0.z,
						v0.z * v1.x - v1.z * v0.x, 
						v0.x * v1.y - v1.x * v0.y);
				}
			}
			static float length(const VecN& v0)
			{
				return std::sqrt(dot(v0, v0));
			}

			static constexpr VecN Zero()
			{
				return VecN(0.0f);
			}
			static constexpr VecN One()
			{
				return VecN(1.0f);
			}
			static constexpr VecN UnitX()
			{
				return VecN(1.0f, 0.0f, 0.0f, 0.0f);
			}
			static constexpr VecN UnitY()
			{
				return VecN(0.0f, 1.0f, 0.0f, 0.0f);
			}
			static constexpr VecN UnitZ()
			{
				return VecN(0.0f, 0.0f, 1.0f, 0.0f);
			}
			static constexpr VecN UnitW()
			{
				return VecN(0.0f, 0.0f, 0.0f, 1.0f);
			}
		};
		
		using Vec2 = VecN<2>;
		using Vec3 = VecN<3>;
		using Vec4 = VecN<4>;


		namespace
		{
			// Func(Vec, Vec)
			template<typename VecType, typename BinaryOpType>
			inline constexpr VecType VecTypeBinaryOp(const VecType& v0, const VecType& v1, BinaryOpType op)
			{
				if constexpr (2 == VecType::DIMENSION)
					return VecType(op(v0.x, v1.x), op(v0.y, v1.y));
				else if constexpr (3 == VecType::DIMENSION)
					return VecType(op(v0.x, v1.x), op(v0.y, v1.y), op(v0.z, v1.z));
				else if constexpr (4 == VecType::DIMENSION)
					return VecType(op(v0.x, v1.x), op(v0.y, v1.y), op(v0.z, v1.z), op(v0.w, v1.w));
			}

			// Func(Vec, float)
			template<typename VecType, typename BinaryOpType>
			inline constexpr VecType VecTypeBinaryOp(const VecType& v0, const float v1, BinaryOpType op)
			{
				if constexpr (2 == VecType::DIMENSION)
					return VecType(op(v0.x, v1), op(v0.y, v1));
				else if constexpr (3 == VecType::DIMENSION)
					return VecType(op(v0.x, v1), op(v0.y, v1), op(v0.z, v1));
				else if constexpr (4 == VecType::DIMENSION)
					return VecType(op(v0.x, v1), op(v0.y, v1), op(v0.z, v1), op(v0.w, v1));
			}
		}

		// -Vec
		template<typename VecType>
		inline constexpr VecType operator-(const VecType& v)
		{
			constexpr auto func = [](float e) {return -e; };

			if constexpr (2 == VecType::DIMENSION)
				return VecType(v.x, v.y, 0, 0, func);
			else if constexpr (3 == VecType::DIMENSION)
				return VecType(v.x, v.y, v.z, 0, func);
			else if constexpr (4 == VecType::DIMENSION)
				return VecType(v.x, v.y, v.z, v.w, func);
		}
		// Vec+Vec
		template<typename VecType>
		inline constexpr VecType operator+(const VecType& v0, const VecType& v1)
		{
			constexpr auto op = [](float e0, float e1) {return e0 + e1; };
			return VecTypeBinaryOp(v0, v1, op);
		}
		// Vec-Vec
		template<typename VecType>
		inline constexpr VecType operator-(const VecType& v0, const VecType& v1)
		{
			constexpr auto op = [](float e0, float e1) {return e0 - e1; };
			return VecTypeBinaryOp(v0, v1, op);
		}
		// Vec*Vec
		template<typename VecType>
		inline constexpr VecType operator*(const VecType& v0, const VecType& v1)
		{
			constexpr auto op = [](float e0, float e1) {return e0 * e1; };
			return VecTypeBinaryOp(v0, v1, op);
		}
		// Vec/Vec
		template<typename VecType>
		inline constexpr VecType operator/(const VecType& v0, const VecType& v1)
		{
			constexpr auto op = [](float e0, float e1) {return e0 / e1; };
			return VecTypeBinaryOp(v0, v1, op);
		}

		// Vec*float
		template<typename VecType>
		inline constexpr VecType operator*(const VecType& v0, const float v1)
		{
			constexpr auto op = [](float e0, float e1) {return e0 * e1; };
			return VecTypeBinaryOp(v0, v1, op);
		}
		// float*Vec
		template<typename VecType>
		inline constexpr VecType operator*(const float v0, const VecType& v1)
		{
			return v1 * v0;
		}
		// Vec/float
		template<typename VecType>
		inline constexpr VecType operator/(const VecType& v0, const float v1)
		{
			return v0 * (1.0f / v1);
		}
		// float/Vec
		template<typename VecType>
		inline constexpr VecType operator/(const float v0, const VecType& v1)
		{
			return VecType(v0) / v1;
		}




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



			ngl::math::Vec3 v3_4(1,2,3);
			v3_4 /= ngl::math::Vec3(3,2,1) / ngl::math::Vec3(2);

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
		};

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

			constexpr Mat34(const Mat44& m)
				: r0(m.r0), r1(m.r1), r2(m.r2)
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