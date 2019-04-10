#include <boost/test/unit_test.hpp>
#include <QtTools/JsonTools.hpp>

using namespace QtTools::Json;

BOOST_AUTO_TEST_SUITE(JsonTools)

BOOST_AUTO_TEST_CASE(parsing_tests)
{
	auto json = R"(
		{"array":
		[[
			{"key1": true, "key2": 123.0},
			{"key1": true, "key2": 123.0}
		]]
		}
	)";

	BOOST_CHECK_NO_THROW(parse_json(json));

	auto bad_json = R"(
		{"array":
		[[
			{"key1": true, "key2": 123.0},
			{"key1": true, "key2": 123.0}
		]
		}
	)";

	BOOST_CHECK_THROW(parse_json(bad_json), json_parse_exception);
}

BOOST_AUTO_TEST_CASE(path_tests)
{
	auto json = R"(
		{"array":
		[[
			{"key1": true, "key2": 123.0},
			{"key3": "text", "key1": 123}
		]]
		}
	)";

	auto jdoc = parse_json(json);

	BOOST_CHECK_EQUAL(get_value(jdoc, "array/0/0/key1").toBool(), true);
	BOOST_CHECK_EQUAL(get_value(jdoc, "array/0/0/key2").toDouble(), 123.0);
	BOOST_CHECK_EQUAL(get_value(jdoc, "array/0/1/key3").toString(), "text");
	BOOST_CHECK_EQUAL(get_value(jdoc, "array/0/1/key1").toInt(), 123);

	BOOST_CHECK_EQUAL(get_string(jdoc, "array/0/1/key3"), "text");
}

BOOST_AUTO_TEST_SUITE_END()
