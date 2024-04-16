#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <winerror.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/param.h>
  #include <sys/un.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <sys/time.h>
  #include <sys/select.h>
#endif

#include <mruby.h>
#if MRB_INT_BIT < 64
#error "need 64 bit mruby"
#endif
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/value.h>
#include <mruby/ext/io.h>

#include <ares.h>

#if (__GNUC__ >= 3) || (__INTEL_COMPILER >= 800) || defined(__clang__)
# define likely(x) __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000UL
#endif

#define NELEMS(argv) (sizeof(argv) / sizeof(argv[0]))

struct cares_ctx {
  mrb_state *mrb;
  mrb_value state_callback;
  ares_channel channel;
  mrb_value cname_storage;
  mrb_value ai_storage;
  mrb_value error_storage;
  sa_family_t family;
};

static void
mrb_cares_ctx_free(mrb_state *mrb, void *p)
{
  struct cares_ctx *cares_ctx = (struct cares_ctx *) p;
  ares_destroy(cares_ctx->channel);
  mrb_free(mrb, p);
}

static const struct mrb_data_type mrb_cares_ctx_type = {
  "$i_mrb_cares_ctx_t", mrb_cares_ctx_free
};