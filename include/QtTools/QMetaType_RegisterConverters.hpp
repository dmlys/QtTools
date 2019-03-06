#pragma once
#include <cstddef>
#include <cstdlib>
#include <climits>
#include <limits>
#include <string>
#include <charconv>

#include <ext/itoa.hpp>
#include <ext/functors/ctpred.hpp>
#include <ext/strings/aci_string.hpp>

#include <QtCore/QString>
#include <QtCore/QDate>
#include <QtCore/QTime>
#include <QtCore/QDateTime>

#include <QtTools/ToolsBase.hpp>
#include <QtTools/DateUtils.hpp>

namespace QtTools
{
	namespace QMetaTypeRegisterStringHelpers
	{
		template <class SignedType, class StringType>
		std::enable_if_t<std::is_signed_v<SignedType>, SignedType> ToIntegral(const StringType & str)
		{
			const char * first = str.data();

			long long result = std::strtoll(first, nullptr, 10);
			constexpr long long min = std::numeric_limits<SignedType>::min();
			constexpr long long max = std::numeric_limits<SignedType>::max();
			return static_cast<SignedType>(std::clamp(min, result, max));
		}

		template <class UnsignedType, class StringType>
		std::enable_if_t<std::is_unsigned_v<UnsignedType>, UnsignedType> ToIntegral(const StringType & str)
		{
			const char * first = str.data();

			unsigned long long result = std::strtoull(first, nullptr, 10);
			constexpr unsigned long long min = std::numeric_limits<UnsignedType>::min();
			constexpr unsigned long long max = std::numeric_limits<UnsignedType>::max();
			return static_cast<UnsignedType>(std::clamp(min, result, max));
		}

		template <class StringType, class IntegralType>
		StringType FromIntegral(IntegralType val)
		{
			static_assert(std::is_integral_v<IntegralType>);

			ext::itoa_buffer<IntegralType> buffer;
			auto * last = std::end(buffer) - 1;
			auto * first = ext::itoa(val, buffer);
			return StringType(first, last - first);
		}

		template <class Double, class StringType>
		Double ToDouble(const StringType & str)
		{
			const char * first = str.data();
			if constexpr (std::is_same_v<Double, float>)
				return std::strtof(first, nullptr);
			else if constexpr (std::is_same_v<Double, double>)
				return std::strtod(first, nullptr);
		    else if constexpr (std::is_same_v<Double, long double>)
				return std::strtold(first, nullptr);
		}

		template <class StringType, class FloatType>
		StringType FromDouble(FloatType val)
		{
			QString qstr;
			qstr.setNum(val);
			return FromQString<StringType>(qstr);
		}

		template <class StringType, class DateType>
		StringType FromDateTime(const DateType & datetime)
		{
			if constexpr (std::is_same_v<DateType, std::chrono::system_clock::time_point>)
			{
				auto qdt = ToQDateTime(datetime);
				auto qstr = qdt.toString(Qt::DateFormat::ISODateWithMs);
				return FromQString<StringType>(qstr);
			}
			else
			{
				auto qstr = datetime.toString(Qt::DateFormat::ISODateWithMs);
				return FromQString<StringType>(qstr);
			}
		}

		template <class DateType, class StringType>
		DateType ToDateTime(const StringType & str)
		{
			if constexpr (std::is_same_v<DateType, std::chrono::system_clock::time_point>)
			{
				auto qstr = ToQString(str);
				return ToStdChrono(QDateTime::fromString(qstr, Qt::DateFormat::ISODateWithMs));
			}
			else
			{
				auto qstr = ToQString(str);
				return DateType::fromString(qstr, Qt::DateFormat::ISODateWithMs);
			}
		}

		template <class StringType>
		QByteArray ToQByteArray(const StringType & str)
		{
			const char * first = str.data();
			int size = static_cast<int>(str.size());
			return QByteArray(first, size);
		}

		template <class StringType>
		StringType FromQByteArray(const QByteArray & val)
		{
			return StringType(val.data(), val.size());
		}

		template <class StringType>
		StringType FromBool(bool val)
		{
			return val ? "true" : "false";
		}

		template <class StringType>
		bool ToBool(const StringType & str)
		{
			ext::ctpred::equal_to<ext::aci_char_traits> equal;
			std::string_view sv = str;
			return not sv.empty() and not equal(sv, "0") and not equal(sv, "false");
		}
	}

	template <class String>
	void QMetaType_RegisterStringConverters()
	{
		using string_type = String;
		using namespace QMetaTypeRegisterStringHelpers;

		QMetaType::registerConverter<string_type, QString>(static_cast<QString(*)(const string_type & )>(ToQString));
		QMetaType::registerConverter<QString, string_type>(static_cast<string_type(*)(const QString & )>(FromQString<string_type>));

		QMetaType::registerConverter<string_type, QByteArray>(ToQByteArray<string_type>);
		QMetaType::registerConverter<QByteArray, string_type>(FromQByteArray<string_type>);

		QMetaType::registerConverter<bool, string_type>(FromBool<string_type>);
		QMetaType::registerConverter<string_type, bool>(ToBool<string_type>);

		#define REGISTER_NUM_CONV(type) \
	        QMetaType::registerConverter<string_type, type>(ToIntegral<type, string_type>); \
	        QMetaType::registerConverter<type, string_type>(FromIntegral<string_type, type>);

		#define REGISTER_FLOAT_CONV(type) \
	        QMetaType::registerConverter<string_type, type>(ToDouble<type, string_type>); \
	        QMetaType::registerConverter<type, string_type>(FromDouble<string_type, type>);

		#define REGISTER_DATE_CONV(type) \
	        QMetaType::registerConverter<string_type, type>(ToDateTime<type, string_type>); \
	        QMetaType::registerConverter<type, string_type>(FromDateTime<string_type, type>);


		REGISTER_NUM_CONV(signed short);
		REGISTER_NUM_CONV(unsigned short);
		REGISTER_NUM_CONV(signed int);
		REGISTER_NUM_CONV(unsigned int);
		REGISTER_NUM_CONV(signed long);
		REGISTER_NUM_CONV(unsigned long);
		REGISTER_NUM_CONV(signed long long);
		REGISTER_NUM_CONV(unsigned long long);

		REGISTER_FLOAT_CONV(float);
		REGISTER_FLOAT_CONV(double);
		//REGISTER_FLOAT_CONV(long double);

		REGISTER_DATE_CONV(QDateTime);
		REGISTER_DATE_CONV(QDate);
		REGISTER_DATE_CONV(QTime);
		REGISTER_DATE_CONV(std::chrono::system_clock::time_point);


		#undef REGISTER_NUM_CONV
		#undef REGISTER_FLOAT_CONV
		#undef REGISTER_DATE_CONV
	}

	template <class DateType>
	void QMetaType_RegisterDateConverters()
	{
		using date_type = DateType;
		QMetaType::registerConverter<date_type, QDateTime>(static_cast<QDateTime(*)(date_type)>(ToQDateTime));
		QMetaType::registerConverter<date_type, QDate>(static_cast<QDate(*)(date_type)>(ToQDate));

		QMetaType::registerConverter<QDateTime, date_type>(static_cast<date_type(*)(const QDateTime &)>(ToStdChrono));
		QMetaType::registerConverter<QDate, date_type>(static_cast<date_type(*)(QDate)>(ToStdChrono));

		QMetaType::registerConverter<date_type, QString>([](date_type val) { return ToQDateTime(val).toString(Qt::DateFormat::ISODateWithMs); });
		QMetaType::registerConverter<QString, date_type>([](const QString & str) { return ToStdChrono(QDateTime::fromString(str, Qt::DateFormat::ISODateWithMs)); });
	}
}
