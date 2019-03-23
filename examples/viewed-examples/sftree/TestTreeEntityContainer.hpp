#pragma once
#include "TestTreeEntity.hpp"
#include <viewed/hash_container.hpp>

using TestTreeContainer = viewed::hash_container<test_tree_entity, boost::multi_index::key<&test_tree_entity::filename>>;
