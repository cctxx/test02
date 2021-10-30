#include "UnityPrefix.h"
#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/Profiler/TimeHelper.h"

#include "Runtime/Math/Simd/float1.h"
#include "Runtime/Math/Simd/float4.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Math/Simd/quaternion.h"
#include "Runtime/mecanim/math/axes.h"

using namespace math;

const float epsilon = 1e-5f;

SUITE (SimdTests)
{
	struct SimdFixture
	{
		SimdFixture()
		{
		}
		~SimdFixture()
		{
		}
	};

	TEST_FIXTURE(SimdFixture, swizzle)
	{
		constant_float4(value, 1,2,3,4);

		float4 a = float4(0,0,0,0);

		a.x() = 3.f;
		CHECK_CLOSE(3, a.x().tofloat(), epsilon);
		CHECK_CLOSE(0, a.y().tofloat(), epsilon);
		CHECK_CLOSE(0, a.z().tofloat(), epsilon);
		CHECK_CLOSE(0, a.w().tofloat(), epsilon);

		a.x() = value.x();
		CHECK_CLOSE(1, a.x().tofloat(), epsilon);
		CHECK_CLOSE(0, a.y().tofloat(), epsilon);
		CHECK_CLOSE(0, a.z().tofloat(), epsilon);
		CHECK_CLOSE(0, a.w().tofloat(), epsilon);



		a.y() = value.y();
		CHECK_CLOSE(1, a.x().tofloat(), epsilon);
		CHECK_CLOSE(2, a.y().tofloat(), epsilon);
		CHECK_CLOSE(0, a.z().tofloat(), epsilon);
		CHECK_CLOSE(0, a.w().tofloat(), epsilon);

		a.z() = value.z();
		CHECK_CLOSE(1, a.x().tofloat(), epsilon);
		CHECK_CLOSE(2, a.y().tofloat(), epsilon);
		CHECK_CLOSE(3, a.z().tofloat(), epsilon);
		CHECK_CLOSE(0, a.w().tofloat(), epsilon);

		a.w() = value.w();
		CHECK_CLOSE(1, a.x().tofloat(), epsilon);
		CHECK_CLOSE(2, a.y().tofloat(), epsilon);
		CHECK_CLOSE(3, a.z().tofloat(), epsilon);
		CHECK_CLOSE(4, a.w().tofloat(), epsilon);

		a.z() = value.y();
		CHECK_CLOSE(1, a.x().tofloat(), epsilon);
		CHECK_CLOSE(2, a.y().tofloat(), epsilon);
		CHECK_CLOSE(2, a.z().tofloat(), epsilon);
		CHECK_CLOSE(4, a.w().tofloat(), epsilon);

		float1 f = value.w();
		CHECK_CLOSE(4, f.tofloat(), epsilon);

		a = value.wxzy();
		CHECK( all(a == float4(4,1,3,2) ) );

		float4 g(value.wxzy());
		CHECK( all(g == float4(4,1,3,2) ) );


		a = value.xzwy();
		CHECK( all(a == float4(1,3,4,2) ) );

		a = value.xwyz();
		CHECK( all(a == float4(1,4,2,3) ) );

		a = value.wyxz();
		CHECK( all(a == float4(4,2,1,3) ) );

		a = value.zywx();
		CHECK( all(a == float4(3,2,4,1) ) );

		a = value.ywzx();
		CHECK( all(a == float4(2,4,3,1) ) );

		a = value.yzxw();
		CHECK( all(a == float4(2,3,1,4) ) );

		a = value.zxyw();
		CHECK( all(a == float4(3,1,2,4) ) );

		a = value.zwxy();
		CHECK( all(a == float4(3,4,1,2) ) );

		a = value.wwwz();
		CHECK( all(a == float4(4,4,4,3) ) );

		a = value.wwzz();
		CHECK( all(a == float4(4,4,3,3) ) );

		a = value.wzyx();
		CHECK( all(a == float4(4,3,2,1) ) );

		a = value.yxwz();
		CHECK( all(a == float4(2,1,4,3) ) );

	}

	TEST_FIXTURE(SimdFixture, float1_op)
	{
		float ATTRIBUTE_ALIGN(ALIGN4F) v[4];

		float1 b(3.f);
		cvec4f(two, 2.f, 2.f, 2.f, 2.f);
		float1 c(two);
		float4 cx;
		float1 e;
		
		{
		Vstorepf(b.eval(), v, 0);
		CHECK_CLOSE(3.f, v[0], epsilon);
		CHECK_CLOSE(3.f, v[1], epsilon);
		CHECK_CLOSE(3.f, v[2], epsilon);
		CHECK_CLOSE(3.f, v[3], epsilon);

		Vstorepf(c.eval(), v, 0);
		CHECK_CLOSE(2.f, v[0], epsilon);
		CHECK_CLOSE(2.f, v[1], epsilon);
		CHECK_CLOSE(2.f, v[2], epsilon);
		CHECK_CLOSE(2.f, v[3], epsilon);

		cx = float4(10.f,2.f,3.f,4.f);

		float1 d(cx.x());

		Vstorepf(d.eval(), v, 0);
		CHECK_CLOSE(10.f, v[0], epsilon);
		CHECK_CLOSE(10.f, v[1], epsilon);
		CHECK_CLOSE(10.f, v[2], epsilon);
		CHECK_CLOSE(10.f, v[3], epsilon);

		e = cx.y();

		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(2.f, v[0], epsilon);
		CHECK_CLOSE(2.f, v[1], epsilon);
		CHECK_CLOSE(2.f, v[2], epsilon);
		CHECK_CLOSE(2.f, v[3], epsilon);

		e = float1(4.f);
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(4.f, v[0], epsilon);
		CHECK_CLOSE(4.f, v[1], epsilon);
		CHECK_CLOSE(4.f, v[2], epsilon);
		CHECK_CLOSE(4.f, v[3], epsilon);

		e = float1(cx.x());
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(10.f, v[0], epsilon);
		CHECK_CLOSE(10.f, v[1], epsilon);
		CHECK_CLOSE(10.f, v[2], epsilon);
		CHECK_CLOSE(10.f, v[3], epsilon);

		e = cx.z();
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(3.f, v[0], epsilon);
		CHECK_CLOSE(3.f, v[1], epsilon);
		CHECK_CLOSE(3.f, v[2], epsilon);
		CHECK_CLOSE(3.f, v[3], epsilon);

		e += cx.w();
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(7.f, v[0], epsilon);
		CHECK_CLOSE(7.f, v[1], epsilon);
		CHECK_CLOSE(7.f, v[2], epsilon);
		CHECK_CLOSE(7.f, v[3], epsilon);

		e -= cx.x();
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(-3.f, v[0], epsilon);
		CHECK_CLOSE(-3.f, v[1], epsilon);
		CHECK_CLOSE(-3.f, v[2], epsilon);
		CHECK_CLOSE(-3.f, v[3], epsilon);

		e *= cx.y();
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(-6.f, v[0], epsilon);
		CHECK_CLOSE(-6.f, v[1], epsilon);
		CHECK_CLOSE(-6.f, v[2], epsilon);
		CHECK_CLOSE(-6.f, v[3], epsilon);

		e /= cx.z();
		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(-2.f, v[0], epsilon);
		CHECK_CLOSE(-2.f, v[1], epsilon);
		CHECK_CLOSE(-2.f, v[2], epsilon);
		CHECK_CLOSE(-2.f, v[3], epsilon);
		}
		{
		float1 f = e++;
		Vstorepf(f.eval(), v, 0);
		CHECK_CLOSE(-2.f, v[0], epsilon);
		CHECK_CLOSE(-2.f, v[1], epsilon);
		CHECK_CLOSE(-2.f, v[2], epsilon);
		CHECK_CLOSE(-2.f, v[3], epsilon);

		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(-1.f, v[0], epsilon);
		CHECK_CLOSE(-1.f, v[1], epsilon);
		CHECK_CLOSE(-1.f, v[2], epsilon);
		CHECK_CLOSE(-1.f, v[3], epsilon);

		float1 g = ++e;
		Vstorepf(g.eval(), v, 0);
		CHECK_CLOSE(0.f, v[0], epsilon);
		CHECK_CLOSE(0.f, v[1], epsilon);
		CHECK_CLOSE(0.f, v[2], epsilon);
		CHECK_CLOSE(0.f, v[3], epsilon);

		Vstorepf(e.eval(), v, 0);
		CHECK_CLOSE(0.f, v[0], epsilon);
		CHECK_CLOSE(0.f, v[1], epsilon);
		CHECK_CLOSE(0.f, v[2], epsilon);
		CHECK_CLOSE(0.f, v[3], epsilon);


		float1 h(float1::zero());
		Vstorepf(h.eval(), v, 0);
		CHECK_CLOSE(0.f, v[0], epsilon);
		CHECK_CLOSE(0.f, v[1], epsilon);
		CHECK_CLOSE(0.f, v[2], epsilon);
		CHECK_CLOSE(0.f, v[3], epsilon);

		float1 i(float1::one());
		Vstorepf(i.eval(), v, 0);
		CHECK_CLOSE(1.f, v[0], epsilon);
		CHECK_CLOSE(1.f, v[1], epsilon);
		CHECK_CLOSE(1.f, v[2], epsilon);
		CHECK_CLOSE(1.f, v[3], epsilon);

		float1 j(4.f);
		float1 l(3.f);

		float1 m = j + l;
		Vstorepf(m.eval(), v, 0);
		CHECK_CLOSE(7.f, v[0], epsilon);
		CHECK_CLOSE(7.f, v[1], epsilon);
		CHECK_CLOSE(7.f, v[2], epsilon);
		CHECK_CLOSE(7.f, v[3], epsilon);

		float1 n = j - l;
		Vstorepf(n.eval(), v, 0);
		CHECK_CLOSE(1.f, v[0], epsilon);
		CHECK_CLOSE(1.f, v[1], epsilon);
		CHECK_CLOSE(1.f, v[2], epsilon);
		CHECK_CLOSE(1.f, v[3], epsilon);

		float1 o = j * l;
		Vstorepf(o.eval(), v, 0);
		CHECK_CLOSE(12.f, v[0], epsilon);
		CHECK_CLOSE(12.f, v[1], epsilon);
		CHECK_CLOSE(12.f, v[2], epsilon);
		CHECK_CLOSE(12.f, v[3], epsilon);

		float1 p = j / l;
		Vstorepf(p.eval(), v, 0);
		CHECK_CLOSE(4.f/3.f, v[0], epsilon);
		CHECK_CLOSE(4.f/3.f, v[1], epsilon);
		CHECK_CLOSE(4.f/3.f, v[2], epsilon);
		CHECK_CLOSE(4.f/3.f, v[3], epsilon);
		}
		
		bool1 bvalue = float1(4.f) < float1(3.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(4.f) < float1(4.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(4.f) < float1(5.f);
		CHECK( (bool)bvalue );

		bvalue = float1(4.f) <= float1(3.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(4.f) <= float1(4.f);
		CHECK( (bool)bvalue );

		bvalue = float1(4.f) <= float1(5.f);
		CHECK( (bool)bvalue );

		bvalue = float1(4.f) > float1(3.f);
		CHECK( (bool)bvalue );

		bvalue = float1(4.f) > float1(4.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(4.f) > float1(5.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(4.f) >= float1(3.f);
		CHECK( (bool)bvalue );

		bvalue = float1(4.f) >= float1(4.f);
		CHECK( (bool)bvalue );

		bvalue = float1(4.f) >= float1(5.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(10.f) == float1(5.f);
		CHECK( (bool)!bvalue );

		bvalue = float1(10.f) == float1(10.f);
		CHECK( (bool)bvalue );

		bvalue = float1(10.f) != float1(5.f);
		CHECK( (bool)bvalue );

		bvalue = float1(10.f) != float1(10.f);
		CHECK( (bool)!bvalue );

	}

	TEST_FIXTURE(SimdFixture, Operator1)
	{
		float ATTRIBUTE_ALIGN(ALIGN4F) v[4];

		float4 a(1,2,3,4);
		float4 b(4,3,2,1);
		float4 e(54,3,42,2);

		float4 c = a+b;
		CHECK_CLOSE( 5.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 5.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 5.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 5.f, c.w().tofloat(), epsilon);

		c = a+b.wwwz();
		CHECK_CLOSE( 2.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 3.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 4.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 6.f, c.w().tofloat(), epsilon);

		c = a+b.z();
		CHECK_CLOSE( 3.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 4.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 5.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 6.f, c.w().tofloat(), epsilon);

		c = a+b.wwwz()+e.y();
		CHECK_CLOSE( 5.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 6.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 7.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 9.f, c.w().tofloat(), epsilon);

		float4 d = a;
		CHECK_CLOSE( 1.f, d.x().tofloat(), epsilon);
		CHECK_CLOSE( 2.f, d.y().tofloat(), epsilon);
		CHECK_CLOSE( 3.f, d.z().tofloat(), epsilon);
		CHECK_CLOSE( 4.f, d.w().tofloat(), epsilon);

		float1 a1 = float1(10.f);

		d = a+a1;
		CHECK_CLOSE( 11.f, d.x().tofloat(), epsilon);
		CHECK_CLOSE( 12.f, d.y().tofloat(), epsilon);
		CHECK_CLOSE( 13.f, d.z().tofloat(), epsilon);
		CHECK_CLOSE( 14.f, d.w().tofloat(), epsilon);

		a.x() = 0;
		CHECK( all(a == float4(0,2,3,4)) );

		a.y() = float1(12);
		CHECK( all(a == float4(0,12,3,4)) );

		a = float4(1,2,3,4);

		c = a+b;
		CHECK( all(c == float4(5,5,5,5)) );

		c = a*b;
		CHECK( all(c == float4(4.f,6.f,6.f,4.f)) );

		c = a/b;
		CHECK( all(c == float4(1.f/4.f,2.f/3.f,3.f/2.f,4.f/1.f)) );

		c = ++a;
		CHECK( all(c == float4(2.f,3.f,4.f,5.f)) );
		CHECK( all(a == float4(2.f,3.f,4.f,5.f)) );

		c = a++;
		CHECK( all(c == float4(2.f,3.f,4.f,5.f)) );
		CHECK( all(a == float4(3.f,4.f,5.f,6.f)) );

		c += b;
		CHECK( all(c == float4(6.f,6.f,6.f,6.f)) );

		c -= a;
		CHECK( all(c == float4(3.f,2.f,1.f,0.f)) );

		c += 5.f;
		CHECK( all(c == float4(8.f,7.f,6.f,5.f)) );

		c *= b;
		CHECK( all(c == float4(32.f,21.f,12.f,5.f)) );

		c /= b;
		CHECK( all(c == float4(8.f,7.f,6.f,5.f)) );

		c = -c;
		CHECK( all(c == float4(-8.f,-7.f,-6.f,-5.f)) );

		c -= .5f;
		CHECK( all(c == float4(-8.5f,-7.5f,-6.5f,-5.5f)) );

		c *= 2.f;
		CHECK( all(c == float4(-17.f,-15.f,-13.f,-11.f)) );

		c /= 3.f;
		Vstorepf(c.eval(), v, 0);
		CHECK_CLOSE(-17.f/3.f, v[0], epsilon);
		CHECK_CLOSE(-15.f/3.f, v[1], epsilon);
		CHECK_CLOSE(-13.f/3.f, v[2], epsilon);
		CHECK_CLOSE(-11.f/3.f, v[3], epsilon);
	}

	TEST_FIXTURE( SimdFixture, vecexpr1_operator )
	{
		float ATTRIBUTE_ALIGN(ALIGN4F) v[4];

		constant_float4(c,-1,2,-3,4);
		float4 t(5,6,7,8);

		t.x() *= float1(-1.f);
		CHECK( all(t == float4(-5.f, 6.f, 7.f, 8.f)));

		t.y() += float1(4.f);
		CHECK( all(t == float4(-5.f, 10.f, 7.f, 8.f)));

		t.z() -= float1(-2.f);
		CHECK( all(t == float4(-5.f, 10.f, 9.f, 8.f)));

		t.w() /= float1(-2.f);
		CHECK( all(t == float4(-5.f, 10.f, 9.f, -4.f)));

		t.x() *= c.w();
		CHECK( all(t == float4(-20.f, 10.f, 9.f, -4.f)));

		t.y() /= c.z();
		Vstorepf(t.eval(), v, 0);
		CHECK_CLOSE(-20.f, v[0], epsilon);
		CHECK_CLOSE(10.f/-3.f, v[1], epsilon);
		CHECK_CLOSE(9.f, v[2], epsilon);
		CHECK_CLOSE(-4.f, v[3], epsilon);

		t.w() += c.y();
		Vstorepf(t.eval(), v, 0);
		CHECK_CLOSE(-20.f, v[0], epsilon);
		CHECK_CLOSE(10.f/-3.f, v[1], epsilon);
		CHECK_CLOSE(9.f, v[2], epsilon);
		CHECK_CLOSE(-2.f, v[3], epsilon);

		t.z() -= c.x();
		Vstorepf(t.eval(), v, 0);
		CHECK_CLOSE(-20.f, v[0], epsilon);
		CHECK_CLOSE(10.f/-3.f, v[1], epsilon);
		CHECK_CLOSE(10.f, v[2], epsilon);
		CHECK_CLOSE(-2.f, v[3], epsilon);

		float x = -c.x().tofloat();
		CHECK( x == 1.f );
	}


	TEST_FIXTURE( SimdFixture, generic )
	{
		float ATTRIBUTE_ALIGN(ALIGN4F) v[4];

		float4 a(-1.f, -.263f, 345.f, 0.f);
		float4 b(5.f, 2.34f, -12.76f, 54.f);
		float4 c;

		float1 s;

		c = abs(a);
		CHECK( all(c == float4(1.f, .263f, 345.f, 0.f)));

		c = math::clamp(c, float4(0.f, 1.f, 100.f, -2.f), float4(2.f, 3.f, 200.f, -10.f));
		CHECK( all(c == float4(1.f, 1.f, 200.f, -10.f)));

		c = cond(bool4(true), a, b);
		CHECK( all(c == a));

		c = cond(bool4(false), a, b);
		CHECK( all(c == b));
		
		c = cond(a<b, a, b);
		CHECK( all(c == float4(-1.f, -.263f, -12.76f, 0.f)));

		
		a = float4(-1.f, 0.f, 0.f, 0.f);
		b = float4(0.f, 1.f, 0.f, 0.f);
		c = cross(a, b);
		CHECK( all(c == float4(0.f, 0.f, -1.f, 0.f)));

		a = float4(-1.f, 2.f, -4.f, 1.f);
		b = float4(4.f, 1.f, -3.f, 1.f);
		c = cross(a, b);
		CHECK( all(c == float4(-2.f, -19.f, -9.f, 0.f)));

		c = degrees(float4( float(M_PIf), float(M_PI_2f), float(M_PI_4f), 0.f));
		CHECK( all(c == float4(180.f, 90.f, 45.f, 0.f)));

		c = radians(float4(180.f, 90.f, 45.f, 0.f));
		CHECK( all(c == float4( float(M_PIf), float(M_PI_2f), float(M_PI_4f), 0.f)));

		float1 teta = dot(float4(1,0,0,0), float4(0,1,0,0));
		CHECK_CLOSE( 0.f, teta.tofloat(), epsilon);

		teta = dot(float4(1,0,0,0), float4(1,0,0,0));
		CHECK_CLOSE( 1.f, teta.tofloat(), epsilon);

		teta = dot(float4(1,0,0,0), normalize(float4(1,1,0,0)));
		CHECK_CLOSE( 0.70710f, teta.tofloat(), epsilon);

		s = dot(float4( 10.f, 5.f, 2.f, 0.f));
		CHECK_CLOSE( 129.0f, s.tofloat(), epsilon);

		s = length( float4( 1.f, 0.f, 0.f, 0.f));
		CHECK_CLOSE( 1.f, s.tofloat(), epsilon );

		s = length( float4( 10.f, 5.f, 2.f, 0.f));
		CHECK_CLOSE( 11.357816f, s.tofloat(), epsilon);
        
        s = length( float4( 0.f, 0.f, 0.f, 0.f));
		CHECK_CLOSE( 0.f, s.tofloat(), epsilon );

		s = lerp(float1(3), float1(6), float1(.3333333f));
		CHECK_CLOSE( 4.f, s.tofloat(), epsilon);

		c = lerp(float4(1,2,3,4), float4(3,4,5,6), float1(.5f));
		CHECK_CLOSE( 2.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 3.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 4.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 5.f, c.w().tofloat(), epsilon);

		c = lerp(float4(1,2,3,4), float4(3,4,5,6), float4(-.5f,0,1.0,1.5f));
		CHECK_CLOSE( 0.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 2.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 5.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 7.f, c.w().tofloat(), epsilon);

		s = maximum(float4(-1.f, -.263f, 345.f, 0.f));
		CHECK_CLOSE( 345.f, s.tofloat(), epsilon);

		s = minimum(float4(-1.f, -.263f, 345.f, 0.f));
		CHECK_CLOSE( -1.f, s.tofloat(), epsilon);

		c = normalize(float4( 0.f, 0.f, 0.f, 1.f));
		CHECK_CLOSE( 0.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 0.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 0.f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 1.f, c.w().tofloat(), epsilon);

		c = normalize(float4( 10.f, 5.f, 2.f, 0.f));
		CHECK_CLOSE( 0.880451f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 0.440225f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 0.176090f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 0.f, c.w().tofloat(), epsilon);

		c = rcp(float4( -25.f, 45.f, .5f, 1.f));
		CHECK_CLOSE( 1.f/-25.f, c.x().tofloat(), epsilon);
		CHECK_CLOSE( 1.f/45.f, c.y().tofloat(), epsilon);
		CHECK_CLOSE( 1.f/.5f, c.z().tofloat(), epsilon);
		CHECK_CLOSE( 1.f, c.w().tofloat(), epsilon);

		//float4 rsqrt(float4 const& r);

		float f = saturate(-2.f);
		CHECK_CLOSE( 0.f, f, epsilon);

		f = saturate(.5f);
		CHECK_CLOSE( .5f, f, epsilon);

		f = saturate(1.5f);
		CHECK_CLOSE( 1.f, f, epsilon);

		c = saturate(float4( -25.f, 0.f, .5f, 1.5f));
		CHECK( all(c == float4( 0.f, 0.f, .5f, 1.f)));

		f = sgn(-25.f);
		CHECK_CLOSE( -1.f, f, epsilon);

		f = sgn(0.f);
		CHECK_CLOSE( 1.f, f, epsilon);

		f = sgn(3.f);
		CHECK_CLOSE( 1.f, f, epsilon);

		c = sgn(float4( -25.f, 0.f, .5f, 1.5f));
		CHECK( all(c == float4( -1.f, 1.f, 1.f, 1.f)));

		c = sgn(float4( 25.f, 0.f, -.5f, -1.5f));
		CHECK( all(c == float4( 1.f, 1.f, -1.f, -1.f)));
	
		// inconsistant how sgn of -0 is interpreted. should not matter in any real world scenarios
/*		c = sgn(float4( -25.f, -0.f, .5f, -1.5f));
		CHECK( all(c == float4( -1.f, -1.f, 1.f, -1.f)));
*/
		f = sign(-25.f);
		CHECK_CLOSE( -1.f, f, epsilon);

		f = sign(0.f);
		CHECK_CLOSE( 0.f, f, epsilon);

		f = sign(3.f);
		CHECK_CLOSE( 1.f, f, epsilon);

		c = sign(float4( -25.f, 0.f, .5f, 1.5f));
		CHECK( all(c == float4( -1.f, 0.f, 1.f, 1.f)));

		c = sign(float4( 25.f, -0.f, .5f, -1.5f));
		CHECK( all(c == float4( 1.f, 0.f, 1.f, -1.f)));

		c = sqrt(float4( 9.f, 81.f, 49.f, 74.f));
		Vstorepf(c.eval(), v, 0);
		CHECK_CLOSE(3.f, v[0], epsilon);
		CHECK_CLOSE(9.f, v[1], epsilon);
		CHECK_CLOSE(7.f, v[2], epsilon);
		CHECK_CLOSE(8.602325f, v[3], epsilon);

		s = sum(float4( 9.f, 81.f, 49.f, 74.f));
		CHECK_CLOSE( 213.f, s.tofloat(), epsilon);

		c = math::vector(float4( -25.f, 0.f, .5f, 1.5f));
		CHECK( all(c == float4( -25.f, 0.f, .5f, 0.f)));

		a = float4(-1.f, -4.f, 8.f, 0.f);
		b = float4(5.f, 2.f, -2.f, 54.f);
		c = float4( -25.f, 0.f, .5f, 1.5f);
		float4 d = float4(Vmadd(a.eval(),b.eval(),c.eval()));
		CHECK(all(d == float4(-30.f,-8.f,-15.5f,1.5f)));
		
		d = float4(Vmsub(a.eval(),b.eval(),c.eval()));
		CHECK(all(d == float4(20.f,-8.f,-16.5f,-1.5f)));
		
		bool4 bv = bool4(0, 0, 0, 0);
		CHECK( any(bv) == false );

		bv = bool4(0, 1, 0, 0);
		CHECK( any(bv) == true );

		bv = bool4(1, 1, 1, 1);
		CHECK( any(bv) == true );
	}

	TEST_FIXTURE( SimdFixture, quaternion )
	{
		float epsilon = 1e-4f;

		float4 qx(1,0,0,0);
		float4 qy(0,1,0,0);
		float4 qz(0,0,1,0);

		float4 vz(0,0,1,0);

		float4 v;

		v = quatMulVec(qy, vz);
		CHECK( all(v == float4(0.f, 0.f, -1.f, 0.f)));

		v = quatMulVec(qz, vz);
		CHECK( all(v == float4(0.f, 0.f, 1.f, 0.f)));

		v = quatMulVec(qx, vz);
		CHECK( all(v == float4(0.f, 0.f, -1.f, 0.f)));

		float4 euler(radians(-38.22f), radians(16.16f), radians(-45.96f), 0.f );
		float4 q = quatEulerToQuat(euler);
		v = quatMulVec(q, float4(32.21f, 61.03f, -11.19f, 0.f) );
		CHECK_CLOSE( 41.990852f,	v.x().tofloat(), epsilon);
		CHECK_CLOSE( 15.592499f,	v.y().tofloat(), epsilon);
		CHECK_CLOSE( -53.674984f,	v.z().tofloat(), epsilon);
		CHECK_CLOSE( 0.f,			v.w().tofloat(), epsilon);

		float4 q1(radians(-38.22f), radians(16.16f), radians(-45.96f), 0.f );
		float4 q2(radians(79.24f), radians(-1.61f), radians(-33.15f), 0.f );
		float4 q3(radians(38.40f), radians(-6.50f), radians(-70.45f), 0.f);
		q3 = quatEulerToQuat(q3);
		q = quatMul(quatEulerToQuat(q1), quatEulerToQuat(q2));
		CHECK_CLOSE( q.x().tofloat(), q3.x().tofloat(), epsilon);
		CHECK_CLOSE( q.y().tofloat(), q3.y().tofloat(), epsilon);
		CHECK_CLOSE( q.z().tofloat(), q3.z().tofloat(), epsilon);
		CHECK_CLOSE( q.w().tofloat(), q3.w().tofloat(), epsilon);

		q3 = quatConj(q3);
		CHECK_CLOSE( -q.x().tofloat(), q3.x().tofloat(), epsilon);
		CHECK_CLOSE( -q.y().tofloat(), q3.y().tofloat(), epsilon);
		CHECK_CLOSE( -q.z().tofloat(), q3.z().tofloat(), epsilon);
		CHECK_CLOSE( q.w().tofloat(), q3.w().tofloat(), epsilon);
		
		Axes cAxes;
		q3 = ToAxes(cAxes,q2);
		CHECK_CLOSE( 3.121f, q3.x().tofloat(), epsilon);
		CHECK_CLOSE( 0.792092f, q3.y().tofloat(), epsilon);
		CHECK_CLOSE( -0.0492416f, q3.z().tofloat(), epsilon);
		CHECK_CLOSE( 0.0f, q3.w().tofloat(), epsilon);
		
		q3 = FromAxes(cAxes,q3);
		CHECK_CLOSE( 0.922363f, q3.x().tofloat(), epsilon);
		CHECK_CLOSE( -0.0187406f, q3.y().tofloat(), epsilon);
		CHECK_CLOSE( -0.38587f, q3.z().tofloat(), epsilon);
		CHECK_CLOSE( 0.0f, q3.w().tofloat(), epsilon);
		
		Axes aAxiz (float4(0,-0.268f,-0.364f,1),float4(0,-2,1,0),float4(-1,-1,1,1),17,math::kZYRoll);
		q3 = ToAxes(aAxiz,q2);
		CHECK_CLOSE( 1.28582f, q3.x().tofloat(), epsilon);
		CHECK_CLOSE( 1.69701f, q3.y().tofloat(), epsilon);
		CHECK_CLOSE( -0.652772f, q3.z().tofloat(), epsilon);
		CHECK_CLOSE( 0.0f, q3.w().tofloat(), epsilon);
		
		q3 = FromAxes(aAxiz,q3);
		CHECK_CLOSE( 0.922363f, q3.x().tofloat(), epsilon);
		CHECK_CLOSE( -0.0187405f, q3.y().tofloat(), epsilon);
		CHECK_CLOSE( -0.38587f, q3.z().tofloat(), epsilon);
		CHECK_CLOSE( -0.0f, q3.w().tofloat(), epsilon);
	
		
		
		
	/*	float4 left(0.999886f, 0.011893f, -0.006366f, 0);
		float4 up(-0.012257f, 0.998085f, -0.060634f, 0);
		float4 front(0.005632f, 0.060705f, 0.998116f, 0);
		float4 rootX( -0.068884f, -0.000000f, -0.000000f, 0.997625f);

		math::float4 ret = math::normalize(math::quatMul(math::quatMatrixToQuat(left,up,front),math::quatConj(rootX)));
		CHECK_CLOSE(  0.278572f, ret.x().tofloat(), epsilon);
		CHECK_CLOSE(  0.247044f, ret.y().tofloat(), epsilon);
		CHECK_CLOSE(  0.216015f, ret.z().tofloat(), epsilon);
		CHECK_CLOSE(  0.902610f, ret.w().tofloat(), epsilon);
	*/
	}

	TEST_FIXTURE( SimdFixture, trigonometric )
	{

		int degree;
		for(degree=-90;degree<90;degree++)
		{
			float rad = radians( static_cast<float>(degree) );

			float			sin_stl   = math::sin(rad);
			math::float4	sin_unity = math::sin_est( math::float4(rad) );

			CHECK_CLOSE( sin_stl, sin_unity.x().tofloat(), 9.2e-5f);
		}

		for(degree=-90;degree<90;degree++)
		{
			float rad = radians( static_cast<float>(degree) );

			float			cos_stl   = math::cos(rad);
			math::float4	cos_unity = math::cos_est( math::float4(rad) );

			CHECK_CLOSE( cos_stl, cos_unity.x().tofloat(), 9.0e-4);
		}

		/*
		float sin_stl = 0; 
		ABSOLUTE_TIME time_stl = START_TIME;

		for(int i=0;i<1000;i++)
		{
		  
		for(degree=-90;degree<90;degree++)
		{
			float rad = radians( static_cast<float>(degree) );
			sin_stl   += math::sin(rad);
		}
		}

		time_stl = ELAPSED_TIME(time_stl);

		math::float4 sin_unity = math::float4::zero();   
		ABSOLUTE_TIME time_unity = START_TIME;

		for(int i=0;i<1000;i++)
		{
		for(degree=-90;degree<90;degree++)
		{
			float rad = radians( static_cast<float>(degree) );
			sin_unity  += math::sin_est( math::float4(rad) );
		}
		}

		time_unity = ELAPSED_TIME(time_unity);

		CHECK_CLOSE( sin_stl, sin_unity.x().tofloat(), 9.0e-4);
		CHECK_CLOSE( time_stl, time_unity, 1);
		*/
	}
}

#endif
