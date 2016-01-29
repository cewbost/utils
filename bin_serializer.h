#ifndef BIN_SERIALIZER_H_INCLUDED
#define BIN_SERIALIZER_H_INCLUDED

/** 
  @file
  @author Erik Boström <cewbostrom@gmail.com>
  @date 7.1.2015
  
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
  
  A class for serializing data into binary buffers.
*/

#include <memory>

#include <cstring>
#include <cstdio>

/**
  @brief Class for serializing binary data.
*/
class BinSerializer
{
  uintptr_t _capacity = 0;
  uintptr_t _size;
  std::unique_ptr<char> _data;
  char* _reader = 0;
  
  void _reserve()
  {
    if(_capacity == 0)
      reserve(16);
    else reserve(_capacity * 2);
  }
  
  void _reserveToNewSize(uintptr_t new_size)
  {
    uintptr_t new_cap;
    if(_capacity == 0)
      new_cap = new_size;
    else
    {
      new_cap = _capacity << 1;
      while(new_cap < new_size)
        new_cap <<= 1;
    }
    reserve(new_cap);
  }

public:
  /**
    @brief Constructor
    @param size Initial capacity of the binary buffer. Capacity will grow by a factor
    of two whenever the data outgrows the capacity.
  */
  BinSerializer(int size = 1024):
    _capacity(size),
    _size(0),
    _data(new char[size]),
    _reader(_data.get())
    {}
  BinSerializer(const BinSerializer& other)
  {
    _capacity = other._capacity;
    _size = other._size;
    _data.reset(new char[_capacity]);
    memcpy(_data.get(), other._data.get(), _size);
    _reader = _data.get() + (other._reader - other._data.get());
  }
  BinSerializer(BinSerializer&&) = default;
  
  BinSerializer& operator=(const BinSerializer& other)
  {
    _capacity = other._capacity;
    _size = other._size;
    _data.reset(new char[_capacity]);
    memcpy(_data.get(), other._data.get(), _size);
    _reader = _data.get() + (other._reader - other._data.get());
    return *this;
  }
  BinSerializer& operator=(BinSerializer&&) = default;
  
  /**
    @brief Reserves more memory for the buffer.
    @param capacity Requested capacity in bytes. If lower than current capacity this
    function does nothing.
  */
  void reserve(uintptr_t capacity)
  {
    if(capacity < _capacity)
      return;
    
    std::unique_ptr<char> new_buffer(new char[capacity]);
    memcpy(new_buffer.get(), _data.get(), _size);
    _capacity = capacity;
    _reader = new_buffer.get() + (_reader - _data.get());
    _data = std::move(new_buffer);
  }
  
  /**
    @brief Gets a pointer to the buffer.
  */
  char* get()
  {
    return _data.get();
  }
  
  /**
    @brief Releases the ownership of the buffer and returns a pointer to it.
  */
  char* steal()
  {
    char* data = _data.release();
    _capacity = 0;
    _size = 0;
    return data;
  }
  
  /**
    @brief Moves the position indicator. Works like fseek in the C standard library.
    @param offset in bytes to move the position indicator. Positive to move forward,
    negative to move backward.
    @param whence The position in the buffer to which the offset is relative to. Valid
    values are SEEK_SET, SEEK_CUR, SEEK_END. If not a valid value this function does
    nothing.
    @return A reference to the object called.
  */
  BinSerializer& seek(intptr_t offset, int whence)
  {
    switch(whence)
    {
    case SEEK_CUR:
      break;
    case SEEK_SET:
      _reader = _data.get();;
      break;
    case SEEK_END:
      _reader = _data.get() + _size;
      break;
    default:
      return *this;
    }
    
    _reader += offset;
    if((uintptr_t)_reader < (uintptr_t)_data.get())
      _reader = _data.get();
    else if((uintptr_t)_reader > (uintptr_t)_data.get() + _size)
      _reader = _data.get() + _size;
    
    return *this;
  }
  
  /**
    @brief Returns the position indicator in bytes from the beginning of the buffer.
  */
  uintptr_t tell()
  {
    return uintptr_t(_reader - _data.get());
  }
  
  /**
    @brief Writes a C-style string to the buffer.
    @param str The string to write.
    @return A reference to the object called.
  */
  BinSerializer& write(const char* str)
  {
    int len = strlen(str);
    if((uintptr_t)((_reader - _data.get()) + len) > _size)
    {
      uintptr_t new_size = (_reader - _data.get()) + len;

      if(new_size > _capacity)
        _reserveToNewSize(new_size);
      
      _size = new_size;
    }
    
    memcpy(_reader, str, len);
    _reader += len;
    
    return *this;
  }
  
  /**
    @brief Writes binary data into the buffer.
    @param obj Object to serialize.
    @return A reference to the object called.
  */
  template<class T>
  BinSerializer& write(T&& obj)
  {
    const uintptr_t fin_len = (_reader - _data.get()) + sizeof(T);
    if(fin_len > _size)
    {
      uintptr_t new_size = fin_len;
      if(new_size > _capacity)
        _reserveToNewSize(new_size);
      _size = new_size;
    }
    memcpy(_reader, (void*)&obj, sizeof(T));
    _reader += sizeof(T);
    
    return *this;
  }
  
  /**
    @brief Writes a number of objects as binary data into the buffer.
    @param it An iterator to the first object.
    @param len Number of objects to write.
    @return A reference to the object called.
  */
  template<class T>
  BinSerializer& write(T it, int len)
  {
    using ValType = decltype(*it);
    const uintptr_t fin_len = (_reader - _data.get()) + len * sizeof(ValType);
    if(fin_len > _size)
    {
      uintptr_t new_size = fin_len;
      
      if(new_size > _capacity)
      {
        _reserveToNewSize(new_size);
      }
      
      _size = new_size;
    }
    
    for(int i = 0; i < len; ++i)
    {
      memcpy(_reader, (void*)&(*it), sizeof(ValType));
      _reader += sizeof(ValType);
      ++it;
    }
    
    return *this;
  }
  
  /**
    @brief Writes a range of objects as binary data into the buffer.
    @param it1 Iterator to the first element in the range.
    @param it2 Iterator past the end of the range.
    @return A reference to the object called.
  */
  template<class T>
  BinSerializer& write(const T it1, const T it2)
  {
    using ValType = decltype(*it1);
    if(_capacity < sizeof(ValType))
      reserve(sizeof(ValType) * 2);
    
    uintptr_t fin_len = (_reader - _data.get());
    
    while(it1 != it2)
    {
      fin_len += sizeof(ValType);
      if(fin_len > _capacity)
        _reserve();
      memcpy(_reader, (void*)&(*it1), sizeof(ValType));
      _reader += sizeof(ValType);
      ++it1;
    }
    
    if(fin_len > _size)
      _size = fin_len;
      
    return *this;
  }
};

#endif