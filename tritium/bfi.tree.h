#ifndef CNF
#define DISABLE_GNULIGHTNING
#define DISABLE_TCCLIB
#define DISABLE_DLOPEN
#define DISABLE_DYNASM
#define DISABLE_SSL
#define DISABLE_GMP
#endif

#if defined(__GNUC__) \
    && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define UNUSED __attribute__ ((__unused__))

#if (_XOPEN_VERSION+0) < 500 && _POSIX_VERSION < 200809L
#if !defined(strdup) && !defined(_WIN32)
#define strdup(str) \
    ({char*_s=(str);int _l=strlen(_s)+1;void*_t=malloc(_l);if(_t)memcpy(_t,_s,_l);_t;})
#endif
#endif

#else
#define UNUSED
#ifndef __attribute__
#define __attribute__(__ignored__)
#endif
#endif

#ifndef __has_builtin         // Optional of course.
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers.
#endif

extern const char * bfname;
extern int noheader, enable_trace, hard_left_limit, memsize, most_neg_maad_loop;
extern int min_pointer, max_pointer;
extern int min_safe_mem, max_safe_mem;
extern int opt_level;
extern int iostyle, eofcell;
extern char * input_string;
extern int opt_regen_mov;
extern int only_uses_putch;

extern unsigned cell_length;
extern int cell_size;
extern int cell_mask;
extern int cell_smask;

#define UM(vx) ((vx) & cell_mask)
#define SM(vx) ((UM(vx) ^ cell_smask) - cell_smask)
#define XM(vx) (cell_size<=8?UM(vx):SM(vx))

#define TOKEN_LIST(Mac) \
    Mac(MOV) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    Mac(SET) Mac(CALC) Mac(CALCMULT) Mac(CHR) Mac(STR) \
    Mac(IF) Mac(ENDIF) Mac(MULT) Mac(CMULT) \
    Mac(ZFIND) Mac(MFIND) Mac(ADDWZ) Mac(LT) Mac(DIV) \
    Mac(CALC2) Mac(CALC3) Mac(CALC4) Mac(CALC5) \
    Mac(PRTI) Mac(INPI) Mac(STOP) Mac(FINI) Mac(DUMP) \
    Mac(NOP) Mac(DEAD) Mac(CALL) Mac(SUSP)

#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) TCOUNT};

struct string { int length; int maxlen; char buf[0]; };

struct bfi
{
    struct bfi *next;
    unsigned char type;
    unsigned char orgtype;

    int count;
    int offset;
    struct bfi *jmp;

    int count2;
    int offset2;
    int count3;
    int offset3;

    int line, col;
    int inum;
    int iprof;

    struct bfi *prev;
    struct string *str;
};

extern struct bfi *bfprog;
extern const char* tokennames[];
extern int node_type_counts[TCOUNT+1];
extern int total_nodes;
extern int max_indent;

/* How far to search for constants. */
#define SEARCHDEPTH     16
#define SEARCHRANGE     1000

void print_banner(FILE * fd, char const * program);
void calculate_stats(void);
void printtreecell(FILE * efd, int indent, struct bfi * n);
void delete_tree(void);
struct bfi * add_node_after(struct bfi * p);
void bf_basic_hack(void);

void
find_known_value(struct bfi * n, int v_offset, struct bfi ** n_found,
		int * const_found_p, int * known_value_p, int * unsafe_p);

