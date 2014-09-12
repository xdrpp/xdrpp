// -*- C++ -*-

#ifndef _UNION_H_
#define _UNION_H_ 1

/** \file union.h Support for unions of types with non-trivial
 * constructors/destructors. */

#include <exception>
#include <iostream>
#include <memory>
#include <new>
#include <typeinfo>

#ifndef UNION_COPY_CONSTRUCT
#define UNION_COPY_CONSTRUCT 1
#endif // !UNION_COPY_CONSTRUCT

class union_entry_base {
protected:
  // Do not call destructor (might get wrong vtable), call destroy().
  virtual ~union_entry_base() {}
  void want_type(const std::type_info &wanted) const {
    const std::type_info &actual = typeid(*this);
    if (wanted != actual) {
      std::cerr << "union_entry: wanted = " << wanted.name()
		<< ", actual = " << actual.name() << std::endl;
      std::terminate();
    }
  }
#if UNION_COPY_CONSTRUCT
  virtual void copy_construct_to(union_entry_base *dest) const {
    new (static_cast<void *>(dest)) union_entry_base;
  }
#endif // UNION_COPY_CONSTRUCT
  virtual void move_construct_to(union_entry_base *dest) {
    new (static_cast<void *>(dest)) union_entry_base;
  }
public:
  union_entry_base() = default;
  union_entry_base(union_entry_base &&ueb) { ueb.move_construct_to(this); }
#if UNION_COPY_CONSTRUCT
  union_entry_base(const union_entry_base &ueb) { ueb.copy_construct_to(this); }
  union_entry_base &operator=(const union_entry_base &ueb) {
    destroy();
    try { ueb.copy_construct_to(this); }
    catch(...) { new (static_cast<void *>(this)) union_entry_base; }
    return *this;
  }
#endif // UNION_COPY_CONSTRUCT
  union_entry_base &operator=(union_entry_base &&ueb) {
    destroy();
    try { ueb.move_construct_to(this); }
    catch(...) { new (static_cast<void *>(this)) union_entry_base; }
    return *this;
  }
  void destroy() volatile { this->~union_entry_base(); }
};

template<typename T> class union_entry : public union_entry_base {
  T val_;

protected:
#if UNION_COPY_CONSTRUCT
  void copy_construct_to(union_entry_base *dest) const override {
    new (static_cast<void *>(dest)) union_entry{val_};
  }
#endif // UNION_COPY_CONSTRUCT
  void move_construct_to(union_entry_base *dest) override {
    new (static_cast<void *>(dest)) union_entry{std::move(val_)};
  }

public:
  template<typename ...A> union_entry(A&&...a)
    : val_(std::forward<A>(a)...) {}
  union_entry &operator=(const union_entry &) = delete;

  void verify() const { want_type(typeid(union_entry)); }

  T *get() { verify(); return &val_; }
  const T *get() const { verify(); return &val_; }
  operator T*() { return get(); }
  operator const T*() const { return get(); }
  T *operator->() { return get(); }
  const T *operator->() const { return get(); }
  T &operator*() { return *get(); }
  const T &operator*() const { return *get(); }

  bool constructed() const { return typeid(union_entry) == typeid(*this); }
  T &select() {
    if (!constructed()) {
      destroy(); 
      try { new (static_cast<void *>(this)) union_entry; }
      catch(...) { new (static_cast<void *>(this)) union_entry_base; throw; }
    }
    return val_;
  }
  template<typename ...A> void emplace(A&&...a) {
    destroy();
    try { new (static_cast<void *>(this)) union_entry{std::forward<A>(a)...}; }
    catch(...) { new (static_cast<void *>(this)) union_entry_base; throw; }
  }
};

template<> class union_entry<void> : public union_entry_base {
public:
  bool constructed() const { return typeid(union_entry) == typeid(*this); }
  void select() {
    if (!constructed()) {
      destroy(); 
      new (static_cast<void *>(this)) union_entry;
    }
  }
};

template<typename T> class union_ptr
  : public union_entry<std::shared_ptr<T>> {
  using super = union_entry<std::shared_ptr<T>>;
public:
  using super::super;

  void select() {
    if (!super::constructed()) {
      super::select();
      super::get()->reset(new T);
    }
  }
  template<typename ...A> void reset(A&&...a) {
    if (!super::constructed())
      super::select();
    get_ptr().reset(std::forward<A>(a)...);
  }

  std::shared_ptr<T> &get_ptr() { return *super::get(); }
  const std::shared_ptr<T> &get_ptr() const { return *super::get(); }

  T *get() { return get_ptr().get(); }
  const T *get() const { return get_ptr().get(); }
  operator T*() { return get(); }
  operator const T*() const { return get(); }
  T *operator->() { return get(); }
  const T *operator->() const { return get(); }
  T &operator*() { return *get(); }
  const T &operator*() const { return *get(); }
};

#endif /* !_UNION_H_ */
