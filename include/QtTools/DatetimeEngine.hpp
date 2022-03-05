#pragma once
#include <chrono>
#include <utility>
#include <tuple>
#include <boost/operators.hpp>
#include <boost/chrono.hpp>

#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>
#include <QtTools/DateUtils.hpp>

namespace QtTools
{
	/// Класс отвечающий за функции работы с датой, необходимые приложению.
	/// На данный момент это работа с time_point_type.
	/// Поддерживается только грегорианский календарь, другие календари не поддерживаются.
	/// 
	/// Необходимо:
	///	  * округлять даты, Floor/Ceil функции
	///	  * получать интервал вокруг даты, по факту [FloorX(point); CeilX(point))
	///	  
	///	для реализации используется QDateTime, при этом внутри хранится объект-буффер QDateTime,
	///	что бы минимизировать аллокации(QDateTime внутри аллоцирует shared state)
	///	
	/// Класс учитывает локаль(QLocale) и Qt::TimeSpec/QTimeZone:
	///  * локаль задает первый день недели
	///  * Qt::TimeSpec/QTimeZone используется для срезания времени при округление по календарным периодам(день и выше),
	///    срез происходит не на 00:00:00 по гринвичу, а по смещению указанному во временной зоне/локальному времени
	///	
	/// функции реализуются и добавляются по требованию.
	/// Тут ряд функций реализующих эффективно округление, получения обрамляющего интервала.
	///
	/// WARN: на момент написания, Qt 5.4 имела баги в поддержки QTimeZone, например Europe/Moscow по факту была +0, вместо +3
	class DatetimeEngine
	{
	public:
		/// календарный период, день, месяц, ...
		enum CalendarPeriod
		{
			Day,       /// дневной интервал
			Week,      /// недельный интервал
			Month,     /// месячный интервал 
			Quarter,   /// квартальный интервал
			Year,      /// годовой интервал
		
			PeriodCount = Year + 1, /// кол-во различных периодов
		};
		
		using duration = std::chrono::system_clock::duration;
		using time_point = std::chrono::system_clock::time_point;
		
		using boost_time_point = boost::chrono::system_clock::duration;
		using boost_duration = boost::chrono::system_clock::duration;
		
		/// Класс представляет собой интервал, он может быть как календарный, так и фиксированный временной интервал.
		/// автоматически не преобразуется ни в calendar_period, ни в time_duration.
		class time_period : private boost::totally_ordered<time_period>
		{
			duration m_interval;
			
		public:
			bool operator  <(const time_period & oi) const { return m_interval  < oi.m_interval; }
			bool operator ==(const time_period & oi) const { return m_interval == oi.m_interval; }
		
			bool is_calendar() const { return m_interval.count() < 0; }
			bool is_time() const     { return not is_calendar(); }
		
			duration       as_time()     const { return m_interval; }
			CalendarPeriod as_calendar() const 
			{ return static_cast<CalendarPeriod>(static_cast<int>(CalendarPeriod::PeriodCount) + m_interval.count()); }
		
			time_period() = default;
			time_period(const time_period &) = default;
			time_period & operator =(const time_period &) = default;
		
			time_period(CalendarPeriod cp)
			{ m_interval = duration(static_cast<int>(cp) - static_cast<int>(CalendarPeriod::PeriodCount)); }
		
			time_period(duration i) { m_interval = i; }
		};
		
		using interval = std::tuple<time_point, time_point>;
		
	private:
		mutable QDateTime m_dtbuffer;
		unsigned m_first_dayofweek;
		unsigned m_weeksize;
		
	public:
		void FloorDay(QDateTime & dt) const;
		void FloorWeek(QDateTime & dt) const;
		void FloorMonth(QDateTime & dt) const;
		void FloorQuarter(QDateTime & dt) const;
		void FloorYear(QDateTime & dt) const;
	
		void CeilDay(QDateTime & dt) const;
		void CeilWeek(QDateTime & dt) const;
		void CeilMonth(QDateTime & dt) const;
		void CeilQuarter(QDateTime & dt) const;
		void CeilYear(QDateTime & dt) const;
	
		void AddDays(QDateTime & dt, int days)         const { dt.setDate(dt.date().addDays(days)); }
		void AddWeeks(QDateTime & dt, int weeks)       const { AddDays(dt, weeks * 7); }
		void AddMonths(QDateTime & dt, int months)     const { dt.setDate(dt.date().addMonths(months)); }
		void AddQuarters(QDateTime & dt, int quarters) const { AddMonths(dt, quarters * 3); }
		void AddYears(QDateTime & dt, int years)       const { dt.setDate(dt.date().addYears(years)); }
		
	public:
		time_point FloorDay(time_point val) const;
		time_point FloorWeek(time_point val) const;
		time_point FloorMonth(time_point val) const;
		time_point FloorQuarter(time_point val) const;
		time_point FloorYear(time_point val) const;
	
		time_point Floor(time_point val, time_point::duration period) const;
		time_point Floor(time_point val, CalendarPeriod period) const;
		time_point Floor(time_point val, time_period period) const;
	
		time_point CeilDay(time_point val) const;
		time_point CeilWeek(time_point val) const;
		time_point CeilMonth(time_point val) const;
		time_point CeilQuarter(time_point val) const;
		time_point CeilYear(time_point val) const;
	
		time_point Ceil(time_point val, time_point::duration period) const;
		time_point Ceil(time_point val, CalendarPeriod period) const;
		time_point Ceil(time_point val, time_period period) const;
	
	
		interval DayInterval(time_point val) const;
		interval WeekInterval(time_point val) const;
		interval MonthInterval(time_point val) const;
		interval QuarterInterval(time_point val) const;
		interval YearInterval(time_point val) const;
	
		interval PeriodInterval(time_point val, time_point::duration period) const;
		interval PeriodInterval(time_point val, CalendarPeriod period) const;
		interval PeriodInterval(time_point val, time_period period) const;
	
	
		time_point AddDays(time_point val, int days) const;
		time_point AddWeeks(time_point val, int weeks) const;
		time_point AddMonths(time_point val, int months) const;
		time_point AddQuarters(time_point val, int quarters) const;
		time_point AddYears(time_point val, int years) const;
	
		time_point AddPeriod(time_point val, time_point::duration period, int units) const;
		time_point AddPeriod(time_point val, CalendarPeriod period, int units) const;
		time_point AddPeriod(time_point val, time_period period, int units) const;
		
	public:
		/// доступ к QDateTime буферу
		QDateTime & DatetimeBuffer() const { return m_dtbuffer; }
	
		/// инициализирует объект заданной локалью и заданным TimeSpec/TimeZone
		/// TimeSpec передается в QDateTime::setTimeSpec, и как следствие допустимы только Qt::LocalTime, Qt::UTC
		/// повторные вызовы Init - разрешены
		void Init(const QLocale & lc, Qt::TimeSpec timeSpec = Qt::LocalTime);
		void Init(const QLocale & lc, const QTimeZone & tz);
		/// инициализирует объект заданной локалью и заданным QDateTime. Из QDateTime копируются параметры TimeZone/TimeSpec
		void Init(const QLocale & lc, const QDateTime & dt);
	
		/// Конструкторы, вызывают соответствующий init(...)
		explicit DatetimeEngine()                                                          { Init(QLocale::system(), Qt::LocalTime); }
		explicit DatetimeEngine(const QLocale & lc, Qt::TimeSpec timeSpec = Qt::LocalTime) { Init(lc, timeSpec); }
		explicit DatetimeEngine(const QLocale & lc, const QTimeZone & tz)                  { Init(lc, tz); }
		explicit DatetimeEngine(const QLocale & lc, const QDateTime & dt)                  { Init(lc, dt); }
	};
	
	template <class Type>
	Type get(const DatetimeEngine::time_period &) = delete;
	
	template <>
	inline auto get<DatetimeEngine::duration>(const DatetimeEngine::time_period & i) -> DatetimeEngine::duration
	{
		return i.as_time();
	}
	
	template <>
	inline auto get<DatetimeEngine::CalendarPeriod>(const DatetimeEngine::time_period & i) -> DatetimeEngine::CalendarPeriod
	{
		return i.as_calendar();
	}

	/// возвращает максимальный временной промежуток для заданного периода
	/// 365 * boost::chrono::hours(24) для года
	/// ...
	/// если period.is_time() - возвращает period.as_time()
	DatetimeEngine::duration IntervalMaximum(DatetimeEngine::time_period period);
	
	/************************************************************************/
	/*                     Floor                                            */
	/************************************************************************/
	inline auto DatetimeEngine::FloorDay(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		FloorDay(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::FloorWeek(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		FloorWeek(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::FloorMonth(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		FloorMonth(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::FloorQuarter(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		FloorQuarter(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::FloorYear(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		FloorYear(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::Floor(time_point val, time_point::duration period) const -> time_point
	{
		std::chrono::system_clock::time_point t(val.time_since_epoch() / period * period);
		if (t > val) t -= period;
		return t;
	}
	
	inline auto DatetimeEngine::Floor(time_point val, time_period period) const -> time_point
	{
		if (period.is_calendar())
			return Floor(val, period.as_calendar());
		else
			return Floor(val, period.as_time());
	}
	
	/************************************************************************/
	/*                     Ceil                                             */
	/************************************************************************/
	inline auto DatetimeEngine::CeilDay(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		CeilDay(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::CeilWeek(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		CeilWeek(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::CeilMonth(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		CeilMonth(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::CeilQuarter(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		CeilQuarter(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::CeilYear(time_point val) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		CeilYear(m_dtbuffer);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::Ceil(time_point val, time_point::duration period) const -> time_point
	{
		std::chrono::system_clock::time_point t(val.time_since_epoch() / period * period);
		if (t < val) t += period;
		return t;
	}
	
	inline auto DatetimeEngine::Ceil(time_point val, time_period period) const -> time_point
	{
		if (period.is_calendar())
			return Ceil(val, period.as_calendar());
		else
			return Ceil(val, period.as_time());
	}
	
	/************************************************************************/
	/*                  Interval                                            */
	/************************************************************************/
	inline auto DatetimeEngine::PeriodInterval(time_point val, time_point::duration period) const -> interval 
	{
		auto first = Floor(val, period);
		auto last = first + period;
		return {first, last};
	}
	
	inline auto DatetimeEngine::PeriodInterval(time_point val, time_period period) const -> interval 
	{
		if (period.is_calendar())
			return PeriodInterval(val, period.as_calendar());
		else
			return PeriodInterval(val, period.as_time());
	}
	
	/************************************************************************/
	/*                   Add                                                */
	/************************************************************************/
	inline auto DatetimeEngine::AddDays(time_point val, int days) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		AddDays(m_dtbuffer, days);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::AddWeeks(time_point val, int weeks) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		AddWeeks(m_dtbuffer, weeks);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::AddMonths(time_point val, int months) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		AddMonths(m_dtbuffer, months);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::AddQuarters(time_point val, int quarters) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		AddQuarters(m_dtbuffer, quarters);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::AddYears(time_point val, int years) const -> time_point
	{
		ToQDateTime(val, m_dtbuffer);
		AddYears(m_dtbuffer, years);
		return ToStdChrono(m_dtbuffer);
	}
	
	inline auto DatetimeEngine::AddPeriod(time_point val, time_point::duration period, int units) const -> time_point
	{
		return val + period * units;
	}
	
	inline auto DatetimeEngine::AddPeriod(time_point val, time_period period, int units) const -> time_point
	{
		if (period.is_calendar())
			return AddPeriod(val, period.as_calendar(), units);
		else
			return AddPeriod(val, period.as_time(), units);
	}
}
