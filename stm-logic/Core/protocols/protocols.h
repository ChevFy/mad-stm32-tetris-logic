#ifndef __PROTOCOLS_H__
# define __PROTOCOLS_H__

# ifdef __cplusplus
extern "C"
{
# endif

# include "main.h"

# define PROTO_SYNC 0xAAu
# define PROTO_MAX_PAYLOAD 112u

# define PROTO_TYPE_JOIN 0x01u
# define PROTO_TYPE_STATE 0x02u
# define PROTO_TYPE_ATTACK 0x03u
# define PROTO_TYPE_GAMEOVER 0x04u
# define PROTO_TYPE_SKIP_WAIT 0x05u
# define PROTO_TYPE_CREDIT 0x06u   /* RFID/pay status relayed to the Hub */
# define PROTO_TYPE_LEADERBOARD 0x07u  /* one leaderboard row relayed to Hub */
# define PROTO_TYPE_NEWRECORD 0x08u    /* member beat their high score */

# define PROTO_CREDIT_DENIED 0u    /* tap rejected: not enough balance */
# define PROTO_CREDIT_OK 1u        /* paid: balance sufficient */
# define PROTO_CREDIT_NO_CARD 2u   /* card not found in the database */

# define PROTO_OK 0
# define PROTO_ERR_ARG (-1)
# define PROTO_ERR_TX (-2)

	typedef struct
	{
		uint8_t		player;
		uint8_t		board[100];
		uint8_t		next;
		uint8_t		hold;
		uint32_t	score;
		uint16_t	lines;
		uint8_t		level;
		uint8_t		garbage_wait;
	} protocols_state_t;

	uint8_t protocols_crc8(const uint8_t *data, uint32_t len);
	void protocols_pack_board(const uint8_t cells[200], uint8_t board[100]);
	int protocols_hub_send(uint8_t type, const uint8_t *payload, uint8_t len);
	int protocols_send_join(uint8_t player, uint8_t card_type,
		const char name[16], uint32_t points);
	int protocols_send_state(const protocols_state_t *state);
	int protocols_send_attack(uint8_t rows);
	int protocols_send_gameover(void);
	int protocols_send_skip_wait(void);
	int protocols_send_credit(uint8_t status, int32_t balance, int32_t spent);
	int protocols_send_leaderboard(uint8_t rank, uint8_t count,
		uint32_t points, const char *name);
	int protocols_send_newrecord(uint32_t points);

# ifdef __cplusplus
}
# endif

#endif /* __PROTOCOLS_H__ */
