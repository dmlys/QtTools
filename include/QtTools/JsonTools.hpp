#pragma once
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>

#include <QtTools/ToolsBase.hpp>

namespace QtTools::Json
{
	class json_path_exception : public std::runtime_error
	{
	public:
		using runtime_error::runtime_error;
	};

	class json_parse_exception : public std::runtime_error
	{
	public:
		int line, column, offset;
		QJsonParseError parse_error;

		// inherit constructors
		using runtime_error::runtime_error;
	};

	/// formats error message:
	/// $err_str at offset = $offset, line = $line, column = $column
	std::string error_report(const QJsonParseError & error, const QByteArray & parse_source);
	/// creates and fills json_parse_exception
	json_parse_exception create_json_parse_exception(const QJsonParseError & error, const QByteArray & parse_source);

	/// throws json_path_exception for given path
	[[noreturn]] void throw_json_path_exception(std::string_view path);
	[[noreturn]] void throw_json_path_exception(const std::string & path);
	[[noreturn]] void throw_json_path_exception(QString path);

	/// parses json from given text, throws std::runtime_error in case of error
	QJsonDocument parse_json(const QByteArray & utf8_data);
	QJsonDocument parse_json(std::istream & utf8_is);

	inline QJsonDocument parse_json(std::string_view utf8_str)
	{
		auto utf8_data= QByteArray::fromRawData(utf8_str.data(), utf8_str.size());
		return parse_json(utf8_data);
	}

	inline QJsonDocument parse_json(const char * utf8_str)
	{ return parse_json(std::string_view(utf8_str)); }

	/// path is similar to filesystem path: node/0/key
	/// root nodes are not supported(/root/inner) because QJsonValue does not provide a way to retrieve a parent -> can't go up
	/// if current node is an array then current segment should be number, than it's used as index to json array: key/12/

	namespace detail
	{
		template <class String>
		inline auto make_str(const String & str) -> std::enable_if_t<std::is_convertible_v<String, std::string_view>, std::string_view>
		{
			return std::string_view(str);
		}

		template <class String>
		inline auto make_str(const String & str) -> std::enable_if_t<not std::is_convertible_v<String, std::string_view> and std::is_convertible_v<String, QString>, QString>
		{
			return QString(str);
		}

		template <class Type>
		inline QJsonValue make_qjsonqvalue(Type val)
		{
			return QJsonValue(val);
		}

		inline QJsonValue make_qjsonqvalue(QJsonValue val)
		{
			return val;
		}

		inline QJsonValue make_qjsonqvalue(QJsonDocument doc)
		{
			if (doc.isArray())
				return QJsonValue(doc.array());
			else if (doc.isObject())
				return QJsonValue(doc.object());
			else
				return QJsonValue(QJsonValue::Undefined);
		}
	}


	QJsonValue find_child(QJsonValue node, QString name);
	QJsonValue find_child(QJsonValue node, std::string_view name);

	QJsonValue find_path(QJsonValue node, QString path);
	QJsonValue find_path(QJsonValue node, std::string_view path);

	template <class NodeArg, class StringArg>
	inline QJsonValue find_child(NodeArg && node, StringArg && str)
	{
		return find_child(detail::make_qjsonqvalue(std::forward<NodeArg>(node)), detail::make_str(std::forward<StringArg>(str)));
	}

	template <class NodeArg, class StringArg>
	inline QJsonValue find_path(NodeArg && node, StringArg && str)
	{
		return find_path(detail::make_qjsonqvalue(std::forward<NodeArg>(node)), detail::make_str(std::forward<StringArg>(str)));
	}

	template <class NodeArg, class StringArg>
	QJsonValue get_path(NodeArg && node_arg, StringArg && str_arg)
	{
		auto node = detail::make_qjsonqvalue(std::forward<NodeArg>(node_arg));
		auto path = detail::make_str(std::forward<StringArg>(str_arg));

		node = find_path(node, path);
		if (node.isNull() or node.isUndefined())
			throw_json_path_exception(path);

		return node;
	}

	template <class NodeArg, class StringArg>
	QVariant find_value(NodeArg && node_arg, StringArg && str_arg)
	{
		auto node = detail::make_qjsonqvalue(std::forward<NodeArg>(node_arg));
		auto path = detail::make_str(std::forward<StringArg>(str_arg));

		node = find_path(node, path);
		return node.toVariant();
	}

	template <class NodeArg, class StringArg>
	QVariant get_value(NodeArg && node_arg, StringArg && str_arg)
	{
		auto node = detail::make_qjsonqvalue(std::forward<NodeArg>(node_arg));
		auto path = detail::make_str(std::forward<StringArg>(str_arg));

		node = find_path(node, path);
		if (node.isNull() or node.isUndefined())
			throw_json_path_exception(path);

		return node.toVariant();
	}

	template <class NodeArg, class StringArg>
	inline QString find_qstring(NodeArg && node_arg, StringArg && str_arg)
	{
		return find_value(std::forward<NodeArg>(node_arg), std::forward<StringArg>(str_arg)).toString();
	}

	template <class NodeArg, class StringArg>
	inline QString get_qstring(NodeArg && node_arg, StringArg && str_arg)
	{
		return get_value(std::forward<NodeArg>(node_arg), std::forward<StringArg>(str_arg)).toString();
	}

	template <class NodeArg, class StringArg>
	inline std::string find_string(NodeArg && node_arg, StringArg && str_arg)
	{
		return QtTools::FromQString(find_qstring(std::forward<NodeArg>(node_arg), std::forward<StringArg>(str_arg)));
	}

	template <class NodeArg, class StringArg>
	inline std::string get_string(NodeArg && node_arg, StringArg && str_arg)
	{
		return QtTools::FromQString(get_qstring(std::forward<NodeArg>(node_arg), std::forward<StringArg>(str_arg)));
	}
}
