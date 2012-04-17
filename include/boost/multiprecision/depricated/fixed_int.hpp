///////////////////////////////////////////////////////////////
//  Copyright 2011 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

#ifndef BOOST_MP_PACKED_INT_HPP
#define BOOST_MP_PACKED_INT_HPP

#include <iostream>
#include <iomanip>
#include <boost/cstdint.hpp>
#include <boost/multiprecision/mp_number.hpp>
#include <boost/multiprecision/detail/cpp_int_core.hpp>
#include <boost/array.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_floating_point.hpp>

namespace boost{
namespace multiprecision{

template <unsigned Bits, bool Signed>
struct fixed_int
{
   typedef mpl::list<signed_limb_type, signed_double_limb_type>      signed_types;
   typedef mpl::list<limb_type, double_limb_type>                    unsigned_types;
   typedef mpl::list<long double>                                    float_types;

   BOOST_STATIC_CONSTANT(unsigned, limb_bits = sizeof(limb_type) * CHAR_BIT);
   BOOST_STATIC_CONSTANT(unsigned, limb_count = Bits / limb_bits + (Bits % limb_bits ? 1 : 0));
   BOOST_STATIC_CONSTANT(limb_type, max_limb_value = ~static_cast<limb_type>(0u));
   BOOST_STATIC_CONSTANT(limb_type, upper_limb_mask = (Bits % limb_bits ? (1 << (Bits % limb_bits)) - 1 : max_limb_value));
   BOOST_STATIC_CONSTANT(limb_type, upper_limb_not_mask = ~upper_limb_mask);
   BOOST_STATIC_CONSTANT(limb_type, sign_bit_mask = limb_type(1u) << ((Bits % limb_bits ? Bits % limb_bits : limb_bits) - 1));
   typedef boost::array<limb_type, limb_count> data_type;

   fixed_int(){}
   fixed_int(const fixed_int& o)
   {
      m_value = o.m_value;
   }
#ifndef BOOST_NO_RVALUE_REFERENCES
   fixed_int(fixed_int&& o) : m_value(o.m_value) {}
#endif
   fixed_int& operator = (const fixed_int& o)
   {
      m_value = o.m_value;
      return *this;
   }
   fixed_int& operator = (limb_type i)
   {
      m_value[limb_count - 1] = i;
      for(int j = static_cast<int>(limb_count) - 2; j >= 0; --j)
         m_value[j] = 0;
      m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      return *this;
   }
   fixed_int& operator = (signed_limb_type i)
   {
      m_value[limb_count - 1] = i;
      // sign extend:
      for(int j = static_cast<int>(limb_count) - 2; j >= 0; --j)
         m_value[j] = i < 0 ? max_limb_value : 0;
      m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      return *this;
   }
   fixed_int& operator = (double_limb_type i)
   {
      BOOST_STATIC_ASSERT(sizeof(i) % sizeof(limb_type) == 0);
      double_limb_type mask = max_limb_value;
      unsigned shift = 0;
      for(int j = limb_count - 1; j >= static_cast<int>(limb_count) - static_cast<int>(sizeof(signed_double_limb_type) / sizeof(limb_type)); --j)
      {
         m_value[j] = static_cast<limb_type>((i & mask) >> shift);
         mask <<= limb_bits;
         shift += limb_bits;
      }
      for(int j = static_cast<int>(limb_count) - static_cast<int>(sizeof(signed_double_limb_type) / sizeof(limb_type)) - 1; j >= 0; --j)
         m_value[j] = 0;
      m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      return *this;
   }
   fixed_int& operator = (signed_double_limb_type i)
   {
      BOOST_STATIC_ASSERT(sizeof(i) % sizeof(limb_type) == 0);
      double_limb_type mask = max_limb_value;
      unsigned shift = 0;
      for(int j = limb_count - 1; j >= static_cast<int>(limb_count) - static_cast<int>(sizeof(signed_double_limb_type) / sizeof(limb_type)); --j)
      {
         m_value[j] = static_cast<limb_type>((i & mask) >> shift);
         mask <<= limb_bits;
         shift += limb_bits;
      }
      for(int j = static_cast<int>(limb_count) - static_cast<int>(sizeof(signed_double_limb_type) / sizeof(limb_type)) - 1; j >= 0; --j)
         m_value[j] = i < 0 ? max_limb_value : 0;
      m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      return *this;
   }
   fixed_int& operator = (long double a)
   {
      BOOST_STATIC_ASSERT(Bits >= (unsigned)std::numeric_limits<long double>::digits);
      using std::frexp;
      using std::ldexp;
      using std::floor;

      if (a == 0) {
         return *this = static_cast<limb_type>(0u);
      }

      if (a == 1) {
         return *this = static_cast<limb_type>(1u);
      }

      BOOST_ASSERT(!(boost::math::isinf)(a));
      BOOST_ASSERT(!(boost::math::isnan)(a));

      int e;
      long double f, term;
      *this = static_cast<limb_type>(0u);

      f = frexp(a, &e);

      static const limb_type shift = std::numeric_limits<limb_type>::digits;

      while(f)
      {
         // extract int sized bits from f:
         f = ldexp(f, shift);
         term = floor(f);
         e -= shift;
         eval_left_shift(*this, shift);
         if(term > 0)
            eval_add(*this, static_cast<limb_type>(term));
         else
            eval_subtract(*this, static_cast<limb_type>(-term));
         f -= term;
      }
      if(e > 0)
         eval_left_shift(*this, e);
      else if(e < 0)
         eval_right_shift(*this, -e);
      data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      return *this;
   }
   fixed_int& operator = (const char* s)
   {
      std::size_t n = s ? std::strlen(s) : 0;
      *this = static_cast<limb_type>(0u);
      unsigned radix = 10;
      bool isneg = false;
      if(n && (*s == '-'))
      {
         --n;
         ++s;
         isneg = true;
      }
      if(n && (*s == '0'))
      {
         if((n > 1) && ((s[1] == 'x') || (s[1] == 'X')))
         {
            radix = 16;
            s +=2;
            n -= 2;
         }
         else
         {
            radix = 8;
            n -= 1;
         }
      }
      if(n)
      {
         if(radix == 8 || radix == 16)
         {
            unsigned shift = radix == 8 ? 3 : 4;
            unsigned block_count = limb_bits / shift;
            unsigned block_shift = shift * block_count;
            limb_type val, block;
            while(*s)
            {
               block = 0;
               for(unsigned i = 0; (i < block_count); ++i)
               {
                  if(*s >= '0' && *s <= '9')
                     val = *s - '0';
                  else if(*s >= 'a' && *s <= 'f')
                     val = 10 + *s - 'a';
                  else if(*s >= 'A' && *s <= 'F')
                     val = 10 + *s - 'A';
                  else
                     val = max_limb_value;
                  if(val > radix)
                  {
                     m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
                     BOOST_THROW_EXCEPTION(std::runtime_error("Unexpected content found while parsing character string."));
                  }
                  block <<= shift;
                  block |= val;
                  if(!*++s)
                  {
                     // final shift is different:
                     block_shift = (i + 1) * shift;
                     break;
                  }
               }
               eval_left_shift(*this, block_shift);
               m_value[limb_count - 1] |= block;
            }
         }
         else
         {
            // Base 10, we extract blocks of size 10^9 at a time, that way
            // the number of multiplications is kept to a minimum:
            limb_type block_mult = max_block_10;
            while(*s)
            {
               limb_type block = 0;
               for(unsigned i = 0; i < digits_per_block_10; ++i)
               {
                  limb_type val;
                  if(*s >= '0' && *s <= '9')
                     val = *s - '0';
                  else
                     BOOST_THROW_EXCEPTION(std::runtime_error("Unexpected character encountered in input."));
                  block *= 10;
                  block += val;
                  if(!*++s)
                  {
                     block_mult = block_multiplier(i);
                     break;
                  }
               }
               eval_multiply(*this, block_mult);
               eval_add(*this, block);
            }
         }
      }
      m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      if(isneg)
         negate();
      return *this;
   }
   void swap(fixed_int& o)
   {
      std::swap(m_value, o.m_value);
   }
   std::string str(std::streamsize digits, std::ios_base::fmtflags f)const
   {
      int base = 10;
      if((f & std::ios_base::oct) == std::ios_base::oct)
         base = 8;
      else if((f & std::ios_base::hex) == std::ios_base::hex)
         base = 16;
      std::string result;

      if(base == 8 || base == 16)
      {
         limb_type shift = base == 8 ? 3 : 4;
         limb_type mask = static_cast<limb_type>((1u << shift) - 1);
         fixed_int t(*this);
         result.assign(Bits / shift + (Bits % shift ? 1 : 0), '0');
         int pos = result.size() - 1;
         for(unsigned i = 0; i < Bits / shift; ++i)
         {
            char c = '0' + (t.data()[limb_count-1] & mask);
            if(c > '9')
               c += 'A' - '9' - 1;
            result[pos--] = c;
            eval_right_shift(t, shift);
         }
         if(Bits % shift)
         {
            mask = static_cast<limb_type>((1u << (Bits % shift)) - 1);
            char c = '0' + (t.data()[limb_count-1] & mask);
            if(c > '9')
               c += 'A' - '9';
            result[pos] = c;
         }
         //
         // Get rid of leading zeros:
         //
         std::string::size_type n = result.find_first_not_of('0');
         if(!result.empty() && (n == std::string::npos))
            n = result.size() - 1;
         result.erase(0, n);
         if(f & std::ios_base::showbase)
         {
            const char* pp = base == 8 ? "0" : "0x";
            result.insert(0, pp);
         }
      }
      else
      {
         result.assign(Bits / 3 + 1, '0');
         int pos = result.size() - 1;
         fixed_int t(*this);
         fixed_int r;
         bool neg = false;
         if(Signed && (t.data()[0] & sign_bit_mask))
         {
            t.negate();
            neg = true;
         }
         if(limb_count == 1)
         {
            result = boost::lexical_cast<std::string>(t.data()[0]);
         }
         else
         {
            while(eval_get_sign(t) != 0)
            {
               fixed_int t2;
               divide_unsigned_helper(t2, t, max_block_10, r);
               t = t2;
               limb_type v = r.data()[limb_count - 1];
               for(unsigned i = 0; i < digits_per_block_10; ++i)
               {
                  char c = '0' + v % 10;
                  v /= 10;
                  result[pos] = c;
                  if(pos-- == 0)
                     break;
               }
            }
         }
         unsigned n = result.find_first_not_of('0');
         result.erase(0, n);
         if(result.empty())
            result = "0";
         if(neg)
            result.insert(0, 1, '-');
         else if(f & std::ios_base::showpos)
            result.insert(0, 1, '+');
      }
      return result;
   }
   void negate()
   {
      double_limb_type carry = 1;
      for(int i = fixed_int<Bits, Signed>::limb_count - 1; i >= 0; --i)
      {
         carry += static_cast<double_limb_type>(~m_value[i]);
         m_value[i] = static_cast<limb_type>(carry);
         carry >>= limb_bits;
      }
      m_value[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
   }
   int compare(const fixed_int& o)const
   {
      int result = 0;
      if(Signed && ((m_value[0] & sign_bit_mask) != (o.data()[0] & sign_bit_mask)))
      {
         return m_value[0] & sign_bit_mask ? -1 : 1;
      }
      for(typename data_type::size_type i = 0; i < limb_count; ++i)
      {
         if(m_value[i] != o.data()[i])
         {
            result = (m_value[i] < o.data()[i]) ? -1 : 1;
            break;
         }
      }
      return Signed && (m_value[0] & sign_bit_mask) ? -result : result ;
   }
   template <class Arithmatic>
   typename enable_if<is_arithmetic<Arithmatic>, int>::type compare(Arithmatic i)const
   {
      // braindead version:
      fixed_int<Bits, Signed> t;
      t = i;
      return compare(t);
   }
   data_type& data() { return m_value; }
   const data_type& data()const { return m_value; }
private:
   data_type m_value;
};

template <unsigned Bits, bool Signed>
inline void eval_add(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& o)
{
   eval_add(result, result, o);
}
template <unsigned Bits, bool Signed>
inline void eval_add(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   // Addition using modular arithmatic.
   // Nothing fancy, just let uintmax_t take the strain:
   double_limb_type carry = 0;
   for(int i = fixed_int<Bits, Signed>::limb_count - 1; i >= 0; --i)
   {
      carry += static_cast<double_limb_type>(a.data()[i]) + static_cast<double_limb_type>(b.data()[i]);
      result.data()[i] = static_cast<limb_type>(carry);
      carry >>= fixed_int<Bits, Signed>::limb_bits;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_add(fixed_int<Bits, Signed>& result, const limb_type& o)
{
   // Addition using modular arithmatic.
   // Nothing fancy, just let uintmax_t take the strain:
   double_limb_type carry = o;
   for(int i = fixed_int<Bits, Signed>::limb_count - 1; carry && i >= 0; --i)
   {
      carry += static_cast<double_limb_type>(result.data()[i]);
      result.data()[i] = static_cast<limb_type>(carry);
      carry >>= fixed_int<Bits, Signed>::limb_bits;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_add(fixed_int<Bits, Signed>& result, const signed_limb_type& o)
{
   if(o < 0)
      eval_subtract(result, static_cast<limb_type>(-o));
   else if(o > 0)
      eval_add(result, static_cast<limb_type>(o));
}
template <unsigned Bits, bool Signed>
inline void eval_subtract(fixed_int<Bits, Signed>& result, const limb_type& o)
{
   // Subtract using modular arithmatic.
   // This is the same code as for addition, with the twist that we negate o "on the fly":
   double_limb_type carry = static_cast<double_limb_type>(result.data()[fixed_int<Bits, Signed>::limb_count - 1]) 
      + 1uLL + static_cast<double_limb_type>(~o);
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] = static_cast<limb_type>(carry);
   carry >>= fixed_int<Bits, Signed>::limb_bits;
   for(int i = static_cast<int>(fixed_int<Bits, Signed>::limb_count) - 2; (carry != 1) && (i >= 0); --i)
   {
      carry += static_cast<double_limb_type>(result.data()[i]) + fixed_int<Bits, Signed>::max_limb_value;
      result.data()[i] = static_cast<limb_type>(carry);
      carry >>= fixed_int<Bits, Signed>::limb_bits;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_subtract(fixed_int<Bits, Signed>& result, const signed_limb_type& o)
{
   if(o)
   {
      if(o < 0)
         eval_add(result, static_cast<limb_type>(-o));
      else
         eval_subtract(result, static_cast<limb_type>(o));
   }
}
template <unsigned Bits, bool Signed>
inline void eval_increment(fixed_int<Bits, Signed>& result)
{
   static const limb_type one = 1;
   if(result.data().elems[fixed_int<Bits, Signed>::limb_count - 1] < fixed_int<Bits, Signed>::max_limb_value)
      ++result.data().elems[fixed_int<Bits, Signed>::limb_count - 1];
   else
      eval_add(result, one);
}
template <unsigned Bits, bool Signed>
inline void eval_decrement(fixed_int<Bits, Signed>& result)
{
   static const limb_type one = 1;
   if(result.data().elems[fixed_int<Bits, Signed>::limb_count - 1])
      --result.data().elems[fixed_int<Bits, Signed>::limb_count - 1];
   else
      eval_subtract(result, one);
}
template <unsigned Bits, bool Signed>
inline void eval_subtract(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& o)
{
   eval_subtract(result, result, o);
}
template <unsigned Bits, bool Signed>
inline void eval_subtract(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   // Subtract using modular arithmatic.
   // This is the same code as for addition, with the twist that we negate b "on the fly":
   double_limb_type carry = 1;
   for(int i = fixed_int<Bits, Signed>::limb_count - 1; i >= 0; --i)
   {
      carry += static_cast<double_limb_type>(a.data()[i]) + static_cast<double_limb_type>(~b.data()[i]);
      result.data()[i] = static_cast<limb_type>(carry);
      carry >>= fixed_int<Bits, Signed>::limb_bits;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_multiply(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   // Very simple long multiplication, only usable for small numbers of limb_type's
   // but that's the typical use case for this type anyway:
   if(&result == &a)
   {
      fixed_int<Bits, Signed> t(a);
      eval_multiply(result, t, b);
      return;
   }
   if(&result == &b)
   {
      fixed_int<Bits, Signed> t(b);
      eval_multiply(result, a, t);
      return;
   }
   double_limb_type carry = 0;
   for(unsigned i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
      result.data()[i] = 0;
   for(int i = fixed_int<Bits, Signed>::limb_count - 1; i >= 0; --i)
   {
      for(int j = fixed_int<Bits, Signed>::limb_count - 1; j >= static_cast<int>(fixed_int<Bits, Signed>::limb_count) - i - 1; --j)
      {
         carry += static_cast<double_limb_type>(a.data()[i]) * static_cast<double_limb_type>(b.data()[j]);
         carry += result.data()[i + j + 1 - fixed_int<Bits, Signed>::limb_count];
         result.data()[i + j + 1 - fixed_int<Bits, Signed>::limb_count] = static_cast<limb_type>(carry);
         carry >>= fixed_int<Bits, Signed>::limb_bits;
      }
      carry = 0;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_multiply(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a)
{
   // There is no in-place multiply:
   fixed_int<Bits, Signed> b(result);
   eval_multiply(result, b, a);
}
template <unsigned Bits, bool Signed>
inline void eval_multiply(fixed_int<Bits, Signed>& result, const limb_type& a)
{
   double_limb_type carry = 0;
   for(int i = fixed_int<Bits, Signed>::limb_count - 1; i >= 0; --i)
   {
      carry += static_cast<double_limb_type>(result.data()[i]) * static_cast<double_limb_type>(a);
      result.data()[i] = static_cast<limb_type>(carry);
      carry >>= fixed_int<Bits, Signed>::limb_bits;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_multiply(fixed_int<Bits, Signed>& result, const signed_limb_type& a)
{
   if(a > 0)
      eval_multiply(result, static_cast<limb_type>(a));
   else
   {
      eval_multiply(result, static_cast<limb_type>(-a));
      result.negate();
   }
}

/*
template <unsigned Bits, bool Signed>
limb_type bitcount(const fixed_int<Bits, Signed>& a)
{
   // returns the location of the MSB in a:
   limb_type i = 0;
   for(; (i < fixed_int<Bits, Signed>::limb_count) && (a.data()[i] == 0); ++i){}
   limb_type count = (fixed_int<Bits, Signed>::limb_count - i) * fixed_int<Bits, Signed>::limb_bits;
   if(!count)
      return count; // no bits are set, value is zero
   limb_type mask = static_cast<limb_type>(1u) << (fixed_int<Bits, Signed>::limb_bits - 1);
   while((a.data()[i] & mask) == 0)
   {
      --count;
      mask >>= 1;
   }
   return count;
}
template <unsigned Bits, bool Signed>
void divide_unsigned_helper(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& x, const fixed_int<Bits, Signed>& y, fixed_int<Bits, Signed>& r)
{
   if((&result == &x) || (&r == &x))
   {
      fixed_int<Bits, Signed> t(x);
      divide_unsigned_helper(result, t, y, r);
      return;
   }
   if((&result == &y) || (&r == &y))
   {
      fixed_int<Bits, Signed> t(y);
      divide_unsigned_helper(result, x, t, r);
      return;
   }


   using default_ops::eval_subtract;


   if (eval_is_zero(y))
   {
      volatile int i = 0;
      i /= i;
      return;
   }

   if (eval_is_zero(x))
   {
      r = y;
      result = x;
      return;
   }

   if(&result == &r)
   {
      fixed_int<Bits, Signed> rem;
      divide_unsigned_helper(result, x, y, rem);
      r = rem;
      return;
   }

   r = x;
   result = static_cast<limb_type>(0u);
   if(x.compare(y) < 0)
   {
      return; // We already have the answer: zero.
   }

   limb_type n = bitcount(x) - bitcount(y);

   if(n == 0)
   {
      // result is exactly 1:
      result = static_cast<limb_type>(1u);
      eval_subtract(r, y);
      return;
   }

   // Together mask_index and mask give us the bit we may be about to set in the result:
   limb_type mask_index = fixed_int<Bits, Signed>::limb_count - 1 - n / fixed_int<Bits, Signed>::limb_bits;
   limb_type mask = static_cast<limb_type>(1u) << n % fixed_int<Bits, Signed>::limb_bits;
   fixed_int<Bits, Signed> t(y);
   eval_left_shift(t, n);
   while(mask_index < fixed_int<Bits, Signed>::limb_count)
   {
      int comp = r.compare(t);
      if(comp >= 0)
      {
         result.data()[mask_index] |= mask;
         eval_subtract(r, t);
         if(comp == 0)
            break; // no remainder left - we have an exact result!
      }
      eval_right_shift(t, 1u);
      if(0 == (mask >>= 1))
      {
         ++mask_index;
         mask = static_cast<limb_type>(1u) << (fixed_int<Bits, Signed>::limb_bits - 1);
      }
   }
   BOOST_ASSERT(r.compare(y) < 0); // remainder must be less than the divisor or our code has failed
}
*/
template <unsigned Bits, bool Signed>
void divide_unsigned_helper(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& x, const fixed_int<Bits, Signed>& y, fixed_int<Bits, Signed>& r)
{
   if((&result == &x) || (&r == &x))
   {
      fixed_int<Bits, Signed> t(x);
      divide_unsigned_helper(result, t, y, r);
      return;
   }
   if((&result == &y) || (&r == &y))
   {
      fixed_int<Bits, Signed> t(y);
      divide_unsigned_helper(result, x, t, r);
      return;
   }

   /*
    Very simple, fairly braindead long division.
    Start by setting the remainder equal to x, and the
    result equal to 0.  Then in each loop we calculate our
    "best guess" for how many times y divides into r,
    add our guess to the result, and subtract guess*y
    from the remainder r.  One wrinkle is that the remainder
    may go negative, in which case we subtract the current guess
    from the result rather than adding.  The value of the guess
    is determined by dividing the most-significant-limb of the
    current remainder by the most-significant-limb of y.

    Note that there are more efficient algorithms than this
    available, in particular see Knuth Vol 2.  However for small
    numbers of limbs this generally outperforms the alternatives
    and avoids the normalisation step which would require extra storage.
    */


   using default_ops::eval_subtract;

   if(&result == &r)
   {
      fixed_int<Bits, Signed> rem;
      divide_unsigned_helper(result, x, y, rem);
      r = rem;
      return;
   }

   //
   // Find the most significant words of numerator and denominator.
   //
   limb_type y_order = 0;
   while((y.data()[y_order] == 0) && (y_order < fixed_int<Bits, Signed>::limb_count))
      ++y_order;

   if(y_order >= fixed_int<Bits, Signed>::limb_count - 1)
   {
      //
      // Only a single non-zero limb in the denominator, in this case
      // we can use a specialized divide-by-single-limb routine which is
      // much faster.  This also handles division by zero:
      //
      divide_unsigned_helper(result, x, y.data()[y_order], r);
      return;
   }

   limb_type r_order = 0;
   while((x.data()[r_order] == 0) && (r_order < fixed_int<Bits, Signed>::limb_count))
      ++r_order;
   if(r_order == fixed_int<Bits, Signed>::limb_count)
   {
      // x is zero, so is the result:
      r = y;
      result = x;
      return;
   }

   r = x;
   result = static_cast<limb_type>(0u);
   //
   // Check if the remainder is already less than the divisor, if so
   // we already have the result.  Note we try and avoid a full compare
   // if we can:
   //
   if(r_order >= y_order)
   {
      if((r_order > y_order) || (r.compare(y) < 0))
      {
         return;
      }
   }

   fixed_int<Bits, Signed> t;
   bool r_neg = false;

   //
   // See if we can short-circuit long division, and use basic arithmetic instead:
   //
   if(r_order == fixed_int<Bits, Signed>::limb_count - 1)
   {
      result = r.data()[fixed_int<Bits, Signed>::limb_count - 1] / y.data()[fixed_int<Bits, Signed>::limb_count - 1];
      r = x.data()[fixed_int<Bits, Signed>::limb_count - 1] % y.data()[fixed_int<Bits, Signed>::limb_count - 1];
      return;
   }
   else if(r_order == static_cast<unsigned>(fixed_int<Bits, Signed>::limb_count) - 2)
   {
      double_limb_type a, b;
      a = (static_cast<double_limb_type>(r.data()[r_order]) << fixed_int<Bits, Signed>::limb_bits) | r.data()[r_order + 1];
      b = y_order < fixed_int<Bits, Signed>::limb_count - 1 ? 
         (static_cast<double_limb_type>(y.data()[y_order]) << fixed_int<Bits, Signed>::limb_bits) | y.data()[y_order + 1] 
         : y.data()[y_order];
      result = a / b;
      r = a % b;
      return;
   }

   do
   {
      //
      // Update r_order, this can't run past the end as r must be non-zero at this point:
      //
      while(r.data()[r_order] == 0)
         ++r_order;
      //
      // Calculate our best guess for how many times y divides into r:
      //
      limb_type guess;
      if((r.data()[r_order] <= y.data()[y_order]) && (r_order < fixed_int<Bits, Signed>::limb_count - 1))
      {
         double_limb_type a, b, v;
         a = (static_cast<double_limb_type>(r.data()[r_order]) << fixed_int<Bits, Signed>::limb_bits) | r.data()[r_order + 1];
         b = y.data()[y_order];
         v = a / b;
         if(v > fixed_int<Bits, Signed>::max_limb_value)
            guess = 1;
         else
         {
            guess = static_cast<limb_type>(v);
            ++r_order;
         }
      }
      else if(r_order == fixed_int<Bits, Signed>::limb_count - 1)
      {
         guess = r.data()[r_order] / y.data()[y_order];
      }
      else
      {
         double_limb_type a, b, v;
         a = (static_cast<double_limb_type>(r.data()[r_order]) << fixed_int<Bits, Signed>::limb_bits) | r.data()[r_order + 1];
         b = (y_order < fixed_int<Bits, Signed>::limb_count - 1) ? (static_cast<double_limb_type>(y.data()[y_order]) << fixed_int<Bits, Signed>::limb_bits) | y.data()[y_order + 1] : (static_cast<double_limb_type>(y.data()[y_order])  << fixed_int<Bits, Signed>::limb_bits);
         v = a / b;
         guess = static_cast<limb_type>(v);
      }
      BOOST_ASSERT(guess); // If the guess ever gets to zero we go on forever....
      //
      // Update result:
      //
      limb_type shift = y_order - r_order;
      //t = limb_type(0);
      //t.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] = guess;
      if(r_neg)
      {
         if(result.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] > guess)
            result.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] -= guess;
         else
         {
            t = limb_type(0);
            t.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] = guess;
            eval_subtract(result, t);
         }
      }
      else if(fixed_int<Bits, Signed>::max_limb_value - result.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] > guess)
         result.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] += guess;
      else
      {
         t = limb_type(0);
         t.data()[fixed_int<Bits, Signed>::limb_count - 1 - shift] = guess;
         eval_add(result, t);
      }
      //
      // Calculate guess * y, we use a fused mutiply-shift O(N) for this
      // rather than a full O(N^2) multiply:
      //
      double_limb_type carry = 0;
      for(unsigned i = fixed_int<Bits, Signed>::limb_count - 1; i > fixed_int<Bits, Signed>::limb_count - shift - 1; --i)
         t.data()[i] = 0;
      for(int i = fixed_int<Bits, Signed>::limb_count - 1; i >= static_cast<int>(shift); --i)
      {
         carry += static_cast<double_limb_type>(y.data()[i]) * static_cast<double_limb_type>(guess);
         t.data()[i - shift] = static_cast<limb_type>(carry);
         carry >>= fixed_int<Bits, Signed>::limb_bits;
      }
      t.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
      //
      // Update r:
      //
      eval_subtract(r, t);
      if(r.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask)
      {
         r.negate();
         r_neg = !r_neg;
      }
   }
   // Termination condition is really just a check that r > y, but with two common
   // short-circuit cases handled first:
   while((r_order < y_order) || (r.data()[r_order] > y.data()[y_order]) || (r.compare(y) > 0));

   //
   // We now just have to normalise the result:
   //
   if(r_neg)
   {
      // We have one too many in the result:
      eval_decrement(result);
      r.negate();
      eval_add(r, y);
   }

   BOOST_ASSERT(r.compare(y) < 0); // remainder must be less than the divisor or our code has failed
}

template <unsigned Bits, bool Signed>
void divide_unsigned_helper(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& x, limb_type y, fixed_int<Bits, Signed>& r)
{
   if((&result == &x) || (&r == &x))
   {
      fixed_int<Bits, Signed> t(x);
      divide_unsigned_helper(result, t, y, r);
      return;
   }

   if(&result == &r)
   {
      fixed_int<Bits, Signed> rem;
      divide_unsigned_helper(result, x, y, rem);
      r = rem;
      return;
   }

   // As above, but simplified for integer divisor:

   using default_ops::eval_subtract;

   if(y == 0)
   {
      BOOST_THROW_EXCEPTION(std::runtime_error("Integer Division by zero."));
   }
   //
   // Find the most significant word of numerator.
   //
   limb_type r_order = 0;
   while((x.data()[r_order] == 0) && (r_order < fixed_int<Bits, Signed>::limb_count))
      ++r_order;

   //
   // Set remainder and result to their initial values:
   //
   r = x;
   result = static_cast<limb_type>(0u);

   if(r_order == fixed_int<Bits, Signed>::limb_count)
   {
      // All the limbs in x are zero, so is the result:
      return;
   }
   //
   // check for x < y, try to do this without actually having to 
   // do a full comparison:
   //
   if((r_order == fixed_int<Bits, Signed>::limb_count - 1) && (r.data()[r_order] < y))
   {
      return;
   }

   //
   // See if we can short-circuit long division, and use basic arithmetic instead:
   //
   if(r_order == fixed_int<Bits, Signed>::limb_count - 1)
   {
      result = r.data()[fixed_int<Bits, Signed>::limb_count - 1] / y;
      r = x.data()[fixed_int<Bits, Signed>::limb_count - 1] % y;
      return;
   }
   else if(r_order == static_cast<limb_type>(fixed_int<Bits, Signed>::limb_count) - 2)
   {
      double_limb_type a;
      a = (static_cast<double_limb_type>(r.data()[r_order]) << fixed_int<Bits, Signed>::limb_bits) | r.data()[r_order + 1];
      result = a / y;
      r = a % y;
      return;
   }

   do
   {
      //
      // Update r_order, this can't run past the end as r must be non-zero at this point:
      //
      while(r.data()[r_order] == 0)
         ++r_order;
      //
      // Calculate our best guess for how many times y divides into r:
      //
      if((r.data()[r_order] < y) && (r_order < fixed_int<Bits, Signed>::limb_count - 1))
      {
         double_limb_type a, b;
         a = (static_cast<double_limb_type>(r.data()[r_order]) << fixed_int<Bits, Signed>::limb_bits) | r.data()[r_order + 1];
         b = a % y;
         r.data()[r_order] = 0;
         ++r_order;
         r.data()[r_order] = static_cast<limb_type>(b);
         result.data()[r_order] = static_cast<limb_type>(a / y);
      }
      else
      {
         result.data()[r_order] = r.data()[r_order] / y;
         r.data()[r_order] %= y;
      }
   }
   // Termination condition is really just a check that r > y, but with two common
   // short-circuit cases handled first:
   while((r_order < fixed_int<Bits, Signed>::limb_count - 1) || (r.data()[r_order] > y));

   BOOST_ASSERT(r.compare(y) < 0); // remainder must be less than the divisor or our code has failed
}

template <unsigned Bits, bool Signed>
inline void eval_divide(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   fixed_int<Bits, Signed> r;
   if(Signed && (a.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      if(Signed && (b.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
      {
         fixed_int<Bits, Signed> t1(a), t2(b);
         t1.negate();
         t2.negate();
         divide_unsigned_helper(result, t1, t2, r);
      }
      else
      {
         fixed_int<Bits, Signed> t(a);
         t.negate();
         divide_unsigned_helper(result, t, b, r);
         result.negate();
      }
   }
   else if(Signed && (b.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      fixed_int<Bits, Signed> t(b);
      t.negate();
      divide_unsigned_helper(result, a, t, r);
      result.negate();
   }
   else
   {
      divide_unsigned_helper(result, a, b, r);
   }
}
template <unsigned Bits, bool Signed>
inline void eval_divide(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, limb_type& b)
{
   fixed_int<Bits, Signed> r;
   if(Signed && (a.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      fixed_int<Bits, Signed> t(a);
      t.negate();
      divide_unsigned_helper(result, t, b, r);
      result.negate();
   }
   else
   {
      divide_unsigned_helper(result, a, b, r);
   }
}
template <unsigned Bits, bool Signed>
inline void eval_divide(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, signed_limb_type& b)
{
   fixed_int<Bits, Signed> r;
   if(Signed && (a.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      if(b < 0)
      {
         fixed_int<Bits, Signed> t(a);
         t.negate();
         divide_unsigned_helper(result, t, static_cast<limb_type>(-b), r);
      }
      else
      {
         fixed_int<Bits, Signed> t(a);
         t.negate();
         divide_unsigned_helper(result, t, b, r);
         result.negate();
      }
   }
   else if(b < 0)
   {
      divide_unsigned_helper(result, a, static_cast<limb_type>(-b), r);
      result.negate();
   }
   else
   {
      divide_unsigned_helper(result, a, static_cast<limb_type>(b), r);
   }
}
template <unsigned Bits, bool Signed>
inline void eval_divide(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& b)
{
   // There is no in place divide:
   fixed_int<Bits, Signed> a(result);
   eval_divide(result, a, b);
}
template <unsigned Bits, bool Signed>
inline void eval_divide(fixed_int<Bits, Signed>& result, limb_type b)
{
   // There is no in place divide:
   fixed_int<Bits, Signed> a(result);
   eval_divide(result, a, b);
}
template <unsigned Bits, bool Signed>
inline void eval_divide(fixed_int<Bits, Signed>& result, signed_limb_type b)
{
   // There is no in place divide:
   fixed_int<Bits, Signed> a(result);
   eval_divide(result, a, b);
}
template <unsigned Bits, bool Signed>
inline void eval_modulus(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   fixed_int<Bits, Signed> r;
   if(Signed && (a.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      if(Signed && (b.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
      {
         fixed_int<Bits, Signed> t1(a), t2(b);
         t1.negate();
         t2.negate();
         divide_unsigned_helper(r, t1, t2, result);
         result.negate();
      }
      else
      {
         fixed_int<Bits, Signed> t(a);
         t.negate();
         divide_unsigned_helper(r, t, b, result);
         result.negate();
      }
   }
   else if(Signed && (b.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      fixed_int<Bits, Signed> t(b);
      t.negate();
      divide_unsigned_helper(r, a, t, result);
   }
   else
   {
      divide_unsigned_helper(r, a, b, result);
   }
}
template <unsigned Bits, bool Signed>
inline void eval_modulus(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, limb_type b)
{
   fixed_int<Bits, Signed> r;
   if(Signed && (a.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      fixed_int<Bits, Signed> t(a);
      t.negate();
      divide_unsigned_helper(r, t, b, result);
      result.negate();
   }
   else
   {
      divide_unsigned_helper(r, a, b, result);
   }
}
template <unsigned Bits, bool Signed>
inline void eval_modulus(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, signed_limb_type b)
{
   fixed_int<Bits, Signed> r;
   if(Signed && (a.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      if(b < 0)
      {
         fixed_int<Bits, Signed> t1(a);
         t1.negate();
         divide_unsigned_helper(r, t1, static_cast<limb_type>(-b), result);
         result.negate();
      }
      else
      {
         fixed_int<Bits, Signed> t(a);
         t.negate();
         divide_unsigned_helper(r, t, b, result);
         result.negate();
      }
   }
   else if(b < 0)
   {
      divide_unsigned_helper(r, a, static_cast<limb_type>(-b), result);
   }
   else
   {
      divide_unsigned_helper(r, a, static_cast<limb_type>(b), result);
   }
}
template <unsigned Bits, bool Signed>
inline void eval_modulus(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& b)
{
   // There is no in place divide:
   fixed_int<Bits, Signed> a(result);
   eval_modulus(result, a, b);
}
template <unsigned Bits, bool Signed>
inline void eval_modulus(fixed_int<Bits, Signed>& result, limb_type b)
{
   // There is no in place divide:
   fixed_int<Bits, Signed> a(result);
   eval_modulus(result, a, b);
}
template <unsigned Bits, bool Signed>
inline void eval_modulus(fixed_int<Bits, Signed>& result, signed_limb_type b)
{
   // There is no in place divide:
   fixed_int<Bits, Signed> a(result);
   eval_modulus(result, a, b);
}

template <unsigned Bits, bool Signed>
inline void eval_bitwise_and(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& o)
{
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
      result.data()[i] &= o.data()[i];
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_and(fixed_int<Bits, Signed>& result, limb_type o)
{
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] &= o;
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count - 1; ++i)
      result.data()[i] = 0;
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_and(fixed_int<Bits, Signed>& result, signed_limb_type o)
{
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] &= o;
   limb_type mask = o < 0 ? fixed_int<Bits, Signed>::max_limb_value : 0;
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count - 1; ++i)
      result.data()[i] &= mask;
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_or(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& o)
{
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
      result.data()[i] |= o.data()[i];
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_or(fixed_int<Bits, Signed>& result, limb_type o)
{
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] |= o;
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_or(fixed_int<Bits, Signed>& result, signed_limb_type o)
{
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] |= o;
   limb_type mask = o < 0 ? fixed_int<Bits, Signed>::max_limb_value : 0;
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count - 1; ++i)
      result.data()[i] |= mask;
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_xor(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& o)
{
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
      result.data()[i] ^= o.data()[i];
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_xor(fixed_int<Bits, Signed>& result, limb_type o)
{
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] ^= o;
}
template <unsigned Bits, bool Signed>
inline void eval_bitwise_xor(fixed_int<Bits, Signed>& result, signed_limb_type o)
{
   result.data()[fixed_int<Bits, Signed>::limb_count - 1] ^= o;
   limb_type mask = o < 0 ? fixed_int<Bits, Signed>::max_limb_value : 0;
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count - 1; ++i)
      result.data()[i] ^= mask;
}
template <unsigned Bits, bool Signed>
inline void eval_complement(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& o)
{
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
      result.data()[i] = ~o.data()[i];
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_left_shift(fixed_int<Bits, Signed>& result, double_limb_type s)
{
   if(!s)
      return;
   if(s >= Bits)
   {
      result = static_cast<limb_type>(0);
      return;
   }
   limb_type offset = static_cast<limb_type>(s / fixed_int<Bits, Signed>::limb_bits);
   limb_type shift  = static_cast<limb_type>(s % fixed_int<Bits, Signed>::limb_bits);
   unsigned i = 0;
   if(shift)
   {
      // This code only works when shift is non-zero, otherwise we invoke undefined behaviour!
      for(; i + offset + 1 < fixed_int<Bits, Signed>::limb_count; ++i)
      {
         result.data()[i] = result.data()[i+offset] << shift;
         result.data()[i] |= result.data()[i+offset+1] >> (fixed_int<Bits, Signed>::limb_bits - shift);
      }
      result.data()[i] = result.data()[i+offset] << shift;
      ++i;
      for(; i < fixed_int<Bits, Signed>::limb_count; ++i)
         result.data()[i] = 0;
   }
   else
   {
      for(; i + offset < fixed_int<Bits, Signed>::limb_count; ++i)
         result.data()[i] = result.data()[i+offset];
      for(; i < fixed_int<Bits, Signed>::limb_count; ++i)
         result.data()[i] = 0;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}
template <unsigned Bits, bool Signed>
inline void eval_right_shift(fixed_int<Bits, Signed>& result, double_limb_type s)
{
   if(!s)
      return;
   limb_type fill = (Signed && (result.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask)) ? fixed_int<Bits, Signed>::max_limb_value : 0u;
   if(s >= Bits)
   {
      for(unsigned i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
         result.data()[i] = fill;
      return;
   }
   limb_type offset = static_cast<limb_type>(s / fixed_int<Bits, Signed>::limb_bits);
   limb_type shift  = static_cast<limb_type>(s % fixed_int<Bits, Signed>::limb_bits);
   int i = fixed_int<Bits, Signed>::limb_count - 1;
   if(shift)
   {
      // This code only works for non-zero shift, otherwise we invoke undefined behaviour!
      if(fill && (Bits % fixed_int<Bits, Signed>::limb_bits))
      {
         // We need to sign extend the leftmost bits, otherwise we may shift zeros into the result:
         result.data()[0] |= fill << (Bits % fixed_int<Bits, Signed>::limb_bits);
      }
      for(; i - offset > 0; --i)
      {
         result.data()[i] = result.data()[i-offset] >> shift;
         result.data()[i] |= result.data()[i-offset-1] << (fixed_int<Bits, Signed>::limb_bits - shift);
      }
      result.data()[i] = result.data()[i+offset] >> shift;
      result.data()[i] |= fill << (fixed_int<Bits, Signed>::limb_bits - shift);
      --i;
      for(; i >= 0; --i)
         result.data()[i] = fill;
   }
   else
   {
      for(; i - offset > 0; --i)
         result.data()[i] = result.data()[i - offset];
      for(; i >= 0; --i)
         result.data()[i] = fill;
   }
   result.data()[0] &= fixed_int<Bits, Signed>::upper_limb_mask;
}

template <class R, unsigned Bits, bool Signed>
inline typename enable_if<is_integral<R>, void>::type eval_convert_to(R* result, const fixed_int<Bits, Signed>& backend, const mpl::true_&)
{
   if(backend.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask)
   {
      fixed_int<Bits, Signed> t(backend);
      t.negate();
      eval_convert_to(result, t, mpl::false_());
      *result = -*result;
      return;
   }
   else
      eval_convert_to(result, backend, mpl::false_());
}

template <class R, unsigned Bits, bool Signed>
inline typename enable_if<is_integral<R>, void>::type eval_convert_to(R* result, const fixed_int<Bits, Signed>& backend, const mpl::false_&)
{
   unsigned shift = (fixed_int<Bits, Signed>::limb_count - 1) * fixed_int<Bits, Signed>::limb_bits;
   *result = static_cast<limb_type>(0);
   for(unsigned i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
   {
      *result += static_cast<R>(backend.data()[i]) << shift;
      shift -= fixed_int<Bits, Signed>::limb_bits;
   }
}

template <class R, unsigned Bits, bool Signed>
inline typename enable_if<is_integral<R>, void>::type eval_convert_to(R* result, const fixed_int<Bits, Signed>& backend)
{
   typedef mpl::bool_<Signed && std::numeric_limits<R>::is_signed> tag_type;
   eval_convert_to(result, backend, tag_type());
}

template <class R, unsigned Bits, bool Signed>
inline typename enable_if<is_floating_point<R>, void>::type eval_convert_to(R* result, const fixed_int<Bits, Signed>& backend)
{
   if(Signed && (backend.data()[0] & fixed_int<Bits, Signed>::sign_bit_mask))
   {
      fixed_int<Bits, Signed> t(backend);
      t.negate();
      eval_convert_to(result, t);
      *result = -*result;
      return;
   }
   unsigned shift = (fixed_int<Bits, Signed>::limb_count - 1) * fixed_int<Bits, Signed>::limb_bits;
   *result = static_cast<limb_type>(0);
   for(unsigned i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
   {
      *result += static_cast<R>(std::ldexp(static_cast<double>(backend.data()[i]), shift));
      shift -= fixed_int<Bits, Signed>::limb_bits;
   }
}

template <unsigned Bits, bool Signed>
inline bool eval_is_zero(const fixed_int<Bits, Signed>& val)
{
   for(typename fixed_int<Bits, Signed>::data_type::size_type i = 0; i < fixed_int<Bits, Signed>::limb_count; ++i)
   {
      if(val.data()[i])
         return false;
   }
   return true;
}
template <unsigned Bits>
inline int eval_get_sign(const fixed_int<Bits, false>& val)
{
   return eval_is_zero(val) ? 0 : 1;
}
template <unsigned Bits>
inline int eval_get_sign(const fixed_int<Bits, true>& val)
{
   return eval_is_zero(val) ? 0 : val.data()[0] & fixed_int<Bits, true>::sign_bit_mask ? -1 : 1;
}

namespace detail{
//
// Get the location of the least-significant-bit:
//
template <unsigned Bits, bool Signed>
inline unsigned get_lsb(const fixed_int<Bits, Signed>& a)
{
   BOOST_ASSERT(eval_get_sign(a) != 0);
   
   unsigned result = 0;
   //
   // Find the index of the least significant limb that is non-zero:
   //
   int index = fixed_int<Bits, Signed>::limb_count - 1;
   while(!a.data()[index] && index)
      --index;
   //
   // Find the index of the least significant bit within that limb:
   //
   limb_type l = a.data()[index];
   while(!(l & 1u))
   {
      l >>= 1;
      ++result;
   }

   return result + (fixed_int<Bits, Signed>::limb_count - 1 - index) * fixed_int<Bits, Signed>::limb_bits;
}

}

template <unsigned Bits, bool Signed>
inline void eval_gcd(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   int shift;

   fixed_int<Bits, Signed> u(a), v(b);

   int s = eval_get_sign(u);

   /* GCD(0,x) := x */
   if(s < 0)
   {
      u.negate();
   }
   else if(s == 0)
   {
      result = v;
      return;
   }
   s = eval_get_sign(v);
   if(s < 0)
   {
      v.negate();
   }
   else if(s == 0)
   {
      result = u;
      return;
   }

   /* Let shift := lg K, where K is the greatest power of 2
   dividing both u and v. */

   unsigned us = detail::get_lsb(u);
   unsigned vs = detail::get_lsb(v);
   shift = (std::min)(us, vs);
   eval_right_shift(u, us);
   eval_right_shift(v, vs);

   do 
   {
      /* Now u and v are both odd, so diff(u, v) is even.
      Let u = min(u, v), v = diff(u, v)/2. */
      if(u.compare(v) > 0)
         u.swap(v);
      eval_subtract(v, u);
      // Termination condition tries not to do a full compare if possible:
      if(!v.data()[fixed_int<Bits, Signed>::limb_count - 1] && eval_is_zero(v))
         break;
      vs = detail::get_lsb(v);
      eval_right_shift(v, vs);
      BOOST_ASSERT((v.data()[fixed_int<Bits, Signed>::limb_count - 1] & 1));
      BOOST_ASSERT((u.data()[fixed_int<Bits, Signed>::limb_count - 1] & 1));
   } 
   while(true);

   result = u;
   eval_left_shift(result, shift);
}

template <unsigned Bits, bool Signed>
inline void eval_lcm(fixed_int<Bits, Signed>& result, const fixed_int<Bits, Signed>& a, const fixed_int<Bits, Signed>& b)
{
   fixed_int<Bits, Signed> t;
   eval_gcd(t, a, b);

   if(eval_is_zero(t))
   {
      result = static_cast<limb_type>(0);
   }
   else
   {
      eval_divide(result, a, t);
      eval_multiply(result, b);
   }
   if(eval_get_sign(result) < 0)
      result.negate();
}

template <unsigned Bits, bool Signed>
struct number_category<fixed_int<Bits, Signed> > : public mpl::int_<number_kind_integer>{};

typedef mp_number<fixed_int<128, false> > mp_uint128_t;
typedef mp_number<fixed_int<256, false> > mp_uint256_t;
typedef mp_number<fixed_int<512, false> > mp_uint512_t;

typedef mp_number<fixed_int<128, true> > mp_int128_t;
typedef mp_number<fixed_int<256, true> > mp_int256_t;
typedef mp_number<fixed_int<512, true> > mp_int512_t;

}} // namespaces


namespace std{

template <unsigned Bits, bool Signed, bool ExpressionTemplates>
class numeric_limits<boost::multiprecision::mp_number<boost::multiprecision::fixed_int<Bits, Signed>, ExpressionTemplates> >
{
   typedef boost::multiprecision::mp_number<boost::multiprecision::fixed_int<Bits, Signed>, ExpressionTemplates> number_type;

   struct initializer
   {
      initializer()
      {
         (std::numeric_limits<boost::multiprecision::mp_number<boost::multiprecision::fixed_int<Bits, Signed> > >::min)();
         (std::numeric_limits<boost::multiprecision::mp_number<boost::multiprecision::fixed_int<Bits, Signed> > >::max)();
      }
      void do_nothing()const{}
   };
   static const initializer init;
public:
   BOOST_STATIC_CONSTEXPR bool is_specialized = true;
   //
   // Largest and smallest numbers are bounded only by available memory, set
   // to zero:
   //
   BOOST_STATIC_CONSTEXPR number_type (min)() BOOST_MP_NOEXCEPT
   {
      static bool init = false;
      static number_type val;
      if(!init)
      {
         init = true;
         val = Signed ? number_type(number_type(1) << (Bits - 1)) : 0;
      }
      return val;
   }
   BOOST_STATIC_CONSTEXPR number_type (max)() BOOST_MP_NOEXCEPT 
   { 
      static bool init = false;
      static number_type val;
      if(!init)
      {
         init = true;
         val = Signed ? number_type(~(number_type(1) << Bits - 1)) : number_type(~number_type(0));
      }
      return val;
   }
   BOOST_STATIC_CONSTEXPR number_type lowest() BOOST_MP_NOEXCEPT { return (min)(); }
   BOOST_STATIC_CONSTEXPR int digits = Bits;
   BOOST_STATIC_CONSTEXPR int digits10 = (digits * 301L) / 1000;
   BOOST_STATIC_CONSTEXPR int max_digits10 = digits10 + 2;
   BOOST_STATIC_CONSTEXPR bool is_signed = Signed;
   BOOST_STATIC_CONSTEXPR bool is_integer = true;
   BOOST_STATIC_CONSTEXPR bool is_exact = true;
   BOOST_STATIC_CONSTEXPR int radix = 2;
   BOOST_STATIC_CONSTEXPR number_type epsilon() BOOST_MP_NOEXCEPT { return 0; }
   BOOST_STATIC_CONSTEXPR number_type round_error() BOOST_MP_NOEXCEPT { return 0; }
   BOOST_STATIC_CONSTEXPR int min_exponent = 0;
   BOOST_STATIC_CONSTEXPR int min_exponent10 = 0;
   BOOST_STATIC_CONSTEXPR int max_exponent = 0;
   BOOST_STATIC_CONSTEXPR int max_exponent10 = 0;
   BOOST_STATIC_CONSTEXPR bool has_infinity = false;
   BOOST_STATIC_CONSTEXPR bool has_quiet_NaN = false;
   BOOST_STATIC_CONSTEXPR bool has_signaling_NaN = false;
   BOOST_STATIC_CONSTEXPR float_denorm_style has_denorm = denorm_absent;
   BOOST_STATIC_CONSTEXPR bool has_denorm_loss = false;
   BOOST_STATIC_CONSTEXPR number_type infinity() BOOST_MP_NOEXCEPT { return 0; }
   BOOST_STATIC_CONSTEXPR number_type quiet_NaN() BOOST_MP_NOEXCEPT { return 0; }
   BOOST_STATIC_CONSTEXPR number_type signaling_NaN() BOOST_MP_NOEXCEPT { return 0; }
   BOOST_STATIC_CONSTEXPR number_type denorm_min() BOOST_MP_NOEXCEPT { return 0; }
   BOOST_STATIC_CONSTEXPR bool is_iec559 = false;
   BOOST_STATIC_CONSTEXPR bool is_bounded = true;
   BOOST_STATIC_CONSTEXPR bool is_modulo = true;
   BOOST_STATIC_CONSTEXPR bool traps = false;
   BOOST_STATIC_CONSTEXPR bool tinyness_before = false;
   BOOST_STATIC_CONSTEXPR float_round_style round_style = round_toward_zero;
};

template <unsigned Bits, bool Signed, bool ExpressionTemplates>
typename numeric_limits<boost::multiprecision::mp_number<boost::multiprecision::fixed_int<Bits, Signed>, ExpressionTemplates> >::initializer const
   numeric_limits<boost::multiprecision::mp_number<boost::multiprecision::fixed_int<Bits, Signed>, ExpressionTemplates> >::init;
}

#endif