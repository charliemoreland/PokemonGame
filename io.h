#ifndef IO_H
# define IO_H
class Pokemon;
class Character;
typedef int16_t pair_t[2];

void io_init_terminal(void);
void io_reset_terminal(void);
void io_display(void);
void io_handle_input(pair_t dest);
void io_queue_message(const char *format, ...);
void io_battle(Character *aggressor, Character *defender);
void io_encounter_pokemon(void);
void io_choose_pokemon(uint32_t count);
void io_pokemon(void);
void io_wildpokemon_battle(Pokemon p);
void io_wildpokemon_battle(Pokemon p,uint32_t count);
void bag_move(Pokemon p, Pokemon current);
bool move_progression(Pokemon wild, Pokemon current);
bool escape_attempt(Pokemon wild, Pokemon current);
void move(Pokemon p, Pokemon current);
#endif
