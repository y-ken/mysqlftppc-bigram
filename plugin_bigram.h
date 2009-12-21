#ifdef __cplusplus
extern "C" {
#endif

#include <plugin.h>
static char* bigram_unicode_normalize;
static char* bigram_unicode_version;
static char  bigram_info[128];

static int bigram_unicode_normalize_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value);
static int bigram_unicode_version_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value);

static int bigram_parser_plugin_init(void *arg);
static int bigram_parser_plugin_deinit(void *arg);

static int bigram_parser_parse(MYSQL_FTPARSER_PARAM *param);
static int bigram_parser_init(MYSQL_FTPARSER_PARAM *param);
static int bigram_parser_deinit(MYSQL_FTPARSER_PARAM *param);

static MYSQL_SYSVAR_STR(normalization, bigram_unicode_normalize,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Set unicode normalization (OFF, C, D, KC, KD, FCD)",
  bigram_unicode_normalize_check, NULL, "OFF");

static MYSQL_SYSVAR_STR(unicode_version, bigram_unicode_version,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Set unicode version (3.2, DEFAULT)",
  bigram_unicode_version_check, NULL, "DEFAULT");

static struct st_mysql_show_var bigram_status[] = {
	{"Bigram_info", (char *)bigram_info, SHOW_CHAR},
	{NULL,NULL,SHOW_UNDEF}
};

static struct st_mysql_sys_var* bigram_system_variables[]= {
#if HAVE_ICU
	MYSQL_SYSVAR(normalization),
	MYSQL_SYSVAR(unicode_version),
#endif
	NULL
};

static struct st_mysql_ftparser bigram_parser_descriptor= {
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  bigram_parser_parse,              /* parsing function       */
  bigram_parser_init,               /* parser init function   */
  bigram_parser_deinit              /* parser deinit function */
};

mysql_declare_plugin(ft_bigram) {
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &bigram_parser_descriptor,  /* descriptor                      */
  "bigram",                   /* name                            */
  "Hiroaki Kawai",            /* author                          */
  "Bi-gram Full-Text Parser", /* description                     */
  PLUGIN_LICENSE_BSD,
  bigram_parser_plugin_init,  /* init function (when loaded)     */
  bigram_parser_plugin_deinit,/* deinit function (when unloaded) */
  0x0200,                     /* version                         */
  bigram_status,              /* status variables                */
  bigram_system_variables,    /* system variables                */
  NULL
}
mysql_declare_plugin_end;

#ifdef __cplusplus
}

class BigramBuffer : public FtCharBuffer {
	MYSQL_FTPARSER_PARAM *param;
	MYSQL_FTPARSER_BOOLEAN_INFO *info;
	FtMemPool* pool;
	FtMemBuffer *membuffer;
	size_t used;
	my_wc_t wcs[2];
	bool bigramQuote;
	char dummy;
	
	void tokenFlush();
public:
	BigramBuffer(MYSQL_FTPARSER_PARAM *param);
	~BigramBuffer();
	void append(my_wc_t wc);
	void flush();
	void reset();
	//
	void setInfo(MYSQL_FTPARSER_BOOLEAN_INFO *ptr);
};

#endif
