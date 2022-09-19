/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include "broadphase_structure.hpp"
#include "equal.hpp"
#include "narrowphase/collision_shape.hpp"
#include "point3.hpp"
#include "vec3.hpp"

#include <list>
#include <unordered_map>
#include <vector>

namespace throttle {
namespace geometry {

template <typename T, typename t_shape = collision_shape<T>,
          typename = std::enable_if_t<std::is_base_of_v<collision_shape<T>, t_shape>>>
class uniform_grid : broadphase_structure<uniform_grid<T>, t_shape> {

  struct cell {
    point3<unsigned> point_t m_minimum_corner;
  };

  struct cell_hash {
    // compute hash bucket index in range [0, NUM_BUCKETS-1]
    int operator()(const cell &cell) {
      const int h1 = 0x8da6b343; // Large multiplication constants;
      const int h2 = 0xd8163841; // here arbirarily chosen primes
      const int h3 = 0xcb1ab31f;

      int hash = h1 * cell.x + h2 * cell.y + h3 * cell.z;
      return hash;
    }
  };

  /* A list of indexes of shapes in m_stored_shapes */
  using shape_list_t = typename std::list<unsigned>;

  /* map cell into shape_list_t */
  using map_t = typename std::unordered_map<cell, shape_list_t, cell_hash>;

  /* grid's cells size */
  T m_cell_size;

  /* queue of shapes to insert */
  std::vector<t_shape> m_waiting_queue;

  /* vector of inserted elements */
  std::vector<t_shape> m_stored_shapes;
  map_t                m_map;

  /* minimum and maximum values of the bounding box coordinates */
  std::optional<T> m_min_val, m_max_val;

public:
  using shape_type = t_shape;
  using vector_type = vec3<int>;

  /* ctor with hint about the number of shapes to insert */
  uniform_grid(unsigned number_hint) {
    m_waiting_queue.reserve(number_hint);
    m_stored_shapes.reserve(number_hint);
  }

  void add_collision_shape(const shape_type &shape) {
    m_waiting_queue.push_back(shape);
    auto &bbox = shape.bounding_box();
    auto  bbox_max_corner = bbox.maximum_corner();
    auto  bbox_min_corner = bbox.minimum_corner();
    auto  max_width = bbox.max_width();

    /* remain cell size to be large enough to fit the largest shape in any rotation */
    if (is_definitely_greater(max_width, m_cell_size)) m_cell_size = max_width;

    if (!m_min_val) { /* first insertion */
      m_min_val = vmin(bbox_min_corner.x, bbox_min_corner.y, bbox_min_corner.z);
      m_max_val = vmax(bbox_max_corner.x, bbox_max_corner.y, bbox_max_corner.z);
    } else {
      m_min_val = vmin(m_min_val, bbox_min_corner.x, bbox_min_corner.y, bbox_min_corner.z);
      m_max_val = vmax(m_max_val, bbox_max_corner.x, bbox_max_corner.y, bbox_max_corner.z);
    }
  }

  std::vector<shape_ptr> many_to_many() {
    rebuild();

    auto in_collision = find_all_collisions();

    std::vector<shape_ptr> result;
    std::transform(in_collision.begin(), in_collision.end(), std::back_inserter(result),
                   [&](const auto &elem) { return std::addressof(m_stored_shapes[elem]); });
    return result;
  }

  void rebuild() {
    m_map.clear();

    /* re-insert all old elements into the grid */
    for (auto &idx : m_stored_shapes) {
      m_stored_shapes.push_back(idx);
      insert(idx);
    }

    /* insert all the new shapes into the grid */
    for (auto &idx : m_waiting_queue) {
      m_stored_shapes.push_back(idx);
      insert(idx);
    }

    m_waiting_queue.clear();
  }

private:
  /*
   * find all cells the shape overlaps and insert shape's index into
   * the corresponding lists.
   */
  void insert(unsigned idx) {
    std::set<vector_type> cells{};
    cells.reserve(8);

    bool  straddling = false;
    auto &shape = m_stored_shapes[idx];

    auto          &bbox = shape.bounding_box();
    auto           min_corner = vector_type(bbox.minimum_corner());
    std::vector<T> hwidths = {2 * bbox.m_halfwidth_x, 2 * bbox.m_halfwidth_y, 2 * bbox.m_halfwidth_x};

    cells.insert(min_corner / m_cell_size);

    cells.insert((min_corner + vector_type{hwidths[0], 0, 0}) / m_cell_size);
    cells.insert((min_corner + vector_type{0, hwidths[1], 0}) / m_cell_size);
    cells.insert((min_corner + vector_type{0, 0, hwidths[2]}) / m_cell_size);

    cells.insert((min_corner + vector_type{0, hwidths[1], hwidths[2]}) / m_cell_size);
    cells.insert((min_corner + vector_type{hwidths[0], 0, hwidths[2]}) / m_cell_size);
    cells.insert((min_corner + vector_type{hwidths[0], hwidths[1], 0}) / m_cell_size);

    cells.insert((min_corner + vector_type{hwidths[0], hwidths[1], hwidths[2]}) / m_cell_size);

    for (auto &cell : cells)
      m_map[cell].push_back(idx);
  }

}

/* returns the set off all intersecting shapes in the cell's list */
std::set<unsigned>
find_all_collisions(shape_list_t cell_list) const {}

};

} // namespace geometry
} // namespace throttle