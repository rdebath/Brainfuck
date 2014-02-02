#define VERSION_MAJOR	0
#define VERSION_MINOR	9
#define VERSION_BUILD	577
#define VERSION_DATE	"2014-02-02 09:52:23+00:00"

#define VS_Q(x) #x
#define VS_S(x) VS_Q(x)
#define VS(x) VS_S(VERSION_ ## x)
#define VERSION VS(MAJOR) "." VS(MINOR) "." VS(BUILD)
