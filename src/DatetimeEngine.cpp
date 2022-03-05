#include <QtTools/DatetimeEngine.hpp>
#include <ext/config.hpp>

namespace QtTools
{
	DatetimeEngine::duration IntervalMaximum(DatetimeEngine::time_period period)
	{
		if (period.is_calendar())
		{
			switch (period.as_calendar())
			{
				case DatetimeEngine::CalendarPeriod::Year:    return std::chrono::hours(24) * 365;
				case DatetimeEngine::CalendarPeriod::Quarter: return std::chrono::hours(24) * 31 * 3;
				case DatetimeEngine::CalendarPeriod::Month:   return std::chrono::hours(24) * 31;
				case DatetimeEngine::CalendarPeriod::Week:    return std::chrono::hours(24) * 7;
				case DatetimeEngine::CalendarPeriod::Day:     return std::chrono::hours(24);
	
				default: EXT_UNREACHABLE();
			}
		}
		else
		{
			return period.as_time();
		}
	}
	
	/************************************************************************/
	/*               QDateTime implementations                              */
	/************************************************************************/
	/************************************************************************/
	/*               Floor*                                                 */
	/************************************************************************/
	void DatetimeEngine::FloorDay(QDateTime & dt) const
	{
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	}
	
	void DatetimeEngine::FloorWeek(QDateTime & dt) const
	{
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
	
		int dayofweek = date.dayOfWeek();
		int days = static_cast<int>(m_first_dayofweek) - dayofweek;
		if (days > 0) days -= m_weeksize;
		date = date.addDays(days);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::FloorMonth(QDateTime & dt) const
	{
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		date.setDate(date.year(), date.month(), 1);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::FloorQuarter(QDateTime & dt) const
	{
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		auto month = date.month();
		month = (month - 1) / 3 * 3 + 1;
		date.setDate(date.year(), month, 1);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::FloorYear(QDateTime & dt) const
	{
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		date.setDate(date.year(), 1, 1);
	
		dt.setDate(date);
	}
	
	/************************************************************************/
	/*              Ceil*                                                   */
	/************************************************************************/
	void DatetimeEngine::CeilDay(QDateTime & dt) const
	{
		auto msec = dt.time().msecsSinceStartOfDay();
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		if (msec > 0) date = date.addDays(1);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::CeilWeek(QDateTime & dt) const
	{
		auto msec = dt.time().msecsSinceStartOfDay();
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
	
		// first ceil day
		if (msec > 0) date = date.addDays(1);
	
		int dayofweek = date.dayOfWeek();
		auto days = static_cast<int>(m_first_dayofweek) - dayofweek;
		if (days < 0) days += m_weeksize;
		date = date.addDays(days);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::CeilMonth(QDateTime & dt) const
	{
		auto msec = dt.time().msecsSinceStartOfDay();
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		auto day = date.day();
	
		date.setDate(date.year(), date.month(), 1);
		if (msec > 0 || day > 1) date = date.addMonths(1);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::CeilQuarter(QDateTime & dt) const
	{
		auto msec = dt.time().msecsSinceStartOfDay();
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		auto day = date.day();
		auto month = date.month();
		auto year = date.year();
		int adjust = msec > 0 || day > 1 || month > 1 ? 1 : 0;
		month = (month - 1) / 3 * 3 + 1 + adjust * 3;
		if (month >= 12) year += 1, month -= 12;
		date.setDate(year, month, 1);
	
		dt.setDate(date);
	}
	
	void DatetimeEngine::CeilYear(QDateTime & dt) const
	{
		auto msec = dt.time().msecsSinceStartOfDay();
		dt.setTime(QTime::fromMSecsSinceStartOfDay(0));
	
		auto date = dt.date();
		auto day = date.day();
		auto month = date.month();
	
		date.setDate(date.year(), 1, 1);
		if (msec > 0 || day > 1 || month > 1) date = date.addYears(1);
	
		dt.setDate(date);
	}
	
	/************************************************************************/
	/*                  Others                                              */
	/************************************************************************/
	auto DatetimeEngine::Floor(time_point val, CalendarPeriod period) const -> time_point
	{
		switch (period)
		{
			default:
				Q_UNREACHABLE();
	
			case CalendarPeriod::Day:     return FloorDay(val);
			case CalendarPeriod::Week:    return FloorWeek(val);
			case CalendarPeriod::Month:   return FloorMonth(val);
			case CalendarPeriod::Quarter: return FloorQuarter(val);
			case CalendarPeriod::Year:    return FloorYear(val);
		}
	}
	
	auto DatetimeEngine::Ceil(time_point val, CalendarPeriod period) const -> time_point
	{
		switch (period)
		{
			default:
				Q_UNREACHABLE();
			
			case CalendarPeriod::Day:      return CeilDay(val);
			case CalendarPeriod::Week:     return CeilWeek(val);
			case CalendarPeriod::Month:    return CeilMonth(val);
			case CalendarPeriod::Quarter:  return CeilQuarter(val);
			case CalendarPeriod::Year:     return CeilYear(val);
		}
	}
	
	auto DatetimeEngine::AddPeriod(time_point val, CalendarPeriod period, int units) const -> time_point
	{
		switch (period)
		{
			default:
				Q_UNREACHABLE();
			
			case CalendarPeriod::Day:      return AddDays(val, units);
			case CalendarPeriod::Week:     return AddWeeks(val, units);
			case CalendarPeriod::Month:    return AddMonths(val, units);
			case CalendarPeriod::Quarter:  return AddQuarters(val, units);
			case CalendarPeriod::Year:     return AddYears(val, units);
		}
	}
	
	/************************************************************************/
	/*                    Interval                                          */
	/************************************************************************/
	auto DatetimeEngine::DayInterval(time_point val) const -> interval
	{
		ToQDateTime(val, m_dtbuffer);	
	
		FloorDay(m_dtbuffer);
		auto begin = ToStdChrono(m_dtbuffer);
	
		auto date = m_dtbuffer.date();
		date = date.addDays(1);
		m_dtbuffer.setDate(date);
		auto end = ToStdChrono(m_dtbuffer);
		
		return {begin, end};
	}
	
	auto DatetimeEngine::WeekInterval(time_point val) const -> interval
	{
		ToQDateTime(val, m_dtbuffer);
		
		FloorWeek(m_dtbuffer);
		auto begin = ToStdChrono(m_dtbuffer);
	
		auto date = m_dtbuffer.date();
		date = date.addDays(m_weeksize);
		m_dtbuffer.setDate(date);
		auto end = ToStdChrono(m_dtbuffer);
	
		return {begin, end};
	}
	
	auto DatetimeEngine::MonthInterval(time_point val) const -> interval
	{
		ToQDateTime(val, m_dtbuffer);
	
		FloorMonth(m_dtbuffer);
		auto begin = ToStdChrono(m_dtbuffer);
	
		auto date = m_dtbuffer.date();
		date = date.addMonths(1);
		m_dtbuffer.setDate(date);
		auto end = ToStdChrono(m_dtbuffer);
	
		return {begin, end};
	}
	
	auto DatetimeEngine::QuarterInterval(time_point val) const -> interval
	{
		ToQDateTime(val, m_dtbuffer);
	
		FloorQuarter(m_dtbuffer);
		auto begin = ToStdChrono(m_dtbuffer);
	
		auto date = m_dtbuffer.date();
		date = date.addMonths(3);
		m_dtbuffer.setDate(date);
		auto end = ToStdChrono(m_dtbuffer);
	
		return {begin, end};
	}
	
	auto DatetimeEngine::YearInterval(time_point val) const -> interval
	{
		ToQDateTime(val, m_dtbuffer);
	
		FloorYear(m_dtbuffer);
		auto begin = ToStdChrono(m_dtbuffer);
	
		auto date = m_dtbuffer.date();
		date = date.addYears(1);
		m_dtbuffer.setDate(date);
		auto end = ToStdChrono(m_dtbuffer);
	
		return {begin, end};
	}
	
	auto DatetimeEngine::PeriodInterval(time_point val, CalendarPeriod period) const -> interval
	{
		switch (period)
		{
			default: Q_UNREACHABLE();
	
			case CalendarPeriod::Day:      return DayInterval(val);
			case CalendarPeriod::Week:     return WeekInterval(val);
			case CalendarPeriod::Month:    return MonthInterval(val);
			case CalendarPeriod::Quarter:  return QuarterInterval(val);
			case CalendarPeriod::Year:     return YearInterval(val);
		}
	}
	
	/************************************************************************/
	/*                    init/ctors                                        */
	/************************************************************************/
	void DatetimeEngine::Init(const QLocale & lc, Qt::TimeSpec timeSpec /* = Qt::LocalTime */)
	{
		m_first_dayofweek = lc.firstDayOfWeek();
		m_weeksize = 7;
		
		m_dtbuffer.setTimeSpec(timeSpec);
	}
	
	void DatetimeEngine::Init(const QLocale & lc, const QTimeZone & tz)
	{
		m_first_dayofweek = lc.firstDayOfWeek();
		m_weeksize = 7;
		
		m_dtbuffer.setTimeZone(tz);
	}
	
	void DatetimeEngine::Init(const QLocale & lc, const QDateTime & dt)
	{		
		m_first_dayofweek = lc.firstDayOfWeek();
		m_weeksize = 7;
		
		m_dtbuffer = dt;                  // copy all time options
		m_dtbuffer.setMSecsSinceEpoch(0); // this will detach this object
	}
	
	
}
