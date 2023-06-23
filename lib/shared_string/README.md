# shared_string

Simple implementation of a shared string in C++ (C++11).

All it does is wrap up a std::shared_ptr<const std::basic_string<CharT>> and give it std::basic_string interface, so it's feels like you're using a regular ol' std::string.  Almost all of the implemented methods are thin wrappers around the actual shared string.

### Methods missing that exist in std::basic_string

- reserve()
- capacity()
- shrink_to_fit()
- append()
- assign()
- insert()
- erase()
- replace()
- push_front()
- push_back()
- all methods that return a non-const iterator

It's not meant for manipulating the string underneath.  If you need those methods, you should just use std::string.

Note that you still get operator=() and clear().

### Couldn’t I just use const std::string&?  

Sure.  But it doesn’t help you if, for instance, you want to use those strings as keys in maps.  shared_string allows you to create lots of std::map<shared_string, MyType>’s and not incur a penalty for repeating the keys over and over again.

### Couldn’t I use boost::const_string?

You bet! And you should. But this has different semantics - if you initialize a shared_string with a const char *, a copy is made, and the shared_string takes ownership of that copy - just like std::string.  Just like std::string except that copying a shared_string doesn’t allocate new buffers.
