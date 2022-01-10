#ifndef BYTE_SWAP_H
#define BYTE_SWAP_H

#define swpw(a)                           \
  __asm__ volatile                        \
  ("ror   #8,%0"                          \
  : "=d"(a)          /* outputs */        \
  : "0"(a)           /* inputs  */        \
  : "cc"             /* clobbered */      \
  )

#define swpl(a)                           \
  __asm__ volatile                        \
  ("ror   #8,%0\n\t"                      \
   "swap  %0\n\t"                         \
   "ror   #8,%0"                          \
  : "=d"(a)          /* outputs */        \
  : "0"(a)           /* inputs  */        \
  : "cc"             /* clobbered */      \
  )

#endif
