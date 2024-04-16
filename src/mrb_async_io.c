#include "mrb_async_io.h"

static void
mrb_append_ai(struct cares_ctx *cares_ctx, struct RClass *addrinfo_class, struct ares_addrinfo_node *node)
{
  mrb_value storage;
  if (cares_ctx->family == AF_INET6 && node->ai_family == AF_INET) {
    struct sockaddr_in *sa_in     = (struct sockaddr_in *) node->ai_addr;
    struct sockaddr_in6 sa_in6    = {0};
    sa_in6.sin6_family            = AF_INET6;
    sa_in6.sin6_port              = sa_in->sin_port;
    sa_in6.sin6_addr.s6_addr[10]  = 0xff;
    sa_in6.sin6_addr.s6_addr[11]  = 0xff;
    memcpy(sa_in6.sin6_addr.s6_addr + 12, &sa_in->sin_addr, sizeof(sa_in->sin_addr));
    storage = mrb_str_new(cares_ctx->mrb, (const char *) &sa_in6, sizeof(sa_in6));
  } else {
    storage = mrb_str_new(cares_ctx->mrb, (const char *) node->ai_addr, node->ai_addrlen);
  }
  mrb_value argv[] = {
    storage,
    mrb_fixnum_value(node->ai_family),
    mrb_fixnum_value(node->ai_socktype),
    mrb_fixnum_value(node->ai_protocol)
  };
  mrb_value addrinfo = mrb_obj_new(cares_ctx->mrb, addrinfo_class, NELEMS(argv), argv);
  mrb_ary_push(cares_ctx->mrb, cares_ctx->ai_storage, addrinfo);
}

static void
mrb_append_error(struct cares_ctx *ctx, int status)
{
  switch (status) {
    case ARES_ENOTIMP:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "notimp")));
    break;
    case ARES_EBADNAME:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "badname")));
    break;
    case ARES_ENODATA:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "nodata")));
    break;
    case ARES_ENOTFOUND:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "notfound")));
    break;
    case ARES_ENOMEM:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "nomem")));
    break;
    case ARES_ECANCELLED:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "cancelled")));
    break;
    case ARES_EDESTRUCTION:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_symbol_value(mrb_intern_lit(ctx->mrb, "destruction")));
    break;
    default:
      mrb_ary_push(ctx->mrb, ctx->error_storage, mrb_fixnum_value(status));
  }
}

static void 
mrb_ares_getaddrinfo_callback(void *arg, int status, int timeouts, struct ares_addrinfo *result)
{  
  struct cares_ctx *cares_ctx = (struct cares_ctx *) arg;
  if (likely(status == ARES_SUCCESS)) {
    struct RClass *addrinfo_class = mrb_class_get(cares_ctx->mrb, "Addrinfo");
    struct ares_addrinfo_cname *cname = result->cnames;
    struct ares_addrinfo_node *node = result->nodes;
    while (cname) {
      mrb_ary_push(cares_ctx->mrb, cares_ctx->cname_storage, mrb_str_new_cstr(cares_ctx->mrb, cname->name));
      cname = cname->next;
    }
    while (node) {
      mrb_append_ai(cares_ctx, addrinfo_class, node);
      node = node->ai_next;
    }
  } else {
    mrb_append_error(cares_ctx, status);
  }
  ares_freeaddrinfo(result);
}

static void
mrb_ares_state_callback(void *data, ares_socket_t socket_fd, int readable, int writable)
{
  struct cares_ctx *cares_ctx = (struct cares_ctx *) data;
  mrb_state *mrb = cares_ctx->mrb;
  int arena_index = mrb_gc_arena_save(mrb);
  struct RClass *async_ares_socket_class;
  async_ares_socket_class = mrb_class_get_under(mrb, mrb_class_get_under(mrb, mrb_class_get(mrb, "Async"), "Ares"), "Socket");

  mrb_value argv[] = {mrb_int_value(mrb, socket_fd), mrb_bool_value(readable), mrb_bool_value(writable)};
  mrb_yield(mrb, cares_ctx->state_callback, mrb_obj_new(mrb, async_ares_socket_class, NELEMS(argv), argv));
  mrb_gc_arena_restore(mrb, arena_index);
}

static mrb_value
mrb_ares_new(mrb_state *mrb, mrb_value self)
{
  mrb_value state_callback = mrb_nil_value();
  mrb_get_args(mrb, "&", &state_callback);
  if (mrb_nil_p(state_callback)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  if (MRB_TT_PROC != mrb_type(state_callback)) {
    mrb_raise(mrb, E_TYPE_ERROR, "not a block");
  }

  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "state_callback"), state_callback);

  struct cares_ctx *cares_ctx = mrb_realloc(mrb, DATA_PTR(self), sizeof(*cares_ctx));
  memset(cares_ctx, '\0', sizeof(*cares_ctx));
  mrb_data_init(self, cares_ctx, &mrb_cares_ctx_type);
  cares_ctx->mrb = mrb;
  cares_ctx->state_callback = state_callback;
  struct ares_options options = {
    .sock_state_cb = mrb_ares_state_callback,
    .sock_state_cb_data = cares_ctx
  };

  if (unlikely(ares_init_options(&cares_ctx->channel, &options, ARES_OPT_SOCK_STATE_CB) != ARES_SUCCESS))
    mrb_raise(mrb, E_RUNTIME_ERROR, "c-ares init options failed");

  cares_ctx->cname_storage = mrb_ary_new(mrb);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@cnames"), cares_ctx->cname_storage);
  cares_ctx->ai_storage = mrb_ary_new_capa(mrb, 1);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@ais"), cares_ctx->ai_storage);
  cares_ctx->error_storage = mrb_ary_new(mrb);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@errors"), cares_ctx->error_storage);

  return self;
}

static mrb_value
mrb_ares_getaddrinfo(mrb_state *mrb, mrb_value self)
{
  struct cares_ctx *cares_ctx = DATA_PTR(self);
  mrb_ary_clear(mrb, cares_ctx->cname_storage);
  mrb_ary_clear(mrb, cares_ctx->ai_storage);
  mrb_ary_clear(mrb, cares_ctx->error_storage);

  mrb_value sock;
  const char *node;
  const char *service;
  mrb_get_args(mrb, "ozz", &sock, &node, &service);
  ares_socket_t socket = (ares_socket_t) mrb_integer(mrb_convert_type(mrb, sock, MRB_TT_INTEGER, "Integer", "fileno"));
  struct sockaddr_storage ss;
  socklen_t sslen = sizeof(ss);
  if (unlikely(getsockname(socket, (struct sockaddr *) &ss, &sslen) == -1)) {
    mrb_sys_fail(mrb, "getsockname");
  }
  int socktype;
  socklen_t optlen = sizeof(socktype);
  if (unlikely(getsockopt(socket, SOL_SOCKET, SO_TYPE, &socktype, &optlen) == -1)) {
    mrb_sys_fail(mrb, "getsockopt");
  }
  cares_ctx->family = ss.ss_family;
  struct ares_addrinfo_hints hints = {
    .ai_family = ss.ss_family,
    .ai_socktype = socktype
  };

  switch (ss.ss_family) {
    case AF_INET: {
      ares_getaddrinfo(cares_ctx->channel, node, service, &hints, mrb_ares_getaddrinfo_callback, cares_ctx);
    } break;
    case AF_INET6: {
      int v6_only = 0;
      optlen = sizeof(v6_only);
      getsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, &v6_only, &optlen);
      if (!v6_only) {
        hints.ai_family = AF_UNSPEC;
      }
      ares_getaddrinfo(cares_ctx->channel, node, service, &hints, mrb_ares_getaddrinfo_callback, cares_ctx);
    } break;
    default: {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "Not a IPv4 or IPv6 socket");
    }
  }

  return self;
}

static mrb_value
mrb_ares_timeout(mrb_state *mrb, mrb_value self)
{
  struct cares_ctx *cares_ctx = DATA_PTR(self);
  struct timeval tv;
  struct timeval *timeout = ares_timeout(cares_ctx->channel, NULL, &tv);
  if (!timeout)
    return mrb_nil_value();

  return mrb_float_value(mrb, (mrb_float) timeout->tv_sec + ((mrb_float) timeout->tv_usec / (mrb_float) USEC_PER_SEC));
}

static mrb_value
mrb_ares_process_fd(mrb_state *mrb, mrb_value self)
{
  struct cares_ctx *cares_ctx = DATA_PTR(self);

  mrb_value read_fd;
  mrb_value write_fd;
  mrb_get_args(mrb, "oo", &read_fd, &write_fd);

  ares_process_fd(cares_ctx->channel,
  (ares_socket_t) mrb_integer(mrb_convert_type(mrb, read_fd, MRB_TT_INTEGER, "Integer", "fileno")),
  (ares_socket_t) mrb_integer(mrb_convert_type(mrb, write_fd, MRB_TT_INTEGER, "Integer", "fileno")));

  return self;
}

void
mrb_mruby_async_io_gem_init(mrb_state* mrb)
{
#ifdef _WIN32
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (result != NO_ERROR)
    mrb_raise(mrb, E_RUNTIME_ERROR, "WSAStartup failed");
#endif
  if (unlikely(ares_library_init(ARES_LIB_INIT_ALL) == -1))
    mrb_raise(mrb, E_RUNTIME_ERROR, "c-ares library init failed");

  struct RClass *mrb_async_class, *mrb_async_ares_class;
  mrb_async_class = mrb_define_class(mrb, "Async", mrb->object_class);
#if defined(HAVE_IO_URING_H)
  mrb_define_const(mrb, mrb_async_class, "BACKEND", mrb_symbol_value(mrb_intern_lit(mrb, "io_uring")));
#elif defined(HAVE_POLL_H)
  mrb_define_const(mrb, mrb_async_class, "BACKEND", mrb_symbol_value(mrb_intern_lit(mrb, "poll")));
#elif defined(HAVE_SELECT_H)
  mrb_define_const(mrb, mrb_async_class, "BACKEND", mrb_symbol_value(mrb_intern_lit(mrb, "select")));
#else
  mrb_raise(mrb, E_RUNTIME_ERROR, "No Backend found");
#endif

  mrb_define_class_under(mrb, mrb_async_class, "IO", mrb->object_class);
  mrb_async_ares_class = mrb_define_class_under(mrb, mrb_async_class, "Ares", mrb->object_class);
  MRB_SET_INSTANCE_TT(mrb_async_ares_class, MRB_TT_CDATA);
  mrb_define_method(mrb,  mrb_async_ares_class, "initialize",   mrb_ares_new,         MRB_ARGS_NONE());
  mrb_define_method(mrb,  mrb_async_ares_class, "getaddrinfo",  mrb_ares_getaddrinfo, MRB_ARGS_REQ(3));
  mrb_define_method(mrb,  mrb_async_ares_class, "timeout",      mrb_ares_timeout,     MRB_ARGS_NONE());
  mrb_define_method(mrb,  mrb_async_ares_class, "process_fd",   mrb_ares_process_fd,  MRB_ARGS_REQ(2));
}

void
mrb_mruby_async_io_gem_final(mrb_state* mrb)
{
  ares_library_cleanup();
#ifdef _WIN32
  WSACleanup();
#endif
}