#pragma once
#include <new>
#include <cstddef>
#include <cstdint>
#include <cstring>
inline void* portAlloc(size_t size){return ::operator new(size,std::nothrow);}
inline void portFree(void* pointer){::operator delete(pointer);}
inline void* portCalloc(size_t count,size_t size){if(size && count>SIZE_MAX/size)return nullptr;auto*p=portAlloc(count*size);if(p)memset(p,0,count*size);return p;}
