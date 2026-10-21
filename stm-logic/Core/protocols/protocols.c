#include "protocols.h"
#include "usart.h"
#include <string.h>

static void	put_u16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xFF);
	p[1] = (uint8_t)(v >> 8);
}

static void	put_u32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xFF);
	p[1] = (uint8_t)((v >> 8) & 0xFF);
	p[2] = (uint8_t)((v >> 16) & 0xFF);
	p[3] = (uint8_t)((v >> 24) & 0xFF);
}

uint8_t	protocols_crc8(const uint8_t *data, uint32_t len)
{
	uint8_t	crc;

	crc = 0x00;
	for (uint32_t i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (int b = 0; b < 8; b++)
		{
			crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
		}
	}
	return (crc);
}

void	protocols_pack_board(const uint8_t cells[200], uint8_t board[100])
{
	for (int i = 0; i < 200; i++)
	{
		if (i % 2 == 0)
			board[i / 2] = (uint8_t)(cells[i] << 4);
		else
			board[i / 2] |= (uint8_t)(cells[i] & 0x0F);
	}
}

int	protocols_hub_send(uint8_t type, const uint8_t *payload, uint8_t len)
{
	uint8_t		buf[3 + PROTO_MAX_PAYLOAD + 1];
	uint16_t	total;

	if (len > PROTO_MAX_PAYLOAD)
		return (PROTO_ERR_ARG);
	if (len > 0 && payload == NULL)
		return (PROTO_ERR_ARG);
	buf[0] = PROTO_SYNC;
	buf[1] = type;
	buf[2] = len;
	if (len > 0)
	{
		memcpy(&buf[3], payload, len);
	}
	buf[3 + len] = protocols_crc8(&buf[1], (uint32_t)len + 2);
	total = (uint16_t)(4 + len);
	if (HAL_UART_Transmit(&huart6, buf, total, HAL_MAX_DELAY) != HAL_OK)
	{
		return (PROTO_ERR_TX);
	}
	return (PROTO_OK);
}

int	protocols_send_join(uint8_t player, uint8_t card_type, const char name[16],
		uint32_t points)
{
	uint8_t	payload[22];

	payload[0] = player;
	payload[1] = card_type;
	if (name != NULL)
		memcpy(&payload[2], name, 16);
	else
		memset(&payload[2], 0, 16);
	put_u32(&payload[18], points);
	return (protocols_hub_send(PROTO_TYPE_JOIN, payload, sizeof(payload)));
}

int	protocols_send_state(const protocols_state_t *state)
{
	static uint8_t	seq = 0;
	uint8_t			payload[112];

	if (state == NULL)
		return (PROTO_ERR_ARG);
	payload[0] = state->player;
	payload[1] = seq++;
	memcpy(&payload[2], state->board, 100);
	payload[102] = state->next;
	payload[103] = state->hold;
	put_u32(&payload[104], state->score);
	put_u16(&payload[108], state->lines);
	payload[110] = state->level;
	payload[111] = state->garbage_wait;
	return (protocols_hub_send(PROTO_TYPE_STATE, payload, sizeof(payload)));
}

int	protocols_send_attack(uint8_t rows)
{
	return (protocols_hub_send(PROTO_TYPE_ATTACK, &rows, 1));
}

int	protocols_send_gameover(void)
{
	return (protocols_hub_send(PROTO_TYPE_GAMEOVER, NULL, 0));
}

int	protocols_send_skip_wait(void)
{
	return (protocols_hub_send(PROTO_TYPE_SKIP_WAIT, NULL, 0));
}
