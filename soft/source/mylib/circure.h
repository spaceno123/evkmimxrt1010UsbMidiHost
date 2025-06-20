/*
 * circure.h
 *
 *  Created on: 2024/09/20
 *      Author: M.Akino
 */

#ifndef CIRCURE_H_
#define CIRCURE_H_

typedef struct circure_ {
	uint16_t wpos;
	uint16_t rpos;
	uint16_t size;
	void *buf;
} circure_t;

static inline void circure_clear(circure_t *p)
{
	p->wpos = p->rpos = 0;
}

static inline int16_t circure_get(circure_t *p)
{
	int16_t ret = -1;

	if (p->wpos != p->rpos)
	{
		uint16_t rnxt = p->rpos + 1;
		uint8_t *buf = (uint8_t *)p->buf;

		ret = buf[p->rpos];
		p->rpos = rnxt >= p->size ? 0 : rnxt;
	}

	return ret;
}

static inline int32_t circure_getw(circure_t *p)
{
	int32_t ret = -1;

	if (p->wpos != p->rpos)
	{
		uint16_t rnxt = p->rpos + 1;
		uint16_t *buf = (uint16_t *)p->buf;

		ret = buf[p->rpos];
		p->rpos = rnxt >= p->size ? 0 : rnxt;
	}

	return ret;
}

static inline uint32_t circure_getl(circure_t *p)
{
	uint32_t ret = 0;

	if (p->wpos != p->rpos)
	{
		uint16_t rnxt = p->rpos + 1;
		uint32_t *buf = (uint32_t *)p->buf;

		ret = buf[p->rpos];
		p->rpos = rnxt >= p->size ? 0 : rnxt;
	}

	return ret;
}

static inline int16_t circure_put(circure_t *p, uint8_t dt)
{
	int16_t ret = -1;
	uint16_t wnxt = p->wpos + 1;

	wnxt = wnxt >= p->size ? 0 : wnxt;
	if (wnxt != p->rpos)
	{
		uint8_t *buf = (uint8_t *)p->buf;

		buf[p->wpos] = dt;
		p->wpos = wnxt;
		ret = dt;
	}

	return ret;
}

static inline int32_t circure_putw(circure_t *p, uint16_t dt)
{
	int32_t ret = -1;
	uint16_t wnxt = p->wpos + 1;

	wnxt = wnxt >= p->size ? 0 : wnxt;
	if (wnxt != p->rpos)
	{
		uint16_t *buf = (uint16_t *)p->buf;

		buf[p->wpos] = dt;
		p->wpos = wnxt;
		ret = dt;
	}

	return ret;
}

static inline bool circure_putl(circure_t *p, uint32_t dt)
{
	bool ret = false;
	uint16_t wnxt = p->wpos + 1;

	wnxt = wnxt >= p->size ? 0 : wnxt;
	if (wnxt != p->rpos)
	{
		uint32_t *buf = (uint32_t *)p->buf;

		buf[p->wpos] = dt;
		p->wpos = wnxt;
		ret = true;
	}

	return ret;
}

static inline int16_t circure_remain(circure_t *p)
{
	int16_t ret = p->wpos - p->rpos;

	if (ret < 0)
	{
		ret += p->size;
	}

	return ret;
}

#endif /* CIRCURE_H_ */
