#include <QtTools/ToolsBase.hpp>
#include <QtTools/DateUtils.hpp>
#include <QtTools/QMetaType_RegisterConverters.hpp>

namespace QtTools
{
	namespace detail_qtstdstring
	{
		class QStringWrapper
		{
		private:
			QString * str;

		public:
			QStringWrapper(QString & str)
				: str(&str) {}

		public:
			typedef std::size_t      size_type;
			typedef std::ptrdiff_t   difference_type;

			typedef char16_t           value_type;
			typedef const value_type * const_pointer;
			typedef value_type *       pointer;
			typedef value_type &       reference;
			typedef const value_type & const_reference;

			typedef pointer          iterator;
			typedef const_pointer    const_iterator;

			inline size_type size() const { return str->size(); }
			inline void   resize(size_type sz) { str->resize(qint(sz)); }

			inline value_type * data()             { return detail_qtstdstring::data(*str); }
			inline const value_type * data() const { return detail_qtstdstring::data(*str); }

			inline iterator begin() { return data(); }
			inline iterator end()   { return data() + str->size(); }

			inline const_iterator begin() const { return data(); }
			inline const_iterator end() const   { return data() + str->size(); }
		};
	}

	void QtRegisterStdString()
	{
		qRegisterMetaType<std::string>();
		QMetaType::registerComparators<std::string>();
		QMetaType_RegisterStringConverters<std::string>();
	}

	void QtRegisterStdChronoTypes()
	{
		qRegisterMetaType<std::chrono::system_clock::time_point>();
		qRegisterMetaType<std::chrono::system_clock::duration>();

		QMetaType::registerComparators<std::chrono::system_clock::time_point>();
		QMetaType::registerComparators<std::chrono::system_clock::duration>();

		QMetaType_RegisterDateConverters<std::chrono::system_clock::time_point>();
	}

	void ToQString(const char * str, std::size_t len, QString & res)
	{
		using namespace detail_qtstdstring;
		QStringWrapper wr {res};
		auto input = boost::make_iterator_range_n(str, len);
		ext::codecvt_convert::from_bytes(ext::codecvt_convert::wchar_cvt::u16_cvt, input, wr);
	}
	
	void ToQString(const wchar_t * str, std::size_t len, QString & res)
	{
		if constexpr(sizeof(wchar_t) == sizeof(char16_t))
			res.append(reinterpret_cast<const QChar *>(str), qint(len));
		else
			res.append(QString::fromWCharArray(str, len));
	}
	
	void ToQString(const char16_t * str, std::size_t len, QString & res)
	{
		res.append(QString::fromUtf16(str, len));
	}
	
	void ToQString(const char32_t * str, std::size_t len, QString & res)
	{
		res.append(QString::fromUcs4(str, len));
	}
	
	void ToQString(const char * str, std::size_t len, QString & res, std::size_t maxSize, QChar truncChar /* = 0 */)
	{
		auto strEnd = str + len;
		len = std::min(maxSize, len);
		res.resize(qint(len));

		auto srcRng = boost::make_iterator_range_n(str, len);
		auto retRng = boost::make_iterator_range_n(detail_qtstdstring::data(res), res.size());

		const char * stopped_from;
		char16_t * stopped_to;
		std::tie(stopped_from, stopped_to) = ext::codecvt_convert::from_bytes(ext::codecvt_convert::wchar_cvt::u16_cvt, srcRng, retRng);

		auto sz = stopped_to - retRng.begin();

		/// if was truncation and truncChar is not 0
		if (truncChar != 0 && stopped_from != strEnd)
			res[qint(maxSize - 1)] = truncChar;

		res.resize(qint(sz));
	}	
}
