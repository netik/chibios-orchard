#define KW01_DELAY 1000
#define KW01_PKT_MAXLEN 64
#define KW01_PKT_HDRLEN 3
#define KW01_PKT_HANDLERS_MAX 10

/*
 * The packet header definition is not defined by the hardware. The
 * hardware mandates that the first byte in or out of the FIFO during
 * TX/RX is an 8-byte length field. The last byte is a CRC. Everything
 * else is defined by the user.
 */

typedef struct kw01_pkt_hdr {
	uint8_t		kw01_length;	/* Total frame length */
	uint8_t		kw01_src;	/* Source node */
	uint8_t		kw01_dst;	/* Destination node */
	uint8_t		kw01_prot;	/* Protocol type */
} KW01_PKT_HDR;

typedef struct kw01_pkt {
	uint8_t		kw01_rssi;	/* Signal strength reading */
	KW01_PKT_HDR	kw01_hdr;
	uint8_t		kw01_payload[KW01_PKT_MAXLEN - KW01_PKT_HDRLEN];
} KW01_PKT;

typedef void (*KW01_PKT_FUNC)(KW01_PKT *);

typedef struct kw01_pkt_handler {
	KW01_PKT_FUNC	kw01_handler;
	uint8_t		kw01_prot;
} KW01_PKT_HANDLER;

extern void radioStart (SPIDriver *);
extern void radioStop (SPIDriver *);
extern void radioInterrupt (EXTDriver *, expchannel_t);
extern uint8_t radioRead (uint8_t);
extern void radioWrite (uint8_t, uint8_t);
extern int radioDump (uint8_t addr, void *bfr, int count);
extern void radioAcquire (void);
extern void radioRelease (void);
extern void radioSend(uint8_t dest, uint8_t prot,
			size_t len, const void *payload);

extern int radioModeSet (uint8_t mode);
extern int radioFrequencySet (uint32_t freq);
extern int radioDeviationSet (uint32_t freq);
extern int radioBitrateSet (uint32_t freq);
extern uint32_t radioFrequencyGet (void);
extern uint32_t radioDeviationGet (void);
extern uint32_t radioBitrateGet (void);
extern uint8_t radioAddressGet (void);
extern void radioAddressSet (uint8_t);

extern void radioDefaultHandlerSet (KW01_PKT_FUNC);
extern void radioHandlerSet (uint8_t, KW01_PKT_FUNC);

