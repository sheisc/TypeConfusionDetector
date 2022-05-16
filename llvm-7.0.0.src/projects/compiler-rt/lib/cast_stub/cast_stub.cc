//===-- hextype.cc -- runtime support for HexType  ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===-------------------------------------------------------------------===//


#include <stdint.h>

#define SANITIZER_INTERFACE_ATTRIBUTE __attribute__((visibility("default")))

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void * __au_edu_unsw_static_cast_stub( void *src, void *dst){
  return dst;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void * __au_edu_unsw_dynamic_cast_stub( void *src, void *dst){
  return dst;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void * __au_edu_unsw_reinterpret_cast_stub( void *src, void *dst){
  return dst;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void * __au_edu_unsw_placement_new_stub( void *res){
  return res;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void * __au_edu_unsw_new_stub( void * res){
  return res;
}
