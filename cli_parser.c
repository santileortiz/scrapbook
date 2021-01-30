/*
 * Copyright (C) 2019 Santiago León O.
 */

char* get_cli_arg_opt (char *opt, char **argv, int argc)
{
    char *arg = NULL;

    for (int i=1; arg==NULL && i<argc; i++) {
        if (strcmp (opt, argv[i]) == 0) {
            if (i+1 < argc) {
                arg = argv[i+1];
            } else {
                printf ("Expected argument for option %s.", opt);
            }
        }
    }

    return arg;
}

// This function looks for opt in the argv array and returns true if it finds it
// in it, and false otherwise.
bool get_cli_bool_opt (char *opt, char **argv, int argc)
{
    bool found = false;

    for (int i=1; found==false && i<argc; i++) {
        if (strcmp (opt, argv[i]) == 0) {
            found = true;
        }
    }

    return found;
}

char* get_cli_no_opt_arg (char **argv, int argc)
{
    static char *bool_opts[] = {"--write-output", "--unsafe"};
    char *arg = NULL;

    for (int i=1; arg==NULL && i<argc; i++) {
        if (argv[i][0] == '-') {
            bool found = false;
            for (int j=0; !found && j<ARRAY_SIZE(bool_opts); j++) {
                if (strcmp (bool_opts[j], argv[i]) == 0) {
                    found = true;
                }
            }

            // This option receives an argument skip it.
            if (!found) {
                i++;
            }

        } else {
            arg = argv[i];
        }
    }

    return arg;
}

