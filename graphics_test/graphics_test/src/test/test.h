

namespace ngl
{
	namespace rhi
	{
		class DeviceDep;
	}
}


	// static constexpr ConstexprString test_str { "constexpr text" };
	template <int LENGTH>
	struct ConstexprString
	{
		static constexpr int k_length = LENGTH;
		static constexpr int k_tail_index = LENGTH - 1;
		// 文字列リテラル->Length推論
		constexpr ConstexprString(const char (&s_literal)[LENGTH])
		{
			for (int i = 0; i < LENGTH; i++) buf[i] = s_literal[i];
			buf[k_tail_index] = '\0';
		}
		char buf[LENGTH] = {};
	};



namespace ngl_test
{
	void TestFunc00(ngl::rhi::DeviceDep* p_device);
}
