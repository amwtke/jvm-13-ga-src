/*
 * Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef OS_CPU_LINUX_X86_ORDERACCESS_LINUX_X86_HPP
#define OS_CPU_LINUX_X86_ORDERACCESS_LINUX_X86_HPP

// Included in orderAccess.hpp header file.

// Compiler version last used for testing: gcc 4.8.2
// Please update this information when this file changes

// Implementation of class OrderAccess.

// A compiler barrier, forcing the C++ compiler to invalidate all memory assumptions
static inline void compiler_barrier() {
  //!x64底下的一个空指令，但是前面加了volatile，可以认为是防止编译器重排指令的汇编。
  //!参考：https://app.yinxiang.com/shard/s65/nl/15273355/5eb1191e-1a4a-448b-b9de-9006942e99eb/
  __asm__ volatile ("" : : : "memory");
}
//!xiaojin volatile fences => loadload storestore loadstore storeload

//!xiaojin volatile （exp 几种屏障类型——X86-TSO的内存一致性模型）
/*
参考：https://app.yinxiang.com/shard/s65/nl/15273355/f69767fa-78fb-4ed3-93a7-6762fdf786da/
!https://app.yinxiang.com/shard/s65/nl/15273355/92b7199e-fe76-4aa6-9d93-37c47b8b1542/
在CPU级别，有寄存器，对于短时间频繁使用的变量，编译器可以选择将其值缓存在寄存器级别，在这期间，其他CPU看不到该变量内容的改变
在CPU和高速缓存之间有一个写缓冲区StoreBuffer，CPU会将写操作推入写缓冲区，在之后的某个时间点按照FIFO写入内存，在这期间，其他CPU看不到该变量内容的改变。注意，存在写缓冲区并且以FIFO进行工作，就是TSO-完全存储定序内存模型的工作方式。
Cache，高速缓存，可以有多级，但是因为MESI缓存一致性协议可以保证各个处理器之间缓存一致，所以在这个层次上不会有各处理器数据不一致的情况
内存，各处理器共享，不会有数据不一致的情况
*/
//loadload 相当于刷新invalidation queue
inline void OrderAccess::loadload()   { compiler_barrier(); }
//刷新store-buffer
inline void OrderAccess::storestore() { compiler_barrier(); }
//刷新invalidation queue
inline void OrderAccess::loadstore()  { compiler_barrier(); }
//刷新store-buffer.x64平台可能出现这种重排，所以加入实在的fence。
inline void OrderAccess::storeload()  { fence();            }

//!xiaojin volatile -7.1 x86 就只需要一个编译器的优化禁用就行了。
inline void OrderAccess::acquire()    { compiler_barrier(); }
inline void OrderAccess::release()    { compiler_barrier(); }

inline void OrderAccess::fence() {
   // always use locked addl since mfence is sometimes expensive
#ifdef AMD64
  __asm__ volatile ("lock; addl $0,0(%%rsp)" : : : "cc", "memory");
#else
  __asm__ volatile ("lock; addl $0,0(%%esp)" : : : "cc", "memory");
#endif
  compiler_barrier();
}

inline void OrderAccess::cross_modify_fence() {
  int idx = 0;
#ifdef AMD64
  __asm__ volatile ("cpuid " : "+a" (idx) : : "ebx", "ecx", "edx", "memory");
#else
  // On some x86 systems EBX is a reserved register that cannot be
  // clobbered, so we must protect it around the CPUID.
  __asm__ volatile ("xchg %%esi, %%ebx; cpuid; xchg %%esi, %%ebx " : "+a" (idx) : : "esi", "ecx", "edx", "memory");
#endif
}

template<>
struct OrderAccess::PlatformOrderedStore<1, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(T v, volatile T* p) const {
    __asm__ volatile (  "xchgb (%2),%0"
                      : "=q" (v)
                      : "0" (v), "r" (p)
                      : "memory");
  }
};

template<>
struct OrderAccess::PlatformOrderedStore<2, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(T v, volatile T* p) const {
    __asm__ volatile (  "xchgw (%2),%0"
                      : "=r" (v)
                      : "0" (v), "r" (p)
                      : "memory");
  }
};

template<>
struct OrderAccess::PlatformOrderedStore<4, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(T v, volatile T* p) const {
    __asm__ volatile (  "xchgl (%2),%0"
                      : "=r" (v)
                      : "0" (v), "r" (p)
                      : "memory");
  }
};

#ifdef AMD64
template<>
struct OrderAccess::PlatformOrderedStore<8, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(T v, volatile T* p) const {
    __asm__ volatile (  "xchgq (%2), %0"
                      : "=r" (v)
                      : "0" (v), "r" (p)
                      : "memory");
  }
};
#endif // AMD64

#endif // OS_CPU_LINUX_X86_ORDERACCESS_LINUX_X86_HPP
