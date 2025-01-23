#ifndef EDB_PROPERTY_ACCESSOR_H
#define EDB_PROPERTY_ACCESSOR_H


/*
	This header implements property accessors for C++.  This version excludes preprocessor macros.

	Using this technique you may define a union-based "property block" to make a set of synthetic
	variables based on a set of actual variables.

	The most common use is to hide indirection -- ie, when an object refers to another object,
	the first object can have properties providing access to the second object's variables.
	Proxy property accessors are well-suited for this use.

	These can also be used to model derived quantities and values, e.g. representing an angular
	quantity in both degrees and radians or otherwise facilitating multiple representations.
	In this case you may use value property accessors.
*/



namespace property_access
{
	// Common base type for all property accessors.
	struct property_base
	{
		property_base() = default;

	private:
		// Property accessors have no unique state and may not themselves be copied or moved.
		property_base           (const property_base &o);
		property_base           (property_base      &&o);
		property_base& operator=(const property_base &o);
		property_base& operator=(property_base      &&o);
	};


	// Type traits used by this library.
	template<typename T>
	constexpr bool is_property_accessor_v = std::is_base_of_v<property_access::property_base, std::remove_reference_t<T>>;

	template<typename GetSet_t>
	using getter_result_t = decltype(std::declval<GetSet_t&>().GetSet_t::get());

	template<typename GetSet_t, typename Value_t = getter_result_t<GetSet_t>>
	using setter_result_t = decltype(std::declval<GetSet_t&>().GetSet_t::set(std::declval<Value_t>));

	template<typename GetSet_t, typename T>
	struct has_setter_impl
	{
		template<typename U> static auto check(int) -> decltype(std::declval<U>().set(std::declval<T>()), std::true_type{});
		template<typename U> static std::false_type check(...);

		static constexpr bool value = decltype(check<GetSet_t>(0))::value;
	};
	template<typename GetSet_t, typename Y>
	static constexpr bool has_setter = has_setter_impl<GetSet_t, Y>::value;

	namespace detail
	{
		template<typename To, typename GetterResult_t>
		static constexpr bool prohibit_fwd_convert_v = std::is_rvalue_reference_v<To> || std::is_same_v<std::decay_t<GetterResult_t>, std::decay_t<To>>;

		template<typename To, typename GetterResult_t>
		static constexpr bool forward_convert_v          = !prohibit_fwd_convert_v<To, GetterResult_t> && std::is_constructible_v<To, GetterResult_t>;

		template<typename To, typename GetterResult_t>
		static constexpr bool forward_convert_implicit_v = !prohibit_fwd_convert_v<To, GetterResult_t> && std::is_convertible_v<GetterResult_t, To>;


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


#define EDB_tmp_DetectPropertyOption(OPTION) \
			template<typename T, typename = void>struct option_ ## OPTION                                                           : public std::bool_constant<false> {}; \
			template<typename T>                 struct option_ ## OPTION<T, std::void_t<decltype(T::_property_option_ ## OPTION)>> : public std::bool_constant<true> {};

		EDB_tmp_DetectPropertyOption(pointer_emulation)

#undef EDB_tmp_DetectPropertyOption
	}


	/*
		Specializable base type for all property accessors.  This is a customization point
			which may be used to add named methods and members to a property accessor, usually for
			the purpose of making it behave even more like the target type.
		
		The memory layout of any specialization must be identical to the type 'GetSet_t'.
	*/
	template<typename T, typename GetSet_t, typename Enable = void>
	struct members : public property_base
	{
		union
		{
			// All specializations must provide this variable.
			GetSet_t _property_getset;
		};
	};

	/*
		Default specializations for class, struct and union type property accessors.
			In order to facilitate access to member variables, we add pointer-like semantics.
	*/
	template<typename T, typename GetSet_t>
	struct members<T, GetSet_t, std::enable_if_t<(std::is_class_v<T> || std::is_union_v<T>)>>
	{
		union
		{
			// All specializations must provide this variable.
			GetSet_t _property_getset;
		};

		// Enable this property to emulate a pointer.
		enum { _property_option_pointer_emulation };
	};

	
	// Boilerplate for forwarding binary and unary operators.
#define EDB_tmp_FwdBiOp(OP)           EDB_tmp_FwdBiOp_  (OP, const) EDB_tmp_FwdBiOp_  (OP, )
#define EDB_tmp_FwdPrefOp(OP)         EDB_tmp_FwdPrefOp_(OP, const) EDB_tmp_FwdPrefOp_(OP, )
#define EDB_tmp_FwdPostOp(OP)         EDB_tmp_FwdPostOp_(OP, const) EDB_tmp_FwdPostOp_(OP, )
#define EDB_tmp_FwdBiOp_(OP, CONST)   template<typename Y> \
    decltype(auto) operator OP (Y &&y) CONST {return this->_property_get() OP std::forward<Y>(y);}
#define EDB_tmp_FwdPrefOp_(OP, CONST) decltype(auto) operator OP ()    CONST {return OP this->_property_get();}
#define EDB_tmp_FwdPostOp_(OP, CONST) decltype(auto) operator OP (int) CONST {return this->_property_get() OP;}


	/*
		Implementation details shared by all property accessors.
	*/
	template<typename GetSet_t>
	struct common : public members<std::decay_t<getter_result_t<GetSet_t>>, GetSet_t>
	{
		/*
			Validate any specialization of members<T,GetSet>.
		*/
		static_assert(sizeof (members<std::decay_t<getter_result_t<GetSet_t>>, GetSet_t>) == sizeof (GetSet_t));
		static_assert(alignof(members<std::decay_t<getter_result_t<GetSet_t>>, GetSet_t>) == alignof(GetSet_t));

		static constexpr bool
			_property_option_pointer_emulation = detail::option_pointer_emulation<common>::value;

		// Get methods.
		decltype(std::declval<const GetSet_t>().get()) _property_get() const    {return this->_property_getset.get();}
		decltype(std::declval<      GetSet_t>().get()) _property_get()          {return this->_property_getset.get();}

		// Set methods, if applicable.
		template<typename Y>
		decltype(std::declval<std::enable_if_t<has_setter<const GetSet_t,Y>, const GetSet_t&>>().set(std::declval<Y>()))
			_property_set(Y &&y) const    {return this->_property_getset.set(std::forward<Y>(y));}
		template<typename Y>
		decltype(std::declval<std::enable_if_t<has_setter<      GetSet_t,Y>,       GetSet_t&>>().set(std::declval<Y>()))
			_property_set(Y &&y)          {return this->_property_getset.set(std::forward<Y>(y));}

		/*
			Support implicit conversion to the getter's return type.
		*/
		operator getter_result_t<const GetSet_t>()  const    {return this->_property_get();}
		operator getter_result_t<      GetSet_t>()           {return this->_property_get();}

		/*
			Forward conversion operators to the property value.
				Support for explicit conversion operators requires C++20.
		*/
#if __cplusplus >= 202000L || _MSVC_LANG >= 202000L
		// With explicit operator support
		template<typename T, typename = std::enable_if_t<detail::forward_convert_v<T, getter_result_t<const GetSet_t>>>>
		explicit(!detail::forward_convert_implicit_v<T, getter_result_t<const GetSet_t>>)
		operator T() const    {return T(this->_property_get());}
		template<typename T, typename = std::enable_if_t<detail::forward_convert_v<T, getter_result_t<      GetSet_t>>>>
		explicit(!detail::forward_convert_implicit_v<T, getter_result_t<      GetSet_t>>)
		operator T()          {return T(this->_property_get());}
#else
		// Without explicit operator support
		template<typename T, typename = std::enable_if_t<detail::forward_convert_implicit_v<T, getter_result_t<const GetSet_t>>>>
		operator T() const             {return   this->_property_get();}
		template<typename T, typename = std::enable_if_t<detail::forward_convert_implicit_v<T, getter_result_t<      GetSet_t>>>>
		operator T()                   {return   this->_property_get();}
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
			Forward the dereference operator and arrow operator(s).
				If _property_option_pointer_emulation is enabled (such as with unspecialized class/struct union)
				these will instead make the property itself act as a pointer to its value.
		*/
		decltype(auto) operator* () const    {if constexpr (_property_option_pointer_emulation) return this->_property_get(); else return *this->_property_get();}
		decltype(auto) operator* ()          {if constexpr (_property_option_pointer_emulation) return this->_property_get(); else return *this->_property_get();}
		decltype(auto) operator->() const
		{
			if constexpr (_property_option_pointer_emulation) return detail::arrow_operator<getter_result_t<const GetSet_t>>::apply(this->_property_get());
			else if constexpr (std::is_pointer_v<getter_result_t<const GetSet_t>>) return this->_property_get(); else return this->_property_get().operator->();
		}
		decltype(auto) operator->()
		{
			if constexpr (_property_option_pointer_emulation) return detail::arrow_operator<getter_result_t<      GetSet_t>>::apply(this->_property_get());
			else if constexpr (std::is_pointer_v<getter_result_t<      GetSet_t>>) return this->_property_get(); else return this->_property_get().operator->();
		}
		template<typename M>
		decltype(auto) operator->*(M &&m) const    {if constexpr (_property_option_pointer_emulation) return this->_property_get().*std::forward<M>(m); else this->_property_get()->*std::forward<M>(m);}
		template<typename M>
		decltype(auto) operator->*(M &&m)          {if constexpr (_property_option_pointer_emulation) return this->_property_get().*std::forward<M>(m); else this->_property_get()->*std::forward<M>(m);}
	};


	/*
		Implementation struct for property accessors based on a get() method returning an lvalue reference.
	*/
	template<typename GetSet_t>
	struct proxy : public common<GetSet_t>
	{
		// T must be an lvalue reference type.
		static_assert(std::is_lvalue_reference_v<getter_result_t<const GetSet_t>>,
			"Proxy property accessor requires an lvalue reference type.");
		static_assert(!has_setter<GetSet_t, getter_result_t<const GetSet_t>>,
			"Proxy property accessor does not use set() function; assignments are forwarded to get().");


		// Special case: assigning from another property accessor of the same type.
		decltype(auto) operator=(const proxy &other) const    {return this->_property_get() = *other;}
		decltype(auto) operator=(const proxy &other)          {return this->_property_get() = *other;}

		// Forward address-of operator.
		EDB_tmp_FwdPrefOp(&)

		// Forward assignments to the referent.
		EDB_tmp_FwdBiOp(=)

		// Forward compound assignments to the referent.
		EDB_tmp_FwdBiOp(+=)    EDB_tmp_FwdBiOp(-=)    EDB_tmp_FwdBiOp(*=)    EDB_tmp_FwdBiOp(/=)  
		EDB_tmp_FwdBiOp(%=)    EDB_tmp_FwdBiOp(<<=)   EDB_tmp_FwdBiOp(>>=) 
		EDB_tmp_FwdBiOp(&=)    EDB_tmp_FwdBiOp(|=)    EDB_tmp_FwdBiOp(^=)  

		// Forward increment and decrement operators to the referent.
		EDB_tmp_FwdPrefOp(++)  EDB_tmp_FwdPrefOp(--) 
		EDB_tmp_FwdPostOp(++)  EDB_tmp_FwdPostOp(--) 
	};

	// Boilerplate for applying assigment operators and increments/decrements to a value property accessor
#define EDB_tmp_ValueAssignOp(OP)           EDB_tmp_ValueAssignOp_  (OP, const) EDB_tmp_ValueAssignOp_  (OP, )
#define EDB_tmp_ValueIncrPrefOp(OP)         EDB_tmp_ValueIncrPrefOp_(OP, const) EDB_tmp_ValueIncrPrefOp_(OP, )
#define EDB_tmp_ValueIncrPostOp(OP)         EDB_tmp_ValueIncrPostOp_(OP, const) EDB_tmp_ValueIncrPostOp_(OP, )
#define EDB_tmp_ValueAssignOp_(OP, CONST)   template<typename Y> \
		auto operator OP (Y &&y) CONST -> decltype(std::declval<getter_result_t<CONST GetSet_t>&>() OP std::forward<Y>(y), *this) \
			{auto x=this->_property_get(); x OP std::forward<Y>(y); this->_property_set(x); return *this;}
#define EDB_tmp_ValueIncrPrefOp_(OP, CONST) decltype(auto) operator OP ()    CONST {auto x = this->_property_get(); return (OP x, this->_property_set(x), *this);}
#define EDB_tmp_ValueIncrPostOp_(OP, CONST) decltype(auto) operator OP (int) CONST {auto x = this->_property_get(); auto y = x OP; this->_property_set(x); return y;}

	/*
		Implementation struct for a value property accessor based on a get() and optional set() method.
	*/
	template<typename GetSet_t>
	struct value : public common<GetSet_t>
	{
		// T must be a value type (ie, not a reference or function -- function pointers are ok though)
		static_assert(std::is_object_v<getter_result_t<const GetSet_t>>,
			"Value property accessor requires a non-reference type.");


		/*
			Assignment, compound assignment, increment and decrement operators are valid only if
				the property type supports the operator in question.
		*/

		// Special case: assigning from another property accessor of the same type.
		decltype(auto) operator=(const value &other) const    {return this->_property_set(*other);}
		decltype(auto) operator=(const value &other)          {return this->_property_set(*other);}

		// Assigment operators, where supported by the value.
		EDB_tmp_ValueAssignOp(=)

		// Compound assignment operators, where supported by the value.
		EDB_tmp_ValueAssignOp(+=)  EDB_tmp_ValueAssignOp(-=)  EDB_tmp_ValueAssignOp(*=)  EDB_tmp_ValueAssignOp(/=)
		EDB_tmp_ValueAssignOp(%=)  EDB_tmp_ValueAssignOp(<<=) EDB_tmp_ValueAssignOp(>>=)
		EDB_tmp_ValueAssignOp(&=)  EDB_tmp_ValueAssignOp(|=)  EDB_tmp_ValueAssignOp(^=)

		// Increment and decrement operators, where supported by the value.
		EDB_tmp_ValueIncrPrefOp(++) EDB_tmp_ValueIncrPrefOp(--)
		EDB_tmp_ValueIncrPostOp(++) EDB_tmp_ValueIncrPostOp(--)
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
		using property_t = proxy<getset_member>;

		auto& get() const    {return this->GetSet_t::get().*PointerToMember;}
		auto& get()          {return this->GetSet_t::get().*PointerToMember;}
	};

	// member get/set implementation used when the object is accessed by copy through a value property accessor.
	template<typename GetSet_t, typename Class_t, typename Member_t, Member_t Class_t::*PointerToMember>
	struct getset_member<GetSet_t, PointerToMember,
		std::enable_if_t<std::is_object_v<getter_result_t<const GetSet_t>>>> : GetSet_t
	{
		using property_t = value<getset_member>;

		std::remove_reference_t<Member_t> get() const    {return this->GetSet_t::get().*PointerToMember;}
		std::remove_reference_t<Member_t> get()          {return this->GetSet_t::get().*PointerToMember;}

		template<typename Y, std::enable_if_t<has_setter<const GetSet_t, Y>, bool> = true>
		void set(Y &&y) const    {auto x = this->GetSet_t::get(); x.*PointerToMember = std::forward<Y>(y); this->GetSet_t::set(std::move(x));}
		template<typename Y, std::enable_if_t<has_setter<      GetSet_t, Y>, bool> = true>
		void set(Y &&y)          {auto x = this->GetSet_t::get(); x.*PointerToMember = std::forward<Y>(y); this->GetSet_t::set(std::move(x));}
	};

	template<typename GetSet_t, auto PointerToMember>
	using member = typename getset_member<GetSet_t, PointerToMember>::property_t;

	/*
		Template property_accessor automatically selects either value<> or proxy<> depending on
			the return type of GetSet_t::get().
	*/
	namespace detail
	{
		template<typename GetSet_t, typename Enable = void>
		struct property_implementation;

		template<typename GetSet_t>
		struct property_implementation<GetSet_t, std::enable_if_t<std::is_lvalue_reference_v<getter_result_t<const GetSet_t>>> >
			{using type = property_access::proxy<GetSet_t>;};

		template<typename GetSet_t>
		struct property_implementation<GetSet_t, std::enable_if_t<std::is_object_v<getter_result_t<const GetSet_t>>> >
			{using type = property_access::value<GetSet_t>;};
	}

	template<typename GetSet_t>
	using property_accessor = typename detail::property_implementation<GetSet_t>::type;

	/*
		When a property accessor is the right-hand operand to some operator, substitute the value.
			This allows properties to be used with iostreams among many other applications.
	*/
#define EDB_tmp_FwdRhsOp(OP)         EDB_tmp_FwdRhsOp_(OP, const) EDB_tmp_FwdRhsOp_(OP, )
#define EDB_tmp_FwdRhsOp_(OP, CONST) \
	template<typename X, typename GetSet_t, std::enable_if_t<!is_property_accessor_v<X>, bool> = true> \
    decltype(auto) operator OP(X &&x, CONST common <GetSet_t> &p)  {return (std::forward<X>(x) OP p._property_get());}

	EDB_tmp_FwdRhsOp(+)   EDB_tmp_FwdRhsOp(-)   EDB_tmp_FwdRhsOp(*)   EDB_tmp_FwdRhsOp(/)
	EDB_tmp_FwdRhsOp(+=)  EDB_tmp_FwdRhsOp(-=)  EDB_tmp_FwdRhsOp(*=)  EDB_tmp_FwdRhsOp(/=)
	EDB_tmp_FwdRhsOp(%)   EDB_tmp_FwdRhsOp(<<)  EDB_tmp_FwdRhsOp(>>)
	EDB_tmp_FwdRhsOp(%=)  EDB_tmp_FwdRhsOp(<<=) EDB_tmp_FwdRhsOp(>>=)
	EDB_tmp_FwdRhsOp(&)   EDB_tmp_FwdRhsOp(|)   EDB_tmp_FwdRhsOp(^)
	EDB_tmp_FwdRhsOp(&=)  EDB_tmp_FwdRhsOp(|=)  EDB_tmp_FwdRhsOp(^=)

#undef EDB_tmp_FwdRhsOp_
#undef EDB_tmp_FwdRhsOp

#undef EDB_tmp_ValueAssignOp_
#undef EDB_tmp_ValueAssignOp
#undef EDB_tmp_ValueIncrPrefOp_
#undef EDB_tmp_ValueIncrPrefOp
#undef EDB_tmp_ValueIncrPostOp_
#undef EDB_tmp_ValueIncrPostOp

#undef EDB_tmp_FwdBiOp_
#undef EDB_tmp_FwdBiOp
#undef EDB_tmp_FwdPrefOp_
#undef EDB_tmp_FwdPrefOp
#undef EDB_tmp_FwdPostOp_
#undef EDB_tmp_FwdPostOp
}


#endif // EDB_PROPERTY_ACCESSOR_H
