#ifndef INTRUSIVE_LIST_H_INCLUDED
#define INTRUSIVE_LIST_H_INCLUDED

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
  
  Intrusive linked list with iterators.
*/

#include <cstdint>

/**
  @param p member pointer to object of type I in class T
  @return byte offset of member p in class T
*/
template<class T, class I>
constexpr uintptr_t member_offset(I T::* p)
{return (uintptr_t)&(((T*)nullptr)->*p) - (uintptr_t)nullptr;}

/**
  @param p member pointer to object of type I in class Y
  @return byte offset of member p in class T
  @note Y must be a base class of T
*/
template<class T, class Y, class I>
constexpr uintptr_t member_offset(I Y::* p)
{return (uintptr_t)&(((T*)nullptr)->*p) - (uintptr_t)nullptr;}

/**
  @brief node struct for intrusive list
  
  Declare a member of this type in a class to make it listable.
  Multiple nodes can be present in the same class to make it connectable to multiple
  lists.
  
  @note Copy constructor has been disabled since the destructor automatically unlinks
  the node.
*/
struct IntrusiveNode
{
  IntrusiveNode* next;
  IntrusiveNode* previous;

  void unlink()
  {
    next->previous = previous;
    previous->next = next;
  }
  void linkTo(IntrusiveNode& other)
  {
    unlink();
    previous = &other;
    next = other.next;
    next->previous = this;
    previous->next = this;
  }

  IntrusiveNode()
  {
    next = this;
    previous = this;
  }
  IntrusiveNode(const IntrusiveNode&) = delete;
  IntrusiveNode(IntrusiveNode&& other)
  {
    next = other.next;
    previous = other.previous;
    other.next = &other;
    other.previous = &other;
  }
  void operator=(const IntrusiveNode&) = delete;
  void operator=(IntrusiveNode&& other)
  {
    next = other.next;
    previous = other.previous;
    other.next = &other;
    other.previous = &other;
  }
  ~IntrusiveNode(){unlink();}
};

/**
  @brief intrusive list class
  
  This class contains the head node of a list.
  
  @param Y class containing the list node
  @param N member pointer to node in param
  @param T value type. Must be derived from Y. Default: Y
*/
template<class Y, IntrusiveNode Y::*N, class T = Y>
class IntrusiveList
{
  //static const auto offset = member_offset<T, Y>(N);
  //this is replaced with a static function since it breaks on some implementations
  static uintptr_t offset()
  {
    return member_offset<T, Y>(N);
  }
  
  struct iterator_base
  {
    IntrusiveNode* ptr;
    
    iterator_base(IntrusiveNode* start)
      : ptr(start){}
    iterator_base(const iterator_base& other)
      : ptr(other.ptr){}
    iterator_base(const T& obj)
      : ptr((IntrusiveNode*)(((char*)&obj) + offset())){}
    ~iterator_base(){}

    bool operator!=(const iterator_base& other) const
    {
      return ptr != other.ptr;
    }
    T& operator*() const
    {
      return *(T*)((char*)ptr - offset());
    }
    T* operator->() const
    {
      return (T*)((char*)ptr - offset());
    }
    
  protected:
    void __insert(const iterator_base& it) const
    {
      it.ptr->linkTo(*ptr);
    }
    void __insert(const iterator_base& it1, const iterator_base& it2) const
    {
      it1.ptr->previous->next = it2.ptr->next;
      it2.ptr->next->previous = it1.ptr->previous;
      it1.ptr->previous = ptr;
      it2.ptr->next = ptr->next;
      ptr->next = it1.ptr;
      ptr->next->previous = it2.ptr;
    }
    
    void __remove() const
    {
      ptr->next->unlink();
    }
    void __remove(const iterator_base& it)
    {
      ptr->next = it.ptr;
      it.ptr->previous = ptr;
    }
  };

public:
  
  /**
    @brief forward iterator, derives from iterator_base
    @note value type objects are implicitly convertible to iterators
  */
  struct iterator: public iterator_base
  {
  protected:
    using iterator_base::ptr;
    using iterator_base::__remove;
    
  public:
    iterator(IntrusiveNode* start)
      : iterator_base(start){}
    iterator(const iterator_base& other)
      : iterator_base(other){}
    iterator(const T& obj)
      : iterator_base(obj){}
    
    const iterator& operator++()
    {
      ptr = ptr->next;
      return *this;
    }
    iterator operator++(int)
    {
      auto p = ptr;
      ptr = ptr->next;
      return iterator(p);
    }
    const iterator& operator--()
    {
      ptr = ptr->previous;
      return *this;
    }
    iterator operator--(int)
    {
      auto p = ptr;
      ptr = ptr->previous;
      return iterator(p);
    }
    iterator next() const
    {
      return iterator(ptr->next);
    }
    iterator previous() const
    {
      return iterator(ptr->previous);
    }
    
    /**
      @brief inserts an object into the list after this iterator
      @param it iterator to object to insert
    */
    void insert_after(const iterator_base& it) const
    {
      __insert(it);
    }
    /**
      @brief inserts a range of objects into the list after this iterator
      @param it1 iterator to first object to insert
      @param it2 iterator to last object to insert
      @note it1 and it2 must be iterators to the same list.
      @note it1 must appear earlier in the list than it2.
      @note Lists are circular. Head node must not be present between it1 and it2.
    */
    void insert_after(const iterator_base& it1, const iterator_base& it2) const
    {
      __insert(it1, it2);
    }
    /**
      @brief inserts an object into the list before this iterator
      @param it iterator to object to insert
    */
    void insert_before(const iterator_base& it) const
    {
      previous().__insert(it);
    }
    /**
      @brief inserts a range of objects into the list before this iterator
      @param it1 iterator to first object to insert
      @param it2 iterator to last object to insert
      @note it1 and it2 must be iterators to the same list.
      @note it1 must appear earlier in the list than it2.
      @note Lists are circular. Head node must not be present between it1 and it2.
    */
    void insert_before(const iterator_base& it1, const iterator_base& it2) const
    {
      previous().__insert(it1, it2);
    }
    /**
      @brief removes object after this iterator from list
    */
    void remove_next() const
    {
      __remove();
    }
    /**
      @brief removes object before this iterator from list
    */
    void remove_previous() const
    {
      iterator_base(ptr->previous->previous).__remove();
    }
    /**
      @brief removes all objects between this iterator and it from list
    */
    void remove_between(const iterator_base& it)
    {
      __remove(it);
    }
  };
  
  /**
    @brief reverse iterator, derives from iterator_base
    @note value type objects are implicitly convertible to iterators
    @note All directions are reversed. reverse_iterator::insert_after is the same as
    iterator::insert_before etc.
  */
  struct reverse_iterator: public iterator_base
  {
  protected:
    using iterator_base::ptr;
    using iterator_base::__remove;
    
  public:
    reverse_iterator(IntrusiveNode* start)
      : iterator_base(start){}
    reverse_iterator(const iterator_base& other)
      : iterator_base(other){}
    reverse_iterator(const T& obj)
      : iterator_base(obj){}
  
    const reverse_iterator& operator++()
    {
      ptr = ptr->previous;
      return *this;
    }
    reverse_iterator operator++(int)
    {
      auto p = ptr;
      ptr = ptr->previous;
      return reverse_iterator(p);
    }
    const reverse_iterator& operator--()
    {
      ptr = ptr->next;
      return *this;
    }
    reverse_iterator operator--(int)
    {
      auto p = ptr;
      ptr = ptr->next;
      return reverse_iterator(p);
    }
    iterator next() const
    {
      return iterator(ptr->previous);
    }
    iterator previous() const
    {
      return iterator(ptr->next);
    }
    
    /**
      @brief inserts an object into the list after this iterator
      @param it iterator to object to insert
    */
    void insert_after(const iterator_base& it) const
    {
      previous().__insert(it);
    }
    /**
      @brief inserts a range of objects into the list after this iterator
      @param it1 iterator to first object to insert
      @param it2 iterator to last object to insert
      @note it1 and it2 must be iterators to the same list.
      @note it1 must appear earlier in the list than it2.
      @note Lists are circular. Head node must not be present between it1 and it2.
    */
    void insert_after(const iterator_base& it1, const iterator_base& it2) const
    {
      previous().__insert(it2, it1);
    }
    /**
      @brief inserts an object into the list before this iterator
      @param it iterator to object to insert
    */
    void insert_before(const iterator_base& it) const
    {
      __insert(it);
    }
    /**
      @brief inserts a range of objects into the list before this iterator
      @param it1 iterator to first object to insert
      @param it2 iterator to last object to insert
      @note it1 and it2 must be iterators to the same list.
      @note it1 must appear earlier in the list than it2.
      @note Lists are circular. Head node must not be present between it1 and it2.
    */
    void insert_before(const iterator_base& it1, const iterator_base& it2) const
    {
      __insert(it2, it1);
    }
    /**
      @brief removes object after this iterator from list
    */
    void remove_next() const
    {
      iterator_base(ptr->previous->previous).__remove();
    }
    /**
      @brief removes object before this iterator from list
    */
    void remove_previous() const
    {
      __remove();
    }
    /**
      @brief removes all objects between this iterator and it from list
    */
    void remove_between(const iterator_base& it)
    {
      it.__remove(*this);
    }
  };

  IntrusiveNode head; ///< head node

  iterator begin()
  {return iterator(head.next);}
  iterator end()
  {return iterator(&head);}
  /**
    @brief returns a forward iterator to the last element(one before end()).
  */
  iterator last()
  {return iterator(head.previous);}
  reverse_iterator rbegin()
  {return reverse_iterator(head.previous);}
  reverse_iterator rend()
  {return reverse_iterator(&head);}

  void push_back(T& obj)
  {
    IntrusiveNode* node = (IntrusiveNode*)(((char*)&obj) + offset());
    node->linkTo(*head.previous);
  }
  void push_front(T& obj)
  {
    IntrusiveNode* node = (IntrusiveNode*)(((char*)&obj) + offset());
    node->linkTo(head);
  }void pop_back()
  {
    head.previous->unlink();
  }
  void pop_front()
  {
    head.next->unlink();
  }
  T* front() const
  {
    return (T*)((char*)*head.next - offset());
  }
  T* back() const
  {
    return (T*)((char*)*head.previous - offset());
  }
  void clear()
  {
    head.next = &head;
    head.previous = &head;
  }
  bool empty()
  {
    return head.next == &head;
  }
  int size()
  {
    int ret = 0;
    for(auto it = this->begin(); it != this->end(); ++it)
      ++ret;
    return ret;
  }

  //template<class Y>
  //IntrusiveList(IntrusiveNode Y::* o)
  IntrusiveList()
  {
    //offset = member_offset<T, Y>(o);
    head.next = &head;
    head.previous = &head;
  }
  ~IntrusiveList(){}
};

#endif