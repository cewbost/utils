#ifndef SDF_MAP_H_INCLUDED
#define SDF_MAP_H_INCLUDED

/** 
  @file
  @author Erik Boström <cewbostrom@gmail.com>
  @date 7.1.2016
  
  @section LICENSE
  
  MIT License (MIT)

  Copyright (c) 2015 Erik Boström

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
  
  @section DESCRIPTION
  
  A simple class for generating worley/cell noise in O(n) time. An object of this class
  stores a set of 2-dimensional maps storing the distance from each map element to the
  n:th point where n is the index of the map.
*/

#include <type_traits>
#include <cassert>
#include <vector>
#include <memory>
#include <cstring>

/**
  @brief A signed distance field map template
  @param F Value type for a field element. Should be a trivial scalar type.
  @param DistFunc Function to calculate distance between to points. Will be called with
  two arguments Representing the distance between two points along x and y axis.
  @param L The number of point distances for each element.
  @param no_copy If true disables copy-construction/assignment. defaults to false or
  true if __Worley_default_copyable is defined.
  @todo Memoization of values passed to DistFunc
*/

template<class F, F(*DistFunc)(F, F), int L = 1,
#ifdef __Worley_default_copyable
bool no_copy = false
#else
bool no_copy = true
#endif
>
class Worley
{
  static_assert(std::is_trivially_destructible<F>::value,
    "map elements must be trivially destructible");

protected:
  
  struct __Point
  {
    F x, y;
  };
  
  struct __Field
  {
    int points[L];
    F opacities[L];
    __Field()
    {
      for(auto& p: points)
        p = -1;
      for(auto& o: opacities)
        o = F(0);
    }
    
    void reset()
    {
      for(auto& p: points)
        p = -1;
    }
  };
  
  unsigned _width, _height;

  std::vector<__Point> _points;
  std::unique_ptr<__Field[]> _fields;
  
  bool _isPointPresent(__Field* field, int point)
  {
    for(auto p: field->points)
    {
      if(p == point)
        return true;
    }
    return false;
  }
  
  void _insertPoint(int point, unsigned x, unsigned y)
  {
    if(x >= _width || x < 0 || y >= _height || y < 0)
      return;
    __Field* field = &_fields[x + y * _width];
    if(_isPointPresent(field, point))
      return;
  
    __Point& p = _points[point];
    F opacity = DistFunc(p.x - F(x), p.y - F(y));
    
    //this could be optimized with loop-unrolling, unless the compiler can do it.
    for(int i = 0; i < L; ++i)
    {
      if(field->points[i] == -1)
      {
        field->points[i] = point;
        field->opacities[i] = opacity;
        break;
      }
      if(field->opacities[i] > opacity)
      {
        for(int j = L - 1; j > i; --j)
        {
          field->opacities[j] = field->opacities[j - 1];
          field->points[j] = field->points[j - 1];
        }
        field->points[i] = point;
        field->opacities[i] = opacity;
        break;
      }
    }
  }
  
  void _bleedInto(const __Field& other, int x, int y)
  {
    __Field& field = _fields[x + y * _width];
    
    for(int i = 0; i < L; ++i)
    {
      if(other.points[i] == -1)
        return;
      else if(_isPointPresent(&field, other.points[i]))
        continue;
      else
      {
        __Point& p = _points[other.points[i]];
        F opacity = DistFunc(p.x - F(x), p.y - F(y));
        
        for(int j = 0; j < L; ++j)
        {
          if(field.points[j] == -1 || field.opacities[j] >= opacity)
          {
            for(int k = L - 2; k >= j; --k)
            {
              field.opacities[k + 1] = field.opacities[k];
              field.points[k + 1] = field.points[k];
            }
            field.opacities[j] = opacity;
            field.points[j] = other.points[i];
            break;
          }
        }
      }
    }
  }
  
public:

  /**
    @brief Creates a signed distance field map.
    @param w Width in elements of the map.
    @param h Height in elements of the map.
  */
  Worley(unsigned w, unsigned h)
  {
    _fields.reset(new __Field[w * h]);
    _width = w;
    _height = h;
  }
  Worley(const Worley& other)
    : _width(other._width), _height(other._height), _points(other._points)
  {
    static_assert(!no_copy, "Worley copy constructor is disabled");
    _fields.reset(new __Field[_width * _height]);
    for(unsigned i = 0; i < _width * _height; ++i)
      _fields[i] = other._fields[i];
  }
  Worley(Worley&& other) = default;
  ~Worley() = default;
  
  Worley& operator=(const Worley& other)
  {
    static_assert(!no_copy, "Worley copy assignment is disabled");
    _width = other._width;
    _height = other._height;
    _points = other._points;
    _fields.reset(new __Field[_width * _height]);
    for(unsigned i = 0; i < _width * _height; ++i)
      _fields[i] = other._fields[i];
  }
  Worley& operator=(Worley&& other) = default;
  
  /**
    @brief Inserts a point into the maps point-set.
    @param x X coordinate of the point.
    @param y Y coordinate of the point.
  */
  void insertPoint(F x, F y)
  {
    //screw bounds checking
    _points.push_back({x, y});
    int _x = (int)(x);// + F(0.5));
    int _y = (int)(y);// + F(0.5));
    int p = _points.size() - 1;
    
    _insertPoint(p, _x, _y);
    _insertPoint(p, _x + 1, _y);
    _insertPoint(p, _x, _y + 1);
    _insertPoint(p, _x + 1, _y + 1);
  }
  
  /**
    @brief Computes the distance at every map element to the L nearest points.
  */
  void generateDistances()
  {
    //first direction
    for(unsigned y = 0; y < this->_height - 1; ++y)
    {
      _bleedInto(_fields[y * this->_width], 0, y + 1);
      _bleedInto(_fields[y * this->_width], 1, y + 1);
      _bleedInto(_fields[y * this->_width], 1, y);
      
      for(unsigned x = 1; x < this->_width - 1; ++x)
      {
        _bleedInto(_fields[x + y * this->_width], x - 1, y + 1);
        _bleedInto(_fields[x + y * this->_width], x, y + 1);
        _bleedInto(_fields[x + y * this->_width], x + 1, y + 1);
        _bleedInto(_fields[x + y * this->_width], x + 1, y);
      }
      
      _bleedInto(_fields[(y + 1) * this->_width - 1], this->_width - 2, y + 1);
      _bleedInto(_fields[(y + 1) * this->_width - 1], this->_width - 1, y + 1);
    }
    for(unsigned x = 0; x < this->_width - 1; ++x)
    {
      _bleedInto(_fields[x + this->_width * (this->_height - 1)],
        x + 1, this->_height - 1);
    }
    //other direction
    for(unsigned y = this->_height - 1; y > 1; --y)
    {
      _bleedInto(_fields[this->_width - 1 + y * this->_width], this->_width - 1, y - 1);
      _bleedInto(_fields[this->_width - 1 + y * this->_width], this->_width - 2, y - 1);
      _bleedInto(_fields[this->_width - 1 + y * this->_width], this->_width - 2, y);
      
      for(unsigned x = this->_width - 2; x > 1; --x)
      {
        _bleedInto(_fields[x + y * this->_width], x + 1, y - 1);
        _bleedInto(_fields[x + y * this->_width], x, y - 1);
        _bleedInto(_fields[x + y * this->_width], x - 1, y - 1);
        _bleedInto(_fields[x + y * this->_width], x - 1, y);
      }
      
      _bleedInto(_fields[y * this->_width], 1, y - 1);
      _bleedInto(_fields[y * this->_width], 0, y - 1);
    }
    for(unsigned x = this->_width - 1; x > 1; --x)
    {
      _bleedInto(_fields[x], x - 1, 0);
    }
  }
  
  /**
    @brief Accesses a single map field.
    @param I the index of the map. 0 is the distance to the nearest point.
    @param x X coordinate of map element.
    @param y Y coordinate of map element.
    @return a reference to the map element.
  */
  template<int I = 0>
  F& get(int x, int y)
  {
    static_assert(I >= 0 && I < L, "invalid template param in get");
    return _fields[x + y * _width].opacities[I];
  }
  
  template<int I = 0>
  F get(int x, int y) const
  {
    static_assert(I >= 0 && I < L, "invalid template param in get");
    return _fields[x + y * _width].opacities[I];
  }
  
  //other functions
  
  /**
    @brief Clears all map fields.
  */
  void clear()
  {
    _points.clear();
    for(unsigned i = 0; i < _width * _height; ++i)
      _fields[i].reset();
  }
  
  /**
    @brief Gets the map resolution.
    @param w Pointer to store the width.
    @param h Pointer to store the height.
  */
  void getResolution(int* w, int* h) const
  {
    *w = _width;
    *h = _height;
  }
  
  template<class A, A(*fn)(A, A), int P, bool B>
  friend class Worley;
};

#endif