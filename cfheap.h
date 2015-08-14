#ifndef CFHEAP_H_INCLUDED
#define CFHEAP_H_INCLUDED

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
  
  A heap implemented as a contiguous buffer in memory.
  
  usage:
  
  pretty much the same as std::priority_queue
*/

#include <new>

/**
  @param T value type
*/
template<class T>
class CFHeap
{
  T* storage;
  int capacity;
  int _size;
  
  void allocate_more()
  {
    capacity <<= 1;
    capacity += 1;
    T* temp = static_cast<T*>(::operator new(capacity * sizeof(T)));
    for(int i = 0; i < _size; ++i)
    {
      new(temp + i) T(std::move(storage[i]));
    }
    delete[] storage;
    storage = temp;
  }
  void up_swap()
  {
    int idx = _size;
    while(idx != 0)
    {
      int next_idx = ((idx + 1) >> 1) - 1;
      if(storage[idx] > storage[next_idx])
      {
        std::swap(storage[idx], storage[next_idx]);
        idx = next_idx;
      }
      else break;
    }
  }
  void down_swap()
  {
    int idx = 0;
    do
    {
      int next_idx = ((idx + 1) << 1) - 1;
      if(next_idx >= _size) break;
      if(next_idx + 1 != _size && storage[next_idx] < storage[next_idx + 1])
        ++next_idx;
      if(storage[idx] < storage[next_idx])
      {
        std::swap(storage[idx], storage[next_idx]);
        idx = next_idx;
      }
      else break;
    }while(true);
  }
  
public:

  CFHeap()
  {
    storage = static_cast<T*>(::operator new(15 * sizeof(T)));
    capacity = 15;
    _size = 0;
  }
  CFHeap(const CFHeap<T>& other)
  {
    capacity = other.capacity;
    _size = other._size;
    storage = static_cast<T*>(::operator new(capacity * sizeof(T)));
    for(int i = 0; i < _size; ++i)
      new(storage + i) T(other.storage[i]);
  }
  CFHeap(CFHeap<T>&& other)
  {
    capacity = other.capacity;
    _size = other.size;
    storage = other.storage;
    other.storage = nullptr;
    other._size = 0;
  }
  ~CFHeap()
  {
    for(int i = 0; i < _size; ++i)
      storage[i].~T();
    ::operator delete(storage);
  }

  CFHeap<T>& operator=(const CFHeap<T>& other)
  {
    for(int i = 0; i < _size; ++i)
      storage[i].~T();
    ::operator delete(storage);
    capacity = other.capacity;
    _size = other._size;
    storage = static_cast<T*>(::operator new(capacity * sizeof(T)));
    for(int i = 0; i < _size; ++i)
      new(storage + i) T(other.storage[i]);
    return *this;
  }
  CFHeap<T>& operator=(CFHeap<T>&& other)
  {
    for(int i = 0; i < _size; ++i)
      storage[i].~T();
    ::operator delete(storage);
    capacity = other.capacity;
    _size = other._size;
    storage = other.storage;
    other.storage = nullptr;
    other._size = 0;
  }
  
  void push(const T& element)
  {
    if(_size == capacity)
      allocate_more();
    new(storage + _size) T(element);
    up_swap();
    ++_size;
  }
  void push(T&& element)
  {
    if(_size == capacity)
      allocate_more();
    new(storage + _size) T(element);
    up_swap();
    ++_size;
  }
  void pop()
  {
    storage->~T();
    new(storage) T(std::move(storage[_size - 1]));
    storage[_size - 1].~T();
    --_size;
    down_swap();
  }
  template<class... Args>
  void emplace(Args&&... args)
  {
    if(_size == capacity)
      allocate_more();
    new(storage) T(std::forward<Args>(args)...);
    up_swap();
    ++_size;
  }
  void swap(CFHeap<T>& other)
  {
    int cap = capacity;
    int si = _size;
    auto st = storage;
    capacity = other.capacity;
    _size = other._size;
    storage = other.storage;
    other.capacity = cap;
    other._size = si;
    other.storage = st;
  }

  T& top()
  {
    return *storage;
  }
  
  bool empty()
  {
    return _size == 0;
  }
  int size()
  {
    return _size;
  }
};

#endif