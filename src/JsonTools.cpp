#include <algorithm>
#include <iterator>
#include <utility>
#include <charconv>

#include <ext/itoa.hpp>
#include <QtTools/JsonTools.hpp>

namespace QtTools::Json
{
	constexpr char separator = '/';

	std::string error_report(const QJsonParseError & error, const QByteArray & parse_source)
	{
		auto exception = create_json_parse_exception(error, parse_source);
		return exception.what();
	}

	json_parse_exception create_json_parse_exception(const QJsonParseError & error, const QByteArray & parse_source)
	{
		constexpr char newline = '\n';
		int offset = error.offset;
		int line, column;

		auto * first = parse_source.data();
		auto * last  = first + offset;

		auto last_linestart = std::find(std::make_reverse_iterator(last), std::make_reverse_iterator(first), newline).base();
		line = std::count(first, last_linestart, newline);
		column = last - last_linestart;

		ext::itoa_buffer<int> itoa_buffer;

		// json parse error: $err_str at offset = $offset, line = $line, column = $column
		std::string err_msg;
		err_msg.reserve(128);

		err_msg += FromQString(error.errorString());
		err_msg += " at offset = ";
		err_msg += ext::itoa(offset, itoa_buffer);
		err_msg += ", line = ";
		err_msg += ext::itoa(line, itoa_buffer);
		err_msg += ", column = ";
		err_msg += ext::itoa(column, itoa_buffer);

		json_parse_exception exception(err_msg);
		exception.parse_error = error;
		exception.offset = offset;
		exception.line   = line;
		exception.column = column;

		return exception;
	}

	[[noreturn]] void throw_json_path_exception(std::string_view path)
	{
		throw json_path_exception(std::string(path.data(), path.size()));
	}

	[[noreturn]] void throw_json_path_exception(const std::string & path)
	{
		throw json_path_exception(path);
	}

	[[noreturn]] void throw_json_path_exception(QString path)
	{
		throw json_path_exception(FromQString(path));
	}

	QJsonDocument parse_json(const QByteArray & utf8_data)
	{
		QJsonParseError error;
		QJsonDocument doc = QJsonDocument::fromJson(utf8_data, &error);

		if (not doc.isNull())
			return doc;

		throw create_json_parse_exception(error, utf8_data);
	}

	QJsonDocument parse_json(std::istream & utf8_is)
	{
		auto * sbuf = utf8_is.rdbuf();
		std::string utf8_data;
		const auto step = 4096;
		auto cur_size = utf8_data.size();

		for (;;)
		{
			utf8_data.resize(cur_size + step);
			auto * ptr = utf8_data.data() + cur_size;

			auto read = sbuf->sgetn(ptr, step);
			assert(read >= 0);
			cur_size += read;

			if (read < step)
				break;
		}

		utf8_data.resize(cur_size);
		return parse_json(utf8_data);
	}


	template <class Iterator>
	static Iterator skip_separators(Iterator iter)
	{
		while (*iter == separator) ++iter;
		return iter;
	}

	QJsonValue find_child(QJsonValue node, QString name)
	{
		if (node.isObject())
		{
			auto jobject = node.toObject();
			return jobject[name];
		}
		else if (node.isArray())
		{
			auto jarray = node.toArray();

			bool ok = false;
			int index = name.toInt(&ok);

			if (ok) return jarray[index];
			else    return QJsonValue(QJsonValue::Undefined);
		}
		else
			return QJsonValue(QJsonValue::Undefined);
	}

	QJsonValue find_child(QJsonValue node, std::string_view name)
	{
		if (node.isObject())
		{
			auto jobject = node.toObject();
			return jobject[ToQString(name)];
		}
		else if (node.isArray())
		{
			auto jarray = node.toArray();

			errno = 0;
			auto first = name.data();
			auto last  = first + name.size();
			int index;
			auto [ptr, errc] = std::from_chars(first, last, index);
			if (errc == std::errc() and ptr == last)
				return jarray[index];
			else
				return QJsonValue(QJsonValue::Undefined);
		}
		else
			return QJsonValue(QJsonValue::Undefined);
	}

	QJsonValue find_path(QJsonValue node, std::string_view path)
	{
		auto first = path.data();
		auto last  = first + path.size();

		first = skip_separators(first);
		while (first != last)
		{
			auto next = std::find(first, last, separator);
			node = find_child(node, std::string_view(first, next - first));
			if (node.isNull() or node.isUndefined()) return node;

			first = skip_separators(next);
		}

		return node;
	}

	QJsonValue find_path(QJsonValue node, const QString path)
	{
		const auto first = path.cbegin();
		const auto last  = path.cend();

		auto it = skip_separators(first);
		while (it != last)
		{
			auto next = std::find(it, last, separator);
			node = find_child(node, path.mid(it - first, next - it));
			if (node.isNull() or node.isUndefined()) return node;

			it = skip_separators(next);
		}

		return node;
	}

	QVariant find_value(QJsonValue node, std::string_view path)
	{
		QJsonValue result = find_path(node, path);
		return result.toVariant();
	}

	QVariant find_value(QJsonValue node, QString path)
	{
		QJsonValue result = find_path(node, path);
		return result.toVariant();
	}
}
