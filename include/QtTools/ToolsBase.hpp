#pragma once
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QMetaType>

#include <string>
#include <codecvt>
#include <ext/range.hpp>
#include <ext/codecvt_conv/generic_conv.hpp>

/************************************************************************/
/*                некоторая интеграция QString в ext/boost::range       */
/*                нужна для ext::interpolate                            */
/************************************************************************/
Q_DECLARE_METATYPE(std::string)

static_assert(std::is_same_v<QString::iterator, QChar *>);
static_assert(std::is_same_v<QString::const_iterator, const QChar *>);

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
	template <>
	struct hash<QString>
	{
		std::size_t operator()(const QString & str) const noexcept
		{
			return qHash(str);
		}
	};
}

namespace QtTools
{
	/// регистрирует std::string в Qt meta system
	void QtRegisterStdString();

	namespace detail_qtstdstring
	{
		extern const std::codecvt_utf8<ushort> & u8cvt;
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

	/// преобразует utf-8 строку str длинной len в utf-16 QString, не превышая максимальное кол-во символов maxSize
	/// и опционально заменяя последний символ(ret[maxSize - 1] на truncChar в случае если строка не умещается в maxSize
	void ToQString(const char * str, std::size_t len, QString & res, std::size_t maxSize, QChar truncChar = 0);

	/// преобразует utf-16 QString в utf-8 char range
	template <class Range>
	void FromQString(const QString & qstr, Range & res)
	{
		auto inRng = boost::make_iterator_range_n(qstr.utf16(), qstr.size());
		ext::codecvt_convert::to_bytes(detail_qtstdstring::u8cvt, inRng, res);
	}

	/************************************************************************/
	/*               ToQString/FromQString overloads                        */
	/************************************************************************/

	template <class Range>
	inline void ToQString(const Range & rng, QString & res)
	{
		ToQString(ext::data(rng), boost::size(rng), res);
	}

	template <class Range>
	inline QString ToQString(const Range & rng, std::size_t maxSize, QChar truncChar = 0)
	{
		QString res;
		ToQString(ext::data(rng), boost::size(rng), res, maxSize, truncChar);
		return res;
	}

	template <class Range>
	inline QString ToQString(const Range & rng)
	{
		return QString::fromUtf8(ext::data(rng), qint(boost::size(rng)));
	}

	inline QString ToQString(const char * str)
	{
		return QString::fromUtf8(str);
	}

	/// noop overloads
	inline QString ToQString(const QString & str) noexcept { return str; };
	inline void ToQString(const QString & str, QString & res) noexcept { res = str; }


	inline void FromQString(const QString & qstr, QString & res) noexcept
	{
		res = qstr;
	}

	template <class ResultRange = std::string>
	inline ResultRange FromQString(const QString & qstr)
	{
		ResultRange res;
		FromQString(qstr, res);
		return res;
	}

	template <>
	inline QString FromQString(const QString & qstr) noexcept
	{
		return qstr;
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
