#if !defined(DISABLE_SSL)

#ifndef _BFI_OPENSSL_H
#define _BFI_OPENSSL_H

void run_openssltree(void);
#endif

#ifdef XX
X(openssl,OPENSSL,
    printf("   -fopenssl Run using OpenSSL bignums\n");			,
    /* No short option */						,
    /* No code dump */							,
    case c_openssl: run_openssltree(); break;                           )
#if XX == 4
    if (do_run == -1 && do_codestyle == c_default &&
            (cell_length > 256 ||(cell_length>32 && iostyle == 3)) &&
            cell_length<64*1024*1024 &&
            verbose<3 && !enable_trace && !debug_mode) {
        do_run = 1;
        do_codestyle = c_openssl;
    }
    if (do_codestyle == c_openssl && do_run == -1) do_run = 1;
#endif
#if XX == 9
    if (!strcmp(opt, "-fopenssl")) {
	do_codestyle = c_openssl;
	return 1;
    }
#endif
#endif

#endif
