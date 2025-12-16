/* deflate64_nif.c - Erlang NIF for Deflate64 decompression
 * Uses infback9 from zlib contrib for streaming decompression
 */

#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include "erl_nif.h"
#include "infback9.h"

#define WINDOW_SIZE 65536

/* Memory allocation functions required by zlib */
void *zcalloc(void *opaque, unsigned items, unsigned size) {
    (void)opaque;
    return calloc(items, size);
}

void zcfree(void *opaque, void *ptr) {
    (void)opaque;
    free(ptr);
}

/* Resource type for deflate64 state */
typedef struct {
    z_stream stream;
    unsigned char window[WINDOW_SIZE];
    int initialized;
} deflate64_state;

/* Context for callbacks */
typedef struct {
    unsigned char *in_data;
    size_t in_len;
    size_t in_pos;

    unsigned char *out_data;
    size_t out_capacity;
    size_t out_len;
} callback_ctx;

static ErlNifResourceType *deflate64_state_type = NULL;

/* Callback function for reading input data */
static unsigned inflateBack9_in(void *desc, unsigned char **buf) {
    callback_ctx *ctx = (callback_ctx *)desc;

    if (ctx->in_pos >= ctx->in_len) {
        return 0; /* No more input */
    }

    *buf = ctx->in_data + ctx->in_pos;
    unsigned available = ctx->in_len - ctx->in_pos;
    ctx->in_pos = ctx->in_len;

    return available;
}

/* Callback function for writing output data */
static int inflateBack9_out(void *desc, unsigned char *buf, unsigned len) {
    callback_ctx *ctx = (callback_ctx *)desc;

    if (len == 0) return 0;

    /* Expand output buffer if needed */
    size_t required = ctx->out_len + len;
    if (required > ctx->out_capacity) {
        size_t new_capacity = ctx->out_capacity * 2;
        if (new_capacity < required) new_capacity = required;

        unsigned char *new_buf = enif_alloc(new_capacity);
        if (new_buf == NULL) return 1; /* Error */

        memcpy(new_buf, ctx->out_data, ctx->out_len);
        enif_free(ctx->out_data);

        ctx->out_data = new_buf;
        ctx->out_capacity = new_capacity;
    }

    memcpy(ctx->out_data + ctx->out_len, buf, len);
    ctx->out_len += len;

    return 0;
}

/* NIF: deflate64_init() -> {ok, Resource} | {error, Reason} */
static ERL_NIF_TERM deflate64_init(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
    deflate64_state *state = enif_alloc_resource(deflate64_state_type, sizeof(deflate64_state));
    if (state == NULL) {
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "alloc_failed"));
    }

    memset(state, 0, sizeof(deflate64_state));

    /* Initialize z_stream */
    state->stream.zalloc = Z_NULL;
    state->stream.zfree = Z_NULL;
    state->stream.opaque = Z_NULL;

    int ret = inflateBack9Init(&state->stream, state->window);
    if (ret != Z_OK) {
        enif_release_resource(state);
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "init_failed"));
    }

    state->initialized = 1;

    ERL_NIF_TERM result = enif_make_resource(env, state);
    enif_release_resource(state); /* Erlang now owns it */

    return enif_make_tuple2(env, enif_make_atom(env, "ok"), result);
}

/* NIF: deflate64_inflate(Resource, Binary) -> {ok, Binary} | {error, Reason} */
static ERL_NIF_TERM deflate64_inflate(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
    deflate64_state *state;
    ErlNifBinary in_bin;

    if (!enif_get_resource(env, argv[0], deflate64_state_type, (void **)&state)) {
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "invalid_state"));
    }

    if (!state->initialized) {
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "not_initialized"));
    }

    if (!enif_inspect_binary(env, argv[1], &in_bin)) {
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "invalid_input"));
    }

    /* Setup callback context */
    callback_ctx ctx;
    ctx.in_data = in_bin.data;
    ctx.in_len = in_bin.size;
    ctx.in_pos = 0;

    /* Initial output buffer (will grow as needed) */
    ctx.out_capacity = in_bin.size * 2 + 1024;
    ctx.out_data = enif_alloc(ctx.out_capacity);
    ctx.out_len = 0;

    if (ctx.out_data == NULL) {
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "alloc_failed"));
    }

    /* Decompress */
    int ret = inflateBack9(&state->stream, inflateBack9_in, &ctx, inflateBack9_out, &ctx);

    if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
        enif_free(ctx.out_data);

        const char *msg = state->stream.msg ? state->stream.msg : "unknown error";
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_string(env, msg, ERL_NIF_LATIN1));
    }

    /* Create output binary */
    ErlNifBinary out_bin;
    if (!enif_alloc_binary(ctx.out_len, &out_bin)) {
        enif_free(ctx.out_data);
        return enif_make_tuple2(env,
            enif_make_atom(env, "error"),
            enif_make_atom(env, "alloc_failed"));
    }

    memcpy(out_bin.data, ctx.out_data, ctx.out_len);
    enif_free(ctx.out_data);

    ERL_NIF_TERM result = enif_make_binary(env, &out_bin);
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), result);
}

/* NIF: deflate64_end(Resource) -> ok */
static ERL_NIF_TERM deflate64_end(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
    deflate64_state *state;

    if (!enif_get_resource(env, argv[0], deflate64_state_type, (void **)&state)) {
        return enif_make_atom(env, "ok"); /* Already cleaned up */
    }

    if (state->initialized) {
        inflateBack9End(&state->stream);
        state->initialized = 0;
    }

    return enif_make_atom(env, "ok");
}

/* Resource destructor */
static void deflate64_state_dtor(ErlNifEnv *env, void *obj) {
    deflate64_state *state = (deflate64_state *)obj;
    if (state->initialized) {
        inflateBack9End(&state->stream);
        state->initialized = 0;
    }
}

/* NIF function table */
static ErlNifFunc nif_funcs[] = {
    {"nif_init", 0, deflate64_init, 0},
    {"nif_inflate", 2, deflate64_inflate, 0},
    {"nif_end", 1, deflate64_end, 0}
};

/* NIF module initialization */
static int load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info) {
    deflate64_state_type = enif_open_resource_type(
        env, NULL, "deflate64_state",
        deflate64_state_dtor,
        ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
        NULL);

    if (deflate64_state_type == NULL) {
        return -1;
    }

    return 0;
}

ERL_NIF_INIT(Elixir.Unzip.Deflate64, nif_funcs, load, NULL, NULL, NULL)
