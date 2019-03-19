#include <boost/test/unit_test.hpp>
#include <boost/mp11.hpp>
#include <boost/mp11/mpl.hpp>

#include <QtTools/ToolsBase.hpp>
#include <viewed/sftree_string_traits.hpp>
#include <viewed/sftree_facade_qtbase.hpp>


using traits_list = boost::mp11::mp_list<viewed::sftree_string_traits, viewed::sftree_qstring_traits>;

BOOST_AUTO_TEST_SUITE(sftree_traits_tests)

BOOST_AUTO_TEST_CASE_TEMPLATE(get_name_tests, traits_type, traits_list)
{
	traits_type traits;

	BOOST_CHECK_EQUAL(traits.get_name("test/leaf"), "leaf");
	BOOST_CHECK_EQUAL(traits.get_name("/test/leaf/"), "");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(parse_path_test, traits_type, traits_list)
{
	traits_type traits;
	using pathview_type = typename traits_type::pathview_type;

	pathview_type name, curname, context;
	std::uintptr_t type;

	curname = "//test//inner//leaf";

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::NODE);
	BOOST_CHECK_EQUAL(name, "test");
	BOOST_CHECK_EQUAL(context, "//test//");

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::NODE);
	BOOST_CHECK_EQUAL(name, "inner");
	BOOST_CHECK_EQUAL(context, "//test//inner//");

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::LEAF);
	BOOST_CHECK_EQUAL(name, "leaf");
	BOOST_CHECK_EQUAL(context, "//test//inner//");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(tricky_parse_path_test, traits_type, traits_list)
{
	traits_type traits;
	using pathview_type = typename traits_type::pathview_type;

	pathview_type name, curname, context;
	std::uintptr_t type;

	curname = "//test//inner//leaf///";

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::NODE);
	BOOST_CHECK_EQUAL(name, "test");
	BOOST_CHECK_EQUAL(context, "//test//");

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::NODE);
	BOOST_CHECK_EQUAL(name, "inner");
	BOOST_CHECK_EQUAL(context, "//test//inner//");

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::NODE);
	BOOST_CHECK_EQUAL(name, "leaf");
	BOOST_CHECK_EQUAL(context, "//test//inner//leaf///");

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_CHECK_EQUAL(type, viewed::sftree_constants::LEAF);
	BOOST_CHECK_EQUAL(name, "");
	BOOST_CHECK_EQUAL(context, "//test//inner//leaf///");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(is_child_test, traits_type, traits_list)
{
	traits_type traits;
	using pathview_type = typename traits_type::pathview_type;

	pathview_type name, curname, context;
	std::uintptr_t type;

	curname = "//test/inner//leaf";

	std::tie(type, name, context) = traits.parse_path(curname, context);
	BOOST_REQUIRE_EQUAL(type, viewed::sftree_constants::NODE);
	BOOST_REQUIRE_EQUAL(name, "test");
	BOOST_REQUIRE_EQUAL(context, "//test/");

	BOOST_CHECK(traits.is_child(curname, context));
	BOOST_CHECK(traits.is_child("//test/another", context));
}

BOOST_AUTO_TEST_SUITE_END()
