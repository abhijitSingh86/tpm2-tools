tpm2_dump_capability 1 "SEPTEMBER 2017" tpm2-tools
==================================================

NAME
----

tpm2_dump_capability(1) - Display TPM capabilities in a human readable form.

SYNOPSIS
--------

`tpm2_dump_capability` [OPTIONS]

DESCRIPTION
-----------

tpm2_dump_capability(1) - Query the TPM for it's capabilities / properties and
dump them to the console. This is a thin wrapper around the GetCapability
command.

OPTIONS
-------

  * `-c`, `--capability`=_CAPABILITY\_NAME_:
    The name of the capability group to query.
    Currently supported capability groups are:

    * properties-fixed:
      Display fixed TPM properties.

    * properties-variable:
      Display variable TPM properties.

    * algorithms:
      Display data about supported algorithms.

    * commands:
      Display data about supported commands.

[common options](common/options.md)

[common tcti options](common/tcti.md)

EXAMPLES
--------
```
tpm2_dump_capability --capability="properties-fixed"

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

