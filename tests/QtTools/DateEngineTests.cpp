#include <boost/test/unit_test.hpp>
#include <QtTools/ToolsBase.hpp>
#include <QtTools/DatetimeEngine.hpp>
#include <ext/time_fmt.hpp>

namespace std
{
	static std::ostream & operator<<(std::ostream & os, std::chrono::system_clock::time_point pt)
	{
		std::time_t t = std::chrono::system_clock::to_time_t(pt);
		std::tm tm;
		ext::gmtime(&t, &tm);
		
		const int bufsz = 32;
		char buffer[bufsz];
		auto sz = std::strftime(buffer, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
		os.write(buffer, sz);
		return os;
	}	
}

using namespace QtTools;

static auto MakeDatetimeEngine()
{
	return DatetimeEngine(QLocale::system(), Qt::UTC);
}

static auto parseDateTime(std::string_view str)
{
	auto qstr = ToQString(str);
	auto qdt = QDateTime::fromString(qstr, QStringLiteral("yyyy-M-d H:m:s"));
	qdt.setTimeSpec(Qt::UTC);
	return QtTools::ToStdChrono(qdt);
}

BOOST_AUTO_TEST_SUITE(DateEngineTests)

BOOST_AUTO_TEST_CASE(FloorTests)
{
	auto engine = MakeDatetimeEngine();
	auto point = parseDateTime("2022-03-05 1:17:25");
	
	auto floored_day = engine.Floor(point, DatetimeEngine::Day);
	BOOST_CHECK_EQUAL(floored_day, parseDateTime("2022-03-05 00:00:00"));
	
	auto floored_week = engine.Floor(point, DatetimeEngine::Week);
	BOOST_CHECK_EQUAL(floored_week, parseDateTime("2022-02-28 00:00:00"));
	
	auto floored_month = engine.Floor(point, DatetimeEngine::Month);
	BOOST_CHECK_EQUAL(floored_month, parseDateTime("2022-03-01 00:00:00"));
	
	auto floored_quarter = engine.Floor(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floored_quarter, parseDateTime("2022-01-01 00:00:00"));
	
	auto floored_year = engine.Floor(point, DatetimeEngine::Year);
	BOOST_CHECK_EQUAL(floored_year, parseDateTime("2022-01-01 00:00:00"));
	
	// additional month and quarter tests
	point = parseDateTime("2022-05-01 00:00:00");
	floored_month = engine.Floor(point, DatetimeEngine::Month);
	BOOST_CHECK_EQUAL(floored_month, parseDateTime("2022-05-01 00:00:00"));
	
	point = parseDateTime("2022-05-05 00:00:00");
	floored_quarter = engine.Floor(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floored_quarter, parseDateTime("2022-04-01 00:00:00"));
	
	point = parseDateTime("2022-08-05 00:00:00");
	floored_quarter = engine.Floor(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floored_quarter, parseDateTime("2022-07-01 00:00:00"));
	
	point = parseDateTime("2022-11-05 00:00:00");
	floored_quarter = engine.Floor(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floored_quarter, parseDateTime("2022-10-01 00:00:00"));
	
	point = parseDateTime("2022-12-05 00:00:00");
	floored_quarter = engine.Floor(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floored_quarter, parseDateTime("2022-10-01 00:00:00"));
}

BOOST_AUTO_TEST_CASE(CeilTests)
{
	auto engine = MakeDatetimeEngine();
	auto point = parseDateTime("2022-03-05 1:17:25");
	
	auto ceiled_day = engine.Ceil(point, DatetimeEngine::Day);
	BOOST_CHECK_EQUAL(ceiled_day, parseDateTime("2022-03-06 00:00:00"));
	
	auto ceiled_week = engine.Ceil(point, DatetimeEngine::Week);
	BOOST_CHECK_EQUAL(ceiled_week, parseDateTime("2022-03-07 00:00:00"));
	
	auto ceiled_month = engine.Ceil(point, DatetimeEngine::Month);
	BOOST_CHECK_EQUAL(ceiled_month, parseDateTime("2022-04-01 00:00:00"));
	
	auto ceiled_quarter = engine.Ceil(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(ceiled_quarter, parseDateTime("2022-04-01 00:00:00"));
	
	auto ceiled_year = engine.Ceil(point, DatetimeEngine::Year);
	BOOST_CHECK_EQUAL(ceiled_year, parseDateTime("2023-01-01 00:00:00"));
	
	point = parseDateTime("2022-03-05 1:17:25");
	auto ceiled_interval = engine.Ceil(point, std::chrono::minutes(10));
	BOOST_CHECK_EQUAL(ceiled_interval, parseDateTime("2022-03-05 01:20:00"));
	
	// additional month and quarter tests
	point = parseDateTime("2022-03-31 00:00:00");
	ceiled_month = engine.Ceil(point, DatetimeEngine::Month);
	BOOST_CHECK_EQUAL(ceiled_month, parseDateTime("2022-04-01 00:00:00"));
	
	point = parseDateTime("2022-05-05 00:00:00");
	ceiled_quarter = engine.Ceil(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(ceiled_quarter, parseDateTime("2022-07-01 00:00:00"));
	
	point = parseDateTime("2022-08-05 00:00:00");
	ceiled_quarter = engine.Ceil(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(ceiled_quarter, parseDateTime("2022-10-01 00:00:00"));
	
	point = parseDateTime("2022-11-05 00:00:00");
	ceiled_quarter = engine.Ceil(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(ceiled_quarter, parseDateTime("2023-01-01 00:00:00"));
	
	point = parseDateTime("2022-12-05 00:00:00");
	ceiled_quarter = engine.Ceil(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(ceiled_quarter, parseDateTime("2023-01-01 00:00:00"));
}

BOOST_AUTO_TEST_CASE(IntervalTests)
{
	auto engine = MakeDatetimeEngine();
	auto point = parseDateTime("2022-03-05 1:17:25");
	
	std::chrono::system_clock::time_point floor, ceil;
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Day);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-03-05 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-03-06 00:00:00"));
	
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Week);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-02-28 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-03-07 00:00:00"));
	
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Month);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-03-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-04-01 00:00:00"));
	
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-01-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-04-01 00:00:00"));
	
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Year);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-01-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2023-01-01 00:00:00"));
	
	// additional month and quarter tests
	point = parseDateTime("2022-03-31 00:00:00");
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Month);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-03-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-04-01 00:00:00"));
	
	point = parseDateTime("2022-05-05 00:00:00");
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-04-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-07-01 00:00:00"));
	
	point = parseDateTime("2022-08-05 00:00:00");
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-07-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2022-10-01 00:00:00"));
	
	point = parseDateTime("2022-11-05 00:00:00");
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-10-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2023-01-01 00:00:00"));
	
	point = parseDateTime("2022-12-05 00:00:00");
	std::tie(floor, ceil) = engine.PeriodInterval(point, DatetimeEngine::Quarter);
	BOOST_CHECK_EQUAL(floor, parseDateTime("2022-10-01 00:00:00"));
	BOOST_CHECK_EQUAL(ceil,  parseDateTime("2023-01-01 00:00:00"));
}

BOOST_AUTO_TEST_SUITE_END()
