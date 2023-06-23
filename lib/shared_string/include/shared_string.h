#pragma once

#ifndef _SHARED_STRING_H_INCLUDED_
#define _SHARED_STRING_H_INCLUDED_

/**
 * basic_shared_string
 *
 * Copyright (c) 2015 Scott Schanel http://github.com/sschanel/shared_string
 *
 * License: MIT License
 *
 * Uses std::shared_ptr internally to keep memory allocation to a minimum.
 * This is a boost-less implementation that does not use boost::flyweights.
 * That means that if you try hard enough you can outsmart it and do
 * things very ineffeciently.
 *
 * But!  If you embrace it, you get all the same methods that basic_string
 * has (as of C++11), and you get a true shared string and it's thread-safe.
 * Missing methods from basic_string: reserve(), capacity(), shrink_to_fit().
 * append(), assign(), insert(), erase(), replace(), push_front(), push_back(),
 * all methods that return a non-const iterator.  Almost all of the implemented
 * methods are thin wrappers around the shared string.
 *
 * If you need those methods, make a copy of the internal string by calling
 * str() and work on that temporary string.
 *
 * You still get operator=().  And clear().
 *
 *
 *
 */

#include <string>
#include <memory>
#include <functional>

template <typename CharT,
        class Traits = std::char_traits<CharT>,
        class Allocator = std::allocator < CharT >
>
class basic_shared_string {

public:
    typedef std::basic_string<CharT, Traits, Allocator> string_type;

    typedef typename string_type::const_iterator const_iterator;
    typedef typename string_type::const_reverse_iterator const_reverse_iterator;
    typedef typename string_type::size_type size_type;

private:
    std::shared_ptr<const string_type> string_;

    void replace_contents(string_type&& newString) {
        string_ = std::make_shared<const string_type>(std::move(newString));
    }

public:
    static const size_type npos = (size_t)-1;

    const string_type& str() const {
        static const string_type empty;
        if (string_.get() != nullptr) {
            return *(string_.get());
        }
        return empty;
    }

    operator const string_type&() {
        return str();
    }

    basic_shared_string() {
    }

    basic_shared_string(const CharT * s) {
        replace_contents(string_type(s));
    }

    basic_shared_string(const string_type& s) {
        replace_contents(string_type(s));
    }

    basic_shared_string(string_type&& s) {
        replace_contents(std::move(s));
    }

    basic_shared_string(const basic_shared_string& s) {
        string_ = s.string_;
    }

    void clear() {
        string_ = nullptr;
    }

    basic_shared_string& operator=(const basic_shared_string& s) {
        string_ = s.string_;
        return *this;
    }

    basic_shared_string& operator=(const string_type& str) {
        replace_contents(string_type(str));
        return *this;
    }

    basic_shared_string& operator=(string_type&& str) {
        replace_contents(std::move(str));
        return *this;
    }

    basic_shared_string& operator=(const CharT* s) {
        replace_contents(string_type(s));
        return *this;
    }

    basic_shared_string& operator=(CharT ch) {
        replace_contents(string_type(ch));
        return *this;
    }

    basic_shared_string& operator=(std::initializer_list<CharT> ilist) {
        replace_contents(string_type(ilist));
        return *this;
    }

    void swap(basic_shared_string& rhs) {
        this->string_.swap(rhs.string_);
    }


    CharT at(size_type pos) const {
        return str().at(pos);
    }

    CharT operator[](size_type pos) const {
        return str()[pos];
    }

    CharT front() const {
        return str().front();
    }

    CharT back() const {
        return str().back();
    }

    const CharT * c_str() const {
        return str().c_str();
    }

    const_iterator begin() const {
        return str().begin();
    }

    const_iterator cbegin() const {
        return str().cbegin();
    }

    const_iterator end() const {
        return str().end();
    }

    const_iterator cend() const {
        return str().end();
    }

    const_reverse_iterator rbegin() const {
        return str().rbegin();
    }

    const_reverse_iterator crbegin() const {
        return str().crbegin();
    }

    const_reverse_iterator rend() const {
        return str().rend();
    }

    const_reverse_iterator crend() const {
        return str().crend();
    }



    bool empty() const {
        return str().empty();
    }

    size_t size() const {
        return str().size();
    }

    size_t length() const {
        return str().length();
    }

    size_t max_size() const {
        return str().max_size();
    }

    size_t capacity() const {
        return str().capacity();
    }

    int compare(const string_type& rhs) const {
        return str().compare(rhs);
    }

    int compare(const basic_shared_string& rhs) const {
        return str().compare(rhs.str());
    }

    int compare(size_type pos1, size_type count1,
                const string_type& str) const {
        return str().compare(pos1, count1, str);
    }

    int compare(const CharT* s) const {
        return str().compare(s);
    }

    int compare(size_type pos1, size_type count1,
                const CharT* s) const {

        return str().compare(pos1, count1, s);
    }

    int compare(
            size_type pos1,
            size_type count1,
            const CharT* s,
            size_type count2) const {

        return str().compare(pos1, count1, s, count2);
    }

    basic_shared_string<CharT> substr(
            size_type pos = 0,
            size_type count = npos) const {

        if (pos == 0) {
            if (count == npos) {
                return *this;
            }
            else if (count >= str().size()) {
                return *this;
            }
        }
        return basic_shared_string(str().substr(pos, count));
    }

    size_type copy(
            CharT* dest,
            size_type count,
            size_type pos = 0) const {
        return str().copy(dest, count, pos);
    }

    size_type find(const string_type& str, size_type pos = 0) const {
        return str().find(str, pos);
    }

    size_type find(const CharT* s, size_type pos, size_type count) const {
        return str().find(s, pos, count);
    }

    size_type find(const CharT* s, size_type pos = 0) const {
        return str().find(s, pos);
    }

    size_type find(CharT ch, size_type pos = 0) const {
        return str().find(ch, pos);
    }

    size_type rfind(const string_type& str, size_type pos = npos) const {
        return str().rfind(str, pos);
    }

    size_type rfind(const CharT* s, size_type pos, size_type count) const {
        return str().rfind(s, pos, count);
    }

    size_type rfind(const CharT* s, size_type pos = npos) const {
        return str().rfind(s, pos);
    }

    size_type rfind(CharT ch, size_type pos = npos) const {
        return str().rfind(ch, pos);
    }

    size_type find_first_of(const string_type& str, size_type pos = 0) const {
        return str().find_first_of(str, pos);
    }

    size_type find_first_of(const CharT* s, size_type pos, size_type count) const {
        return str().find_first_of(s, pos, count);
    }

    size_type find_first_of(const CharT* s, size_type pos = 0) const {
        return str().find_first_of(s, pos);
    }

    size_type find_first_of(CharT ch, size_type pos = 0) const {
        return str().find_first_of(ch, pos);
    }

    size_type find_first_not_of(const string_type& str, size_type pos = 0) const {
        return str().find_first_not_of(str, pos);
    }

    size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const {
        return str().find_first_not_of(s, pos, count);
    }

    size_type find_first_not_of(const CharT* s, size_type pos = 0) const {
        return str().find_first_not_of(s, pos);
    }

    size_type find_first_not_of(CharT ch, size_type pos = 0) const {
        return str().find_first_not_of(ch, pos);
    }

    size_type find_last_of(const string_type& str, size_type pos = npos) const {
        return str().find_last_of(str, pos);
    }

    size_type find_last_of(const CharT* s, size_type pos, size_type count) const {
        return str().find_last_of(s, pos, count);
    }

    size_type find_last_of(const CharT* s, size_type pos = npos) const {
        return str().find_last_of(s, pos);
    }
    size_type find_last_of(CharT ch, size_type pos = npos) const {
        return str().find_last_of(ch, pos);
    }
    size_type find_last_not_of(const string_type& str, size_type pos = npos) const {
        return str().find_last_not_of(str, pos);
    }
    size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const {
        return str().find_last_not_of(s, pos, count);
    }
    size_type find_last_not_of(const CharT* s, size_type pos = npos) const {
        return str().find_last_not_of(s, pos);
    }
    size_type find_last_not_of(CharT ch, size_type pos = npos) const {
        return str().find_last_not_of(ch, pos);
    }
};

template< class CharT, class Traits, class Alloc >
bool operator==(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return lhs.str() == rhs.str();
}

template< class CharT, class Traits, class Alloc >
bool operator!=(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return lhs.str() != rhs.str();
}

template< class CharT, class Traits, class Alloc >
bool operator<(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return lhs.str() < rhs.str();
}

template< class CharT, class Traits, class Alloc >
bool operator<=(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return lhs.str() <= rhs.str();
}

template< class CharT, class Traits, class Alloc >
bool operator>(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {

    return lhs.str() > rhs.str();
}

template< class CharT, class Traits, class Alloc >
bool operator>=(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {

    return lhs.str() >= rhs.str();
}

template< class CharT, class Traits, class Alloc >
bool operator==(
        const CharT* lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return operator==(lhs, rhs.str());
}

template< class CharT, class Traits, class Alloc >
bool operator==(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const CharT* rhs) {

    return operator==(lhs.str(), rhs);
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const CharT* lhs, const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return operator!=(lhs, rhs.str());
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const basic_shared_string<CharT, Traits, Alloc>& lhs, const CharT* rhs) {
    return operator!=(lhs.str(), rhs);
}

template< class CharT, class Traits, class Alloc >
bool operator<(const CharT* lhs, const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return operator<(lhs, rhs.str());
}

template< class CharT, class Traits, class Alloc >
bool operator<(const basic_shared_string<CharT, Traits, Alloc>& lhs, const CharT* rhs) {
    return operator<(lhs.str(), rhs);
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const CharT* lhs, const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return operator<=(lhs, rhs.str());
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const basic_shared_string<CharT, Traits, Alloc>& lhs, const CharT* rhs) {
    return operator<=(lhs.str(), rhs);
}

template< class CharT, class Traits, class Alloc >
bool operator>(const CharT* lhs, const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return operator>(lhs, rhs.str());
}

template< class CharT, class Traits, class Alloc >
bool operator>(const basic_shared_string<CharT, Traits, Alloc>& lhs, const CharT* rhs) {
    return operator>(lhs.str(), rhs);
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const CharT* lhs, const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return operator>=(lhs, rhs.str());
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const basic_shared_string<CharT, Traits, Alloc>& lhs, const CharT* rhs) {
    return operator>=(lhs.str(), rhs);
}


template <class CharT, class Traits, class Allocator>
std::basic_ostream<CharT, Traits>& operator<<(
        std::basic_ostream<CharT, Traits>& os,
        const basic_shared_string<CharT, Traits, Allocator>& str) {
    return operator<<(os, str.str());
}

template <class CharT, class Traits, class Allocator>
std::basic_istream<CharT, Traits>& operator>>(
        std::basic_istream<CharT, Traits>& is,
        basic_shared_string<CharT, Traits, Allocator>& str) {

    std::basic_string<CharT, Traits, Allocator> temp;
    operator>>(is, temp);
    str = temp;
    return is;
}

template< class CharT, class Traits, class Alloc >
basic_shared_string<CharT, Traits, Alloc> operator+(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs.str(), rhs.str()));
}

template< class CharT, class Traits, class Alloc >
basic_shared_string<CharT, Traits, Alloc> operator+(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const std::basic_string<CharT, Traits, Alloc>& rhs) {
    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs.str(), rhs));
}

template< class CharT, class Traits, class Alloc >
basic_shared_string<CharT, Traits, Alloc> operator+(
        const std::basic_string<CharT, Traits, Alloc>& lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {
    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs, rhs.str()));
}

template< class CharT, class Traits, class Alloc >
basic_shared_string<CharT, Traits, Alloc> operator+(
        const CharT* lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {

    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs, rhs.str()));
}

template< class CharT, class Traits, class Alloc >
basic_shared_string<CharT, Traits, Alloc> operator+(
        CharT lhs,
        const basic_shared_string<CharT, Traits, Alloc>& rhs) {

    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs, rhs.str()));
}

template< class CharT, class Traits, class Alloc >
basic_shared_string<CharT, Traits, Alloc> operator+(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        const CharT* rhs) {

    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs.str(), rhs));
}

template<class CharT, class Traits, class Alloc>
basic_shared_string<CharT, Traits, Alloc> operator+(
        const basic_shared_string<CharT, Traits, Alloc>& lhs,
        CharT rhs) {

    return basic_shared_string<CharT, Traits, Alloc>(operator+(lhs.str(), rhs));
}

namespace std {

    template <class CharT, class Traits, class Alloc>
    int stoi(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0, int base = 10) {
        return stoi(str.str(), pos, base);
    }

    template <class CharT, class Traits, class Alloc>
    long stol(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0, int base = 10) {
        return stol(str.str(), pos, base);
    }

    template <class CharT, class Traits, class Alloc>
    long long stoll(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0, int base = 10) {
        return stoll(str.str(), pos, base);
    }

    template <class CharT, class Traits, class Alloc>
    unsigned long stoul(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0, int base = 10) {
        return stoul(str.str(), pos, base);
    }

    template <class CharT, class Traits, class Alloc>
    unsigned long long stoull(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0, int base = 10) {
        return stoull(str.str(), pos, base);
    }

    template <class CharT, class Traits, class Alloc>
    float stof(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0) {
        return stof(str.str(), pos);
    }

    template <class CharT, class Traits, class Alloc>
    double stod(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0) {
        return stod(str.str(), pos);
    }

    template <class CharT, class Traits, class Alloc>
    double stold(const basic_shared_string<CharT, Traits, Alloc>& str, std::size_t* pos = 0) {
        return stold(str.str(), pos);
    }

    template <class CharT>
    struct hash < basic_shared_string<CharT> > {
        size_t operator()(const basic_shared_string<CharT>& key) {
            return hash<std::string>()(key.str());
        }
    };
}

typedef basic_shared_string<char> shared_string;
typedef basic_shared_string<wchar_t> shared_wstring;


#endif // _STRING_REF_H_INCLUDED_
