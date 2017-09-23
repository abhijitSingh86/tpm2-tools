tpm2_nvwrite 1 "SEPTEMBER 2017" tpm2-tools
==================================================

NAME
----

tpm2_nvwrite(1) - Write data to a Non-Volatile (NV) index.

SYNOPSIS
--------

`tpm2_nvwrite` [OPTIONS]

DESCRIPTION
-----------

tpm2_nvwrite(1) - Write data to a Non-Volatile (NV) index.

OPTIONS
-------

  * `-x`, `--index`=_NV\_INDEX_:
    Specifies the index to define the space at.

  * `-a`, `--authHandle`=_SECRET\_DATA\_FILE_:
    specifies the handle used to authorize:
    * `0x40000001` for `TPM_RH_OWNER`
    * `0x4000000C` for `TPM_RH_PLATFORM`

  * `-P`, `--handlePasswd`=_HANDLE\_PASSWORD_:
    specifies the password of authHandle. Passwords should follow the
    "password formatting standards, see section "Password Formatting".

  * `-f`, `--file`=_FILE_:
    The data to write.

  * `-S`, `--input-session-handle`=_SIZE_:
    Optional Input session handle from a policy session for authorization.

[common options](common/options.md)

[common tcti options](common/tcti.md)

[password formatting](common/password.md)

EXAMPLES
--------

To write the file nv.data to index 0x150016:

```
tpm2_nvwrite -x 0x1500016 -a 0x40000001 -f nv.data
```

RETURNS
-------
0 on success or 1 on failure.

BUGS
----
[Github Issues](https://github.com/01org/tpm2-tools/issues)

HELP
----
See the [Mailing List](https://lists.01.org/mailman/listinfo/tpm2)