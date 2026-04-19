/*!
 * @brief Template C-header file for DLL
 *
 * This is a template C-header file for DLL.
 * @author  koturn
 * @date    2025 02/02
 * @file    Finally.hpp
 * @version 0.1
 */
#ifndef FINALLY_HPP
#define FINALLY_HPP

#include <utility>

template<typename F>
class
#if defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)
[[nodiscard]]
#endif  // defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)
Finally final
{
private:
#if defined(__cplusplus) && __cplusplus >= 201703L \
  || defined(_MSVC_LANG) && _MSVC_LANG >= 201703L \
  || defined(__cpp_lib_is_invocable)
  static_assert(std::is_invocable_v<F>, "[Finally] Finally function must be callable with no arguments.");
#else
  struct is_invocable_impl
  {
    template<typename T, typename... Args>
    static auto
    check(T&& obj, Args&&... args) -> decltype(obj(std::forward<Args>(args)...), std::true_type{});

    template<typename...>
    static auto
    check(...) -> std::false_type;
  };  // struct is_invocable_impl

  template<typename T, typename... Args>
  class is_invocable
    : public decltype(is_invocable_impl::check(std::declval<T>(), std::declval<Args>()...))
  {};

  static_assert(is_invocable<F>::value, "[Finally] Finally function must be callable with no arguments.");
#endif

public:
  explicit Finally(F&& f)
#if defined(__cplusplus) && __cplusplus >= 201103L \
  || defined(_MSVC_LANG) && _MSVC_LANG >= 201103L \
  || defined(_MSC_VER) && (_MSC_VER > 1800 || (_MSC_VER == 1800 && _MSC_FULL_VER == 180021114))
    noexcept
#else
    throw()
#endif
    : f_{std::forward<F>(f)}
  {}

  ~Finally()
  {
    f_();
  }

  Finally(const Finally&) = delete;
  Finally& operator=(const Finally&) = delete;

#if defined(_MSC_VER) && _MSC_VER == 1800
  Finally(Finally&& other) throw()
    : f_{std::move(other.f_)}
  {}

  Finally&
  operator=(Finally&& rhs) throw()
  {
    f_ = std::move(rhs.f_);
    return *this;
  }
#else
  Finally(Finally&&) = default;
  Finally& operator=(Finally&&) = default;
#endif  // defined(_MSC_VER) && _MSC_VER == 1800

template<typename... Args>
static void*
operator new(std::size_t, Args&&...) = delete;

template<typename... Args>
static void
operator delete(void*, Args&&...) = delete;

private:
  const F f_;
};  // class Finally


namespace
{
template<typename F>
#if !defined(__has_cpp_attribute) || !__has_cpp_attribute(nodiscard)
#  if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ >= 4)
__attribute__((warn_unused_result))
#  elif defined(_MSC_VER) && _MSC_VER >= 1700 && defined(_Check_return_)
_Check_return_
#  endif
#endif
inline Finally<F>
makeFinally(F&& f)
#if defined(__cplusplus) && __cplusplus >= 201103 \
  || defined(_MSVC_LANG) && _MSVC_LANG >= 201103L \
  || defined(_MSC_VER) && (_MSC_VER > 1800 || (_MSC_VER == 1800 && _MSC_FULL_VER == 180021114))
  noexcept
#else
  throw()
#endif
{
  return Finally<F>{std::forward<F>(f)};
}
}  // namespace


#endif  // FINALLY_HPP