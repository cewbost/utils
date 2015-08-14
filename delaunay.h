#ifndef DELAUNAY_H_INCLUDED
#define DELAUNAY_H_INCLUDED

/** 
  @file
  @author Erik Boström <cewbostrom@gmail.com>
  @date 15.8.2015
  
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
  
  A simple class for generating delaunay triangulations in O(log n) time
*/

#include <vector>
#include <utility>
#include <algorithm>
#include <memory>
#include <cmath>
#include <complex>

/**
  @param T value type
*/
template<class T>
class Delaunay
{
  typedef std::pair<T, T> __Coords;
  
  friend __Coords operator+(__Coords a, __Coords b)
  {
    return __Coords(a.first + b.first, a.second + b.second);
  }
  friend __Coords operator-(__Coords a, __Coords b)
  {
    return __Coords(a.first - b.first, a.second - b.second);
  }
  friend __Coords operator*(__Coords a, int b)
  {
    return __Coords(a.first * b, a.second * b);
  }
  friend __Coords operator/(__Coords a, int b)
  {
    return __Coords(a.first / b, a.second / b);
  }
  friend __Coords operator*(__Coords a, double b)
  {
    return __Coords(a.first * b, a.second * b);
  }
  friend __Coords operator/(__Coords a, double b)
  {
    return __Coords(a.first / b, a.second / b);
  }
  friend __Coords _turn90d(__Coords v)
  {
    return __Coords(v.second, -v.first);
  }
  friend T _dotProduct(__Coords a, __Coords b)
  {
    return a.first * b.first + a.second * b.second;
  }
  friend T _distance(__Coords a, __Coords b)
  {
    __Coords helper = a - b;
    return T(std::sqrt(helper.first * helper.first
                    + helper.second * helper.second));
  }
  
  struct __VertCon
  {
    __VertCon* connections[8];
    __VertCon* more;

    void connect_ow(__VertCon& other)
    {
      for(auto& c: connections)
      {
        if(c == nullptr)
        {
          c = &other;
          return;
        }
      }
      if(!more) more = new __VertCon;
      more->connect_ow(other);
    }
    void disconnect_ow(__VertCon& other)
    {
      for(auto& c: connections)
      {
        if(c == &other)
        {
          c = nullptr;
          return;
        }
      }
      more->disconnect_ow(other);
    }

    void connect(__VertCon& other)
    {
      other.connect_ow(*this);
      this->connect_ow(other);
    }

    void disconnect(__VertCon& other)
    {
      other.disconnect_ow(*this);
      this->disconnect_ow(other);
    }

    void disconnectAll(__VertCon* real_this)
    {
      for(auto& c: connections)
      {
        if(c == nullptr) continue;
        c->disconnect_ow(*real_this);

        c =  nullptr;
      }
      if(more) more->disconnectAll(real_this);
    }

    void disconnectAll()
    {disconnectAll(this);}

    void listConnections(std::vector<intptr_t>& list)
    {
      for(auto c: connections)
      {
        if(c) list.push_back((intptr_t)c);
      }
      if(more) more->listConnections(list);
    }

    __VertCon()
    {
      for(auto& c: connections)
        c = nullptr;
      more = nullptr;
    }
    ~__VertCon()
    {
      for(auto& c: connections)
      {
        if(c) c->disconnect_ow(*this);
      }
      delete more;
    }

    friend bool isConnected(const __VertCon& a, const __VertCon& b)
    {
      const __VertCon* ptr = &a;
      do
      {
        for(auto c: ptr->connections)
        {
          if(c == &b)
            return true;
        }
        ptr = ptr->more;
      }while(ptr);
      return false;
    }
    
    friend __VertCon* getCommonConnection
        (const __VertCon& a, const __VertCon& b, const __VertCon& c)
    {
      const __VertCon* a_ptr = &a;
      do
      {
        for(auto con: a_ptr->connections)
        {
          if(con != nullptr)
          {
            if(con == &c)
              continue;
            else if(isConnected(*con, b)) return con;
          }
        }
        a_ptr = a_ptr->more;
      }while(a_ptr);
      return nullptr;
    }
  };
  
  const __Coords* _beg;
  const __Coords* _end;
  const std::pair<int, int>* _con_beg;
  const std::pair<int, int>* _con_end;
  //std::vector<std::pair<int, int>>* _cons;
  
  typename std::unique_ptr<__VertCon[]> _vert_cons;
  
  std::vector<int> _rev_sort_map;
  std::vector<int> _forward_sort_map;
  
  const __Coords& _vert(int idx)
  {
    return _beg[_rev_sort_map[idx]];
  }
  /*const std::pair<int, int>& _constraint(int idx)
  {
    return _con_beg[_rev_sort_map[idx]];
  }*/
  
  void _sort()
  {
    const int size = _end - _beg;
    
    
    _rev_sort_map.clear();
    _rev_sort_map.reserve(size);
    for(int i = 0; i < size; ++i)
      _rev_sort_map.push_back(i);
    
    std::sort(_rev_sort_map.begin(), _rev_sort_map.end(),
    [this](int i1, int i2)->bool
    {
      if(this->_beg[i1].first < this->_beg[i2].first)
        return true;
      else if(this->_beg[i1].first > this->_beg[i2].first)
        return false;
      else return (this->_beg[i1].second < this->_beg[i2].second);
    });
    
    _forward_sort_map.resize(size, 0);
    for(int i = 0; i < size; ++i)
      _forward_sort_map[_rev_sort_map[i]] = i;
  }
  
  
  void _retriangulate_h(
      std::vector<int>::iterator b,
      std::vector<int>::iterator e,
      __VertCon* con_array)
  {
    if(e - b <= 2) return; 
    
    --e;
    
    std::complex<T> main_line, sec_line;
    main_line = {_vert(*e).first - _vert(*b).first,
                _vert(*e).second - _vert(*b).second};
    std::vector<int>::iterator t = b + 1;
    std::vector<int>::iterator l = t;
    sec_line = {_vert(*t).first - _vert(*b).first,
                _vert(*t).second - _vert(*b).second};
    T angle1 = std::arg(sec_line / main_line);
    T angle2;
    if(angle1 > 0.)
    {
      for(++t; t != e; ++t)
      {
        sec_line = {_vert(*t).first - _vert(*b).first,
                    _vert(*t).second - _vert(*b).second};
        angle2 = std::arg(sec_line / main_line);
        if(angle2 < angle1)
        {
          if(t - l != 1)
          {
            con_array[*l].connect(con_array[*t]);
            _retriangulate_h(l, e, con_array);
          }
          con_array[*b].connect(con_array[*t]);
          l = t;
        }
      }
      if(e - l != 1)
      {
        con_array[*l].connect(con_array[*e]);
        _retriangulate_h(l, e, con_array);
      }
    }
    else
    {
      for(++t; t != e; ++t)
      {
        sec_line = {_vert(*t).first - _vert(*b).first,
                    _vert(*t).second - _vert(*b).second};
        angle2 = std::arg(sec_line / main_line);
        if(angle2 > angle1)
        {
          if(t - l != 1)
          {
            con_array[*l].connect(con_array[*t]);
            _retriangulate_h(l, e, con_array);
          }
          con_array[*b].connect(con_array[*t]);
          l = t;
        }
      }
      if(e - l != 1)
      {
        con_array[*l].connect(con_array[*e]);
        _retriangulate_h(l, e, con_array);
      }
    }
  }

public:

  /**
    @brief specifies the set of vertices
    
    vertices must be stored contiguously in memory as pairs of T
    
    @param beg pointer to the first vertex
    @param end pointer past the end of the range
    @return reference to this object
    
    @note calling this function will remove constraints
    @note passing a range shorter than 3 vertex pairs will likely break something
    @note passing a range of length not divisible by 2 will likely break something
  */
  Delaunay<T>& vertices(const T* beg, const T* end)
  {
    _beg = (const __Coords*)beg;
    _end = (const __Coords*)end;
    _con_beg = nullptr;
    _sort();
    return *this;
  }
  /**
    @brief specifies the set of vertices
    
    vertices must be stored contiguously in memory as pairs of T
    
    @param beg iterator to the first vertex
    @param end iterator past the end of the range
    @return reference to this object
    
    @note calling this function will remove constraints
    @note passing a range shorter than 3 vertex pairs will likely break something
    @note passing a range of length not divisible by 2 will likely break something
  */
  Delaunay<T>& vertices(
    typename std::vector<T>::const_iterator beg,
    typename std::vector<T>::const_iterator end)
  {
    _beg = (const __Coords*)&*beg;
    _end = (const __Coords*)&*end;
    _con_beg = nullptr;
    _sort();
    return *this;
  }
  /**
    @brief specifies the set of vertices
    
    vertices must be stored contiguously in memory as pairs of T
    
    @param verts vector containing vertices
    @return reference to this object
    
    @note calling this function will remove constraints
    @note passing a range shorter than 3 vertex pairs will likely break something
    @note passing a range of length not divisible by 2 will likely break something
  */
  Delaunay<T>& vertices(const std::vector<T>& verts)
  {
    _beg = (const __Coords*)&*verts.begin();
    _end = (const __Coords*)&*verts.end();
    _con_beg = nullptr;
    _sort();
    return *this;
  }
  
  /**
    @brief specifies constraints for triangulation
    
    the constraints must be stored contiguously in memory as pairs of vertex indices
    
    @param beg pointer to the first index
    @param end pointer past the end of the range
    @return reference to this object
    
    @note passing a range of length not divisible by 2 will likely break something
  */
  Delaunay<T>& constraints(
    const int* beg,
    const int* end)
  {
    _con_beg = (const std::pair<int, int>*)beg;
    _con_end = (const std::pair<int, int>*)end;
    return *this;
  }
  /**
    @brief specifies constraints for triangulation
    
    the constraints must be stored contiguously in memory as pairs of vertex indices
    
    @param beg iterator to the first index
    @param end iterator past the end of the range
    @return reference to this object
    
    @note passing a range of length not divisible by 2 will likely break something
  */
  Delaunay<T>& constraints(
    typename std::vector<int>::const_iterator beg,
    typename std::vector<int>::const_iterator end)
  {
    _con_beg = (const std::pair<int, int>*)&*beg;
    _con_end = (const std::pair<int, int>*)&*end;
    return *this;
  }
  /**
    @brief specifies constraints for triangulation
    
    the constraints must be stored contiguously in memory as pairs of vertex indices
    
    @param cons vector containing vertex indices
    @return reference to this object
    
    @note passing a range of length not divisible by 2 will likely break something
  */
  Delaunay<T>& constraints(const std::vector<int>& cons)
  {
    _con_beg = (const std::pair<int, int>*)&*cons.begin();
    _con_end = (const std::pair<int, int>*)&*cons.end();
    
    return *this;
  }
  
  /**
    @brief performs the triangulation
    
    this is where the magic happens
    
    @return reference to this object
  */
  Delaunay<T>& triangulate()
  {
    /*
      _beg: iterator to vertices sorted by x-coordinate
      sub_seq: array of sub-sequence indices
      num_sub_seq: number of sub-sequences
      index_list: list of triangle indices,
      initialized with num_sub_seq place-holders
      place_holder_list: array of place-holder iterators

      to fix:
        nothing, but if something fails check
        the initial triangulation code.
    */

    /*
      possible optimization:
        candidate lists might not always have to be recalculated
    */

    /*
      this function object calculates if a point lies outside the 
      circumcircle of a triangle
      arguments:
        a, b, c: points of the triangle, in counterclockwise order
        d: point to check
      note: returns true when all points lie on the same line
    */
    
    constexpr T __Pi(3.141592653589793);

    auto isDelaunay = [](__Coords a, __Coords b, __Coords c, __Coords d)->bool
    {
      T aa, ba, ca, ab, bb, cb, ac, bc, cc;
      aa = a.first - d.first;
      ba = a.second - d.second;
      ca = a.first * a.first + a.second * a.second
          - d.first * d.first - d.second * d.second;
      ab = b.first - d.first;
      bb = b.second - d.second;
      cb = b.first * b.first + b.second * b.second
          - d.first * d.first - d.second * d.second;
      ac = c.first - d.first;
      bc = c.second - d.second;
      cc = c.first * c.first + c.second * c.second
          - d.first * d.first - d.second * d.second;

      return (aa*bb*cc + ba*cb*ac + ca*ab*bc
              - ca*bb*ac - ba*ab*cc - aa*cb*bc) <= T(0);
    };

    ///another function object for calculating angle coefficients
    auto getAngleCoef = [](__Coords left, __Coords right)->T
    {return (T)(right.second - left.second)
      / (T)(right.first - left.first);};

    constexpr T theta = T(.000001);

    const unsigned v_size = _end - _beg;
    
    if(v_size < 3) return *this;

    std::vector<intptr_t> current_v_cons;
    using Candidate = std::pair<T, int>;
    std::vector<Candidate> current_v_cands;

    //this might be redundant
    current_v_cands.reserve(16);
    current_v_cons.reserve(16);
    
    _vert_cons.reset(new __VertCon[v_size]);

    ///initial triangulation
    //needlessly difficult
    //printf("Initial triangulation.\n");
    unsigned num_sub_seq = 0;
    std::unique_ptr<int[]> sub_seq(new int[v_size / 2 + 1]);
    {
      int first = -1;
      unsigned current;

      for(current = 0; current < v_size; ++current)
      {
        if(first == -1)
        {
          sub_seq[num_sub_seq] = current;
          ++num_sub_seq;
          first = current;
        }
        else if(current - first == 3)
        {
          if(_vert(current - 1).first == _vert(current).first)
          {
            _vert_cons[first].connect(_vert_cons[first + 1]);
            current -= 2;
          }
          else
          {
            if(getAngleCoef(_vert(first), _vert(first + 1))
            == getAngleCoef(_vert(first), _vert(first + 2)))
            {
              _vert_cons[first].connect(_vert_cons[first + 1]);
              _vert_cons[first + 1].connect(_vert_cons[first + 2]);
            }
            else
            {
              _vert_cons[first].connect(_vert_cons[first + 1]);
              _vert_cons[first + 1].connect(_vert_cons[first + 2]);
              _vert_cons[first].connect(_vert_cons[first + 2]);
            }

            --current;
          }
          first = - 1;
        }
        else if(current - first == 2)
        {
          if(_vert(first + 1).first == _vert(current).first)
          {
            do
            {
              ++current;
            }while(current < v_size
                  && _vert(first + 1).first
                  == _vert(current).first);
            --current;
            for(unsigned m = first + 1; m < current; ++m)
            {
              _vert_cons[first].connect(_vert_cons[m]);
              _vert_cons[m].connect(_vert_cons[m + 1]);
            }
            _vert_cons[first].connect(_vert_cons[current]);
            first = -1;
          }
        }
        else if(_vert(first).first == _vert(current).first)
        {
          do
          {
            ++current;
          }while(current < v_size
                && _vert(first).first
                == _vert(current).first);

          if(current == v_size)
          {
            --current;
            for(unsigned m = first; m < current; ++m)
              _vert_cons[m].connect(_vert_cons[m + 1]);
          }
          else
          {
            for(unsigned m = first; m < current - 1; ++m)
            {
              _vert_cons[m].connect(_vert_cons[m + 1]);
              _vert_cons[m].connect(_vert_cons[current]);
            }
            _vert_cons[current - 1].connect(_vert_cons[current]);
          }
          first = -1;
        }
      }

      switch(current - first)
      {
      case 3:
        if(getAngleCoef(_vert(first), _vert(first + 1)) ==
          getAngleCoef(_vert(first), _vert(first + 2)))
        {
          _vert_cons[first].connect(_vert_cons[first + 1]);
          _vert_cons[first + 1].connect(_vert_cons[first + 2]);
        }
        else
        {
          _vert_cons[first].connect(_vert_cons[first + 1]);
          _vert_cons[first + 1].connect(_vert_cons[first + 2]);
          _vert_cons[first].connect(_vert_cons[first + 2]);
        }

        break;
      case 2:
        _vert_cons[first].connect(_vert_cons[first + 1]);

        break;
      default: break;
      }

      sub_seq[num_sub_seq] = v_size;
    }

    ///main triangulation loop
    int left, middle, right;
    int low_l, low_r;
    int l_cand, r_cand;
    //std::vector<decltype(index_list.begin())> connected_triangles;
    //__Coords temp;

    for(unsigned n = 2; (n >> 1) < num_sub_seq; n <<= 1)
    {
      //if(n > 2) break;
      for(unsigned m = 0; m < num_sub_seq; m += n)
      {
        if(m + n / 2 >= num_sub_seq) break;
        left = sub_seq[m];
        middle = sub_seq[m + n / 2];
        right = m + n >= num_sub_seq? 
          sub_seq[num_sub_seq] : sub_seq[m + n];

        ///find lowest
        low_l = left; low_r = middle;
        for(int v = left + 1; v < middle; ++v)
        {
          if(_vert(low_l).second >= _vert(v).second)
            low_l = v;
        }
        for(int v = middle + 1; v < right; ++v)
        {
          if(_vert(low_r).second > _vert(v).second)
            low_r = v;
        }

        {
          T temp = getAngleCoef(_vert(low_l), _vert(low_r));
          T temp2;

          //positive angle
          if(temp > 0.)
          {
            int old_low_l;
            do
            {
              old_low_l = low_l;
              for(int v = low_r + 1; v < right; ++v)
              {
                temp2 = getAngleCoef(_vert(low_l),
                                    _vert(v));
                if(temp2 < temp)
                {
                  low_r = v;
                  temp = temp2;
                }
              }
              for(int v = low_l + 1; v < middle; ++v)
              {
                temp2 = getAngleCoef(_vert(v),
                      _vert(low_r));
                if(temp2 >= temp)
                {
                  low_l = v;
                  temp = temp2;
                }
              }
            }while(old_low_l != low_l);
          }
          //negative angle
          else if(temp < 0.)
          {
            int old_low_r;
            do
            {
              old_low_r = low_r;
              for(int v = low_l - 1; v >= left; --v)
              {
                temp2 = getAngleCoef(_vert(v),
                                    _vert(low_r));
                if(temp2 > temp)
                {
                  low_l = v;
                  temp = temp2;
                }
              }
              for(int v = low_r - 1; v >= middle; --v)
              {
                temp2 = getAngleCoef(_vert(low_l),
                                    _vert(v));
                if(temp2 <= temp)
                {
                  low_r = v;
                  temp = temp2;
                }
              }
            }while(old_low_r != low_r);
          }
        }

        ///sewing loop
        do
        {

          ///find candidates
          std::complex<T> cpx;

          /*
            keep in mind:
              the candidates are listed and then culled,
              removing all candidates with an angle >180deg.
              might be that that requirement only aplies
              to the first candidate in any given comparison.
          */

          //left candidates
          cpx = {_vert(low_r).first - _vert(low_l).first,
              _vert(low_r).second - _vert(low_l).second};
          current_v_cons.clear();
          current_v_cands.clear();
          _vert_cons[low_l].listConnections(current_v_cons);
          for(auto i: current_v_cons)
          {
            auto j = i - (intptr_t)_vert_cons.get();
            j /= sizeof(__VertCon);
            std::complex<T> comp(
              _vert(j).first - _vert(low_l).first,
              _vert(j).second - _vert(low_l).second);
            current_v_cands.push_back({std::arg(comp / cpx), j});
          }
          current_v_cands.erase(std::remove_if(
            current_v_cands.begin(), current_v_cands.end(),
            [middle](Candidate& cand) -> bool
            {return cand.first < .0
              || cand.first > __Pi - theta
              || cand.second >= middle;}),
            current_v_cands.end());

          std::sort(current_v_cands.begin(), current_v_cands.end(),
          [](const Candidate& a, const Candidate& b)
          {
            //if(a.first == b.first)
            //  printf("GGG.\n");
            return a.first < b.first;
          });

          if(current_v_cands.size() == 0)
            l_cand = -1;
          else
          {
            auto it2 = current_v_cands.begin();
            auto it1 = it2++;
            l_cand = it1->second;
            while(it2 != current_v_cands.end())
            {
              if(isDelaunay(_vert(low_l),
                _vert(low_r),
                _vert(it1->second),
                _vert(it2->second)))
                break;
              _vert_cons[low_l].disconnect(_vert_cons[it1->second]);
              it1 = it2++;
              l_cand = it1->second;
            }
          }

          //right candidates
          cpx = -cpx;
          current_v_cons.clear();
          current_v_cands.clear();
          _vert_cons[low_r].listConnections(current_v_cons);
          for(auto i: current_v_cons)
          {
            intptr_t j = i - (intptr_t)_vert_cons.get();
            j /= sizeof(__VertCon);
            std::complex<T> comp(
              _vert(j).first - _vert(low_r).first,
              _vert(j).second - _vert(low_r).second);
            current_v_cands.push_back(Candidate(
              -std::arg(comp / cpx), j));
          }
          current_v_cands.erase(std::remove_if(
            current_v_cands.begin(), current_v_cands.end(),
            [middle](Candidate& cand) -> bool
            {return cand.first < 0.
              || cand.first > __Pi - theta
              || cand.second < middle;}),
            current_v_cands.end());

          std::sort(current_v_cands.begin(), current_v_cands.end(),
          [](const Candidate& a, const Candidate& b)
          {
            //if(a.first == b.first)
            //  printf("HHHl\n");
            return a.first < b.first;
          });

          if(current_v_cands.size() == 0)
            r_cand = -1;
          else
          {
            auto it2 = current_v_cands.begin();
            auto it1 = it2++;
            r_cand = it1->second;
            while(it2 != current_v_cands.end())
            {
              if(isDelaunay(_vert(low_l),
                            _vert(low_r),
                            _vert(it1->second),
                            _vert(it2->second)))
                break;
              _vert_cons[low_r].disconnect(_vert_cons[it1->second]);
              it1 = it2++;
              r_cand = it1->second;
            }
          }

          ///sew together
          _vert_cons[low_l].connect(_vert_cons[low_r]);
          //printf("connected %i and %i\n", low_l, low_r);
          if(l_cand != -1)
          {
            if(r_cand != -1)
            {
              if(isDelaunay(_vert(low_l), _vert(low_r),
                            _vert(l_cand), _vert(r_cand)))
                low_l = l_cand;
              else low_r = r_cand;
            }
            else low_l = l_cand;
          }
          else if(r_cand != -1)
            low_r = r_cand;
          else break;

        }while(true);//end sewing loop

      }
    }
    
    if(_con_beg)
    {
      for(auto con = _con_beg; con != _con_end; ++con)
      {
        int curr = _forward_sort_map[con->first];
        int targ = _forward_sort_map[con->second];
        if(!isConnected(_vert_cons[curr], _vert_cons[targ]))
        {
          //missing connection
          std::vector<int> left_side, right_side;
          std::complex<T> main_comp = {
            _vert(targ).first - _vert(curr).first,
            _vert(targ).second - _vert(curr).second};
          
          //find first two cons and push them into left_side and right_side vectors
          int l_con, r_con;
          
          l_con = r_con = -1;
          
          T l_angle = 10.;
          T r_angle = -10.;
          
          current_v_cons.clear();
          _vert_cons[curr].listConnections(current_v_cons);
          
          for(auto i: current_v_cons)
          {
            intptr_t j = i - (intptr_t)_vert_cons.get();
            j /= sizeof(__VertCon);
            
            T temp_d = std::arg(std::complex<T>(
              _vert(j).first - _vert(curr).first,
              _vert(j).second - _vert(curr).second)
              / main_comp);
            if(temp_d > 0.)
            {
              if(temp_d < l_angle)
              {
                l_angle = temp_d;
                l_con = j;
              }
            }
            else
            {
              if(temp_d > r_angle)
              {
                r_angle = temp_d;
                r_con = j;
              }
            }
          }
          left_side.push_back(l_con);
          right_side.push_back(r_con);
          
          //list cons
          __VertCon *next_con, *last_con;
          last_con = &_vert_cons[curr];
          while((next_con = getCommonConnection(_vert_cons[l_con],
                _vert_cons[r_con], *last_con)) != _vert_cons.get() + targ)
          {
            intptr_t j = (intptr_t)next_con - (intptr_t)_vert_cons.get();
            j /= sizeof(__VertCon);
            
            if(std::arg(std::complex<T>(
              _vert(j).first - _vert(curr).first,
              _vert(j).second - _vert(curr).second)
              / main_comp) > 0.)
            {
              last_con = &_vert_cons[l_con];
              l_con = j;
              left_side.push_back(l_con);
            }
            else
            {
              last_con = &_vert_cons[r_con];
              r_con = j;
              right_side.push_back(r_con);
            }
          }
          
          //disconnect all connections that cross the constraint
          for(auto l: left_side)
          for(auto r: right_side)
          if(isConnected(_vert_cons[l], _vert_cons[r]))
              _vert_cons[l].disconnect(_vert_cons[r]);
          
          //push curr and targ into containers (_retriangulate_h expects it)
          left_side.insert(left_side.begin(), curr);
          right_side.insert(right_side.begin(), curr);
          left_side.push_back(targ);
          right_side.push_back(targ);
          
          //new triangulation
          _vert_cons[curr].connect(_vert_cons[targ]);
          _retriangulate_h(left_side.begin(), left_side.end(), _vert_cons.get());
          _retriangulate_h(right_side.begin(), right_side.end(), _vert_cons.get());
        }
      }
    }

    return *this;
  }
  
  /**
    @brief returns list of vertex indices
    
    the returned vector will contain the edges of the triangulation as index pairs
    
    @return vector containing vertex index pairs
  */
  std::vector<int> edges()
  {
    const unsigned v_size = _end - _beg;
    
    std::vector<int> edges;
    
    std::vector<intptr_t> current_v_cons;
    
    current_v_cons.reserve(16);
    
    for(unsigned n = 0; n < v_size; ++n)
    {
      current_v_cons.clear();
      _vert_cons[n].listConnections(current_v_cons);
      for(auto i: current_v_cons)
      {
        intptr_t j = i - (intptr_t)_vert_cons.get(); j /= sizeof(__VertCon);
        if(j > n)
        {
          edges.push_back(_rev_sort_map[(int)n]);
          edges.push_back(_rev_sort_map[j]);
        }
      }
    }
    
    return edges;
  }
  
  /**
    @brief returns list of vertex indices
    
    the returned vector will contain the triangles of the triangulation as integer
    triplets
    
    @return vector containing vertex index triplets
  */
  std::vector<int> triangles()
  {
    const unsigned v_size = _end - _beg;
  
    std::vector<int> triangles;
    
    std::vector<intptr_t> current_v_cons;
    using Candidate = std::pair<T, int>;
    std::vector<Candidate> current_v_cands;

    //this might be redundant
    current_v_cands.reserve(16);
    current_v_cons.reserve(16);
    
    for(unsigned n = 0; n < v_size - 1; ++n)
    {
      current_v_cons.clear();
      current_v_cands.clear();
      _vert_cons[n].listConnections(current_v_cons);
      for(auto i: current_v_cons)
      {
        intptr_t j = i - (intptr_t)_vert_cons.get(); j /= sizeof(__VertCon);
        if(j > n)
        {
          std::complex<T> comp(
            _vert(j).first - _vert(n).first,
            _vert(j).second - _vert(n).second);
          current_v_cands.push_back(Candidate(std::arg(comp), j));
        }
      }
      std::sort(current_v_cands.begin(), current_v_cands.end(),
      [](const Candidate& a, const Candidate& b)
      {return a.first < b.first;});
      for(unsigned m = 0; m < current_v_cands.size() - 1; ++m)
      {
        auto v1 = current_v_cands[m].second;
        auto v2 = current_v_cands[m + 1].second;
        triangles.push_back(_rev_sort_map[(int)n]);
        triangles.push_back(_rev_sort_map[v2]);
        triangles.push_back(_rev_sort_map[v1]);
      }
    }
    
    return triangles;
  }
  
  //constructors
  Delaunay()
  {
    _con_beg = nullptr;
  }
  /**
    @brief constructs Delaunay object and assigns vertex range
    
    @param beg pointer to the first vertex
    @param end pointer past the end of the range
    @param b perform triangulation immediately. default: false
    
    @note see vertices
  */
  Delaunay(const T* beg, const T* end, bool b = false)
  {
    vertices(beg, end);
    if(b) triangulate();
  }
  /**
    @brief constructs Delaunay object and assigns vertex and constraint range
    
    @param beg pointer to the first vertex
    @param end pointer past the end of the range
    @param c_beg pointer to the first index
    @param c_end pointer past the end of the range
    @param b perform triangulation immediately. default: false
    
    @note see vertices, constraints
  */
  Delaunay(
    const T* beg,
    const T* end,
    const int* c_beg,
    const int* c_end,
    bool b = false)
  {
    vertices(beg, end);
    constraints(c_beg, c_end);
    if(b) triangulate();
  }
  /**
    @brief constructs Delaunay object and assigns vertex range
    
    @param beg iterator to the first vertex
    @param end iterator past the end of the range
    @param b perform triangulation immediately. default: false
    
    @note see vertices
  */
  Delaunay(
    typename std::vector<T>::const_iterator beg,
    typename std::vector<T>::const_iterator end,
    bool b = false)
  {
    vertices(beg, end);
    if(b) triangulate();
  }
  /**
    @brief constructs Delaunay object and assigns vertex and constraint range
    
    @param beg iterator to the first vertex
    @param end iterator past the end of the range
    @param c_beg iterator to the first index
    @param c_end iterator past the end of the range
    @param b perform triangulation immediately. default: false
    
    @note see vertices, constraints
  */
  Delaunay(
    typename std::vector<T>::const_iterator beg,
    typename std::vector<T>::const_iterator end,
    typename std::vector<int>::const_iterator c_beg,
    typename std::vector<int>::const_iterator c_end,
    bool b = false)
  {
    vertices(beg, end);
    constraints(c_beg, c_end);
    if(b) triangulate();
  }
  /**
    @brief constructs Delaunay object and assigns vertex range
    
    @param verts std::vector containing vertices
    @param b perform triangulation immediately. default: false
    
    @note see vertices
  */
  Delaunay(const std::vector<T>& verts, bool b = false)
  {
    vertices(verts);
    if(b) triangulate();
  }
  /**
    @brief constructs Delaunay object and assigns vertex range
    
    @param verts vector containing vertices
    @param cons vector containing vertex indices
    @param b perform triangulation immediately. default: false
    
    @note see vertices
  */
  Delaunay(
    const std::vector<T>& verts,
    const std::vector<int>& cons,
    bool b = false)
  {
    vertices(verts);
    constraints(cons);
    if(b) triangulate();
  }
  
  ~Delaunay() = default;
};

#endif
