#ifndef SSO_VECTOR_H_INCLUDED
#define SSO_VECTOR_H_INCLUDED

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
  
  A dynamic buffer with "small vector optimization". Vector sizes below a certain
  threshold are stored inside the object itself, avoiding dynamic allocations. This
  can be used for optimization purposes when you know that a vector will likely hold
  a small amount of elements, but you need it to be able to hold an unlimited amount.
  
  usage:
  
  pretty much the same as std::vector
*/

#include <new>
#include <cstddef>
#include <iterator>
#include <initializer_list>
#include <utility>

/**
  @param Type value type
  @param Size max size for in-object buffer
*/
template<class Type, int Size>
class SSOVector
{
public:

  typedef Type value_type;

private:
  size_t _size = 0;
  union
  {
    char _ss_buffer[sizeof(Type) * Size];
    struct
    {
      size_t capacity;
      char* buffer;
    }_ls_buffer;
  };
  
  Type* _sBuff(){return reinterpret_cast<Type*>(_ss_buffer);}
  const Type* _sBuff()const{return reinterpret_cast<const Type*>(_ss_buffer);}
  Type* _lBuff(){return reinterpret_cast<Type*>(_ls_buffer.buffer);}
  const Type* _lBuff()const{return reinterpret_cast<const Type*>(_ls_buffer.buffer);}
  
  static Type* _osBuff(SSOVector& other)
  {return reinterpret_cast<Type*>(other._ss_buffer);}
  static const Type* _osBuff(const SSOVector& other)
  {return reinterpret_cast<const Type*>(other._ss_buffer);}
  static Type* _olBuff(SSOVector& other)
  {return reinterpret_cast<Type*>(other._ls_buffer.buffer);}
  static const Type* _olBuff(const SSOVector& other)
  {return reinterpret_cast<const Type*>(other._ls_buffer.buffer);}
  
  void _destroyContent()
  {
    if(_size > Size)
    {
      for(size_t i = 0; i < _size; ++i)
        _lBuff()[i].~Type();
      delete[] _ls_buffer.buffer;
    }
    else
    {
      for(size_t i = 0; i < _size; ++i)
        _sBuff()[i].~Type();
    }
  }
  
  void _makeRoomInitial()
  {
    char* buffer = new char[sizeof(Type) * Size * 2];
    
    for(size_t i = 0; i < _size; ++i)
    {
      new(buffer + sizeof(Type) * i) Type(std::move(_sBuff()[i]));
      _sBuff()[i].~Type();
    }
    
    _ls_buffer.buffer = buffer;
    _ls_buffer.capacity = Size * 2;
  }
  
  void _makeRoomForMore()
  {
    size_t new_capacity = (_ls_buffer.capacity * 3) >> 1;
    char* buffer = new char[sizeof(Type) * new_capacity];
    
    for(size_t i = 0; i < _size; ++i)
    {
      new(buffer + sizeof(Type) * i) Type(std::move(_lBuff()[i]));
      _lBuff()[i].~Type();
    }
    
    delete[] _ls_buffer.buffer;
    _ls_buffer.buffer = buffer;
    _ls_buffer.capacity = new_capacity;
  }
  
  void _moveBack()
  {
    Type* buffer = _lBuff();
    for(size_t i = 0; i < _size; ++i)
    {
      new(_sBuff() + i) Type(std::move(buffer[i]));
      buffer[i].~Type();
    }
    delete[] reinterpret_cast<char*>(buffer);
  }

  
  Type* _activeBuffer()
  {
    if(_size > Size) return _lBuff();
    else return _sBuff();
  }
  Type* _activeBuffer() const
  {
    if(_size > Size) return _lBuff();
    else return _sBuff();
  }
  
  static void _moveForward(Type* from, Type* to, size_t steps)
  {
    if(from == to) return;
    Type* current = to - 1;
    Type* target = current + steps;
    
    if((size_t)(to - from) <= steps)
    {
      do
      {
        new(target) Type(std::move(*current));
        current->~Type();
        --current;
        --target;
      }while((uintptr_t)current >= (uintptr_t)from);
    }
    else
    {
      for(int i = steps - 1; i >= 0; --i)
      {
        new(target) Type(std::move(*current));
        --current;
        --target;
      }
      do
      {
        *target = std::move(*current);
        --target;
        --current;
      }while(current >= from);
      for(size_t i = 0; i < steps; ++i)
        from[i].~Type();
    }
  }
  
  static void _moveForward(Type* from, Type* to)
  {
    if(from == to) return;
    
    new(to) Type(std::move(*(to - 1)));
    --to;
    if(to != from)
    {
      *to = std::move(*(to - 1));
      --to;
    }
    from->~Type();
  }
  
  static void _moveBackward(Type* from, Type* to, size_t steps)
  {
    Type* target = from - steps;
    if(steps >= (size_t)(to - from))
    {
      while(from != to)
      {
        new(target) Type(std::move(*from));
        from->~Type();
        ++target;
        ++from;
      }
    }
    else
    {
      for(size_t i = 0; i < steps; ++i)
      {
        new(target) Type(std::move(*from));
        ++target;
        ++from;
      }
      while(from != to)
      {
        *target = std::move(*from);
        ++target;
        ++from;
      }
      from -= steps;
      for(size_t i = 0; i < steps; ++i)
      {
        from->~Type();
        ++from;
      }
    }
  }
  
  static void _moveBackward(Type* from, Type* to)
  {
    new(from - 1) Type(std::move(*from));
    ++from;
    while(from != to)
    {
      *(from - 1) = std::move(*from);
      ++from;
    }
    (from - 1)->~Type();
  }
  
  static void _moveToBuffer(Type* it1, Type* it2, Type* it3, Type* target, int gap)
  {
    while(it1 != it2)
    {
      new((target++)) Type(std::move(*it1));
      it1->~Type();
      ++it1;
    }
    target += gap;
    while(it1 != it3)
    {
      new((target++)) Type(std::move(*it1));
      it1->~Type();
      ++it1;
    }
  }
  
  static void _moveToBuffer(
    Type* begin1,
    Type* end1,
    Type* begin2,
    Type* end2,
    Type* target)
  {
    while(begin1 != end1)
    {
      new(target) Type(std::move(*begin1));
      begin1->~Type();
      ++target;
      ++begin1;
    }
    while(begin2 != end2)
    {
      new(target) Type(std::move(*begin2));
      begin2->~Type();
      ++target;
      ++begin2;
    }
  }
  
  void _split(int at)
  {
    if(_size < Size)
    {
      _moveForward(_sBuff() + at, _sBuff() + _size);
    }
    else if(_size == Size)
    {
      char* buffer = new char[sizeof(Type) * Size * 2];
      _moveToBuffer(_sBuff(), _sBuff() + at, _sBuff() + _size, (Type*)buffer, 1);
      _ls_buffer.buffer = buffer;
      _ls_buffer.capacity = Size * 2;
    }
    else if(_size < _ls_buffer.capacity)
    {
      _moveForward(_lBuff() + 1, _lBuff() + _size);
    }
    else
    {
      size_t new_capacity = (_ls_buffer.capacity * 3) >> 1;
      char* buffer = new char[sizeof(Type) * new_capacity];
      _moveToBuffer(_lBuff(), _lBuff() + at, _lBuff() + _size, (Type*)buffer, 1);
      delete[] _ls_buffer.buffer;
      _ls_buffer.buffer = buffer;
      _ls_buffer.capacity = new_capacity;
    }
    --_size;
  }
  
  void _split(int from, int gap)
  {
    size_t new_size = _size + gap;
    if(_size <= Size)
    {
      if(new_size <= Size)
      {
        _moveForward(_sBuff() + from, _sBuff() + _size, gap);
      }
      else
      {
        size_t new_capacity = Size * 2;
        while(new_capacity < new_size) new_capacity <<= 1;
        char* buffer = new char[sizeof(Type) * new_capacity];
        _moveToBuffer(_sBuff(), _sBuff() + from, _sBuff() + _size, (Type*)buffer, gap);
        _ls_buffer.buffer = buffer;
        _ls_buffer.capacity = new_capacity;
      }
    }
    else
    {
      if(new_size <= _ls_buffer.capacity)
      {
        _moveForward(_lBuff() + from, _lBuff() + _size, gap);
      }
      else
      {
        size_t new_capacity = _ls_buffer.capacity << 1;
        while(new_capacity < new_size) new_capacity <<= 1;
        char* buffer = new char[sizeof(Type) * new_capacity];
        _moveToBuffer(_lBuff(), _lBuff() + from, _lBuff() + _size, (Type*)buffer, gap);
        delete[] _ls_buffer.buffer;
        _ls_buffer.buffer = buffer;
        _ls_buffer.capacity = new_capacity;
      }
    }
    _size = new_size;
  }
  
  void _join(size_t at)
  {
    if(at == _size - 1);
    else if(_size <= Size)
    {
      _moveBackward(_sBuff() + at + 1, _sBuff() + _size);
    }
    else if(_size == Size + 1)
    {
      char* buffer = reinterpret_cast<char*>(_lBuff());
      _moveToBuffer(
        _lBuff(), _lBuff() + at,
        _lBuff() + at + 1, _lBuff() + _ls_buffer.capacity,
        _sBuff());
      delete[] buffer;
    }
    else
    {
      _moveBackward(_lBuff() + at + 1, _lBuff() + _size);
    }
    --_size;
  }
  void _join(size_t from, size_t num)
  {
    if(from + num >= _size - 1);
    else if(_size <= Size)
    {
      _moveBackward(_sBuff() + from + num, _sBuff() + _size, num);
    }
    else if(_size - num <= Size)
    {
      Type* buffer = _lBuff();
      _moveToBuffer(
        _lBuff(), _lBuff() + from,
        _lBuff() + from + num, _lBuff() + _ls_buffer.capacity,
        _sBuff());
        delete[] buffer;
    }
    else
    {
      _moveBackward(_lBuff() + from + num, _lBuff() + _size, num);
    }
    _size -= num;
  }

public:

  //iterator types
  typedef Type* iterator;
  typedef const Type* const_iterator;
  typedef std::reverse_iterator<Type*> reverse_iterator;
  typedef std::reverse_iterator<const Type*> const_reverse_iterator;
  
  //construction and assignment
  
  SSOVector(): _size(0){}
  SSOVector(const SSOVector& other)
  {
    _size = other._size;
    if(_size <= Size)
    {
      for(size_t i = 0; i < _size; ++i)
        new(_ss_buffer + sizeof(Type) * i) Type(_osBuff(other)[i]);
    }
    else
    {
      _ls_buffer.capacity = other._ls_buffer.capacity;
      _ls_buffer.buffer = new char[sizeof(Type) * _ls_buffer.capacity];
      for(size_t i = 0; i < _size; ++i)
        new(_ls_buffer.buffer + sizeof(Type) * i) Type(_olBuff(other)[i]);
    }
  }
  SSOVector(SSOVector&& other)
  {
    _size = other._size;
    if(_size > Size)
    {
      _ls_buffer.capacity = other._ls_buffer.capacity;
      _ls_buffer.buffer = other._ls_buffer.buffer;
      other._size = 0;
      //delete[] other._ls_buffer.buffer;
      //warning: this leaves a dangling pointer in the moved from object
      //_size being zero will prevent deleting
    }
    else
    {
      for(size_t i = 0; i < _size; ++i)
        new(_ss_buffer + sizeof(Type) * i) Type(_osBuff(other)[i]);
      //same as copying for small objects
    }
  }
  template<class InputIt>
  SSOVector(InputIt b, InputIt e)
  {
    _size = e - b;
    char* pos;
    if(_size > Size)
    {
      _ls_buffer.capacity = (_size * 3) >> 1;
      _ls_buffer.buffer = new char[_ls_buffer.capacity * sizeof(Type)];
      pos = _ls_buffer.buffer;
    }
    else pos = _ss_buffer;
    for(size_t i = 0; i < _size; ++i)
      new(pos + sizeof(Type) * i) Type(*(b++));
  }
  template<class InType>
  SSOVector(const std::initializer_list<InType>& list)
  {
    _size = list.size();
    auto it = list.begin();
    char* pos;
    if(_size > Size)
    {
      _ls_buffer.capacity = (_size * 3) >> 1;
      _ls_buffer.buffer = new char[_ls_buffer.capacity * sizeof(Type)];
      pos = _ls_buffer.buffer;
    }
    else pos = _ss_buffer;
    for(size_t i = 0; i < _size; ++i)
      new(pos + sizeof(Type) * i) Type(*(it++));
  }
  template<class InType>
  SSOVector(std::initializer_list<InType>&& list)
  {
    _size = list.size();
    auto it = list.begin();
    char* pos;
    if(_size > Size)
    {
      _ls_buffer.capacity = (_size * 3) >> 1;
      _ls_buffer.buffer = new char[_ls_buffer.capacity * sizeof(Type)];
      pos = _ls_buffer.buffer;
    }
    else pos = _ss_buffer;
    for(size_t i = 0; i < _size; ++i)
      new(pos + sizeof(Type) * i) Type(std::move(*(it++)));
  }
  
  SSOVector& operator=(const SSOVector& other)
  {
    _destroyContent();
    _size = other._size;
    if(_size <= Size)
    {
      for(size_t i = 0; i < _size; ++i)
        new(_ss_buffer + sizeof(Type) * i) Type(_osBuff(other)[i]);
    }
    else
    {
      _ls_buffer.capacity = other._ls_buffer.capacity;
      _ls_buffer.buffer = new char[sizeof(Type) * _ls_buffer.capacity];
      for(size_t i = 0; i < _size; ++i)
        new(_ls_buffer.buffer + sizeof(Type) * i) Type(_olBuff(other)[i]);
    }
    return *this;
  }
  SSOVector& operator=(SSOVector&& other)
  {
    _destroyContent();
    _size = other._size;
    if(_size > Size)
    {
      _ls_buffer.capacity = other._ls_buffer.capacity;
      _ls_buffer.buffer = other._ls_buffer.buffer;
      other._size = 0;
      //warning: same as in move constructor
    }
    else
    {
      for(size_t i = 0; i < _size; ++i)
        new(_ss_buffer + sizeof(Type) * i) Type(_osBuff(other)[i]);
    }
    return *this;
  }
  template<class InType>
  SSOVector& operator=(const std::initializer_list<InType>& list)
  {
    _destroyContent();
    _size = list.size();
    auto it = list.begin();
    char* pos;
    if(_size > Size)
    {
      _ls_buffer.capacity = (_size * 3) >> 1;
      _ls_buffer.buffer = new char[_ls_buffer.capacity * sizeof(Type)];
      pos = _ls_buffer.buffer;
    }
    else pos = _ss_buffer;
    for(size_t i = 0; i < _size; ++i)
      new(pos + sizeof(Type) * i) Type(*(it++));
    return *this;
  }
  template<class InType>
  SSOVector& operator=(std::initializer_list<InType>&& list)
  {
    _destroyContent();
    _size = list.size();
    auto it = list.begin();
    char* pos;
    if(_size > Size)
    {
      _ls_buffer.capacity = (_size * 3) >> 1;
      _ls_buffer.buffer = new char[_ls_buffer.capacity * sizeof(Type)];
      pos = _ls_buffer.buffer;
    }
    else pos = _ss_buffer;
    for(size_t i = 0; i < _size; ++i)
      new(pos + sizeof(Type) * i) Type(std::move(*(it++)));
    return *this;
  }
  
  ~SSOVector()
  {
    _destroyContent();
  }
  
  //element access
  
  Type& operator[](int idx)
  {
    return _activeBuffer()[idx];
  }
  const Type& operator[](int idx) const
  {
    return _activeBuffer()[idx];
  }
  
  Type& front()
  {
    return _activeBuffer()[0];
  }
  const Type& front() const
  {
    return _activeBuffer()[0];
  }
  
  Type& back()
  {
    return _activeBuffer()[_size - 1];
  }
  const Type& back() const
  {
    return _activeBuffer()[_size - 1];
  }
  
  Type* data()
  {
    return _activeBuffer();
  }
  const Type* data() const
  {
    return _activeBuffer();
  }
  
  //capacity
  
  bool empty() const
  {
    return _size > 0;
  }
  size_t size() const
  {
    return _size;
  }
  size_t capacity() const
  {
    if(_size > Size) return _ls_buffer.capacity;
    else return Size;
  }
  
  //modifiers
  
  void clear()
  {
    _destroyContent();
    _size = 0;
  }
  
  void push_back(const Type& val)
  {
    if(_size == Size)
      _makeRoomInitial();
    else if(_size == capacity())
      _makeRoomForMore();
    ++_size;
    new(_activeBuffer() + _size - 1) Type(val);
  }
  void push_back(Type&& val)
  {
    if(_size == Size)
      _makeRoomInitial();
    else if(_size == capacity())
      _makeRoomForMore();
    ++_size;
    new(_activeBuffer() + _size - 1) Type(val);
  }
  template<class... Args>
  void emplace_back(Args&&... args)
  {
    if(_size == Size)
      _makeRoomInitial();
    else if(_size == capacity())
      _makeRoomForMore();
    ++_size;
    new(_activeBuffer() + _size - 1) Type(std::forward<Args>(args)...);
  }
  void pop_back()
  {
    _activeBuffer()[_size - 1].~Type();
    --_size;
    if(_size == Size)
      _moveBack();
  }
  
  iterator insert(const_iterator pos, const Type& val)
  {
    size_t p = pos - _activeBuffer();
    _split(p);
    new(_activeBuffer() + p) Type(val);
    return _activeBuffer() + p;
  }
  iterator insert(const_iterator pos, Type&& val)
  {
    size_t p = pos - _activeBuffer();
    _split(p);
    new(_activeBuffer() + p) Type(val);
    return _activeBuffer() + p;
  }
  iterator insert(const_iterator pos, size_t num, Type& val)
  {
    size_t p = pos - _activeBuffer();
    _split(p, num);
    for(int i = 0; i < num; ++i)
      new(_activeBuffer() + p + i) Type(val);
    return _activeBuffer() + p;
  }
  template<class InputIt>
  iterator insert(const_iterator pos, InputIt b, InputIt e)
  {
    size_t p = pos - _activeBuffer();
    int num = e - b;
    _split(p, num);
    for(int i = 0; i < num; ++i)
      new(_activeBuffer() + p + i) Type(*(b++));
    return _activeBuffer() + p;
  }
  iterator insert(const_iterator pos, const std::initializer_list<Type>& list)
  {
    size_t p = pos - _activeBuffer();
    int num = list.size();
    _split(p, num);
    auto it = list.begin();
    for(int i = 0; i < num; ++i)
      new(_activeBuffer() + p + i) Type(*(it++));
    return _activeBuffer() + p;
  }
  iterator insert(const_iterator pos, std::initializer_list<Type>&& list)
  {
    size_t p = pos - _activeBuffer();
    int num = list.size();
    _split(p, num);
    auto it = list.begin();
    for(int i = 0; i < num; ++i)
      new(_activeBuffer() + p + i) Type(std::move(*(it++)));
    return _activeBuffer() + p;
  }
  template<class... Args>
  iterator emplace(const_iterator pos, Args&&... args)
  {
    size_t p = pos - _activeBuffer();
    _split(p);
    new(_activeBuffer() + p) Type(std::forward<Args>(args)...);
    return _activeBuffer() + p;
  }
  
  iterator erase(const_iterator pos)
  {
    size_t p = pos - _activeBuffer();
    pos->~Type();
    _join(p);
    return _activeBuffer() + p;
  }
  iterator erase(const_iterator b, const_iterator e)
  {
    size_t p = b - _activeBuffer();
    size_t n = e - b;
    while(b != e)
      (b++)->~Type();
    _join(p, n);
    return _activeBuffer() + p;
  }
  
  //iterators
  
  iterator begin()
  {
    return _activeBuffer();
  }
  iterator end()
  {
    return _activeBuffer() + _size;
  }
  const_iterator begin() const
  {
    return _activeBuffer();
  }
  const_iterator end() const
  {
    return _activeBuffer() + _size;
  }
  const_iterator cbegin() const
  {
    return _activeBuffer();
  }
  const_iterator cend() const
  {
    return _activeBuffer() + _size;
  }
  reverse_iterator rbegin()
  {
    return _activeBuffer() + _size - 1;
  }
  reverse_iterator rend()
  {
    return _activeBuffer() - 1;
  }
  const_reverse_iterator rbegin() const
  {
    return _activeBuffer() + _size - 1;
  }
  const_reverse_iterator rend() const
  {
    return _activeBuffer() - 1;
  }
  const_reverse_iterator crbegin() const
  {
    return _activeBuffer() + _size - 1;
  }
  const_reverse_iterator crend() const
  {
    return _activeBuffer() - 1;
  }
};

#endif
