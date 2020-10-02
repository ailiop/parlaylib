// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011-2019 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
//#include <charconv> -- not widely available yet
#include "primitives.h"

#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
//#include "../../../common/get_time.h"

namespace parlay {

  // Reads a character sequence from a file :
  //    if end is zero or larger than file, then returns full file
  //    if start past end of file then returns an empty string.
  inline sequence<char> char_seq_from_file(std::string filename,
					   size_t start=0, size_t end=0);

  // Returns a read only character range containing the contents of the file
  // Result is accessible until destructed.
  // Cannot be copied, but can be moved
  // Meant for large files, and can be significantly faster than char_seq_from_file
  // Uses mmap when available (!!currently only works with mmap)
  // auto char_range_from_file(std::string const &filename);

  // Writes a character sequence to a file, returns 0 if successful
  int char_seq_to_file(sequence<char> const &S, std::string const &filename);

  // Writes a character sequence to a stream
  inline void char_seq_to_stream(sequence<char> const &S, std::ostream& os);

  // Same but using <<
  extern inline std::ostream& operator<<(std::ostream& os, sequence<char> const &s) {
    char_seq_to_stream(s, os); return os;}
  
  // Returns a sequence of sequences of characters, one per token.
  // The tokens are the longest contiguous subsequences of non space characters.
  // where spaces are define by the unary predicate is_space.
  template <class Range, class UnaryPred>
  sequence<sequence<char>>
  tokens(Range const &R, UnaryPred const &is_space);

  // Same as above, but takes a function f to map over a slice for each token.
  // Useful to e.g., parse strings to ints, or just return the slices
  // Avoids allocating memory for each subsequence
  template <class Range, class UnaryPred, class F>
  auto tokens(Range &R, UnaryPred const &is_space, const F &f)
    -> sequence<decltype(f(make_slice(R)))>;

  // Returns a sequence of sequences of characters, one per partition.
  // The partitions are marked by start flags, which must be of the same length.
  // There will always be one more subsequence than number of flags.
  template <class CharRange, class BoolRange>
  auto partition_at(CharRange const &R, BoolRange const &StartFlags)
    -> sequence<sequence<char>>;

  // Same as above, but takes a function f to map over a slice for each partition
  template <class CharRange, class BoolRange, class F>
  auto partition_at(CharRange const &R, BoolRange const &StartFlags, F const &f)
    -> sequence<decltype(make_slice(R))>;

  // Convers a character range to long our double
  template <class Range>
  long char_range_to_l(const Range &R);
  template <class Range>
  double char_range_to_d(const Range &R);

  // Converts many types to a "pretty printed" character sequence
  //template <typename T>
  //sequence<char> to_char_seq(T s);

  // ********************************
  // char_seq_from_file
  // ********************************

  inline sequence<char> char_seq_from_file(std::string filename,
					   size_t start, size_t end) {
    std::ifstream file (filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      std::cout << "Unable to open file: " << filename << std::endl;
      exit(1);
    }
    size_t length = file.tellg();
    start = std::min(start,length);
    if (end == 0) end = length;
    else end = std::min(end,length);
    size_t n = end - start;
    file.seekg (start, std::ios::beg);
    //sequence<char> bytes(n+1);
    sequence<char> bytes = sequence<char>::uninitialized(n+1);
    file.read (bytes.data(), n);
    file.close();
    bytes[n] = 0;
    return bytes;
  }

  // ********************************
  // char_range_from_file
  // ********************************

  struct foo {
    int x;
  };
  
  struct char_range_from_file {
  private:
    char* begin_p;
    char* end_p;
  public:
    const char operator[] (size_t i) { return begin_p[i];}
    size_t size() {return end_p - begin_p;}
    char* begin() {return begin_p;}
    char* end() {return end_p;}

    // should be ifdefed so that uses regular memory if no mmap available
    char_range_from_file(std::string const &filename) {
      // old fashioned c to deal with mmap
      struct stat sb;
      int fd = open(filename.c_str(), O_RDONLY);
      if (fd == -1) { perror("open"); exit(-1);  }
      if (fstat(fd, &sb) == -1) { perror("fstat"); exit(-1); }
      if (!S_ISREG (sb.st_mode)) { perror("not a file\n");  exit(-1); }
    
      char *p = static_cast<char*>(mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
      if (p == MAP_FAILED) { close(fd); perror("mmap"); exit(-1); }
      if (close(fd) == -1) { perror("close"); exit(-1); }
      begin_p = p;
      end_p = p + sb.st_size;
    }
    ~char_range_from_file() {
      if (begin_p != nullptr)
	munmap(begin_p, end_p - begin_p);}

    // moveable but not copyable
    char_range_from_file(const char_range_from_file &x) = delete;
    char_range_from_file(char_range_from_file &&x) : begin_p(x.begin_p), end_p(x.end_p) {
      x.begin_p = nullptr;}
    void swap(char_range_from_file &x) {
      std::swap(x.begin_p, begin_p);
      std::swap(x.end_p, end_p);}
    char_range_from_file& operator=(char_range_from_file x) {
      swap(x);
      return *this;}
  };

  // ********************************
  // char_seg_to_stream and _file
  // ********************************
    
  extern inline void char_seq_to_stream(sequence<char> const &S, std::ostream& os) {
    os.write(S.data(), S.size());
  }

  extern inline int char_seq_to_file(sequence<char> const &S, std::string const &filename) {
    std::ofstream file_stream (filename, std::ios::out | std::ios::binary);
    if (!file_stream.is_open()) {
      std::cout << "Unable to open file for writing: " << filename << std::endl;
      return 1;
    }
    char_seq_to_stream(S, file_stream);
    file_stream.close();
    return 0;
  }

  // ********************************
  // tokens
  // ********************************

  template <class Range, class UnaryPred, class F>
  auto tokens(Range &R, UnaryPred const &is_space, const F &f)
    -> sequence<decltype(f(make_slice(R)))> {
    //timer t("tokens");
    auto S = make_slice(R);
    size_t n = S.size();
    if (n == 0) return sequence<decltype(f(make_slice(R)))>();
    auto Flags = sequence<bool>::uninitialized(n+1);
    parallel_for(1, n, [&] (long i) {
	Flags[i] = is_space(S[i-1]) != is_space(S[i]);
      }, 10000);
    //t.next("for");
	  
    Flags[0] = !is_space(S[0]);
    Flags[n] = !is_space(S[n-1]);

    sequence<long> Locations = pack_index<long>(Flags);
    //t.next("pack index");
    auto x = tabulate(Locations.size()/2, [&] (size_t i) {
	return f(S.cut(Locations[2*i], Locations[2*i+1]));});
    //t.next("tab");
    return x;
  }

  template <class Range, class UnaryPred>
  auto tokens(Range const &R, UnaryPred const &is_space)
    -> sequence<sequence<char>> {
    return tokens(R, is_space, [] (auto x) {return to_sequence(x);});}

  // ********************************
  // partition_at
  // ********************************
  
  template <class CharRange, class BoolRange, class F>
  auto partition_at(CharRange const &R, BoolRange const &StartFlags, F const &f)
    -> sequence<decltype(make_slice(R))>
  {
    auto S = make_slice(R);
    auto Flags = make_slice(StartFlags);
    size_t n = S.size();
    if (Flags.size() != n)
      std::cout << "Unequal sizes in pbbs::partition_at" << std::endl;

    sequence<long> Locations = pack_index<long>(Flags);

    return tabulate(Locations.size(), [&] (size_t i) {
	size_t start = (i==0) ? 0 : Locations[i-1] + 1;
	size_t end = (i==n) ? n : Locations[i];
	return f(S.cut(start, end));});
  }

  template <class CharRange, class BoolRange>
  auto partition_at(CharRange const &R, BoolRange const &StartFlags)
    -> sequence<sequence<char>> {
    return partition_at(R, StartFlags, [] (auto x) {return to_sequence(x);});}

  // ********************************
  // Reading from character ranges 
  // ********************************
  
  template <class Range>
  long char_range_to_l(const Range &R) {
    auto str = make_slice(R);
    size_t i = 0;
    auto read_digits = [&] () {
      long r = 0;
      while (i < str.size() && std::isdigit(str[i]))
	r = r*10 + (str[i++] - 48);
      return r;
    };
    if (str[i] == '-') {i++; return -read_digits();}
    else return read_digits();
  }

  template <class Range>
  double char_range_to_d(const Range &str) {
    char str_padded[str.size()+1];
    for (int i=0; i < str.size(); i++) 
      str_padded[i] = str[i];
    str_padded[str.size()] = 0;
    return atof(str_padded);
  }

  // ********************************
  // Printing to a character sequence
  // ********************************

  template <typename T>
  sequence<char> to_char_seq(T const &x);

  template <>
  inline sequence<char> to_char_seq<char>(char const &c) {
    return sequence<char>(1,c);
  }

  template <>
  inline sequence<char> to_char_seq<bool>(bool const &v) {
    return to_char_seq(v ? '1' : '0');
  }

  template <>
  inline sequence<char> to_char_seq(long const &v) {
    int max_len = 21;
    char s[max_len+1];
    int l = snprintf(s, max_len, "%ld", v);
    return sequence<char>(s, s + std::min(max_len-1, l));
  }

  template <>
  inline sequence<char> to_char_seq<int>(int const &v) {
    return to_char_seq((long) v);};

  template <>
  inline sequence<char> to_char_seq<unsigned long>(unsigned long const &v) {
    int max_len = 21;
    char s[max_len+1];
    int l = snprintf(s, max_len, "%lu", v);
    return sequence<char>(s, s + std::min(max_len-1, l));
  }

  template <>
  inline sequence<char> to_char_seq<unsigned int>(unsigned int const &v) {
    return to_char_seq((unsigned long) v);};

  template <>
  inline sequence<char> to_char_seq<double>(double const &v) {
    int max_len = 20;
    char s[max_len+1];
    int l = snprintf(s, max_len, "%.11le", v);
    return sequence<char>(s, s + std::min(max_len-1, l));
  }

  template <>
  inline sequence<char> to_char_seq<float>(float const &v) {
    return to_char_seq((double) v);};

  template <>
  inline sequence<char> to_char_seq<std::string>(std::string const &s) {
    return tabulate(s.size(), [&] (size_t i) -> char {return s[i];});
  }

  template <>
  inline sequence<char> to_char_seq<char*>(char* const &s) {
    size_t l = strlen(s);
    return tabulate(l, [&] (size_t i) -> char {return s[i];});
  }

  template <class A, class B>
  sequence<char> to_char_seq(std::pair<A,B> const &P) {
    sequence<sequence<char>> s = {
      to_char_seq('('), to_char_seq(P.first),
      to_char_seq((std::string) ", "),
      to_char_seq(P.second), to_char_seq(')')};
    return flatten(s);
  }

  template <class T>
  sequence<char> to_char_seq(slice<T,T> const &A) {
    auto n = A.size();
    if (n==0) return to_char_seq((std::string) "[]");
    auto separator = to_char_seq((std::string) ", ");
    return flatten(tabulate(2 * n + 1, [&] (size_t i) {
	  if (i == 0) return to_char_seq('[');
	  if (i == 2*n) return to_char_seq(']');
	  if (i & 1) return to_char_seq(A[i/2]);
	  return separator;
	}));
  }

  template <class T>
  sequence<char> to_char_seq(sequence<T> const &A) {
    return to_char_seq(make_slice(A));
  }

  template <>
  inline sequence<char> to_char_seq<sequence<char>>(sequence<char> const &s) {
    return s; }

  
  // std::to_chars not widely available yet. sigh!!
  // template<typename T, 
  // 	   typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
  // sequence<char> to_char_seq(T v) {
  //   char a[20];
  //   auto r = std::to_chars(a, a+20, v);
  //   return sequence<char>(slice<char*>(a, r.ptr));
  // }


}

