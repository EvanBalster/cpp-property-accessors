#ifndef EDB_PROPERTY_ACCESSOR_H
#define EDB_PROPERTY_ACCESSOR_H


/*
	This header implements property accessors for C++.

	Using this technique you may define a union-based "property block" to make a set of synthetic
	variables based on a set of actual variables.

	The most common use is to hide indirection -- ie, when an object refers to another object,
	the first object can have properties providing access to the second object's variables.
	Proxy property accessors are well-suited for this use.

	These can also be used to model derived quantities and values, e.g. representing an angular
	quantity in both degrees and radians or otherwise facilitating multiple representations.
	In this case you may use value property accessors.
*/


#include <utility>
#include <type_traits>


#if !defined(PROPERTY_ACCESS_NO_MACROS)

	/*
		PropertyAccessors(ACTUAL_STRUCT, ...) generates a group of property accessors.
			This macro encapsulates the complete function of this library.

		The first argument ACTUAL_STRUCT refers to a struct type containing the actual variables,
			which will be accessible in all subsequent EXPRESSIONs.
			Note: you may define an unnamed struct inline but it must not contain commas.

		Each subsequent argument must be one of the following pseudo-macros.  Up to 69 may be used.

		Proxy  (TYPE, NAME, REF_EXPRESSION)                                -- Proxy (reference) property.
		GetOnly(TYPE, NAME, GET_EXPRESSION)                                -- Read-only value property.
		GetSet (TYPE, NAME, GET_EXPRESSION, SET_PARAMETER, SET_EXPRESSION) -- Read-write value property.
		Custom (NAME, ...GET/SET...)                                       -- property based on custom getter/setter.
		UnionMember(...)                                  -- Adds declarations verbatim to the union.  Use with care!

		Arguments to these macros are as follows:

		TYPE           -- the type referenced and imitated by this property.  Do not include trailing &.
		NAME           -- the name of this property accessor.
		REF_EXPRESSION -- an expression yielding an lvalue reference to TYPE, using variables from ACTUAL_STRUCT.
		GET_EXPRESSION -- an expression returning a value of type TYPE, using variables from ACTUAL_STRUCT.
		SET_PARAMETER  -- a parameter declaration for the set expression.
		SET_EXPRESSION -- an expression that changes the value to SET_PARAMETER.
		...GET/SET...  -- implement any number of get() and set() methods yourself, using variables from ACTUAL_STRUCT.
		*                 (Custom properties enable greater control over const correctness and overloading set())

		e.g:

			struct Object
			{
				int x;
				int mass() const {return 5+x*10;}
			};
			struct MyObjectPtr {Object *object;};

			PropertyAccessors(MyObjectPtr,

				// Adding this declaration to the union makes the object pointer visible as a class member.
				UnionMember(Object *object;),

				// This property acts as a reference to x and can be treated like an int.
				Proxy  (int, x,    object->x),

				// This read-only property makes the mass() function look like a const int.
				GetOnly(int, mass, object->mass()),

				// Two different ways to implement a get-set property.
				GetSet (int, x_times_2,                          object->x*2,           int x2,  object->x = x2/2),
				Custom (     x_times_3,  int get() const {return object->x*3;} void set(int x3) {object->x = x3/3;})
			);
	*/
	#define PropertyAccessors(ACTUAL_STRUCT, ...) \
		\
		struct _properties {using _property_actual_t = ACTUAL_STRUCT;  EDB_PP_MAP(EDB_PropertyAccessors_Setup, __VA_ARGS__) };\
		union {      _properties::_property_actual_t _property_actual; EDB_PP_MAP(EDB_PropertyAccessors_Union, __VA_ARGS__) }


	/*
		This macro enables property accessors to more closely mimic objects by enabling the dot operator (.)
			for listed member variables and member functions.
			This works by template specialization.  The macro must be placed outside any namespace,
			and must be visible to any property accessor declarations using the specified type.

		TYPE      -- the class/struct/union type to specialize.
		VARIABLES -- either `NoVariables` or `Variables(a,b,c)` replacing a,b,c by member variables to expose.
		METHODS   -- either `NoMethods` or `Methods(f1,f2,f3)` replacing f1,f2,f3 by member functions to expose.

		This macro supports forwarding up to 69 variables and 69 methods.

		e.g:

			struct vector2D {float x, y;  float norm() const {return x*x + y*y;}};

			PropertyAccess_Members(vector2D, Variables(x, y), Methods(norm));
	*/
#if __cplusplus >= 202000L || _MSVC_LANG >= 202000L
	#define PropertyAccess_Members(TYPE, VARIABLES, METHODS) \
		template<typename GetSet_t> struct property_access::members<TYPE, GetSet_t> { \
			using _property_class_t = TYPE; \
			EDB_PropertyMembers_Argument_ ## VARIABLES \
			EDB_PropertyMembers_Argument_ ## METHODS   \
			~members() = default; ~members() noexcept requires (!std::is_trivially_destructible_v<TYPE>) {} }
#else
	#define PropertyAccess_Members(TYPE, VARIABLES, METHODS) \
		template<typename GetSet_t> struct property_access::members<TYPE, GetSet_t> { \
			using _property_class_t = TYPE; \
			EDB_PropertyMembers_Argument_ ## VARIABLES \
			EDB_PropertyMembers_Argument_ ## METHODS   \
			~members() noexcept {} }
#endif



	// implementation details of the PropertyAccessors macro.
	#define EDB_PropertyAccessors_Setup(CALL) EDB_PropertyAccessors_Setup_ ## CALL
	#define EDB_PropertyAccessors_Union(CALL) EDB_PropertyAccessors_Union_ ## CALL

	#define EDB_PropertyAccessors_Setup_UnionMember(...)
	#define EDB_PropertyAccessors_Setup_Proxy(  TYPE, NAME, REF_EXPR)                      struct _gs_ ## NAME : _property_actual_t {  TYPE& get() const {return (REF_EXPR);}  };
	#define EDB_PropertyAccessors_Setup_GetOnly(TYPE, NAME, GET_EXPR)                      struct _gs_ ## NAME : _property_actual_t {  TYPE  get() const {return (GET_EXPR);}  };
	#define EDB_PropertyAccessors_Setup_GetSet( TYPE, NAME, GET_EXPR, SET_PARAM, SET_EXPR) struct _gs_ ## NAME : _property_actual_t {  TYPE  get() const {return (GET_EXPR);}  void set(SET_PARAM) {(SET_EXPR);}  };
	#define EDB_PropertyAccessors_Setup_Custom(NAME, ...)                                  struct _gs_ ## NAME : _property_actual_t {__VA_ARGS__};

	#define EDB_PropertyAccessors_Union_UnionMember(...) __VA_ARGS__
	#define EDB_PropertyAccessors_Union_Proxy(  TYPE, NAME, ...) property_access::property<_properties::_gs_ ## NAME> NAME;
	#define EDB_PropertyAccessors_Union_GetOnly(TYPE, NAME, ...) property_access::property<_properties::_gs_ ## NAME> NAME;
	#define EDB_PropertyAccessors_Union_GetSet( TYPE, NAME, ...) property_access::property<_properties::_gs_ ## NAME> NAME;
	#define EDB_PropertyAccessors_Union_Custom(NAME, ...)        property_access::property<_properties::_gs_ ## NAME> NAME;

	// Implementation details of the PropertyAccess_Members macro.
	#define EDB_PropertyMembers_Variable(NAME) \
		property_access::member<GetSet_t, &_property_class_t::NAME> NAME;

	#define EDB_PropertyMembers_Method(METHOD) \
		template<typename...A> decltype(auto) METHOD(A&&...a) const    {return _property_getset.get().METHOD(std::forward<A>(a)...);} \
		template<typename...A> decltype(auto) METHOD(A&&...a)          {return _property_getset.get().METHOD(std::forward<A>(a)...);}

	#define EDB_PropertyMembers_Argument_Variables(...) union {GetSet_t _property_getset; EDB_PP_MAP(EDB_PropertyMembers_Variable, __VA_ARGS__)};
	#define EDB_PropertyMembers_Argument_NoVariables    union {GetSet_t _property_getset;};
	#define EDB_PropertyMembers_Argument_Methods(...) EDB_PP_MAP(EDB_PropertyMembers_Method, __VA_ARGS__)
	#define EDB_PropertyMembers_Argument_NoMethods

#endif //!defined(PROPERTY_ACCESS_NO_MACROS)



namespace property_access
{
	template<typename GetSet_t>
	using getter_result_t = decltype(std::declval<GetSet_t&>().GetSet_t::get());

	template<typename GetSet_t, typename Value_t = getter_result_t<GetSet_t>>
	using setter_result_t = decltype(std::declval<GetSet_t&>().GetSet_t::set(std::declval<Value_t>));

	namespace detail
	{
		template<typename GetSet_t, typename T>
		struct has_setter_impl
		{
			template<typename U> static auto check(int) -> decltype(std::declval<U>().set(std::declval<T>()), std::true_type{});
			template<typename U> static std::false_type check(...);

			static constexpr bool value = decltype(check<GetSet_t>(0))::value;
		};
		template<typename GetSet_t, typename Y>
		static constexpr bool has_setter = has_setter_impl<GetSet_t, Y>::value;


		/*
			This template detects if a type is a property accessor by checking for the presence of a member named _property_accessor_tag.
				Reference qualifiers will be ignored.
		*/
		template<typename T, typename = void> struct is_property_accessor : public std::bool_constant<false> {}; \
		template<typename T>
		struct is_property_accessor<T, std::void_t<decltype(std::remove_reference_t<T>::_property_accessor_tag)>> : public std::bool_constant<true> {};

		template<typename T> constexpr bool is_property_accessor_v = is_property_accessor<T>::value;


		template<typename To, typename GetterResult_t>
		static constexpr bool prohibit_fwd_convert_v = std::is_rvalue_reference_v<To> || std::is_same_v<std::decay_t<GetterResult_t>, std::decay_t<To>>;

		template<typename To, typename GetterResult_t>
		static constexpr bool misc_convertible_explicit_v = !prohibit_fwd_convert_v<To, GetterResult_t> && std::is_constructible_v<To, GetterResult_t>;

		template<typename To, typename GetterResult_t>
		static constexpr bool misc_convertible_implicit_v = !prohibit_fwd_convert_v<To, GetterResult_t> && std::is_convertible_v<GetterResult_t, To>;


		// This type allows using -> to access members of a value property accessor's values.
		template<typename T>
		struct arrow_operator
		{
			const T _v;
			const T* operator->() const {return &_v;}

			template<typename M>
			decltype(auto) operator->*(M &&m) const    {return _v->*std::forward<M>(m);}

			static arrow_operator<T> apply(T t)    {return {std::move(t)};}
		};

		template<typename T>
		struct arrow_operator<T&>    {static T* apply(T &t) {return &t;}};


#define EDB_tmp_DetectablePropertyOption(OPTION) \
			template<typename T, typename = void>struct option_ ## OPTION                                                           : public std::bool_constant<false> {}; \
			template<typename T>                 struct option_ ## OPTION<T, std::void_t<decltype(T::_property_option_ ## OPTION)>> : public std::bool_constant<T::_property_option_ ## OPTION> {};

		EDB_tmp_DetectablePropertyOption(pointer_emulation)
		EDB_tmp_DetectablePropertyOption(implicit_conversion)

#undef EDB_tmp_DetectPropertyOption
	}


	/*
		Specializable base type for all property accessors.  This is a customization point
			which may be used to add named methods and members to a property accessor, usually for
			the purpose of making it behave even more like the target type.
		
		The memory layout of any specialization must be identical to the type 'GetSet_t'.
	*/
	template<typename T, typename GetSet_t, typename Enable = void>
	struct members
	{
		/*
			Any specialization of members<T, GetSet_t> must provide a variable matching this one
				and its memory layout (size, alignment, contents) must be identical to GetSet_t.
				It may be placed in an anonymous union to facilitate member variable sub-properties.
		*/
		GetSet_t _property_getset;
	};

	/*
		Default specializations for class, struct and union type property accessors.
			In order to facilitate access to member variables, we add pointer-like semantics.
	*/
	template<typename T, typename GetSet_t>
	struct members<T, GetSet_t, std::enable_if_t<(std::is_class_v<T> || std::is_union_v<T>)>>
	{
		GetSet_t _property_getset;

		// If T is a class/struct/union type.
		static constexpr bool _property_option_pointer_emulation = true;
	};

	
	// Boilerplate for forwarding binary and unary operators.
#define EDB_tmp_FwdBiOp(OP)           EDB_tmp_FwdBiOp_  (OP, const) EDB_tmp_FwdBiOp_  (OP, )
#define EDB_tmp_FwdPrefOp(OP)         EDB_tmp_FwdPrefOp_(OP, const) EDB_tmp_FwdPrefOp_(OP, )
#define EDB_tmp_FwdPostOp(OP)         EDB_tmp_FwdPostOp_(OP, const) EDB_tmp_FwdPostOp_(OP, )
#define EDB_tmp_FwdBiOp_(OP, CONST)   template<typename Y, std::enable_if_t<!detail::is_property_accessor_v<Y>, bool> = true> \
    decltype(auto) operator OP (Y &&y) CONST {return this->_property_get() OP std::forward<Y>(y);}
#define EDB_tmp_FwdPrefOp_(OP, CONST) decltype(auto) operator OP ()    CONST {return OP this->_property_get();}
#define EDB_tmp_FwdPostOp_(OP, CONST) decltype(auto) operator OP (int) CONST {return this->_property_get() OP;}


	/*
		Implementation details shared by all property accessors.
	*/
	template<typename GetSet_t>
	struct property : public members<std::decay_t<getter_result_t<GetSet_t>>, GetSet_t>
	{
		using _property_get_t       = getter_result_t<      GetSet_t>;
		using _property_get_const_t = getter_result_t<const GetSet_t>;
		using _property_members_t = members<std::decay_t<_property_get_t>, GetSet_t>;

		static_assert(std::is_object_v<_property_get_const_t> || std::is_lvalue_reference_v<_property_get_const_t>,
			"Property accessor's get() function must return either an object (ie, a value type) or an lvalue reference.");

		// Classify the property accessor.
		static constexpr bool _property_by_proxy = std::is_lvalue_reference_v<_property_get_const_t>;
		static constexpr bool _property_by_value = std::is_object_v          <_property_get_const_t>;

		// TODO is this constraint necessary?
		static_assert( ! ( _property_by_proxy && detail::has_setter<GetSet_t, _property_get_const_t> ),
			"A property accessor whose get() function returns a reference may not have a set() function.");

		/*
			Validate any specialization of members<T,GetSet>.
		*/
		static_assert(sizeof (_property_members_t) == sizeof (GetSet_t));
		static_assert(alignof(_property_members_t) == alignof(GetSet_t));

		// Metadata about this property accessor type.
		static struct {}      _property_accessor_tag;
		static constexpr bool _property_option_pointer_emulation   = detail::option_pointer_emulation  <_property_members_t>::value;
		static constexpr bool _property_option_implicit_conversion = detail::option_implicit_conversion<_property_members_t>::value;

		// Get methods.
		decltype(std::declval<const GetSet_t>().get()) _property_get() const    {return this->_property_getset.get();}
		decltype(std::declval<      GetSet_t>().get()) _property_get()          {return this->_property_getset.get();}

		// Set methods, if applicable.
		template<typename Y, std::enable_if_t<_property_by_proxy || detail::has_setter<const GetSet_t,Y>, bool> = true>
		decltype(auto) _property_set(Y &&y) const    {if constexpr (_property_by_proxy) return this->_property_get() = std::forward<Y>(y); else return this->_property_getset.set(std::forward<Y>(y));}
		template<typename Y, std::enable_if_t<_property_by_proxy || detail::has_setter<      GetSet_t,Y>, bool> = true>
		decltype(auto) _property_set(Y &&y)          {if constexpr (_property_by_proxy) return this->_property_get() = std::forward<Y>(y); else return this->_property_getset.set(std::forward<Y>(y));}

		/*
			Support implicit conversion to the getter's return type.
		*/
		operator _property_get_const_t()  const    {return this->_property_get();}
		operator _property_get_t      ()           {return this->_property_get();}

		/*
			Properties can be explicitly converted to any type that the getter's return type
				is explicitly or implicitly convertible to.
				Implicit conversion to types other than the result of get() is disabled by default,
				because there is no way to treat the real type as "preferred" in overload resolution.
		*/
#if __cplusplus >= 202000L || _MSVC_LANG >= 202000L
		// With explicit operator support
		template<typename T, typename = std::enable_if_t<detail::misc_convertible_explicit_v<T, _property_get_const_t>>>
		explicit(!_property_option_implicit_conversion || !detail::misc_convertible_implicit_v<T, _property_get_const_t>)
		operator T() const    {return T(this->_property_get());}
		template<typename T, typename = std::enable_if_t<detail::misc_convertible_explicit_v<T, _property_get_t      >>>
		explicit(!_property_option_implicit_conversion || !detail::misc_convertible_implicit_v<T, _property_get_t      >)
		operator T()          {return T(this->_property_get());}
#else
		// Without explicit operator support
		template<typename T, typename = std::enable_if_t<detail::misc_convertible_explicit_v<T, _property_get_const_t>>>
		explicit operator T() const             {return   this->_property_get();}
		template<typename T, typename = std::enable_if_t<detail::misc_convertible_explicit_v<T, _property_get_t      >>>
		explicit operator T()                   {return   this->_property_get();}
#endif

		/*
			Forward function-call operator and array subscript operator.
		*/
		template<typename...A> decltype(auto) operator()(A&&...a) const    {return this->_property_get()(std::forward<A>(a)...);}
		template<typename...A> decltype(auto) operator()(A&&...a)          {return this->_property_get()(std::forward<A>(a)...);}
#if __cplusplus >= 202302L || _MSVC_LANG >= 202302L
		template<typename...I> decltype(auto) operator[](I&&...i) const    {return this->_property_get()[std::forward<I>(i)...];}
		template<typename...I> decltype(auto) operator[](I&&...i)          {return this->_property_get()[std::forward<I>(i)...];}
#else
		template<typename   I> decltype(auto) operator[](I&&   i) const    {return this->_property_get()[std::forward<I>(i)   ];}
		template<typename   I> decltype(auto) operator[](I&&   i)          {return this->_property_get()[std::forward<I>(i)   ];}
#endif

		/*
			Forward regular unary and binary operators (arithmetic, logical and comparisons).
				In the case of value property accessors, these are assumed not to mutate the value.
		*/
		EDB_tmp_FwdBiOp(>)    EDB_tmp_FwdBiOp(>=)   EDB_tmp_FwdBiOp(<)    EDB_tmp_FwdBiOp(<=)  
		EDB_tmp_FwdBiOp(==)   EDB_tmp_FwdBiOp(!=)  
		EDB_tmp_FwdPrefOp(+)  EDB_tmp_FwdPrefOp(-)  EDB_tmp_FwdPrefOp(!)  EDB_tmp_FwdPrefOp(~) 
		EDB_tmp_FwdBiOp(+)    EDB_tmp_FwdBiOp(-)    EDB_tmp_FwdBiOp(*)    EDB_tmp_FwdBiOp(/)   
		EDB_tmp_FwdBiOp(%)    EDB_tmp_FwdBiOp(<<)   EDB_tmp_FwdBiOp(>>)  
		EDB_tmp_FwdBiOp(&)    EDB_tmp_FwdBiOp(|)    EDB_tmp_FwdBiOp(^)   

		/*
			The address-of operator is only supported when the property's get() function returns a reference.
		*/
		EDB_tmp_FwdPrefOp(&)

		/*
			Forward the dereference operator and arrow operator(s).
				If _property_option_pointer_emulation is enabled (such as with unspecialized class/struct union)
				these will instead make the property itself act as a pointer to its value.
		*/
		decltype(auto) operator* () const    {if constexpr (_property_option_pointer_emulation) return this->_property_get(); else return *this->_property_get();}
		decltype(auto) operator* ()          {if constexpr (_property_option_pointer_emulation) return this->_property_get(); else return *this->_property_get();}
		decltype(auto) operator->() const
		{
			if constexpr (_property_option_pointer_emulation) return detail::arrow_operator<_property_get_const_t>::apply(this->_property_get());
			else if constexpr (std::is_pointer_v<_property_get_const_t>) return this->_property_get(); else return this->_property_get().operator->();
		}
		decltype(auto) operator->()
		{
			if constexpr (_property_option_pointer_emulation) return detail::arrow_operator<_property_get_t      >::apply(this->_property_get());
			else if constexpr (std::is_pointer_v<_property_get_t      >) return this->_property_get(); else return this->_property_get().operator->();
		}
		template<typename M>
		decltype(auto) operator->*(M &&m) const    {if constexpr (_property_option_pointer_emulation) return this->_property_get().*std::forward<M>(m); else this->_property_get()->*std::forward<M>(m);}
		template<typename M>
		decltype(auto) operator->*(M &&m)          {if constexpr (_property_option_pointer_emulation) return this->_property_get().*std::forward<M>(m); else this->_property_get()->*std::forward<M>(m);}


		/*
			Assignment operators.  These work differently for reference accessors vs. value accessors.
		*/

		// Special case: assigning from another instance of the same property accessor type.
		decltype(auto) operator=(const property &other) const    {return (this->_property_set(other._property_get()), *this);}
		decltype(auto) operator=(const property &other)          {return (this->_property_set(other._property_get()), *this);}

		// Assigment operators, where supported by the value.
		template<typename Y> decltype(auto) operator=(Y &&y) const {return (this->_property_set(std::forward<Y>(y)), *this);}
		template<typename Y> decltype(auto) operator=(Y &&y)       {return (this->_property_set(std::forward<Y>(y)), *this);}


		// Boilerplate for applying assigment operators and increments/decrements to a value property accessor
#define EDB_tmp_CompoundAssignOp(OP)           EDB_tmp_CompoundAssignOp_  (OP, const) EDB_tmp_CompoundAssignOp_  (OP, )
#define EDB_tmp_CompoundAssignOp_(OP, CONST)   template<typename Y, std::enable_if_t<!detail::is_property_accessor_v<Y>, bool> = true> decltype(auto) operator OP (Y &&y) CONST \
			{if constexpr (_property_by_proxy) return this->_property_get() OP std::forward<Y>(y); \
			else {auto x=this->_property_get(); return (x OP std::forward<Y>(y), this->_property_set(x), *this);}}

		// Compound assignment operators, where supported by the value.
		EDB_tmp_CompoundAssignOp(+=)  EDB_tmp_CompoundAssignOp(-=)  EDB_tmp_CompoundAssignOp(*=)  EDB_tmp_CompoundAssignOp(/=)
		EDB_tmp_CompoundAssignOp(%=)  EDB_tmp_CompoundAssignOp(<<=) EDB_tmp_CompoundAssignOp(>>=)
		EDB_tmp_CompoundAssignOp(&=)  EDB_tmp_CompoundAssignOp(|=)  EDB_tmp_CompoundAssignOp(^=)

		// Increment and decrement operators, where supported by the value.
#define EDB_tmp_IncrPrefOp(OP)         EDB_tmp_IncrPrefOp_(OP, const) EDB_tmp_IncrPrefOp_(OP, )
#define EDB_tmp_IncrPostOp(OP)         EDB_tmp_IncrPostOp_(OP, const) EDB_tmp_IncrPostOp_(OP, )
#define EDB_tmp_IncrPrefOp_(OP, CONST) decltype(auto) operator OP ()    CONST {if constexpr (_property_by_proxy) return OP this->_property_get(); else {auto x = this->_property_get(); return (OP x, this->_property_set(x), *this);}}
#define EDB_tmp_IncrPostOp_(OP, CONST) decltype(auto) operator OP (int) CONST {if constexpr (_property_by_proxy) return this->_property_get() OP; else {auto x = this->_property_get(), y = x; return (x OP, this->_property_set(x), y);}}

		EDB_tmp_IncrPrefOp(++) EDB_tmp_IncrPrefOp(--)
		EDB_tmp_IncrPostOp(++) EDB_tmp_IncrPostOp(--)


	private:
		// Property accessors don't independently exist and shouldn't be copy-constructed or move-constructed.
		property(const property &o);
	};

	/*
		A get/set rule for a member of some object represented by another property accessor.
			Useful for implementing specializations of the members template.
	*/
	template<typename GetSet_t, auto PointerToMember, typename Enable = void>
	struct getset_member;

	// member get/set implementation used when the object is accessed by reference through a proxy property accessor.
	template<typename GetSet_t, typename Class_t, typename Member_t, Member_t Class_t::*PointerToMember>
	struct getset_member<GetSet_t, PointerToMember,
		std::enable_if_t<std::is_lvalue_reference_v<getter_result_t<const GetSet_t>>>> : GetSet_t
	{
		auto& get() const    {return this->GetSet_t::get().*PointerToMember;}
		auto& get()          {return this->GetSet_t::get().*PointerToMember;}
	};

	// member get/set implementation used when the object is accessed by copy through a value property accessor.
	template<typename GetSet_t, typename Class_t, typename Member_t, Member_t Class_t::*PointerToMember>
	struct getset_member<GetSet_t, PointerToMember,
		std::enable_if_t<std::is_object_v<getter_result_t<const GetSet_t>>>> : GetSet_t
	{
		std::remove_reference_t<Member_t> get() const    {return this->GetSet_t::get().*PointerToMember;}
		std::remove_reference_t<Member_t> get()          {return this->GetSet_t::get().*PointerToMember;}

		template<typename Y, std::enable_if_t<detail::has_setter<const GetSet_t, Y>, bool> = true>
		void set(Y &&y) const    {auto x = this->GetSet_t::get(); x.*PointerToMember = std::forward<Y>(y); this->GetSet_t::set(std::move(x));}
		template<typename Y, std::enable_if_t<detail::has_setter<      GetSet_t, Y>, bool> = true>
		void set(Y &&y)          {auto x = this->GetSet_t::get(); x.*PointerToMember = std::forward<Y>(y); this->GetSet_t::set(std::move(x));}
	};

	template<typename GetSet_t, auto PointerToMember>
	using member = property<getset_member<GetSet_t, PointerToMember>>;

	/*
		When a property accessor is the right-hand operand to some operator, substitute the value.
			This allows properties to be used with iostreams among many other applications.
	*/
#define EDB_tmp_FwdRhsOp(OP)         EDB_tmp_FwdRhsOp_(OP, const) EDB_tmp_FwdRhsOp_(OP, )
#define EDB_tmp_FwdRhsOp_(OP, CONST) \
	template<typename X, typename GetSet_t> \
	decltype(auto) operator OP(X &&x, CONST property <GetSet_t> &p)  {return (std::forward<X>(x) OP p._property_get());}

	EDB_tmp_FwdRhsOp(+)   EDB_tmp_FwdRhsOp(-)   EDB_tmp_FwdRhsOp(*)   EDB_tmp_FwdRhsOp(/)
	EDB_tmp_FwdRhsOp(+=)  EDB_tmp_FwdRhsOp(-=)  EDB_tmp_FwdRhsOp(*=)  EDB_tmp_FwdRhsOp(/=)
	EDB_tmp_FwdRhsOp(%)   EDB_tmp_FwdRhsOp(<<)  EDB_tmp_FwdRhsOp(>>)
	EDB_tmp_FwdRhsOp(%=)  EDB_tmp_FwdRhsOp(<<=) EDB_tmp_FwdRhsOp(>>=)
	EDB_tmp_FwdRhsOp(&)   EDB_tmp_FwdRhsOp(|)   EDB_tmp_FwdRhsOp(^)
	EDB_tmp_FwdRhsOp(&=)  EDB_tmp_FwdRhsOp(|=)  EDB_tmp_FwdRhsOp(^=)

#undef EDB_tmp_FwdRhsOp_
#undef EDB_tmp_FwdRhsOp

#undef EDB_tmp_CompoundAssignOp_
#undef EDB_tmp_CompoundAssignOp
#undef EDB_tmp_IncrPrefOp_
#undef EDB_tmp_IncrPrefOp
#undef EDB_tmp_IncrPostOp_
#undef EDB_tmp_IncrPostOp

#undef EDB_tmp_FwdBiOp_
#undef EDB_tmp_FwdBiOp
#undef EDB_tmp_FwdPrefOp_
#undef EDB_tmp_FwdPrefOp
#undef EDB_tmp_FwdPostOp_
#undef EDB_tmp_FwdPostOp
}


/*
	This alias is placed in the global root namespace and allows the library to be easily used
		without referencing the property_access namespace.
*/
template<typename GetSet_t>
using property_accessor = property_access::property<GetSet_t>;



/*
	=========================================================
	The following code is a MAP metafunction derived from the visit_struct library by Christopher Beck and Jarod42.
	
	This part of the code is subject to the Boost Software License:

	Boost Software License - Version 1.0 - August 17th, 2003

	Permission is hereby granted, free of charge, to any person or organization
	obtaining a copy of the software and accompanying documentation covered by
	this license (the "Software") to use, reproduce, display, distribute,
	execute, and transmit the Software, and to prepare derivative works of the
	Software, and to permit third-parties to whom the Software is furnished to
	do so, all subject to the following:

	The copyright notices in the Software and this entire statement, including
	the above license grant, this restriction and the following disclaimer,
	must be included in all copies of the Software, in whole or in part, and
	all derivative works of the Software, unless such copies or derivative
	works are solely in the form of machine-executable object code generated by
	a source language processor.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
	SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
	FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
	DEALINGS IN THE SOFTWARE.
	=========================================================
*/

#if !defined(PROPERTY_ACCESS_NO_MACROS)

// After C++20 we can use __VA_OPT__ and can also visit empty struct
# ifndef EDB_PP_HAS_VA_OPT
#   if (defined _MSVC_TRADITIONAL && !_MSVC_TRADITIONAL) || (defined __cplusplus && __cplusplus >= 202000L)
#     define EDB_HAS_VA_OPT true
#   else
#     define EDB_HAS_VA_OPT false
#   endif
# endif

#ifndef EDB_PP_MAP
/*** Generated code ***/

static constexpr const int max_visitable_members = 69;

#define EDB_EXPAND(x) x
#define EDB_PP_ARG_N( \
		_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10,\
		_11, _12, _13, _14, _15, _16, _17, _18, _19, _20,\
		_21, _22, _23, _24, _25, _26, _27, _28, _29, _30,\
		_31, _32, _33, _34, _35, _36, _37, _38, _39, _40,\
		_41, _42, _43, _44, _45, _46, _47, _48, _49, _50,\
		_51, _52, _53, _54, _55, _56, _57, _58, _59, _60,\
		_61, _62, _63, _64, _65, _66, _67, _68, _69, N, ...) N

#if EDB_PP_HAS_VA_OPT
  #define EDB_PP_NARG(...) EDB_EXPAND(EDB_PP_ARG_N(0 __VA_OPT__(,) __VA_ARGS__,  \
		69, 68, 67, 66, 65, 64, 63, 62, 61, 60,  \
		59, 58, 57, 56, 55, 54, 53, 52, 51, 50,  \
		49, 48, 47, 46, 45, 44, 43, 42, 41, 40,  \
		39, 38, 37, 36, 35, 34, 33, 32, 31, 30,  \
		29, 28, 27, 26, 25, 24, 23, 22, 21, 20,  \
		19, 18, 17, 16, 15, 14, 13, 12, 11, 10,  \
		9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#else
  #define EDB_PP_NARG(...) EDB_EXPAND(EDB_PP_ARG_N(0, __VA_ARGS__,  \
		69, 68, 67, 66, 65, 64, 63, 62, 61, 60,  \
		59, 58, 57, 56, 55, 54, 53, 52, 51, 50,  \
		49, 48, 47, 46, 45, 44, 43, 42, 41, 40,  \
		39, 38, 37, 36, 35, 34, 33, 32, 31, 30,  \
		29, 28, 27, 26, 25, 24, 23, 22, 21, 20,  \
		19, 18, 17, 16, 15, 14, 13, 12, 11, 10,  \
		9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#endif

/* need extra level to force extra eval */
#define EDB_CONCAT_(a,b) a ## b
#define EDB_CONCAT(a,b) EDB_CONCAT_(a,b)

#define EDB_APPLYF0(f)
#define EDB_APPLYF1(f,_1) f(_1)
#define EDB_APPLYF2(f,_1,_2) f(_1) f(_2)
#define EDB_APPLYF3(f,_1,_2,_3) f(_1) f(_2) f(_3)
#define EDB_APPLYF4(f,_1,_2,_3,_4) f(_1) f(_2) f(_3) f(_4)
#define EDB_APPLYF5(f,_1,_2,_3,_4,_5) f(_1) f(_2) f(_3) f(_4) f(_5)
#define EDB_APPLYF6(f,_1,_2,_3,_4,_5,_6) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6)
#define EDB_APPLYF7(f,_1,_2,_3,_4,_5,_6,_7) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7)
#define EDB_APPLYF8(f,_1,_2,_3,_4,_5,_6,_7,_8) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8)
#define EDB_APPLYF9(f,_1,_2,_3,_4,_5,_6,_7,_8,_9) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9)
#define EDB_APPLYF10(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10)
#define EDB_APPLYF11(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11)
#define EDB_APPLYF12(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12)
#define EDB_APPLYF13(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13)
#define EDB_APPLYF14(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14)
#define EDB_APPLYF15(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15)
#define EDB_APPLYF16(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16)
#define EDB_APPLYF17(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17)
#define EDB_APPLYF18(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18)
#define EDB_APPLYF19(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19)
#define EDB_APPLYF20(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20)
#define EDB_APPLYF21(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21)
#define EDB_APPLYF22(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22)
#define EDB_APPLYF23(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23)
#define EDB_APPLYF24(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24)
#define EDB_APPLYF25(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25)
#define EDB_APPLYF26(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26)
#define EDB_APPLYF27(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27)
#define EDB_APPLYF28(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28)
#define EDB_APPLYF29(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29)
#define EDB_APPLYF30(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30)
#define EDB_APPLYF31(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31)
#define EDB_APPLYF32(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32)
#define EDB_APPLYF33(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33)
#define EDB_APPLYF34(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34)
#define EDB_APPLYF35(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35)
#define EDB_APPLYF36(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36)
#define EDB_APPLYF37(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37)
#define EDB_APPLYF38(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38)
#define EDB_APPLYF39(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39)
#define EDB_APPLYF40(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40)
#define EDB_APPLYF41(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41)
#define EDB_APPLYF42(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42)
#define EDB_APPLYF43(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43)
#define EDB_APPLYF44(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44)
#define EDB_APPLYF45(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45)
#define EDB_APPLYF46(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46)
#define EDB_APPLYF47(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47)
#define EDB_APPLYF48(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48)
#define EDB_APPLYF49(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49)
#define EDB_APPLYF50(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50)
#define EDB_APPLYF51(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51)
#define EDB_APPLYF52(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52)
#define EDB_APPLYF53(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53)
#define EDB_APPLYF54(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54)
#define EDB_APPLYF55(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55)
#define EDB_APPLYF56(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56)
#define EDB_APPLYF57(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57)
#define EDB_APPLYF58(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58)
#define EDB_APPLYF59(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59)
#define EDB_APPLYF60(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60)
#define EDB_APPLYF61(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61)
#define EDB_APPLYF62(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62)
#define EDB_APPLYF63(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63)
#define EDB_APPLYF64(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63) f(_64)
#define EDB_APPLYF65(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63) f(_64) f(_65)
#define EDB_APPLYF66(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65,_66) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63) f(_64) f(_65) f(_66)
#define EDB_APPLYF67(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65,_66,_67) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63) f(_64) f(_65) f(_66) f(_67)
#define EDB_APPLYF68(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65,_66,_67,_68) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63) f(_64) f(_65) f(_66) f(_67) f(_68)
#define EDB_APPLYF69(f,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65,_66,_67,_68,_69) f(_1) f(_2) f(_3) f(_4) f(_5) f(_6) f(_7) f(_8) f(_9) f(_10) f(_11) f(_12) f(_13) f(_14) f(_15) f(_16) f(_17) f(_18) f(_19) f(_20) f(_21) f(_22) f(_23) f(_24) f(_25) f(_26) f(_27) f(_28) f(_29) f(_30) f(_31) f(_32) f(_33) f(_34) f(_35) f(_36) f(_37) f(_38) f(_39) f(_40) f(_41) f(_42) f(_43) f(_44) f(_45) f(_46) f(_47) f(_48) f(_49) f(_50) f(_51) f(_52) f(_53) f(_54) f(_55) f(_56) f(_57) f(_58) f(_59) f(_60) f(_61) f(_62) f(_63) f(_64) f(_65) f(_66) f(_67) f(_68) f(_69)

#define EDB_APPLY_F_(M, ...) EDB_EXPAND(M(__VA_ARGS__))
#if EDB_PP_HAS_VA_OPT
	#define EDB_PP_MAP(f, ...) EDB_EXPAND(EDB_APPLY_F_(EDB_CONCAT(EDB_APPLYF, EDB_PP_NARG(__VA_ARGS__)), f __VA_OPT__(,) __VA_ARGS__))
#else
	#define EDB_PP_MAP(f, ...) EDB_EXPAND(EDB_APPLY_F_(EDB_CONCAT(EDB_APPLYF, EDB_PP_NARG(__VA_ARGS__)), f, __VA_ARGS__))
#endif

/*** End generated code ***/
#endif

#endif //!defined(PROPERTY_ACCESS_NO_MACROS)


#endif // EDB_PROPERTY_ACCESSOR_H
