#ifndef _PIT_H_
#define _PIT_H_

typedef void (*PIT_FUNC)(void);

typedef struct pit_driver {
	uint8_t *	pit_base;
	PIT_FUNC	pit_func;
} PITDriver;

#ifdef UPDATER
#define KINETIS_PIT_IRQ_VECTOR	38
#else
#define KINETIS_PIT_IRQ_VECTOR Vector98
#endif

#define CSR_READ_4(drv, addr)					\
        *(volatile uint32_t *)((drv)->pit_base + addr)

#define CSR_WRITE_4(drv, addr, data)				\
        do {							\
            volatile uint32_t *pReg =				\
                (uint32_t *)((drv)->pit_base + addr);		\
            *(pReg) = (uint32_t)(data);				\
        } while ((0))

extern PITDriver PIT1;

extern void pitStart (PITDriver *, PIT_FUNC);

#endif /* _PIT_H_ */
