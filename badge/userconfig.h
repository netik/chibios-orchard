#define CONFIG_SIGNATURE  0xdeadbeef  // duh
#define CONFIG_BLOCK      1
#define CONFIG_OFFSET     0
#define CONFIG_VERSION    0

#define CONFIG_NAME_MAXLEN   16

typedef struct userconfig {
  uint32_t  signature;
  uint32_t  version;
  uint32_t  won;
  uint32_t  lost;
  char      name[CONFIG_NAME_MAXLEN];
} userconfig;

void configStart(void);

const userconfig *getConfig(void);
void configFlush(void); // call on power-down to flush config state
void configLazyFlush(void);  // call periodically to sync state, but only when dirty
