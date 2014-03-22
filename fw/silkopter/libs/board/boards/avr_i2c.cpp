#include "Config.h"

#if BOARD_TYPE == CRIUS_AIOP2

#include <avr/io.h>
#include <avr/interrupt.h>
#include "board/boards/avr_i2c.h"
#include "board/clock.h"
#include "board/scheduler.h"
#include "debug/debug.h"

namespace board
{
namespace i2c
{

#ifndef F_CPU
#	define CPU_FREQ 16000000L
#else
#	define CPU_FREQ F_CPU
#endif

#define START           0x08
#define REPEATED_START  0x10
#define MT_SLA_ACK      0x18
#define MT_DATA_ACK     0x28
#define MR_SLA_ACK      0x40
#define MR_DATA_ACK     0x50
#define MR_DATA_NACK    0x58
#define TWI_STATUS      (TWSR & 0xF8)

#define SLA_W(address)  (address << 1)
#define SLA_R(address)  ((address << 1) + 0x01)

#define cbi(sfr, bit)   (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit)   (_SFR_BYTE(sfr) |= _BV(bit))

static bool s_is_initialized = false;

static uint16_t s_lockup_count = 0;
//static uint16_t s_timeout_delay_us = 1000;
static int16_t s_delay_loop_count = 30000;


static void _handle_lockup()
{
	TWCR = 0; /* Releases SDA and SCL lines to high impedance */
	TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA); /* Reinitialize TWI */
	s_lockup_count++;
}

static uint8_t _wait_for_interrupt()
{
	int16_t loop_count = s_delay_loop_count + 1; //to have at least one iteration
	// Wait while polling for timeout
	while (!(TWCR & _BV(TWINT)) && loop_count-- >= 0);

	if (loop_count <= 0)
	{
		_handle_lockup();
		return 1;
	}

	return 0;
}

static uint8_t _wait_for_stop()
{
	int16_t loop_count = s_delay_loop_count + 1; //to have at least one iteration
	// Wait while polling for timeout
	while ((TWCR & _BV(TWSTO)) && loop_count-- >= 0);

	if (loop_count <= 0)
	{
		_handle_lockup();
		return 1;
	}

	return 0;
}

static uint8_t _start() 
{
	TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
	auto stat = _wait_for_interrupt();
	if (stat) 
	{
		return stat;
	}

	return (TWI_STATUS == START || TWI_STATUS == REPEATED_START) ? 0 : TWI_STATUS;
}

static uint8_t _stop() 
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
	return _wait_for_stop();
}

static uint8_t _send_address(uint8_t addr) 
{
	TWDR = addr;
	TWCR = _BV(TWINT) | _BV(TWEN);
	return _wait_for_interrupt();
}

static uint8_t _send_byte(uint8_t data) 
{
	TWDR = data;
	TWCR = _BV(TWINT) | _BV(TWEN);
	auto stat = _wait_for_interrupt();
	if (stat) 
	{
		return stat;
	}

	return (TWI_STATUS == MT_DATA_ACK) ? 0 : TWI_STATUS;
}

static uint8_t _receive_byte_ack() 
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
	auto stat = _wait_for_interrupt();
	if (stat) 
	{
		return stat;
	}
	return TWI_STATUS;
}
static uint8_t _receive_byte() 
{
	TWCR = _BV(TWINT) | _BV(TWEN);
	auto stat = _wait_for_interrupt();
	if (stat) 
	{
		return stat;
	}
	return TWI_STATUS;
}


SIGNAL(TWI_vect)
{
	switch(TWI_STATUS) 
	{
	case 0x20:
	case 0x30:
	case 0x48:
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);  // send a stop
		break;
	case 0x38:
	case 0x68:
	case 0x78:
	case 0xB0:
		TWCR = 0;  //releases SDA and SCL lines to high impedance
		TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);  //reinitialize TWI
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
// PUBLIC API

void set_high_speed(bool active);

void init() 
{
	if (s_is_initialized)
	{
		return;
	}
	s_is_initialized = true;
	
	if (!lock(chrono::micros(10000)))
	{
		PANIC_MSG("Cannot lock i2c to initialize");
	}
	
    // activate internal pull-ups for twi
    // as per note from atmega128 manual pg204
    sbi(PORTD, 0);
    sbi(PORTD, 1);

    // initialize twi prescaler and bit rate
    cbi(TWSR, TWPS0);
    cbi(TWSR, TWPS1);

    // start in high speed. When a driver gets an error it drops it to
    // low speed
    set_high_speed(true);

    // enable twi module, acks, and twi interrupt
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
	
	unlock();
}

// void end() 
// {
//     TWCR = 0;
// }

static bool s_is_locked = false;
bool try_lock()
{
	scheduler::suspend();
	bool x = !s_is_locked;
	s_is_locked = true;
	scheduler::resume();
	return x;
}
bool lock(chrono::micros timeout /* = micros(0) */)
{
	bool locked = try_lock();
	if (!locked)
	{
		auto start = clock::now_us();
		do
		{
			locked = try_lock();
		} while (!locked && clock::now_us() - start <= timeout);
	}
	return locked;
}
void unlock()
{
	s_is_locked = false;
}
 
void set_high_speed(bool active) 
{
	ASSERT(s_is_initialized);
	ASSERT(s_is_locked);

    if (active) 
	{
        TWBR = ((CPU_FREQ / 400000) - 16) / 2;
    } 
	else 
	{
        TWBR = ((CPU_FREQ / 100000) - 16) / 2;
    }
}

uint8_t write(uint8_t addr, const uint8_t* data, uint8_t size)
{
	ASSERT(s_is_initialized);
	ASSERT(s_is_locked);
	ASSERT(data && size > 0);

    auto stat = _start();
    if (stat) goto error;
    stat = _send_address(SLA_W(addr));
    if (stat) goto error;
    for (int8_t i = size; i > 0; i--)
    {
        stat = _send_byte(*data++);
	    if (stat) goto error;
    }
    stat = _stop();
    if (stat) goto error;
    return stat;
error:
    s_lockup_count++;
    return stat;
}

uint8_t write_registers(uint8_t addr, uint8_t reg, const uint8_t* data, uint8_t size)
{
	ASSERT(s_is_initialized);
	ASSERT(s_is_locked);
	ASSERT(data && size > 0);

    auto stat = _start();
    if (stat) goto error;
    stat = _send_address(SLA_W(addr));
    if (stat) goto error;
    stat = _send_byte(reg);
    if (stat) goto error;
    for (int8_t i = size; i > 0; i--)
    {
        stat = _send_byte(*data++);
        if (stat) goto error;
    }
    stat = _stop();
    if (stat) goto error;
    return stat;
error:
//    if (!_ignore_errors) 
	{
		s_lockup_count++;
    }
    return stat;
}

uint8_t write_register(uint8_t addr, uint8_t reg, uint8_t val) 
{
    return write_registers(addr, reg, &val, 1);
}

uint8_t read(uint8_t addr, uint8_t* data, uint8_t size)
{
	ASSERT(s_is_initialized);
	ASSERT(s_is_locked);
	ASSERT(data && size > 0);
    if (!data || size == 0)
	{
		return 0;
	}
    uint8_t stat = _start();
    if (stat) goto error;
    stat = _send_address(SLA_R(addr));
    if (stat) goto error;
    for (int8_t i = size - 1; i > 0; i--) //write size - 1 butes
	{
        stat = _receive_byte_ack();
        if (stat != MR_DATA_ACK) goto error;
        *data++ = TWDR;
    }
    //last item
	{
        stat = _receive_byte();
        if (stat != MR_DATA_NACK) goto error;
        *data++ = TWDR;
    } 

    stat = _stop();
    if (stat) goto error;
    return 0;
error:
    s_lockup_count++;
    return stat;
}

uint8_t read_registers(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t size)
{
	ASSERT(s_is_initialized);
	ASSERT(s_is_locked);
	ASSERT(data && size > 0);
	if (!data || size == 0)
	{
		return 0;
	}
    uint8_t stat = _start();
    if (stat) goto error;
    stat = _send_address(SLA_W(addr));
    if (stat) goto error;
    stat = _send_byte(reg);
    if (stat) goto error;
    stat = _start();
    if (stat) goto error;
    stat = _send_address(SLA_R(addr));
    if (stat) goto error;
    for (int8_t i = size - 1; i > 0; i--) //write size - 1 butes
	{
        stat = _receive_byte_ack();
        if (stat != MR_DATA_ACK) goto error;
        *data++ = TWDR;
    }
    //last item
	{
        stat = _receive_byte();
        if (stat != MR_DATA_NACK) goto error;
        *data++ = TWDR;
    } 
    stat = _stop();
    if (stat) goto error;
    return 0;
error:
    s_lockup_count++;
    return stat;
}
uint8_t read_registers_le(uint8_t addr, uint8_t reg, uint16_t* data, uint8_t size)
{
	ASSERT(s_is_initialized);
	ASSERT(s_is_locked);
	ASSERT(data && size > 0);
	if (!data || size == 0)
	{
		return 0;
	}
	uint8_t* data_ptr8 = reinterpret_cast<uint8_t*>(data) + 1;
	//the data_ptr8 will advance like this
	//1, 0, 3, 2, 5, 4, 7, 6, 9, 8

	uint8_t stat = _start();
	if (stat) goto error;
	stat = _send_address(SLA_W(addr));
	if (stat) goto error;
	stat = _send_byte(reg);
	if (stat) goto error;
	stat = _start();
	if (stat) goto error;
	stat = _send_address(SLA_R(addr));
	if (stat) goto error;


	for (int8_t i = size - 1; i > 0; i--) //write size - 1 bytes
	{
		stat = _receive_byte_ack();
		if (stat != MR_DATA_ACK) goto error;
		*data_ptr8 = TWDR;
		data_ptr8 -= 1;

		stat = _receive_byte_ack();
		if (stat != MR_DATA_ACK) goto error;
		*data_ptr8 = TWDR;
		data_ptr8 += 3;
	}

	//last bytes
	{
		stat = _receive_byte_ack();
		if (stat != MR_DATA_ACK) goto error;
		*data_ptr8 = TWDR;
		data_ptr8 -= 1;

		stat = _receive_byte(); //last byte - no ack
		if (stat != MR_DATA_NACK) goto error;
		*data_ptr8 = TWDR;
		//data_ptr8 += 3;
	}

	stat = _stop();
	if (stat) goto error;
	return 0;
error:
	s_lockup_count++;
	return stat;
}

uint8_t read_register(uint8_t addr, uint8_t reg, uint8_t& data) 
{
    return read_registers(addr, reg, &data, 1);
}

uint16_t get_lockup_count()
{
	return s_lockup_count;
}

}
}

#endif
