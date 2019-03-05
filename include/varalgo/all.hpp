#pragma once

/// wrappers of standard algorithms,
/// those wrappers allows to pass std::variant<Pred1, Pred2, ...> in place of sort predicate
/// that variant will be dispatched and algorithm will be called with actual predicate

#include <varalgo/sorting_algo.hpp>
#include <varalgo/on_sorted_algo.hpp>
#include <varalgo/set_operations.hpp>

#include <varalgo/minmax.hpp>
#include <varalgo/minmax_element.hpp>

#include <varalgo/partition_algo.hpp>

#include <varalgo/non_modifying_algo.hpp>
#include <varalgo/modifying_algo.hpp>
#include <varalgo/for_each.hpp>
