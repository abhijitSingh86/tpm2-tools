//**********************************************************************;
// Copyright (c) 2015, Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of Intel Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//**********************************************************************;

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <limits.h>
#include <ctype.h>

#include <sapi/tpm20.h>

#include "tpm2_options.h"
#include "tpm2_password_util.h"
#include "files.h"
#include "log.h"
#include "tpm2_tool.h"
#include "tpm2_util.h"

typedef struct tpm_encrypt_decrypt_ctx tpm_encrypt_decrypt_ctx;
struct tpm_encrypt_decrypt_ctx {
    TPMS_AUTH_COMMAND session_data;
    TPMI_YES_NO is_decrypt;
    TPMI_DH_OBJECT key_handle;
    TPM2B_MAX_BUFFER data;
    char *out_file_path;
    char *context_key_file;
    struct {
        UINT8 k : 1;
        UINT8 P : 1;
        UINT8 D : 1;
        UINT8 I : 1;
        UINT8 o : 1;
        UINT8 c : 1;
        UINT8 X : 1;
    } flags;
};

static tpm_encrypt_decrypt_ctx ctx = {
    .session_data = TPMS_AUTH_COMMAND_EMPTY_INIT,
    .is_decrypt = NO,
    .data = TPM2B_EMPTY_INIT,
};

static bool encrypt_decrypt(TSS2_SYS_CONTEXT *sapi_context) {

    TPM2B_MAX_BUFFER out_data = TPM2B_TYPE_INIT(TPM2B_MAX_BUFFER, buffer);

    TPM2B_IV iv_out = TPM2B_TYPE_INIT(TPM2B_IV, buffer);

    TSS2_SYS_CMD_AUTHS sessions_data;
    TPMS_AUTH_RESPONSE session_data_out;
    TSS2_SYS_RSP_AUTHS sessions_data_out;
    TPMS_AUTH_COMMAND *session_data_array[1];
    TPMS_AUTH_RESPONSE *session_data_out_array[1];

    session_data_array[0] = &ctx.session_data;
    sessions_data.cmdAuths = &session_data_array[0];
    session_data_out_array[0] = &session_data_out;
    sessions_data_out.rspAuths = &session_data_out_array[0];
    sessions_data_out.rspAuthsCount = 1;

    sessions_data.cmdAuthsCount = 1;
    sessions_data.cmdAuths[0] = &ctx.session_data;

    TPM2B_IV iv_in = {
        .t = {
            .size = MAX_SYM_BLOCK_SIZE,
            .buffer = { 0 }
        },
    };

    TPM_RC rval = Tss2_Sys_EncryptDecrypt(sapi_context, ctx.key_handle,
            &sessions_data, ctx.is_decrypt, TPM_ALG_NULL, &iv_in, &ctx.data, &out_data,
            &iv_out, &sessions_data_out);
    if (rval != TPM_RC_SUCCESS) {
        LOG_ERR("EncryptDecrypt failed, error code: 0x%x", rval);
        return false;
    }

    return files_save_bytes_to_file(ctx.out_file_path, (UINT8 *) out_data.t.buffer,
            out_data.t.size);
}

static bool on_option(char key, char *value) {

    bool result;

    switch (key) {
    case 'k':
        result = tpm2_util_string_to_uint32(value, &ctx.key_handle);
        if (!result) {
            LOG_ERR("Could not convert keyhandle to number, got: \"%s\"",
                    value);
            return false;
        }
        ctx.flags.k = 1;
        break;
    case 'P':
        result = tpm2_password_util_from_optarg(value, &ctx.session_data.hmac);
        if (!result) {
            LOG_ERR("Invalid object key password, got\"%s\"", value);
            return false;
        }
        ctx.flags.P = 1;
        break;
    case 'D':
        if (!strcasecmp("YES", value)) {
            ctx.is_decrypt = YES;
        } else if (!strcasecmp("NO", value)) {
            ctx.is_decrypt = NO;
        } else {
            LOG_ERR("Invalid operation type, got\"%s\"", value);
            return false;
        }
        break;
    case 'I':
        ctx.data.t.size = sizeof(ctx.data) - 2;
        result = files_load_bytes_from_path(value, ctx.data.t.buffer, &ctx.data.t.size);
        if (!result) {
            return false;
        }
        ctx.flags.I = 1;
        break;
    case 'o':
        result = files_does_file_exist(value);
        if (result) {
            return false;
        }
        ctx.out_file_path = value;
        ctx.flags.o = 1;
        break;
    case 'c':
        if (ctx.context_key_file) {
            LOG_ERR("Multiple specifications of -c");
            return false;
        }
        ctx.context_key_file = value;
        ctx.flags.c = 1;
        break;
    case 'S':
        result = tpm2_util_string_to_uint32(value, &ctx.session_data.sessionHandle);
        if (!result) {
            LOG_ERR("Could not convert session handle to number, got: \"%s\"",
                    value);
            return false;
        }
        break;
    }

    return true;
}

bool tpm2_tool_onstart(tpm2_options **opts) {

    const struct option topts[] = {
        {"keyHandle",   required_argument, NULL, 'k'},
        {"pwdk",        required_argument, NULL, 'P'},
        {"decrypt",     required_argument, NULL, 'D'},
        {"inFile",      required_argument, NULL, 'I'},
        {"outFile",     required_argument, NULL, 'o'},
        {"keyContext",  required_argument, NULL, 'c'},
        {"input-session-handle",1,         NULL, 'S'},
        {NULL,          no_argument,       NULL, '\0'}
    };

    ctx.session_data.sessionHandle = TPM_RS_PW;

    *opts = tpm2_options_new("k:P:D:I:o:c:S:", ARRAY_LEN(topts), topts, on_option, NULL);

    return *opts != NULL;
}

int tpm2_tool_onrun(TSS2_SYS_CONTEXT *sapi_context, tpm2_option_flags flags) {

    UNUSED(flags);
    bool result;

    if (!((ctx.flags.k || ctx.flags.c) && ctx.flags.I && ctx.flags.o)) {
        LOG_ERR("Invalid arguments");
        return 1;
    }

    if (ctx.flags.c) {
        result = files_load_tpm_context_from_file(sapi_context, &ctx.key_handle,
                                                  ctx.context_key_file);
        if (!result) {
            return 1;
        }
    }

    return encrypt_decrypt(sapi_context) != true;
}