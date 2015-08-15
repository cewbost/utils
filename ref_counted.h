#ifndef REF_COUNTED_H_INCLUDED
#define REF_COUNTED_H_INCLUDED

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
  
  Reference counting without atomic operations. Reference count object is inherited,
  making it possible to get a reference counting pointer from a regular pointer, for
  instance from this
*/

#include <cstddef>
#include <new>

template<class T>
class counted_ptr;
template<class T>
class counted_weak_ptr;

/**
  @brief Inherit from this class to make a pointer reference countable.
*/
class RefCounted
{
private:

  unsigned short _refcount;
  unsigned short _weakrefcount;

public:

  RefCounted(): _refcount(0), _weakrefcount(0){}
  RefCounted(const RefCounted&): _refcount(0), _weafrefcount(0){}
  virtual ~RefCounted(){}
  
  void operator=(const RefCounted&)
  {
    _refcount = 0;
    _weakrefcount = 0;
  }

  unsigned short getRefCount(){return _refcount;}
  unsigned short getWeakRefCount(){return _weakrefcount;}
  
  template<class T>
  friend class counted_ptr;
  template<class T>
  friend class counted_weak_ptr;
};

/**
  @brief Smart pointer that does reference counting.
  @param T value type, must inherit from RefCounted
  @note Do not assign objects to this pointer unless they are allocated with new
*/
template<class T>
class counted_ptr
{
private:
  T* _ptr;

  void incRefCount(){++_ptr->_refcount;}
  void decRefCount()
  {
    --_ptr->_refcount;
    if(_ptr->_refcount == 0)
    {
      _ptr->~RefCounted();
      if(_ptr->_weakrefcount == 0)
        _ptr->operator delete(_ptr);
    }
  }
  
public:

  //counstructors
  counted_ptr()
  {
    _ptr = nullptr;
  }
  counted_ptr(const counted_ptr<T>& other)
  {
    _ptr = other.get();
    if(_ptr) incRefCount();
  }
  counted_ptr(counted_ptr<T>&& other)
  {
    _ptr = other._ptr;
    other._ptr = nullptr;
  }
  counted_ptr(T* other)
  {
    _ptr = other;
    if(_ptr) incRefCount();
  }
  counted_ptr(std::nullptr_t null)
  {
    _ptr = nullptr;
  }
  template<class Y>
  counted_ptr(Y* other)
  {
    _ptr = static_cast<T*>(other);
    if(_ptr) incRefCount();
  }
  template<class Y>
  counted_ptr(const counted_ptr<Y>& other)
  {
    _ptr = static_cast<T*>(other.get());
    if(_ptr) incRefCount();
  }

  //destructor
  ~counted_ptr()
  {
    if(_ptr) decRefCount();
  }

  //assignment
  template<class Y>
  counted_ptr<T>& operator=(Y* other)
  {
    if(_ptr) decRefCount();
    _ptr = (T*)other;
    if(_ptr) incRefCount();
    return *this;
  }
  template<class Y>
  counted_ptr<T>& operator=(const counted_ptr<Y>& other)
  {
    if(_ptr) decRefCount();
    _ptr = (T*)other.get();
    if(_ptr) incRefCount();
    return *this;
  }
  counted_ptr<T>& operator=(counted_ptr<T>&& other)
  {
    if(_ptr) decRefCount();
    _ptr = other._ptr;
    other._ptr = nullptr;
    return *this;
  }
  counted_ptr<T>& operator=(std::nullptr_t)
  {
    if(_ptr) decRefCount();
    _ptr = nullptr;
    return *this;
  }

  //dereference
  T& operator*()
  {
    return *_ptr;
  }
  T* operator->()
  {
    return _ptr;
  }

  //comparison
  bool operator==(const counted_ptr<T>& other) const
  {
    return _ptr == other._ptr;
  }
  bool operator!=(const counted_ptr<T>& other) const
  {
    return _ptr != other._ptr;
  }
  bool operator==(T* other) const
  {
    return _ptr == other;
  }
  bool operator!=(T* other) const
  {
    return _ptr != other;
  }
  bool operator==(std::nullptr_t) const
  {
    return _ptr == nullptr;
  }
  bool operator!=(std::nullptr_t) const
  {
    return _ptr != nullptr;
  }

  //typecasting
  template<class Y>
  operator Y() const
  {
    return (Y)_ptr;
  }
  operator bool() const
  {
    return _ptr != nullptr;
  }

  //other functions
  /**
    @brief Returns the underlying pointer
  */
  T* get() const
  {
    return _ptr;
  }
  void swap(counted_ptr<T>& other)
  {
    T* temp = other._ptr;
    other._ptr = _ptr;
    _ptr = temp;
  }
  /**
    @brief Returns a weak pointer to the pointed-to object
  */
  counted_weak_ptr<T> weak()
  {
    return _ptr;
  }
  
  /**
    @brief Gets the reference count.
    @return The reference count of the pointed-to object or 0 if the pointer is null.
  */
  unsigned short getRefCount()
  {if(_ptr) return _ptr->getRefCount(); else return 0;}
  /**
    @brief Gets the weak reference count.
    @return The weak reference count of the pointed-to object or 0 if the pointer is
    null.
  */
  unsigned short getWeakRefCount()
  {if(_ptr) return _ptr->getWeakRefCount(); else return 0;}
};

/**
  @brief Smart weak reference pointer that does reference counting.
  
  This pointer does not keep the object alive. It can point to already destructed
  objects.
  
  @param T value type, must inherit from RefCounted
  @note Do not assign objects to this pointer unless they are allocated with new
*/
template<class T>
class counted_weak_ptr
{
private:
  T* _ptr;
  
  void incRefCount(){++_ptr->_weakrefcount;}
  void decRefCount()
  {
    --_ptr->_weakrefcount;
    if(_ptr->_refcount + _ptr->_weakrefcount == 0)
      _ptr->operator delete(_ptr);
  }

public:

  //counstructors
  counted_weak_ptr()
  {
    _ptr = nullptr;
  }
  counted_weak_ptr(const counted_ptr<T>& other)
  {
    _ptr = other.get();
    if(_ptr) incRefCount();
  }
  counted_weak_ptr(const counted_weak_ptr<T>& other)
  {
    _ptr = other.get();
    if(_ptr) incRefCount();
  }
  counted_weak_ptr(counted_weak_ptr<T>&& other)
  {
    _ptr = other._ptr;
    other._ptr = nullptr;
  }
  counted_weak_ptr(T* other)
  {
    _ptr = other;
    if(_ptr) incRefCount();
  }
  counted_weak_ptr(std::nullptr_t null)
  {
    _ptr = nullptr;
  }
  template<class Y>
  counted_weak_ptr(Y* other)
  {
    _ptr = static_cast<T*>(other);
    if(_ptr) incRefCount();
  }
  template<class Y>
  counted_weak_ptr(const counted_weak_ptr<Y>& other)
  {
    _ptr = static_cast<T*>(other.get());
    if(_ptr) incRefCount();
  }
  template<class Y>
  counted_weak_ptr(const counted_ptr<Y>& other)
  {
    _ptr = static_cast<T*>(other.get());
    if(_ptr) incRefCount();
  }

  //destructor
  ~counted_weak_ptr()
  {
    if(_ptr) decRefCount();
  }

  //assignment
  counted_weak_ptr<T>& operator=(counted_ptr<T>& other)
  {
    if(_ptr) decRefCount();
    _ptr = other.get();
    if(_ptr) incRefCount();
    return *this;
  }
  counted_weak_ptr<T>& operator=(counted_weak_ptr<T>& other)
  {
    if(_ptr) decRefCount();
    _ptr = other;
    if(_ptr) incRefCount();
    return *this;
  }
  counted_weak_ptr<T>& operator=(T* other)
  {
    if(_ptr) decRefCount();
    _ptr = other;
    if(_ptr) incRefCount();
    return *this;
  }
  counted_weak_ptr<T>& operator=(std::nullptr_t)
  {
    if(_ptr) decRefCount();
    _ptr = nullptr;
    return *this;
  }

  //comparison
  bool operator==(const counted_weak_ptr<T>& other)
  {
    return _ptr == other._ptr;
  }
  bool operator!=(const counted_weak_ptr<T>& other)
  {
    return _ptr != other._ptr;
  }
  bool operator==(const counted_ptr<T>& other) const
  {
    return _ptr == other._ptr;
  }
  bool operator!=(const counted_ptr<T>& other) const
  {
    return _ptr != other._ptr;
  }
  bool operator==(T* other) const
  {
    return _ptr == other;
  }
  bool operator!=(T* other) const
  {
    return _ptr != other;
  }
  bool operator==(std::nullptr_t) const
  {
    return _ptr == nullptr;
  }
  bool operator!=(std::nullptr_t) const
  {
    return _ptr != nullptr;
  }

  //other functions
  void swap(counted_weak_ptr<T>& other)
  {
    T* temp = other._ptr;
    other._ptr = _ptr;
    _ptr = temp;
  }
  /**
    @brief Gets the underlying pointer. This should be used to access objects through
    weak pointers as it checks if the object has expired.
    @return Pointer to the underlying object or nullptr if it has expired.
  */
  T* get()
  {
    if(!expired()) return _ptr;
    else return nullptr;
  }

  /**
    @brief Gets the reference count.
    @return The reference count of the pointed-to object or 0 if the pointer is null.
  */
  unsigned short getRefCount()
  {if(_ptr) return _ptr->getRefCount(); else return 0;}
  /**
    @brief Gets the weak reference count.
    @return The weak reference count of the pointed-to object or 0 if the pointer is
    null.
  */
  unsigned short getWeakRefCount()
  {if(_ptr) return _ptr->getWeakRefCount(); else return 0;}
  /**
    @brief Checks if the pointed to object has expired.
    @return true if the object has expired or the pointer is null. Else returns false.
  */
  bool expired()
  {if(_ptr) return _ptr->getRefCount() == 0; else return true;}

  operator bool() const{return expired();}
};

#endif