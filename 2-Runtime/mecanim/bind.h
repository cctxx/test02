#pragma once


#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"

// Value binders
// inspiration come from C++ template, The complete guide
//
// Bound function parameters to a specific value.
// We do support first and second parameter bounding.
//
// 

namespace mecanim
{
	template<typename rt,	typename p1 = void, 
							typename p2 = void, 
							typename p3 = void, 
							typename p4 = void, 
							typename p5 = void, 
							typename p6 = void>
	class function_ptr_type
	{
	public:
		enum {numParams = 6};
		typedef rt (*function_type)(p1, p2, p3, p4, p5, p6);
	};

	// partial specialization for 5 parameter
	template<typename rt,	typename p1, 
							typename p2, 
							typename p3, 
							typename p4, 
							typename p5>
	class function_ptr_type<rt, p1, p2, p3, p4, p5, void>
	{
	public:
		enum {numParams = 5};
		typedef rt (*function_type)(p1, p2, p3, p4, p5);
	};

	// partial specialization for 4 parameter
	template<typename rt,	typename p1, 
							typename p2, 
							typename p3, 
							typename p4>
	class function_ptr_type<rt, p1, p2, p3, p4, void, void>
	{
	public:
		enum {numParams = 4};
		typedef rt (*function_type)(p1, p2, p3, p4);
	};

	// partial specialization for 3 parameter
	template<typename rt,	typename p1, 
							typename p2, 
							typename p3>
	class function_ptr_type<rt, p1, p2, p3, void, void, void>
	{
	public:
		enum {numParams = 3};
		typedef rt (*function_type)(p1, p2, p3);
	};

	// partial specialization for 2 parameter
	template<typename rt,	typename p1, 
							typename p2>
	class function_ptr_type<rt, p1, p2, void, void, void, void>
	{
	public:
		enum {numParams = 2};
		typedef rt (*function_type)(p1, p2);
	};

	// partial specialization for 1 parameter
	template<typename rt,	typename p1>
	class function_ptr_type<rt, p1, void, void, void, void, void>
	{
	public:
		enum {numParams = 1};
		typedef rt (*function_type)(p1);
	};

	// partial specialization for 0 parameter
	template<typename rt>
	class function_ptr_type<rt, void, void, void, void, void, void>
	{
	public:
		enum {numParams = 0};
		typedef rt (*function_type)();
	};

	template<typename T>
	class forwardparam
	{
	public:
		// class type are passed by copy, 
		// thus invoke the copy constructor
		// In this case we should use a const reference
		typedef T type;
	};

	template<>
	class forwardparam<void>
	{
	private:
		class empty{};
	public:
		typedef empty type;
	};

	// The class function_ptr encapsulate a function pointer,
	// passing function call argument can have a side effect 
	// if the corresponding parameter has a class type, it's copy constructor
	// is invoked. To avoid this extra cost we need to change
	// the class forwardparam, in this case a reference to the corresponding
	// const class should be use.
	template<typename rt,	typename p1 = void, 
							typename p2 = void, 
							typename p3 = void, 
							typename p4 = void, 
							typename p5 = void, 
							typename p6 = void>
	class function_ptr
	{
	private:
		typedef typename function_ptr_type<rt, p1, p2, p3, p4, p5, p6>::function_type function_type;
		function_type mFuntionPtr;
	public:
		enum { numParams = function_ptr_type<rt, p1, p2, p3, p4, p5, p6>::numParams };
		typedef rt return_type;
		typedef p1 parameter_type1;
		typedef p2 parameter_type2;
		typedef p3 parameter_type3;
		typedef p4 parameter_type4;
		typedef p5 parameter_type5;
		typedef p6 parameter_type6;

		function_ptr():mFuntionPtr(0) {}
		function_ptr(function_type ptr):mFuntionPtr(ptr) {}

		return_type operator()() { return mFuntionPtr(); }
		return_type operator()(typename forwardparam<parameter_type1>::type a1) 
		{ 
			return mFuntionPtr(a1); 
		}
		return_type operator()(typename forwardparam<parameter_type1>::type a1, 
								typename forwardparam<parameter_type2>::type a2) 
		{ 
			return mFuntionPtr(a1, a2); 
		}
		return_type operator()(typename forwardparam<parameter_type1>::type a1, 
								typename forwardparam<parameter_type2>::type a2, 
								typename forwardparam<parameter_type3>::type a3) 
		{ 
			return mFuntionPtr(a1, a2, a3); 
		}
		return_type operator()(typename forwardparam<parameter_type1>::type a1, 
								typename forwardparam<parameter_type2>::type a2,
								typename forwardparam<parameter_type3>::type a3, 
								typename forwardparam<parameter_type4>::type a4) 
		{ 
			return mFuntionPtr(a1, a2, a3, a4); 
		}
		return_type operator()(typename forwardparam<parameter_type1>::type a1, 
								typename forwardparam<parameter_type2>::type a2,
								typename forwardparam<parameter_type3>::type a3, 
								typename forwardparam<parameter_type4>::type a4, 
								typename forwardparam<parameter_type5>::type a5) 
		{ 
			return mFuntionPtr(a1, a2, a3, a4, a5); 
		}
		return_type operator()(typename forwardparam<parameter_type1>::type a1, 
								typename forwardparam<parameter_type2>::type a2, 
								typename forwardparam<parameter_type3>::type a3, 
								typename forwardparam<parameter_type4>::type a4, 
								typename forwardparam<parameter_type5>::type a5, 
								typename forwardparam<parameter_type6>::type a6) 
		{ 
			return mFuntionPtr(a1, a2, a3, a4, a5, a6); 
		}
	};

	template<typename signature> class function;

	template<typename rt>
	class function<rt (void)>
	{
	public:
		enum {numParams = 0};
		typedef rt						return_type;
		typedef rt (*function_type)();
		typedef function_ptr<rt,void,void,void,void,void,void> ptr;
	};

	template<typename rt, typename p1>
	class function<rt (p1)>
	{
	public:
		enum {numParams = 1};
		typedef rt						return_type;
		typedef p1						parameter_type1;
		typedef rt (*function_type) (p1);
		typedef function_ptr<rt, p1,void,void,void,void,void> ptr;
	};

	template<typename rt, typename p1, typename p2>
	class function<rt (p1, p2)>
	{
	public:
		enum {numParams = 2};
		typedef rt						return_type;
		typedef p1						parameter_type1;
		typedef p2						parameter_type2;
		typedef rt (*function_type) (p1, p2);
		typedef function_ptr<rt, p1, p2,void,void,void,void> ptr;
	};

	template<typename rt, typename p1, typename p2, typename p3>
	class function<rt (p1, p2, p3)>
	{
	public:
		enum {numParams = 3};
		typedef rt						return_type;
		typedef p1						parameter_type1;
		typedef p2						parameter_type2;
		typedef p3						parameter_type3;
		typedef rt (*function_type) (p1, p2, p3);
		typedef function_ptr<rt, p1, p2, p3,void,void,void> ptr;
	};

	template<typename rt, typename p1, typename p2, typename p3, typename p4>
	class function<rt (p1, p2, p3, p4)>
	{
	public:
		enum {numParams = 4};
		typedef rt						return_type;
		typedef p1						parameter_type1;
		typedef p2						parameter_type2;
		typedef p3						parameter_type3;
		typedef p4						parameter_type4;
		typedef rt (*function_type) (p1, p2, p3, p4);
		typedef function_ptr<rt, p1, p2, p3, p4,void,void> ptr;
	};

	template<typename rt, typename p1, typename p2, typename p3, typename p4, typename p5>
	class function<rt (p1, p2, p3, p4, p5)>
	{
	public:
		enum {numParams = 5};
		typedef rt						return_type;
		typedef p1						parameter_type1;
		typedef p2						parameter_type2;
		typedef p3						parameter_type3;
		typedef p4						parameter_type4;
		typedef p5						parameter_type5;
		typedef rt (*function_type) (p1, p2, p3, p4, p5);
		typedef function_ptr<rt, p1, p2, p3, p4, p5,void> ptr;
	};

	template<typename rt, typename p1, typename p2, typename p3, typename p4, typename p5, typename p6>
	class function<rt (p1, p2, p3, p4, p5, p6)>
	{
	public:
		enum {numParams = 6};
		typedef rt						return_type;
		typedef p1						parameter_type1;
		typedef p2						parameter_type2;
		typedef p3						parameter_type3;
		typedef p4						parameter_type4;
		typedef p5						parameter_type5;
		typedef p6						parameter_type6;
		typedef rt (*function_type) (p1, p2, p3, p4, p5, p6);
		typedef function_ptr<rt, p1, p2, p3, p4, p5, p6> ptr;
	};

	template <typename t> class bound_value_1
	{	
	public:
		typedef t value_type;
		bound_value_1(){}
		bound_value_1(value_type v):value(v){}
		value_type get(){return value;}
	private:
		value_type value;
	};
	template <typename t> class bound_value_2
	{	
	public:
		typedef t value_type;
		bound_value_2(){}
		bound_value_2(value_type v):value(v){}
		value_type get(){return value;}
	private:
		value_type value;
	};
	template <typename t> class bound_value_3
	{	
	public:
		typedef t value_type;
		bound_value_3(){}
		bound_value_3(value_type v):value(v){}
		value_type get(){return value;}
	private:
		value_type value;
	};
	template <typename t> class bound_value_4
	{	
	public:
		typedef t value_type;
		bound_value_4(){}
		bound_value_4(value_type v):value(v){}
		value_type get(){return value;}
	private:
		value_type value;
	};
	template <typename t> class bound_value_5
	{	
	public:
		typedef t value_type;
		bound_value_5(){}
		bound_value_5(value_type v):value(v){}
		value_type get(){return value;}
	private:
		value_type value;
	};
	template <typename t> class bound_value_6
	{	
	public:
		typedef t value_type;
		bound_value_6(){}
		bound_value_6(value_type v):value(v){}
		value_type get(){return value;}
	private:
		value_type value;
	};


	template<typename ft, typename p1> class binder1 : private ft, private bound_value_1<p1>
	{
	public:
		enum { numParams = ft::numParams - 1 };
		
		typedef typename ft::return_type									return_type;		
		typedef typename forwardparam<typename ft::parameter_type2>::type	parameter_type1;
		typedef typename forwardparam<typename ft::parameter_type3>::type	parameter_type2;
		typedef typename forwardparam<typename ft::parameter_type4>::type	parameter_type3;
		typedef typename forwardparam<typename ft::parameter_type5>::type	parameter_type4;
		typedef typename forwardparam<typename ft::parameter_type6>::type	parameter_type5;

		binder1(ft f, bound_value_1<p1> const& value1)
			:ft(f),
			bound_value_1<p1>(value1)
		{
		}

		binder1(ft f, p1 const& value1)
			:ft(f),
			bound_value_1<p1>(bound_value_1<p1>(value1))
		{
		}

		return_type operator()()
		{
			return ft::operator()(bound_value_1<p1>::get());
		}

		return_type operator()(parameter_type1 a1)
		{
			return ft::operator()(bound_value_1<p1>::get(), a1);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2)
		{
			return ft::operator()(bound_value_1<p1>::get(), a1, a2);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2, parameter_type3 a3)
		{
			return ft::operator()(bound_value_1<p1>::get(), a1, a2, a3);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2, parameter_type3 a3, parameter_type4 a4)
		{
			return ft::operator()(bound_value_1<p1>::get(), a1, a2, a3, a4);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2, parameter_type3 a3, parameter_type4 a4, parameter_type5 a5)
		{
			return ft::operator()(bound_value_1<p1>::get(), a1, a2, a3, a4, a5);
		}
	};


	template<typename ft, typename p1, typename p2> class binder2 : private ft, private bound_value_1<p1>, private bound_value_2<p2>
	{
	public:
		enum { numParams = ft::numParams - 2 };
		
		typedef typename ft::return_type									return_type;
		typedef typename forwardparam<typename ft::parameter_type3>::type	parameter_type1;
		typedef typename forwardparam<typename ft::parameter_type4>::type	parameter_type2;
		typedef typename forwardparam<typename ft::parameter_type5>::type	parameter_type3;
		typedef typename forwardparam<typename ft::parameter_type6>::type	parameter_type4;

		binder2()
		{
		}

		binder2(ft f, bound_value_1<p1> const& value1, bound_value_2<p2> const& value2)
			:ft(f),
			bound_value_1<p1>(value1),
			bound_value_2<p2>(value2)
		{
		}

		binder2(ft f, p1 const& value1, p2 const& value2)
			:ft(f),
			bound_value_1<p1>(bound_value_1<p1>(value1)),
			bound_value_2<p2>(bound_value_2<p2>(value2))
		{
		}		

		return_type operator()()
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get());
		}

		return_type operator()(parameter_type1 a1)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(), a1);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(), a1, a2);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2, parameter_type3 a3)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(), a1, a2, a3);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2, parameter_type3 a3, parameter_type4 a4)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(), a1, a2, a3, a4);
		}
	};

	template<typename ft, 
		typename p1, 
		typename p2, 
		typename p3> 
	class binder3 : private ft, 
					private bound_value_1<p1>,
					private bound_value_2<p2>,
					private bound_value_3<p3>
	{
	public:
		enum { numParams = ft::numParams - 3 };

		typedef typename forwardparam<typename ft::parameter_type4>::type	parameter_type1;
		typedef typename forwardparam<typename ft::parameter_type5>::type	parameter_type2;
		typedef typename forwardparam<typename ft::parameter_type6>::type	parameter_type3;
		
		typedef typename ft::return_type									return_type;
		
		binder3()
		{
		}

		binder3(ft f, bound_value_1<p1> const& value1, bound_value_2<p2> const& value2,
				bound_value_3<p3> const& value3)
			:ft(f),
			bound_value_1<p1>(value1),
			bound_value_2<p2>(value2),
			bound_value_3<p3>(value3)
		{
		}

		binder3(ft f, p1 const& value1, p2 const& value2, p3 const& value3)
			:ft(f),
			bound_value_1<p1>(bound_value_1<p1>(value1)),
			bound_value_2<p2>(bound_value_2<p2>(value2)),
			bound_value_3<p3>(bound_value_3<p3>(value3))
		{
		}		

		return_type operator()()
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get());
		}

		return_type operator()(parameter_type1 a1)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), a1);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), a1, a2);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2, parameter_type3 a3)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), a1, a2, a3);
		}
	};


	template<typename ft, 
		typename p1, 
		typename p2, 
		typename p3, 
		typename p4> 
	class binder4 : private ft, 
					private bound_value_1<p1>,
					private bound_value_2<p2>,
					private bound_value_3<p3>,
					private bound_value_4<p4>
	{
	public:
		enum { numParams = ft::numParams - 4 };

		typedef typename forwardparam<typename ft::parameter_type5>::type	parameter_type1;
		typedef typename forwardparam<typename ft::parameter_type6>::type	parameter_type2;
		
		typedef typename ft::return_type									return_type;
		
		binder4()
		{
		}

		binder4(ft f, bound_value_1<p1> const& value1, bound_value_2<p2> const& value2,
				bound_value_3<p3> const& value3, bound_value_4<p4> const& value4)
			:ft(f),
			bound_value_1<p1>(value1),
			bound_value_2<p2>(value2),
			bound_value_3<p3>(value3),
			bound_value_4<p4>(value4)
		{
		}

		binder4(ft f, p1 const& value1, p2 const& value2, p3 const& value3, p4 const& value4)
			:ft(f),
			bound_value_1<p1>(bound_value_1<p1>(value1)),
			bound_value_2<p2>(bound_value_2<p2>(value2)),
			bound_value_3<p3>(bound_value_3<p3>(value3)),
			bound_value_4<p4>(bound_value_4<p4>(value4))
		{
		}		

		return_type operator()()
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), bound_value_4<p4>::get());
		}

		return_type operator()(parameter_type1 a1)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), bound_value_4<p4>::get(), a1);
		}

		return_type operator()(parameter_type1 a1, parameter_type2 a2)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), bound_value_4<p4>::get(), a1, a2);
		}
	};


	template<typename ft, 
		typename p1, 
		typename p2, 
		typename p3, 
		typename p4, 
		typename p5> 
	class binder5 : private ft, 
					private bound_value_1<p1>,
					private bound_value_2<p2>,
					private bound_value_3<p3>,
					private bound_value_4<p4>,
					private bound_value_5<p5>
	{
	public:
		enum { numParams = ft::numParams - 5 };

		typedef typename forwardparam<typename ft::parameter_type6>::type	parameter_type1;
		
		typedef typename ft::return_type									return_type;
		
		binder5()
		{
		}

		binder5(ft f, bound_value_1<p1> const& value1, bound_value_2<p2> const& value2,
				bound_value_3<p3> const& value3, bound_value_4<p4> const& value4,
				bound_value_5<p5> const& value5)
			:ft(f),
			bound_value_1<p1>(value1),
			bound_value_2<p2>(value2),
			bound_value_3<p3>(value3),
			bound_value_4<p4>(value4),
			bound_value_5<p5>(value5)
		{
		}

		binder5(ft f, p1 const& value1, p2 const& value2, p3 const& value3, p4 const& value4, p5 const& value5)
			:ft(f),
			bound_value_1<p1>(bound_value_1<p1>(value1)),
			bound_value_2<p2>(bound_value_2<p2>(value2)),
			bound_value_3<p3>(bound_value_3<p3>(value3)),
			bound_value_4<p4>(bound_value_4<p4>(value4)),
			bound_value_5<p5>(bound_value_5<p5>(value5))
		{
		}		

		return_type operator()()
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), bound_value_4<p4>::get(),
									bound_value_5<p5>::get());
		}

		return_type operator()(parameter_type1 a1)
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), bound_value_4<p4>::get(),
									bound_value_5<p5>::get(), a1);
		}
	};

	template<typename ft, 
		typename p1, 
		typename p2, 
		typename p3, 
		typename p4, 
		typename p5, 
		typename p6> 
	class binder6 : private ft, 
					private bound_value_1<p1>,
					private bound_value_2<p2>,
					private bound_value_3<p3>,
					private bound_value_4<p4>,
					private bound_value_5<p5>,
					private bound_value_6<p6>
	{
	public:
		enum { numParams = ft::numParams - 6 };
		
		typedef typename ft::return_type									return_type;
		
		binder6()
		{
		}

		binder6(ft f, bound_value_1<p1> const& value1, bound_value_2<p2> const& value2,
				bound_value_3<p3> const& value3, bound_value_4<p4> const& value4,
				bound_value_5<p5> const& value5, bound_value_6<p6> const& value6)
			:ft(f),
			bound_value_1<p1>(value1),
			bound_value_2<p2>(value2),
			bound_value_3<p3>(value3),
			bound_value_4<p4>(value4),
			bound_value_5<p5>(value5),
			bound_value_6<p6>(value6)
		{
		}

		binder6(ft f, p1 const& value1, p2 const& value2, p3 const& value3, p4 const& value4, p5 const& value5, p6 const& value6)
			:ft(f),
			bound_value_1<p1>(bound_value_1<p1>(value1)),
			bound_value_2<p2>(bound_value_2<p2>(value2)),
			bound_value_3<p3>(bound_value_3<p3>(value3)),
			bound_value_4<p4>(bound_value_4<p4>(value4)),
			bound_value_5<p5>(bound_value_5<p5>(value5)),
			bound_value_6<p6>(bound_value_6<p6>(value6))
		{
		}		

		return_type operator()()
		{
			return ft::operator()(bound_value_1<p1>::get(), bound_value_2<p2>::get(),
									bound_value_3<p3>::get(), bound_value_4<p4>::get(),
									bound_value_5<p5>::get(), bound_value_6<p6>::get());
		}		
	};

	template<typename rt, typename a1> 
	binder1<function_ptr<rt, a1>, a1 > bind(rt (*function)(a1), a1 value1)
	{
		typedef function_ptr<rt, a1> function_type;
		return binder1<function_type, a1 >(function, value1);
	}
	template<typename rt, typename a1, typename a2> 
	binder1<function_ptr<rt, a1, a2>, a1 > bind(rt (*function)(a1, a2), a1 value1)
	{
		typedef function_ptr<rt, a1, a2> function_type;
		return binder1<function_type, a1 >(function, value1);
	}
	template<typename rt, typename a1, typename a2, typename a3> 
	binder1<function_ptr<rt, a1, a2, a3>, a1 > bind(rt (*function)(a1, a2,a3), a1 value1)
	{
		typedef function_ptr<rt, a1, a2, a3> function_type;
		return binder1<function_type, a1 >(function, value1);
	}
	template<typename rt, typename a1, typename a2, typename a3, typename a4> 
	binder1<function_ptr<rt, a1, a2, a3, a4>, a1 > bind(rt (*function)(a1, a2, a3, a4), a1 value1)
	{
		typedef function_ptr<rt, a1, a2, a3, a4> function_type;
		return binder1<function_type, a1 >(function, value1);
	}
	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5> 
	binder1<function_ptr<rt, a1, a2, a3, a4, a5>, a1 > bind(rt (*function)(a1, a2, a3, a4, a5), a1 value1)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5> function_type;
		return binder1<function_type, a1 >(function, value1);
	}
	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5, typename a6> 
	binder1<function_ptr<rt, a1, a2, a3, a4, a5, a6>, a1 > bind(rt (*function)(a1, a2, a3, a4, a5, a6), a1 value1)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5, a6> function_type;
		return binder1<function_type, a1 >(function, value1);
	}

	template<typename rt, typename a1, typename a2> 
	binder2<function_ptr<rt, a1, a2>, a1, a2 > bind(rt (*function)(a1, a2), a1 value1, a2 value2)
	{
		typedef function_ptr<rt, a1, a2> function_type;
		return binder2<function_type, a1, a2 >(function, value1, value2);
	}
	template<typename rt, typename a1, typename a2, typename a3> 
	binder2<function_ptr<rt, a1, a2, a3>, a1, a2 > bind(rt (*function)(a1, a2, a3), a1 value1, a2 value2)
	{
		typedef function_ptr<rt, a1, a2, a3> function_type;
		return binder2<function_type, a1, a2 >(function, value1, value2);
	}
	template<typename rt, typename a1, typename a2, typename a3, typename a4> 
	binder2<function_ptr<rt, a1, a2, a3, a4>, a1, a2 > bind(rt (*function)(a1, a2, a3, a4), a1 value1, a2 value2)
	{
		typedef function_ptr<rt, a1, a2, a3, a4> function_type;
		return binder2<function_type, a1, a2 >(function, value1, value2);
	}
	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5> 
	binder2<function_ptr<rt, a1, a2, a3, a4, a5>, a1, a2 > bind(rt (*function)(a1, a2, a3, a4, a5), a1 value1, a2 value2)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5> function_type;
		return binder2<function_type, a1, a2 >(function, value1, value2);
	}
	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5, typename a6> 
	binder2<function_ptr<rt, a1, a2, a3, a4, a5, a6>, a1, a2 > bind(rt (*function)(a1, a2, a3, a4, a5, a6), a1 value1, a2 value2)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5, a6> function_type;
		return binder2<function_type, a1, a2 >(function, value1, value2);
	}


	template<typename rt, typename a1, typename a2, typename a3> 
	binder3<function_ptr<rt, a1, a2, a3>, a1, a2, a3 > bind(rt (*function)(a1, a2, a3), a1 value1, a2 value2, a3 value3)
	{
		typedef function_ptr<rt, a1, a2, a3> function_type;
		return binder3<function_type, a1, a2, a3>(function, value1, value2, value3);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4> 
	binder3<function_ptr<rt, a1, a2, a3, a4>, a1, a2, a3 > bind(rt (*function)(a1, a2, a3, a4), a1 value1, a2 value2, a3 value3)
	{
		typedef function_ptr<rt, a1, a2, a3, a4> function_type;
		return binder3<function_type, a1, a2, a3>(function, value1, value2, value3);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5> 
	binder3<function_ptr<rt, a1, a2, a3, a4, a5>, a1, a2, a3 > bind(rt (*function)(a1, a2, a3, a4, a5), a1 value1, a2 value2, a3 value3)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5> function_type;
		return binder3<function_type, a1, a2, a3>(function, value1, value2, value3);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5, typename a6> 
	binder3<function_ptr<rt, a1, a2, a3, a4, a5, a6>, a1, a2, a3 > bind(rt (*function)(a1, a2, a3, a4, a5, a6), a1 value1, a2 value2, a3 value3)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5, a6> function_type;
		return binder3<function_type, a1, a2, a3>(function, value1, value2, value3);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4> 
	binder4<function_ptr<rt, a1, a2, a3, a4>, a1, a2, a3, a4 > bind(rt (*function)(a1, a2, a3, a4), a1 value1, a2 value2, a3 value3, a4 value4)
	{
		typedef function_ptr<rt, a1, a2, a3, a4> function_type;
		return binder4<function_type, a1, a2, a3, a4>(function, value1, value2, value3, value4);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5> 
	binder4<function_ptr<rt, a1, a2, a3, a4, a5>, a1, a2, a3, a4 > bind(rt (*function)(a1, a2, a3, a4, a5), a1 value1, a2 value2, a3 value3, a4 value4)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5> function_type;
		return binder4<function_type, a1, a2, a3, a4>(function, value1, value2, value3, value4);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5, typename a6> 
	binder4<function_ptr<rt, a1, a2, a3, a4, a5, a6>, a1, a2, a3, a4 > bind(rt (*function)(a1, a2, a3, a4, a5, a6), a1 value1, a2 value2, a3 value3, a4 value4)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5, a6> function_type;
		return binder4<function_type, a1, a2, a3, a4>(function, value1, value2, value3, value4);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5> 
	binder5<function_ptr<rt, a1, a2, a3, a4, a5>, a1, a2, a3, a4, a5 > bind(rt (*function)(a1, a2, a3, a4, a5), a1 value1, a2 value2, a3 value3, a4 value4, a5 value5)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5> function_type;
		return binder5<function_type, a1, a2, a3, a4, a5>(function, value1, value2, value3, value4, value5);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5, typename a6> 
	binder5<function_ptr<rt, a1, a2, a3, a4, a5, a6>, a1, a2, a3, a4, a5 > bind(rt (*function)(a1, a2, a3, a4, a5, a6), a1 value1, a2 value2, a3 value3, a4 value4, a5 value5)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5, a6> function_type;
		return binder5<function_type, a1, a2, a3, a4, a5>(function, value1, value2, value3, value4, value5);
	}

	template<typename rt, typename a1, typename a2, typename a3, typename a4, typename a5, typename a6> 
	binder6<function_ptr<rt, a1, a2, a3, a4, a5, a6>, a1, a2, a3, a4, a5, a6 > bind(rt (*function)(a1, a2, a3, a4, a5, a6), a1 value1, a2 value2, a3 value3, a4 value4, a5 value5, a6 value6)
	{
		typedef function_ptr<rt, a1, a2, a3, a4, a5, a6> function_type;
		return binder6<function_type, a1, a2, a3, a4, a5, a6 >(function, value1, value2, value3, value4, value5, value6);
	}	
}
