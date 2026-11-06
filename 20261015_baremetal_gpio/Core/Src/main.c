#include <stdint.h>
#include <stdbool.h>

#define RCC_BASE		0x40023800UL
#define GPIOA_BASE		0x40020000UL
#define GPIOB_BASE		0x40020400UL
#define GPIOC_BASE		0x40020800UL
#define GPIOD_BASE		0x40020C00UL

#define REG(addr)		(*(volatile uint32_t *)(addr))

#define RCC_AHB1ENR		REG(RCC_BASE + 0x30)

#define GPIOA_MODER		REG(GPIOA_BASE + 0x00)
#define GPIOA_OTYPER	REG(GPIOA_BASE + 0x04)
#define GPIOA_OSPEEDR	REG(GPIOA_BASE + 0x08)
#define GPIOA_PUPDR		REG(GPIOA_BASE + 0x0c)
#define GPIOA_IDR		REG(GPIOC_BASE + 0x10)
#define GPIOA_ODR		REG(GPIOA_BASE + 0x14)

#define GPIOC_MODER		REG(GPIOC_BASE + 0x00)
#define GPIOC_OTYPER	REG(GPIOC_BASE + 0x04)
#define GPIOC_OSPEEDR	REG(GPIOC_BASE + 0x08)
#define GPIOC_PUPDR		REG(GPIOC_BASE + 0x0c)
#define GPIOC_IDR		REG(GPIOC_BASE + 0x10)
#define GPIOC_ODR		REG(GPIOC_BASE + 0x14)

typedef struct {
	volatile uint32_t MODER;
	volatile uint32_t OTYPER;
	volatile uint32_t OSPEEDR;
	volatile uint32_t PUPDR;
	volatile uint32_t IDR;
	volatile uint32_t ODR;
	volatile uint32_t BSRR;
	volatile uint32_t LCKR;
	volatile uint32_t AFRL;
	volatile uint32_t AFRH;
} gpio_regs_t;

#define GPIOA	((gpio_regs_t *)GPIOA_BASE)
#define GPIOB	((gpio_regs_t *)GPIOB_BASE)
#define GPIOC	((gpio_regs_t *)GPIOC_BASE)
#define GPIOD	((gpio_regs_t *)GPIOD_BASE)

typedef enum {GPIO_MODE_IN, GPIO_MODE_OUT, GPIO_MODE_AF, GPIO_MODE_ANALOG} gpio_mode_t;
typedef enum {GPIO_PULL_NONE, GPIO_PULL_UP, GPIO_PULL_DOWN} gpio_pull_t;

void delay(volatile uint32_t n)
{
	while(n--);
}

void gpio_set_mode(gpio_regs_t *port, uint16_t pin, gpio_mode_t mode)
{
	uint32_t v = port->MODER;
	v &= ~(3 << (pin*2));
	v |= ((uint32_t)mode << (pin * 2));

	port->MODER = v;
}

void gpio_set_pull(gpio_regs_t *port, uint16_t pin, gpio_pull_t pull)
{
	uint32_t v = port->PUPDR;
	v &= ~(3 << (pin*2));
	v |= ((uint32_t)pull << (pin * 2));

	port->PUPDR = v;
}

void gpio_write(gpio_regs_t *port, uint16_t pin, bool level)
{
	port->BSRR = level ? (1 << pin) : (1 << (pin + 16));
}

bool gpio_read(const gpio_regs_t *port, uint16_t pin)
{
	return (port->IDR >> pin) & 1;
}

void gpio_toggle(gpio_regs_t *port, uint16_t pin)
{
	uint32_t mask = 1 << pin;
	uint32_t odr = port->ODR;
	port->BSRR = ((odr & mask) << 16) | (~odr & mask);
}


int main(void)
{
	// AHB1 RCC GPIOA clock enable
	// MODER: Output mode
	// OTYPER: pushpull 0
	// OSPEEDR: 01 Medium speed
	// PUPDR: 01 Pull-up

	RCC_AHB1ENR 	|= 	(1 << 0);
	RCC_AHB1ENR 	|= 	(1 << 2);

	GPIOA->MODER 	|= 	(1 << 10); 	// GPIOA Base Address, MODER
	GPIOA->MODER 	&= ~(1 << 11); 	// GPIOA Base Address, MODER
	GPIOA->OTYPER 	&= ~(1 << 5); 	// pushpull
	GPIOA->OSPEEDR 	|= 	(1 << 10); 	// Medium speed
	GPIOA->OSPEEDR 	&= ~(1 << 11); 	// Medium speed
	GPIOA->PUPDR 	|= 	(1 << 10); 	// Pull-up
	GPIOA->PUPDR 	&= ~(1 << 11); 	// Pull-up

	GPIOC->MODER		&= ~(1 << 26);	// GPIOA Base Address, MODER
	GPIOC->MODER		&= ~(1 << 27);	// GPIOA Base Address, MODER
	GPIOC->PUPDR		|= 	(1 << 26);	// Pull-up
	GPIOC->PUPDR		&= ~(1 << 27);	// Pull-up

	gpio_set_mode(GPIOA, 5, GPIO_MODE_OUT);
	gpio_set_mode(GPIOC, 13, GPIO_MODE_IN);
	gpio_set_pull(GPIOC, 13, GPIO_PULL_UP);

	while(1)
	{
		//GPIOA_ODR |= (1 << 5); // ODR 5bit set
		//delay(1000000);

		//GPIOA_ODR &= (1 << 5); // ODR 5bit reset
		//delay(1000000);

		if (gpio_read(GPIOC, 13) == 0) {
			gpio_write(GPIOA, 5, 1);
		}
		else {
			gpio_write(GPIOA, 5, 0);
		}
	}


	return 0;
}
