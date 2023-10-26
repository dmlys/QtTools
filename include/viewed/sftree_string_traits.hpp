#pragma once
#include <cstdint>
#include <tuple>
#include <string>
#include <string_view>

#include <QtCore/QHash>
#include <QtCore/QString>

namespace viewed
{
	struct sftree_string_traits
	{
		// set_name, get_name(leaf/node_type),
		// sort_pred_type, filter_pred_type
		// get_path should be defined in derived class

		using path_type     = std::string;
		using pathview_type = std::string_view;

		using path_equal_to_type = std::equal_to<>;
		using path_less_type     = std::less<>;
		using path_hash_type     = std::hash<pathview_type>;

		pathview_type get_name(pathview_type path) const;

		auto parse_path(pathview_type path, pathview_type context) const
			-> std::tuple<std::uintptr_t, pathview_type, pathview_type>;
			//                  type          name        new context

		bool is_child(pathview_type path, pathview_type context) const;

		path_type m_separators = "\\/";

		sftree_string_traits() = default;
		sftree_string_traits(path_type separators) : m_separators(std::move(separators)) {}
	};


	namespace sftree_detail
	{
		struct qqstring_hash
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
	}

	struct sftree_qstring_traits
	{
		// set_name, get_name(leaf/node_type),
		// sort_pred_type, filter_pred_type
		// get_path should be defined in derived class


		// QStringRef is somewhat inconvenient, it can be used, but because of it, ref.toString(), and &item.filename constructs must be used.
		// Using QString for both path_type and pathview_type is actually more convenient

		using path_type     = QString;
		using pathview_type = QString;

		using path_equal_to_type = std::equal_to<>;
		using path_less_type     = std::less<>;
		using path_hash_type     = sftree_detail::qqstring_hash;

		pathview_type get_name(const pathview_type & path) const;

		auto parse_path(const pathview_type & context, const pathview_type & path) const
			-> std::tuple<std::uintptr_t, pathview_type, pathview_type>;
			//                  type          name        new context

		bool is_child(const pathview_type & path, const pathview_type & context) const;

		path_type m_separators = QStringLiteral("\\/");

		sftree_qstring_traits() = default;
		sftree_qstring_traits(path_type separators) : m_separators(std::move(separators)) {}
	};

}
