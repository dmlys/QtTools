#include <viewed/sftree_string_traits.hpp>
#include <viewed/sftree_facade_qtbase.hpp>

namespace viewed
{
	inline static auto find_first_of(std::string_view str, std::string_view needle)
	{
		return str.find_first_of(needle);
	}

	static std::string_view::size_type rfind_fist_of(std::string_view str, std::string_view needle)
	{
		using std::make_reverse_iterator;
		auto first = make_reverse_iterator(str.end());
		auto last  = make_reverse_iterator(str.begin());

		auto it = std::find_first_of(first, last, needle.begin(), needle.end());
		return it == last ? str.npos : last - it - 1; // -1 because of reverse iterator, and how it works
	}

	auto sftree_string_traits::get_name(pathview_type path) const -> pathview_type
	{
		int pos = rfind_fist_of(path, m_separators);
		return path.substr(pos + 1);
	}

	auto sftree_string_traits::parse_path(pathview_type path, pathview_type context) const
		-> std::tuple<std::uintptr_t, pathview_type, pathview_type>
	{
		//[first, last) - next segment in leaf_path,
		auto start = context.size();
		start = path.find_first_not_of(m_separators, start);
		if (start == path.npos) start = context.size();

		auto pos = path.find_first_of(m_separators, start);
		if (pos == path.npos)
		{
			pathview_type name = path.substr(start);
			return std::make_tuple(viewed::LEAF, std::move(name), std::move(context));
		}
		else
		{
			pathview_type name = path.substr(start, pos - start);
			pos = path.find_first_not_of(m_separators, pos);
			pathview_type newcontext = path.substr(0, pos);
			return std::make_tuple(viewed::PAGE, std::move(name), std::move(newcontext));
		}
	}

	bool sftree_string_traits::is_child(pathview_type path, pathview_type context) const
	{
		return 0 == path.compare(0, context.size(), context);
	}



	static int rfind_fist_of(const QString & str, const QString & needle)
	{
		using std::make_reverse_iterator;
		auto first = make_reverse_iterator(str.end());
		auto last  = make_reverse_iterator(str.begin());

		auto it = std::find_first_of(first, last, needle.begin(), needle.end());
		return it == last ? -1 : last - it - 1; // -1 because of reverse iterator, and how it works
	}

	static std::u16string_view str_view(const QString & str)
	{
		return std::u16string_view(reinterpret_cast<const char16_t *>(str.utf16()), str.size());
	}

	auto sftree_qstring_traits::get_name(const pathview_type & path) const -> pathview_type
	{
		int pos = rfind_fist_of(path, m_separators);
		return path.mid(pos + 1);
	}

	auto sftree_qstring_traits::parse_path(const pathview_type & path, const pathview_type & context) const
	    -> std::tuple<std::uintptr_t, pathview_type, pathview_type>
	{
		auto path_view    = str_view(path);
		auto context_view = str_view(context);
		auto separators_view = str_view(m_separators);

		//[first, last) - next segment in leaf_path,
		auto start = context_view.size();
		start = path_view.find_first_not_of(separators_view, start);
		if (start == path_view.npos) start = context_view.size();

		auto pos = path_view.find_first_of(separators_view, start);
		if (pos == path_view.npos)
		{
			pathview_type name = path.mid(start);
			return std::make_tuple(viewed::LEAF, std::move(name), std::move(context));
		}
		else
		{
			pathview_type name = path.mid(start, pos - start);
			pos = path_view.find_first_not_of(separators_view, pos);
			pathview_type newcontext = path.mid(0, pos);
			return std::make_tuple(viewed::PAGE, std::move(name), std::move(newcontext));
		}
	}

	bool sftree_qstring_traits::is_child(const pathview_type & path, const pathview_type & context) const
	{
		auto path_view = str_view(path);
		auto context_view = str_view(context);

		return 0 == path_view.compare(0, context_view.size(), context_view);
	}
}
