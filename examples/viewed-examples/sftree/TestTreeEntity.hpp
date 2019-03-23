#pragma once
#include <string>
#include <string_view>
#include <QtCore/QString>

/// this is original entity.
struct test_tree_entity
{
	QString filename;
	std::string sometext;
	int int_value;
};

/// extracts leaf name from path
inline std::string_view get_name(std::string_view filepath)
{
	int pos = filepath.rfind('/') + 1;
	return filepath.substr(pos);
}

/// extracts leaf name from path
inline QStringRef get_name(QStringRef filepath)
{
	int pos = filepath.lastIndexOf('/') + 1;
	return filepath.mid(pos);
}
