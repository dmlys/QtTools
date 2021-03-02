#pragma once
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QMetaType>

#include <QtCore/QObject>
#include <QtWidgets/QWidget>

#include <memory>
#include <string>

#include <ext/range.hpp>
#include <ext/codecvt_conv/generic_conv.hpp>
#include <ext/codecvt_conv/wchar_cvt.hpp>

/************************************************************************/
/*                некоторая интеграция QString в ext/boost::range       */
/*                нужна для ext::interpolate                            */
/************************************************************************/
Q_DECLARE_METATYPE(std::string)
Q_DECLARE_SMART_POINTER_METATYPE(std::unique_ptr)
Q_DECLARE_SMART_POINTER_METATYPE(std::shared_ptr)

static_assert(std::is_same_v<QString::iterator, QChar *>);
static_assert(std::is_same_v<QString::const_iterator, const QChar *>);
static_assert(sizeof(ushort) == sizeof(char16_t) and alignof(ushort) == alignof(char16_t));


template <>
inline void ext::assign<QString, char16_t * >(QString & str, char16_t * first, char16_t * last)
{
	str.setUnicode(reinterpret_cast<QChar *>(first), last - first);
}

template <>
inline void ext::assign<QString, const char16_t * >(QString & str, const char16_t * first, const char16_t * last)
{
	str.setUnicode(reinterpret_cast<const QChar *>(first), last - first);
}

template <>
inline void ext::assign<QString, QChar *>(QString & str, QChar * first, QChar * last)
{
	str.setUnicode(first, last - first);
}

template <>
inline void ext::assign<QString, const QChar *>(QString & str, const QChar * first, const QChar * last)
{
	str.setUnicode(first, last - first);
}

template <>
inline void ext::append<QString, char16_t *>(QString & str, char16_t * first, char16_t * last)
{
	str.append(reinterpret_cast<QChar *>(first), last - first);
}

template <>
inline void ext::append<QString, const char16_t *>(QString & str, const char16_t * first, const char16_t * last)
{
	str.append(reinterpret_cast<const QChar *>(first), last - first);
}

template <>
inline void ext::append<QString, QChar *>(QString & str, QChar * first, QChar * last)
{
	str.append(first, last - first);
}

template <>
inline void ext::append<QString, const QChar *>(QString & str, const QChar * first, const QChar * last)
{
	str.append(first, last - first);
}

#if not BOOST_LIB_STD_GNU
namespace ext::qt_helpers
{
	struct dummy_type { const char16_t & operator *(); };


	using u16sv_iterator = std::conditional_t<
	    std::is_same_v<std::u16string_view::const_iterator, const char16_t *>,
	    dummy_type, std::u16string_view::const_iterator
	>;
}

template <>
inline void ext::assign<QString, ext::qt_helpers::u16sv_iterator>(QString & str, ext::qt_helpers::u16sv_iterator first, ext::qt_helpers::u16sv_iterator last)
{
	return ext::assign(str, &*first, &*last);
}

template <>
inline void ext::append<QString, ext::qt_helpers::u16sv_iterator>(QString & str, ext::qt_helpers::u16sv_iterator first, ext::qt_helpers::u16sv_iterator last)
{
	return ext::append(str, &*first, &*last);
}
#endif

template <>
struct ext::str_view_traits<QString>
{
	using char_type = char16_t;
	using string_view = std::u16string_view;

	inline static string_view str_view(const QString & str)
	{
		auto * ptr = reinterpret_cast<const char16_t *>(str.utf16());
		return string_view(ptr, str.size());
	}
};


inline uint hash_value(const QString & val)
{
	return qHash(val);
}

inline uint hash_value(const QStringRef & val)
{
	return qHash(val);
}

/************************************************************************/
/*                некоторая интеграция QDateTime                        */
/************************************************************************/
inline uint hash_value(const QDateTime & val)
{
	return qHash(val);
}

inline uint hash_value(const QDate & val)
{
	return qHash(val);
}

inline uint hash_value(const QTime & val)
{
	return qHash(val);
}

namespace std
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	template <>
	struct hash<QString>
	{
		std::size_t operator()(const QString & str) const noexcept
		{
			return qHash(str);
		}

		std::size_t operator()(const QStringRef & str) const noexcept
		{
			return qHash(str);
		}
	};

	template <>
	struct hash<QStringRef>
	{
		std::size_t operator()(const QString & str) const noexcept
		{
			return qHash(str);
		}

		std::size_t operator()(const QStringRef & str) const noexcept
		{
			return qHash(str);
		}
	};
#endif

	template <>
	struct hash<QDateTime>
	{
		std::size_t operator()(const QDateTime & val) const noexcept
		{
			return qHash(val);
		}
	};

	template <>
	struct hash<QDate>
	{
		std::size_t operator()(const QDate & val) const noexcept
		{
			return qHash(val);
		}
	};

	template <>
	struct hash<QTime>
	{
		std::size_t operator()(const QTime & val) const noexcept
		{
			return qHash(val);
		}
	};
}

namespace QtTools
{
	/// регистрирует std::string в Qt meta system
	void QtRegisterStdString();

	namespace detail_qtstdstring
	{
		inline const char16_t * data(const QString & str) noexcept { return reinterpret_cast<const char16_t *>(str.utf16()); }
		inline       char16_t * data(      QString & str) noexcept { return const_cast<char16_t *>(reinterpret_cast<const char16_t *>(str.utf16())); }
	}

	/// qt qHash hasher functor for stl like containers
	struct QtHasher
	{
		typedef uint result_type;

		template <class Type>
		inline result_type operator()(const Type & val) const
		{
			return qHash(val);
		}
	};

	/// qt коллекции, модели используют int для индексации,
	/// stl же использует size_t
	/// данная функция должна использоваться там, где нужно преобразовать size_t к int в контексте qt
	inline int qint(std::size_t v)
	{
		return static_cast<int>(v);
	}

	/// qt коллекции, модели используют int для индексации,
	/// stl же использует size_t
	/// данная функция должна использоваться там, где нужно преобразовать int к size_t в контексте qt
	inline std::size_t qsizet(int v)
	{
		return static_cast<std::size_t>(v);
	}

	/// создает отдельную копию str
	inline QString DetachedCopy(const QString & str)
	{
		return QString {str.data(), str.size()};
	}

	/// NOTE: !!!
	/// QString поддерживает переиспользование буфера подобно std::string, но несколько по-другому.
	/// в отличии от std::string, даже если текущий размер больше нового,
	/// вызов str.resize(newsize) приведет к перевыделению памяти(по факту сжатию).
	/// что бы QString переиспользовал память нужно включить резервирование вызовом str.reserve(capacity) -
	/// тогда, пока размер строки укладывается в capacity, обращений к куче не будет.
	/// что бы вернуться к первоначальному поведению следует вызвать squeeze.
	

	/// преобразует utf-8 строку длинной len в utf-16 QString
	/// размер res будет увеличиваться по требованию
	void ToQString(const char * str, std::size_t len, QString & res);
	void ToQString(const wchar_t  * str, std::size_t len, QString & res);
	void ToQString(const char16_t * str, std::size_t len, QString & res);
	void ToQString(const char32_t * str, std::size_t len, QString & res);

	inline QString ToQString(const char * str, std::size_t len = -1)        { return QString::fromUtf8(str, len); }
	inline QString ToQString(const wchar_t  * str, std::size_t len = -1)    { return QString::fromWCharArray(str, len); }
	inline QString ToQString(const char16_t * str, std::size_t len = -1)    { return QString::fromUtf16(str, len); }
	inline QString ToQString(const char32_t * str, std::size_t len = -1)    { return QString::fromUcs4(str, len); }
	
	/// преобразует utf-8 строку str длинной len в utf-16 QString, не превышая максимальное кол-во символов maxSize
	/// и опционально заменяя последний символ(ret[maxSize - 1] на truncChar в случае если строка не умещается в maxSize
	void ToQString(const char * str, std::size_t len, QString & res, std::size_t maxSize, QChar truncChar = 0);

	/// преобразует utf-16 QString в utf-8 char range
	template <class Range>
	void FromQString(const QString & qstr, Range & res)
	{
		auto inRng = boost::make_iterator_range_n(detail_qtstdstring::data(qstr), qstr.size());
		ext::codecvt_convert::wchar_cvt::to_utf8(inRng, res);
	}

	/************************************************************************/
	/*               ToQString/FromQString overloads                        */
	/************************************************************************/
	template <class Range> inline    void ToQString(const Range & rng, QString & res) { ToQString(ext::data(rng), boost::size(rng), res); }
	template <class Range> inline QString ToQString(const Range & rng) { return ToQString(ext::data(rng), qint(boost::size(rng))); }

	/// noop overloads
	inline QString ToQString(const QString & str) noexcept { return str; };
	inline void ToQString(const QString & str, QString & res) noexcept { res.append(str); }
	inline void FromQString(const QString & qstr, QString & res) noexcept { res.append(qstr); }
	
	template <class ResultRange = std::string>
	inline ResultRange FromQString(const QString & qstr)
	{
		ResultRange res;
		FromQString(qstr, res);
		return res;
	}

	template <>
	inline QString FromQString(const QString & qstr)
	{
		return qstr;
	}
	
	
	template <class Range>
	inline QString ToQString(const Range & rng, std::size_t maxSize, QChar truncChar = 0)
	{
		QString res;
		ToQString(ext::data(rng), boost::size(rng), res, maxSize, truncChar);
		return res;
	}
	
} // namespace QtTools

inline std::ostream & operator <<(std::ostream & os, const QString & str)
{
	return os << QtTools::FromQString(str);
}

using QtTools::qint;
using QtTools::qsizet;
using QtTools::ToQString;
using QtTools::FromQString;
using QtTools::DetachedCopy;


namespace QtTools
{
	/// находит предка с типом Type
	/// если такого предка нет - вернет nullptr
	/// например, может быть полезно для нахождения QMdiArea
	template <class Type>
	Type * FindAncestor(QWidget * widget)
	{
		while (widget)
		{
			if (auto * w = qobject_cast<Type *>(widget))
				return w;

			widget = widget->parentWidget();
		}

		return nullptr;
	}

	/// находит предка с типом Type
	/// если такого предка нет - вернет nullptr
	/// например, может быть полезно для нахождения QMdiArea
	template <class Type>
	inline const Type * FindAncestor(const QWidget * widget)
	{
		return FindAncestor<Type>(const_cast<QWidget *>(widget));
	}
}
