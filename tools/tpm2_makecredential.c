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

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include <sapi/tpm20.h>

#include "tpm2_options.h"
#include "log.h"
#include "files.h"
#include "tpm2_tool.h"
#include "tpm2_util.h"

typedef struct tpm_makecred_ctx tpm_makecred_ctx;
struct tpm_makecred_ctx {
    TPM_HANDLE rsa2048_handle;
    TPM2B_NAME object_name;
    char *out_file_path;
    TPM2B_PUBLIC public;
    TPM2B_DIGEST credential;
    struct {
        UINT8 e : 1;
        UINT8 s : 1;
        UINT8 n : 1;
        UINT8 o : 1;
    } flags;
};

static tpm_makecred_ctx ctx = {
    .object_name = TPM2B_EMPTY_INIT,
    .public = TPM2B_EMPTY_INIT,
    .credential = TPM2B_EMPTY_INIT,
};

static bool write_cred_and_secret(const char *path, TPM2B_ID_OBJECT *cred,
        TPM2B_ENCRYPTED_SECRET *secret) {

    bool result = false;

    FILE *fp = fopen(path, "w+");
    if (!fp) {
        LOG_ERR("Could not open file \"%s\" error: \"%s\"", path,
                strerror(errno));
        return false;
    }

    size_t items = fwrite(cred, sizeof(TPM2B_ID_OBJECT), 1, fp);
    if (items != 1) {
        LOG_ERR("writing credential to file \"%s\" failed, error: \"%s\"", path,
                strerror(errno));
        goto out;
    }

    items = fwrite(secret, sizeof(TPM2B_ENCRYPTED_SECRET), 1, fp);
    if (items != 1) {
        LOG_ERR("writing secret to file \"%s\" failed, error: \"%s\"", path,
                strerror(errno));
        goto out;
    }

    result = true;

out:
    fclose(fp);
    return result;
}

static bool make_credential_and_save(TSS2_SYS_CONTEXT *sapi_context)
{
    TPMS_AUTH_RESPONSE session_data_out;
    TSS2_SYS_RSP_AUTHS sessions_data_out;
    TPMS_AUTH_RESPONSE *session_data_out_array[1];

    TPM2B_NAME name_ext = TPM2B_TYPE_INIT(TPM2B_NAME, name);

    TPM2B_ID_OBJECT cred_blob = TPM2B_TYPE_INIT(TPM2B_ID_OBJECT, credential);

    TPM2B_ENCRYPTED_SECRET secret = TPM2B_TYPE_INIT(TPM2B_ENCRYPTED_SECRET, secret);

    session_data_out_array[0] = &session_data_out;
    sessions_data_out.rspAuths = &session_data_out_array[0];
    sessions_data_out.rspAuthsCount = 1;

    UINT32 rval = Tss2_Sys_LoadExternal(sapi_context, 0, NULL, &ctx.public,
            TPM_RH_NULL, &ctx.rsa2048_handle, &name_ext, &sessions_data_out);
    if (rval != TPM_RC_SUCCESS) {
        LOG_ERR("LoadExternal failed. TPM Error:0x%x", rval);
        return false;
    }

    rval = Tss2_Sys_MakeCredential(sapi_context, ctx.rsa2048_handle, 0,
            &ctx.credential, &ctx.object_name, &cred_blob, &secret,
            &sessions_data_out);
    if (rval != TPM_RC_SUCCESS) {
        LOG_ERR("MakeCredential failed. TPM Error:0x%x", rval);
        return false;
    }

    rval = Tss2_Sys_FlushContext(sapi_context, ctx.rsa2048_handle);
    if (rval != TPM_RC_SUCCESS) {
        LOG_ERR("Flush loaded key failed. TPM Error:0x%x", rval);
        return false;
    }

    return write_cred_and_secret(ctx.out_file_path, &cred_blob, &secret);
}

static bool on_option(char key, char *value) {

    UINT16 size;

    switch (key) {
    case 'e':
        size = sizeof(ctx.public);
        if (!files_load_bytes_from_path(value, (UINT8 *) &ctx.public, &size)) {
            return false;
        }
        ctx.flags.e = 1;
        break;
    case 's':
        ctx.credential.t.size = BUFFER_SIZE(TPM2B_DIGEST, buffer);
        if (!files_load_bytes_from_path(value, ctx.credential.t.buffer,
                                        &ctx.credential.t.size)) {
            return false;
        }
        ctx.flags.s = 1;
        break;
    case 'n':
        ctx.object_name.t.size = BUFFER_SIZE(TPM2B_NAME, name);
        if (tpm2_util_hex_to_byte_structure(value, &ctx.object_name.t.size,
                                            ctx.object_name.t.name) != 0) {
            return false;
        }
        ctx.flags.n = 1;
        break;
    case 'o':
        ctx.out_file_path = optarg;
        if (files_does_file_exist(ctx.out_file_path)) {
            return false;
        }
            ctx.flags.o = 1;
            break;
    }

    return true;
}

bool tpm2_tool_onstart(tpm2_options **opts) {

    const struct option topts[] = {
      {"encKey"  ,required_argument, NULL, 'e'},
      {"sec"     ,required_argument, NULL, 's'},
      {"name"    ,required_argument, NULL, 'n'},
      {"outFile" ,required_argument, NULL, 'o'},
      {NULL      ,no_argument      , NULL, '\0'}
    };

    *opts = tpm2_options_new("e:s:n:o:", ARRAY_LEN(topts), topts, on_option, NULL);

    return *opts != NULL;
}

int tpm2_tool_onrun(TSS2_SYS_CONTEXT *sapi_context, tpm2_option_flags flags) {

    UNUSED(flags);

    if (!ctx.flags.e || !ctx.flags.n || !ctx.flags.o || !ctx.flags.s) {
        LOG_ERR("Expected options e, n, o and s");
        return false;
    }

    return make_credential_and_save(sapi_context) != true;
}
