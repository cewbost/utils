#ifndef __RING_BUFFER_H_INCLUDED__
#define __RING_BUFFER_H_INCLUDED__

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
  
  A ring-buffer class. This class can be used as a queue without performing any
  unnecesary allocations.
*/


#include <type_traits>
#include <memory>
#include <new>
#include <initializer_list>
#include <iostream>

/**
  @brief A ring-buffer class. The interface is similar to the STL containers.
  @param T Type of object to store in the buffer.
*/
template<class T>
class RingBuffer
{
  /*
    note:
      the range of values on _front and _back is:
        _front: [0, _capacity - 1]
        _back: [1, _capacity]
  */

  struct alignas(T) __Memblock
  {
    char _[sizeof(T)];
    explicit operator T*()
    {return reinterpret_cast<T*>(_);}
    operator T&()
    {return *(this->operator T*());}
  };

  std::unique_ptr<__Memblock[]> _buf;
  size_t _capacity = 0;
  int _front = 0;
  int _back = 0;
  bool _locked = false;

  void _destroyElement(T* elem)
  {
    elem->~T();
  }
  
  void _destroyAllElements()
  {
    for(size_t s = 0; s < size(); ++s)
    {
      _destroyElement((T*)_buf[(_front + s) % _capacity]);
    }
  }
  
  void _copyOver(__Memblock* dest, int num)
  {
    for(int i = 0; i < num; ++i)
      new(&dest[i]) T(_buf[(_front + i) % _capacity]);
  }
  void _moveOver(__Memblock* dest, int num)
  {
    for(int i = 0; i < num; ++i)
      new(&dest[i]) T(std::move(_buf[(_front + i) % _capacity]));
  }
  
  void _copyOver(__Memblock* dest)
  {
    int num = size();
    for(int i = 0; i < num; ++i)
      new(&dest[i]) T(_buf[(_front + i) % _capacity]);
  }
  void _moveOver(__Memblock* dest)
  {
    int num = size();
    for(int i = 0; i < num; ++i)
      new(&dest[i]) T(std::move(_buf[(_front + i) % _capacity]));
  }
  
  void _allocateMore()
  {
    auto current_size = size();
    size_t new_capacity;
    if(_capacity == 0) new_capacity = 16;
    else new_capacity = _capacity * 2;
    
    std::unique_ptr<__Memblock[]> new_buf(new __Memblock[new_capacity]);
    _moveOver(new_buf.get(), current_size);
    _destroyAllElements();
    
    _buf = std::move(new_buf);
    _capacity = new_capacity;
    _front = 0;
    _back = current_size;
  }
  
  bool _atMaxCapacity() const
  {
    return _capacity == 0 || (int)((_back + 1) % _capacity) == _front;
  }
  
  void _makeRoom(int num = 1)
  {
    for(int i = 0; i < num; ++i)
    {
      --_back;
      _destroyElement((T*)_buf[_back]);
      if(_back == 0) _back = _capacity;
    }
  }
  
  void _moveForward(int idx)
  {
    if(_locked && _atMaxCapacity())
      _makeRoom();
    
    int final_idx = (idx + _front) % _capacity;
    int current_idx = _back % _capacity;
    
    while(current_idx != final_idx)
    {
      int last_idx = current_idx;
      --current_idx;
      if(current_idx < 0)
        current_idx = _capacity - 1;
      new(_buf[last_idx]._) T(std::move((T&)_buf[current_idx]));
      _destroyElement((T*)_buf[current_idx]);
    }
    
    ++_back;
    if(_back > (int)_capacity) _back = 1;
  }
  
  void _moveForward(int idx, int num)
  {
    if(_locked)
    {
      int diff = _capacity - size() - 1 - num;
      if(diff < 0)
        _makeRoom(-diff);
    }
  
    int final_idx = (idx + _front) % _capacity;
    int current_idx = _back % _capacity;
    
    while(current_idx != final_idx)
    {
      --current_idx;
      if(current_idx < 0)
        current_idx = _capacity - 1;
      new(_buf[(current_idx + num) % _capacity]._) T(std::move((T&)_buf[current_idx]));
      _destroyElement((T*)_buf[current_idx]);
    }
    
    _back += num;
    while(_back > _capacity) _back -= _capacity;
  }
  
  void _moveBack(int idx)
  {
    int real_idx = (idx + _front) % _capacity;
    int final_idx = _back % _capacity;
    _destroyElement((T*)_buf[real_idx]);
    
    int next_idx;
    while((next_idx = (real_idx + 1) % _capacity) != final_idx)
    {
      (T&)_buf[real_idx] = std::move((T&)_buf[next_idx]);
      real_idx = next_idx;
    }
    
    _destroyElement((T*)_buf[real_idx]);
    
    --_back;
    if(_back == 0) _back = _capacity;
  }
  
  void _moveBack(int idx, int num)
  {
    int real_idx = (idx + _front) % _capacity;
    int final_idx = _back % _capacity;
    
    for(int i = 0; i < num; ++i)
      _destroyElement((T*)_buf[(real_idx + i) % _capacity]);
    
    int targ_idx;
    while((targ_idx = (real_idx + num) % _capacity) != final_idx)
    {
      (T&)_buf[real_idx] = std::move((T&)_buf[targ_idx]);
      real_idx = (real_idx + 1) % _capacity;
    }
    
    for(int i = 0; i < num; ++i)
      _destroyElement((T*)_buf[(real_idx + i) % _capacity]);
    
    _back -= num;
    while(_back <= 0) _back += _capacity;
  }
  
  class __IteratorBase
  {
    friend class RingBuffer<T>;
    const RingBuffer<T>* _obj;
    int _idx;
    
  public:
    __IteratorBase(const RingBuffer<T>* o, int i): _obj(o), _idx(i){}
    __IteratorBase(): _obj(nullptr), _idx(0){}
    
    bool operator==(const __IteratorBase& other) const
    {return _obj == other._obj && _idx == other._idx;}
    bool operator!=(const __IteratorBase& other) const
    {return !(*this == other);}
  };
  
  class __ForwardIteratorBase: public __IteratorBase
  {
  public:
    using __IteratorBase::__IteratorBase;
    __ForwardIteratorBase() = default;
    
    bool operator>(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx > other._idx;}
    bool operator<(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx < other._idx;}
    bool operator>=(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx >= other._idx;}
    bool operator<=(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx <= other._idx;}
    
    size_t operator-(const __IteratorBase& other) const
    {return other._idx - this->_idx;}
    
    __ForwardIteratorBase operator+(int i) const
    {return __ForwardIteratorBase(this->_obj, this->_idx + i);}
    __ForwardIteratorBase operator-(int i) const
    {return __ForwardIteratorBase(this->_obj, this->_idx - i);}
    __ForwardIteratorBase& operator+=(int i)
    {this->_idx += i; return *this;}
    __ForwardIteratorBase& operator-=(int i)
    {this->_idx -= i; return *this;}
    __ForwardIteratorBase& operator++()
    {++this->_idx; return *this;}
    __ForwardIteratorBase& operator--()
    {--this->_idx; return *this;}
    __ForwardIteratorBase operator++(int)
    {__ForwardIteratorBase temp(*this); ++this->_idx; return temp;}
    __ForwardIteratorBase operator--(int)
    {__ForwardIteratorBase temp(*this); --this->_idx; return temp;}
  };
  
  class __ReverseIteratorBase: public __IteratorBase
  {
    public:
    using __IteratorBase::__IteratorBase;
    __ReverseIteratorBase() = default;
    
    bool operator<(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx > other._idx;}
    bool operator>(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx < other._idx;}
    bool operator<=(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx >= other._idx;}
    bool operator>=(const __IteratorBase& other) const
    {return this->_obj == other._obj && this->_idx <= other._idx;}
    
    size_t operator-(const __IteratorBase& other) const
    {return this->_idx - other._idx;}
    
    __ReverseIteratorBase operator-(int i) const
    {return __ReverseIteratorBase(this->_obj, this->_idx + i);}
    __ReverseIteratorBase operator+(int i) const
    {return __ReverseIteratorBase(this->_obj, this->_idx - i);}
    __ReverseIteratorBase& operator-=(int i)
    {this->_idx += i; return *this;}
    __ReverseIteratorBase& operator+=(int i)
    {this->_idx -= i; return *this;}
    __ReverseIteratorBase& operator--()
    {++this->_idx; return *this;}
    __ReverseIteratorBase& operator++()
    {--this->_idx; return *this;}
    __ReverseIteratorBase operator--(int)
    {__ReverseIteratorBase temp(*this); ++this->_idx; return temp;}
    __ReverseIteratorBase operator++(int)
    {__ReverseIteratorBase temp(*this); --this->_idx; return temp;}
  };
  
  template<class Base>
  class __Iterator: public Base
  {
  public:
    using Base::Base;
    __Iterator() = default;
    
    template<class CBase>
    __Iterator(const __Iterator<CBase>& other)
    {
      this->_obj = other._obj;
      this->_idx = other._idx;
    }
    
    T& operator*() const
    {return *(T*)this->_obj->_buf[
      (this->_obj->_front + this->_idx) % this->_obj->_capacity];}
    T* operator->() const
    {return &this->operator*();}
  };
  
  template<class Base>
  class __ConstIterator: public Base
  {
  public:
    using Base::Base;
    __ConstIterator() = default;
    
    template<class CBase>
    __ConstIterator(const __ConstIterator<CBase>& other)
    {
      this->_obj = other._obj;
      this->_idx = other._idx;
    }
    template<class CBase>
    __ConstIterator(const __Iterator<CBase>& other)
    {
      this->_obj = other._obj;
      this->_idx = other._idx;
    }
    
    const T& operator*() const
    {return *(T*)this->_obj->_buf[
      (this->_obj->_front + this->_idx) % this->_obj->_capacity];}
    const T* operator->() const
    {return &this->operator*();}
  };
  
public:

  typedef __Iterator<__ForwardIteratorBase> iterator;
  typedef __ConstIterator<__ForwardIteratorBase> const_iterator;
  typedef __Iterator<__ReverseIteratorBase> reverse_iterator;
  typedef __ConstIterator<__ReverseIteratorBase> const_reverse_iterator;
  
  RingBuffer() = default;
  RingBuffer(const RingBuffer<T>& other)
  {
    _front = 0;
    _back = other.size();
    _capacity = other._capacity;
    _buf.reset(new __Memblock[_capacity]);
    other._copyOver(_buf.get(), _back);
  }
  RingBuffer(RingBuffer<T>&& other) = default;
  RingBuffer& operator=(const RingBuffer<T>& other)
  {
    _destroyAllElements();
    _front = 0;
    _back = other.size();
    if(_capacity < other.size() + 1)
    {
      _capacity = other._capacity;
      _buf.reset(new __Memblock[_capacity]);      
    }
    other._copyOver(_buf.get(), _back);
    return *this;
  }
  RingBuffer& operator=(RingBuffer<T>&& other) = default;
  ~RingBuffer()
  {
    _destroyAllElements();
    _buf = nullptr;
  }
  
  /**
    @brief Returns a reference to the front element.
  */
  T& front() const
  {
    return *(T*)_buf[_front];
  }
  /**
    @brief Returns a reference to the last element.
  */
  T& back() const
  {
    return *(T*)_buf[_back - 1];
  }
  
  /**
    @brief Returns a forward iterator to the first element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  iterator begin() const
  {return iterator(this, 0);}
  /**
    @brief Returns a forward iterator past the last element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  iterator end() const
  {return iterator(this, size());}
  /**
    @brief Returns a const forward iterator to the first element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  const_iterator cbegin() const
  {return const_iterator(this, 0);}
  /**
    @brief Returns a const forward iterator to past the last element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  const_iterator cend() const
  {return const_iterator(this, size());}
  /**
    @brief Returns a reverse iterator to the last element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  reverse_iterator rbegin() const
  {return reverse_iterator(this, size() - 1);}
  /**
    @brief Returns a reverse iterator to before the first element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  reverse_iterator rend() const
  {return reverse_iterator(this, -1);}
  /**
    @brief Returns a const reverse iterator to the last element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  const_reverse_iterator crbegin() const
  {return const_reverse_iterator(this, size() - 1);}
  /**
    @brief Returns a const reverse iterator to before the first element.
    @note Insertion and removal of elements in the container may invalidate all
    references.
  */
  const_reverse_iterator crend() const
  {return const_reverse_iterator(this, -1);}
  
  /**
    @brief Returns the number of elements in the container.
  */
  size_t size() const
  {
    return _back >= _front? (_back - _front) : (_capacity - _front + _back);
  }
  /**
    @brief Returns true if the container is empty.
  */
  bool empty() const
  {
    return _back % _capacity == _front;
  }
  
  /**
    @brief Accesses an element in the container starting from (index 0) the beginner of
    the container.
  */
  T& operator[](int idx) const
  {
    return *(T*)_buf[(_front + idx) % _capacity];
  }
  
  /**
    @brief Shrinks the capacity of the container to just enough to store the current
    elements.
  */
  void shrink_to_fit()
  {
    int new_size = size();
    std::unique_ptr<__Memblock[]> new_buf(new __Memblock[new_size]);
    
    _moveOver(new_buf.get(), new_size);
    
    _destroyAllElements();
    
    _front = 0;
    _back = new_size;
    _capacity = new_size;
    _buf = std::move(new_buf);
  }
  
  /**
    @brief Reserves space for the specified number of elements.
    @param new_size The requested capacity.
  */
  void reserve(size_t new_size)
  {
    new_size += 1;
    if(_capacity >= new_size)
      return;
      
    int old_size = size();
    
    std::unique_ptr<__Memblock[]> new_buf(new __Memblock[new_size]);
    _moveOver(new_buf.get());
    _destroyAllElements();
    
    _front = 0;
    _back = old_size;
    _capacity = new_size;
    _buf = std::move(new_buf);
  }
  
  /**
    @brief Resizes the capacity of the buffer. Works like RingBuffer::reserve but may
    also shrink the buffer to the specified size.
  */
  void resize(size_t new_size)
  {
    if(_capacity == new_size + 1)
      return;
      
    std::unique_ptr<__Memblock[]> new_buf(new __Memblock[new_size + 1]);
    int s;

    if(_capacity <= new_size)
    {
      _moveOver(new_buf.get());
      s = size();
    }
    else
    {
      _moveOver(new_buf.get(), new_size);
      s = new_size;
    }
    
    _destroyAllElements();
    _front = 0;
    _back = s;
    _capacity = new_size + 1;
    _buf = std::move(new_buf);
  }
  
  /**
    @brief Removes all elements from the container.
  */
  void clear()
  {
    _destroyAllElements();
    _front = _back = 0;
  }
  
  /**
    @brief Swaps two container.
  */
  void swap(RingBuffer<T>& other)
  {
    std::swap(_buf, other._buf);
    std::swap(_front, other._front);
    std::swap(_back, other._back);
    std::swap(_capacity, other._capacity);
  }
  
  /**
    @brief Inserts an element at the back of the container.
  */
  void push_back(const T& obj)
  {
    if(_atMaxCapacity())
      _allocateMore();
    else _back = _back % _capacity;
    new(&_buf[_back]) T(obj);
    ++_back;
  }
  /**
    @brief Inserts an element at the back of the container.
  */
  void push_back(const T&& obj)
  {
    if(_atMaxCapacity())
      _allocateMore();
    else _back = _back % _capacity;
    new(&_buf[_back]) T(obj);
    ++_back;
  }
  /**
    @brief Inserts an element at the front of the container.
  */
  void push_front(const T& obj)
  {
    if(_atMaxCapacity())
      _allocateMore();
    --_front;
    if(_front < 0) _front = _capacity - 1;
    new(&_buf[_front]) T(obj);
  }
  /**
    @brief Inserts an element at the front of the container.
  */
  void push_front(const T&& obj)
  {
    if(_atMaxCapacity())
      _allocateMore();
    --_front;
    if(_front < 0) _front = _capacity - 1;
    new(&_buf[_front]) T(obj);
  }
  /**
    @brief Constructs an element into the last position of the buffer. The arguments are
    forwarded to the constructor.
  */
  template<typename... Args>
  void emplace_back(Args&&... args)
  {
    if(_atMaxCapacity())
      _allocateMore();
    else _back = _back % _capacity;
    new(&_buf[_back]) T(std::forward(args...));
    ++_back;
  }
  /**
    @brief Constructs an element into the first position of the buffer. The arguments are
    forwarded to the constructor.
  */
  template<typename... Args>
  void emplace_front(Args... args)
  {
    if(_atMaxCapacity())
      _allocateMore();
    --_front;
    if(_front < 0) _front = _capacity - 1;
    new(&_buf[_front]) T(std::forward(args...));
  }
  
  /**
    @brief Removes the last element in the container.
  */
  void pop_back()
  {
    --_back;
    T t = std::move((T&)_buf[_back]);
    _destroyElement((T*)_buf[_back]);
    if(_back == 0)
      _back = _capacity;
  }
  /**
    @brief Removes the first element in the container.
  */
  void pop_front()
  {
    _destroyElement((T*)_buf[_back]);
    _front = (_front + 1) % _capacity;
  }
  
  //iterator insert/emplace/erase
  
  /**
    @brief Inserts an element into the buffer.
    @param pos Iterator to the position to insert the element.
    @param val Object to insert.
    @return An iterator to the element just inserted.
  */
  iterator insert(const_iterator pos, const T& val)
  {
    if(_atMaxCapacity())
      _allocateMore();
    _moveForward(pos._idx);
    new(_buf[pos._idx]._) T(val);
    return iterator(pos._obj, pos._idx);
  }
  
  /**
    @brief Inserts an element into the buffer.
    @param pos Iterator to the position to insert the element.
    @param val Object to insert.
    @return An iterator to the element just inserted.
  */
  iterator insert(const_iterator pos, T&& val)
  {
    if(_atMaxCapacity())
      _allocateMore();
    _moveForward(pos._idx);
    new(_buf[pos._idx]._) T(val);
    return iterator(pos._obj, pos._idx);
  }
  
  /**
    @brief Inserts a range of elements into the buffer.
    @param pos Iterator to the position to insert the element.
    @param ilist a range of objects to insert.
    @return An iterator to the element just inserted.
  */
  iterator insert(const_iterator pos, std::initializer_list<T> ilist)
  {
    if(size() + ilist.size() + 1 >= _capacity)
      reserve(size() + ilist.size());
    
    _moveForward(pos._idx, ilist.size());
    for(auto& e: ilist)
    {
      new(_buf[pos._idx]._) T(e);
      ++pos;
    }
    return iterator(pos._obj, pos._idx);
  }
  
  /**
    @brief Inserts a range of elements into the buffer.
    @param pos Iterator to the position to insert the element.
    @param first Iterator to the first element in the range to insert.
    @param second Iterator to the end of the range to insert.
    @return An iterator to the element just inserted.
  */
  template<class It>
  iterator insert(const_iterator pos, It first, It second)
  {
    int num = second - first;
    if(size() + num + 1 >= _capacity)
      reserve(size() + num);
    
    _moveForward(pos._idx, num);
    while(second != first)
    {
      new(_buf[pos.idx]._) T(*first);
      ++pos;
      ++first;
    }
    return iterator(pos._obj, pos._idx);
  }
  
  /**
    @brief Constructs an element into the buffer. Constructor arguments are forwarded.
    @param pos Iterator to the position to insert the element.
    @param args Arguments to constructor.
    @return An iterator to the element just inserted.
  */
  template<class... Args>
  iterator emplace(const_iterator pos, Args... args)
  {
    if(_atMaxCapacity())
      _allocateMore();
    _moveForward(pos._idx);
    new(_buf[pos._idx]._) T(std::forward<Args>(args)...);
    return iterator(pos._obj, pos._idx);
  }
  
  /**
    @brief Erases an element in the container.
    @param pos Iterator to the element to remove.
    @return An iterator to the element following the element just removed.
  */
  iterator erase(const_iterator pos)
  {
    _moveBack(pos._idx);
    return iterator(pos._obj, pos._idx);
  }
  
  /**
    @brief Erases a range of elements in the container.
    @param first Iterator to the first element to remove.
    @param second Iterator past the last element in the range to remove.
    @return An iterator to the element following the element just removed.
  */
  iterator erase(const_iterator first, const_iterator last)
  {
    _moveBack(first._idx, last - first);
    return iterator(first._obj, first._idx);
  }
  
  //locking
  
  /**
    @brief Locks the capacity. When capacity is locked and elements are inserted past the
    capacity of the container, it removes elements from the other end of the container
    instead of allocating more memory.
  */
  void lockCapacity()
  {
    if(_capacity == 0)
      reserve(16);
    _locked = true;
  }
  /**
    @brief Locks the capacity. When capacity is locked and elements are inserted past the
    capacity of the container, it removes elements from the other end of the container
    instead of allocating more memory.
    @param cap Resizes the buffer to this size before locking.
  */
  void lockCapacity(int cap)
  {
    resize(cap);
    _locked = true;
  }
};

#endif