/*****************************************************************************
 * Author:   Valient Gough <vgough@pobox.com>
 *
 *****************************************************************************
 * Copyright (c) 2003-2013, Valient Gough
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.  
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cipher/MemoryPool.h"

#include <cstdlib>
#include <cstring>

#include "base/config.h"
#include "base/Error.h"

#include <pthread.h>

#include <sys/mman.h>

#include <glog/logging.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_NOACCESS( a, b )
#define VALGRIND_MAKE_MEM_UNDEFINED( a, b )
#endif

#include <map>
#include <list>

#ifdef WITH_OPENSSL
# include <openssl/crypto.h>
# include <openssl/buffer.h>
#endif

#ifdef WITH_BOTAN
# include <botan/botan.h>
# include <botan/version.h>
#endif

namespace encfs {

#ifdef WITH_OPENSSL
static byte *allocBlock( int size )
{
  byte *block = (byte *)OPENSSL_malloc(size);
  return block;
}

static void freeBlock( byte *block, int size )
{
  OPENSSL_cleanse(block, size);
  OPENSSL_free(block);
}

#else

static byte *allocBlock(int size) {
  byte *block = new byte[size];
  return block;
}

unsigned char cleanse_ctr = 0;
static void freeBlock(byte *data, int len) {
  byte *p = data;
  size_t loop = len, ctr = cleanse_ctr;
  while(loop--)
  {
    *(p++) = (unsigned char)ctr;
    ctr += (17 + ((size_t)p & 0xF));
  }
  // Try to ensure the compiler doesn't optimize away the loop.
  p=(byte *)memchr(data, (unsigned char)ctr, len);
  if(p)
    ctr += (63 + (size_t)p);
  cleanse_ctr = (unsigned char)ctr;
  delete[] data;
}

#endif

void MemBlock::allocate(int size)
{
  rAssert(size > 0);
  this->data = allocBlock(size);
  this->size = size;
}

MemBlock::~MemBlock()
{
  freeBlock(data, size);
}

#ifdef WITH_BOTAN
SecureMem::SecureMem(int len) 
    : data_(new Botan::SecureVector<unsigned char>(len))
{
  rAssert(len > 0);
}

SecureMem::~SecureMem()
{
# if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(1,11,0)
  data_->destroy();
# endif
  delete data_;
}

byte* SecureMem::data() const {
  return const_cast<byte*>(data_->begin());
}

int SecureMem::size() const {
  return data_->size();
}

#else
SecureMem::SecureMem(int len)
{
  rAssert(len > 0);
  data_ = allocBlock(len);
  if (data_)
  {
    size_ = len;
    mlock(data_, size_);
  } else
  {
    size_ = 0;
  }
} 

SecureMem::~SecureMem()
{
  if (size_)
  {
    freeBlock(data_, size_);
    munlock(data_, size_);

    data_ = NULL;
    size_ = 0;
  }
}         
#endif

bool operator == (const SecureMem &a, const SecureMem &b) {
  return (a.size() == b.size()) &&
         (memcmp(a.data(), b.data(), a.size()) == 0);
}

}  // namespace encfs

