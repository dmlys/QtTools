#include <boost/test/unit_test.hpp>
#include <viewed/pointer_variant.hpp>


BOOST_AUTO_TEST_SUITE(pointer_variant_tests)

BOOST_AUTO_TEST_CASE(simple_variant)
{
	auto str_ptr = std::make_unique<std::string>("123");
	auto i_ptr   = std::make_unique<int>(12);
	viewed::pointer_variant<int *, std::string *> v(std::in_place_type<int *>, i_ptr.get());

	v = i_ptr.get();
	BOOST_CHECK_EQUAL(v.index(), 0);

	v = str_ptr.get();
	BOOST_CHECK_EQUAL(v.index(), 1);

	BOOST_CHECK_NO_THROW(viewed::get<std::string *>(v));
	BOOST_CHECK_NO_THROW(viewed::get<1>(v));

	auto visitor = [](auto * ptr) { return ptr; };
	BOOST_CHECK_EQUAL(viewed::visit<void *>(visitor, v), str_ptr.get());
}

BOOST_AUTO_TEST_CASE(non_unique_types)
{
	auto i1_ptr = std::make_unique<int>(12);
	auto i2_ptr = std::make_unique<int>(24);
	auto d_ptr  = std::make_unique<double>(2.0);

	auto get_ptr = [](auto * ptr) -> void * { return ptr; };
	auto get_val = [](auto * ptr) -> int    { return *ptr; };

	using variant = viewed::pointer_variant<int *, int *, double *>;
	variant v1(std::in_place_index<0>, i1_ptr.get());
	variant v2(std::in_place_index<1>, i2_ptr.get());
	variant v3 = d_ptr.get(); // no ambiguation - can init directly without in_place_index hints

	void * ptr1 = viewed::visit(get_ptr, v1);
	void * ptr2 = viewed::visit(get_ptr, v2);
	void * ptr3 = viewed::visit(get_ptr, v3);

	BOOST_CHECK_EQUAL(ptr1, i1_ptr.get());
	BOOST_CHECK_EQUAL(ptr2, i2_ptr.get());
	BOOST_CHECK_EQUAL(ptr3,  d_ptr.get());

	BOOST_CHECK_EQUAL(viewed::visit(get_val, v1), 12);
	BOOST_CHECK_EQUAL(viewed::visit(get_val, v2), 24);
	BOOST_CHECK_EQUAL(viewed::visit(get_val, v3),  2);
}

BOOST_AUTO_TEST_CASE(derived_test)
{
	struct base1 { std::size_t base_mem; };
	struct base2 { std::size_t base_mem; };
	struct derived : base1, base2 { std::size_t mem; };

	auto ptr = std::make_unique<derived>();

	// because of base1, addresses of derived and base2 differ.
	// layout of derived is:
	// 0x00  base1
	// 0x08  base2
	void * derived_ptr = ptr.get();
	void * base_ptr    = static_cast<base2 *>(ptr.get());
	BOOST_CHECK_NE(base_ptr, derived_ptr);

	// internally pointer_variant stores pointer as void *,
	// ensure that when pointer is stored it properly converted to base type and thus adjusted.
	viewed::pointer_variant<base2 *,   int *> vb = ptr.get();
	viewed::pointer_variant<derived *, int *> vd = ptr.get();

	BOOST_CHECK_EQUAL(vb.pointer(), base_ptr);
	BOOST_CHECK_NE(vb.pointer(), derived_ptr);

	BOOST_CHECK_EQUAL(vd.pointer(), derived_ptr);
	BOOST_CHECK_NE(vd.pointer(), base_ptr);
}

BOOST_AUTO_TEST_CASE(virtual_base)
{
	struct A { virtual ~A() = default; };
	struct B : virtual A {};
	struct C : virtual A {};
	struct D : B, C {};

	auto ptr = std::make_unique<D>();

	viewed::pointer_variant<A *, D *> v1 = ptr.get();
	viewed::pointer_variant<A *, D *> v2 = static_cast<B *>(ptr.get());
	viewed::pointer_variant<A *     > v3 = ptr.get();


}

BOOST_AUTO_TEST_CASE(get_test)
{
	auto str = std::make_unique<std::string>("test");
	viewed::pointer_variant<int *, std::string *> v = str.get();

	auto * str_ptr = viewed::get<std::string *>(v);
	BOOST_CHECK_EQUAL(*str_ptr, "test");
	BOOST_CHECK_THROW(viewed::get<int *>(v), viewed::bad_variant_access);
}

BOOST_AUTO_TEST_SUITE_END()
