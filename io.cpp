#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <string>
#include <iostream>
#include "io.h"
#include <vector>
#include <cstring>
#include <unistd.h>
#include "character.h"
#include "poke327.h"
#include "pokemon.h"
using namespace std;
typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

/**************************************************************************
 * Compares trainer distances from the PC according to the rival distance *
 * map.  This gives the approximate distance that the PC must travel to   *
 * get to the trainer (doesn't account for crossing buildings).  This is  *
 * not the distance from the NPC to the PC unless the NPC is a rival.     *
 *                                                                        *
 * Not a bug.                                                             *
 **************************************************************************/
static int compare_trainer_distance(const void *v1, const void *v2)
{
  const Character *const *c1 = (const Character *const *) v1;
  const Character *const *c2 = (const Character *const *) v2;

  return (world.rival_dist[(*c1)->pos[dim_y]][(*c1)->pos[dim_x]] -
          world.rival_dist[(*c2)->pos[dim_y]][(*c2)->pos[dim_x]]);
}

static Character *io_nearest_visible_trainer()
{
  Character **c, *n;
  uint32_t x, y, count;

  c = (Character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  n = c[0];

  free(c);

  return n;
}

void io_display()
{
  uint32_t y, x;
  Character *c;

  clear();
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.cur_map->cmap[y][x]) {
        mvaddch(y + 1, x, world.cur_map->cmap[y][x]->symbol);
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
        case ter_mountain:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, '%');
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_tree:
        case ter_forest:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '^');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_path:
        case ter_exit:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, '#');
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_mart:
          attron(COLOR_PAIR(COLOR_BLUE));
          mvaddch(y + 1, x, 'M');
          attroff(COLOR_PAIR(COLOR_BLUE));
          break;
        case ter_center:
          attron(COLOR_PAIR(COLOR_RED));
          mvaddch(y + 1, x, 'C');
          attroff(COLOR_PAIR(COLOR_RED));
          break;
        case ter_grass:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, ':');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_clearing:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '.');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, '0');
          attroff(COLOR_PAIR(COLOR_CYAN)); 
       }
      }
    }
  }

  mvprintw(23, 1, "PC position is (%2d,%2d) on map %d%cx%d%c.",
           world.pc.pos[dim_x],
           world.pc.pos[dim_y],
           abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_x] - (WORLD_SIZE / 2) >= 0 ? 'E' : 'W',
           abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_y] - (WORLD_SIZE / 2) <= 0 ? 'N' : 'S');
  mvprintw(22, 1, "%d known %s.", world.cur_map->num_trainers,
           world.cur_map->num_trainers > 1 ? "trainers" : "trainer");
  mvprintw(22, 30, "Nearest visible trainer: ");
  if ((c = io_nearest_visible_trainer())) {
    attron(COLOR_PAIR(COLOR_RED));
    mvprintw(22, 55, "%c at %d %c by %d %c.",
             c->symbol,
             abs(c->pos[dim_y] - world.pc.pos[dim_y]),
             ((c->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              'N' : 'S'),
             abs(c->pos[dim_x] - world.pc.pos[dim_x]),
             ((c->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              'W' : 'E'));
    attroff(COLOR_PAIR(COLOR_RED));
  } else {
    attron(COLOR_PAIR(COLOR_BLUE));
    mvprintw(22, 55, "NONE.");
    attroff(COLOR_PAIR(COLOR_BLUE));
  }

  io_print_message_queue(0, 0);

  refresh();
}

uint32_t io_teleport_pc(pair_t dest)
{
  /* Just for fun. And debugging.  Mostly debugging. */

  do {
    dest[dim_x] = rand_range(1, MAP_X - 2);
    dest[dim_y] = rand_range(1, MAP_Y - 2);
  } while (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]                  ||
           move_cost[char_pc][world.cur_map->map[dest[dim_y]]
                                                [dest[dim_x]]] == INT_MAX ||
           world.rival_dist[dest[dim_y]][dest[dim_x]] < 0);

  return 0;
}

static void io_scroll_trainer_list(char (*s)[40], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_trainers_display(Npc **c,
                                     uint32_t count)
{
  uint32_t i;
  char (*s)[40]; /* pointer to array of 40 char */

  s = (char (*)[40]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "You know of %d trainers:", count);
  mvprintw(4, 19, " %-40s ", s[0]);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], 40, "%16s %c: %2d %s by %2d %s",
             char_type_name[c[i]->ctype],
             c[i]->symbol,
             abs(c[i]->pos[dim_y] - world.pc.pos[dim_y]),
             ((c[i]->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              "North" : "South"),
             abs(c[i]->pos[dim_x] - world.pc.pos[dim_x]),
             ((c[i]->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              "West" : "East"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_trainer_list(s, count);
  }

  free(s);
}

static void io_list_trainers()
{
  Character **c;
  uint32_t x, y, count;

  c = (Character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  /* Display it */
  io_list_trainers_display((Npc **)(c), count);
  free(c);

  /* And redraw the map */
  io_display();
}

void io_pokemart()
{
  mvprintw(0, 0, "Welcome to the Pokemart.  Could I interest you in some Pokeballs?");
  refresh();
  getch();
}

void io_pokemon_center()
{
  mvprintw(0, 0, "Welcome to the Pokemon Center.  How can Nurse Joy assist you?");
  refresh();
  getch();
}

void io_choose_pokemon( uint32_t count){
  char (*s)[40]; /* pointer to array of 40 char */
  s = (char (*)[40]) malloc(count * sizeof (*s));
  mvprintw(3, 19, " %-40s ", "");
  Pokemon *p1 = new Pokemon(1);
   Pokemon *p2 = new Pokemon(1);
    Pokemon *p3 = new Pokemon(1);
   const char* p1_name = p1->get_species();
     const char* p2_name = p2->get_species();
     const char* p3_name = p3->get_species();
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "Choose one of the three random pokemon");
  mvprintw(4, 19, " %-40s ", s[0]);
    snprintf(s[0], 40, "Enter a, b, or c respectively");
  mvprintw(5, 19, " %-40s ", s[0]);
  snprintf(s[0], 40,"a. %s",p1_name);
  mvprintw(6,19, " %-40s ", s[0]);
  snprintf(s[0], 40, "b. %s",p2_name);
  mvprintw(7,19, " %-40s ", s[0]);
  snprintf(s[0], 40, "c. %s",p3_name);
  mvprintw(8,19, " %-40s ", s[0]);
  int key;
  do {
    switch (key = getch()) {
    case 'a':
      snprintf(s[0], 40,"You choose %s", p1_name);
  mvprintw(9,19, " %-40s ", s[0]);
  world.pc.pokemon_list.push_back(*p1);
  break;
    case 'b':
      snprintf(s[0], 40,"You choose %s", p2_name);
  mvprintw(9,19, " %-40s ", s[0]);
  
  world.pc.pokemon_list.push_back(*p2);
  break;
    case 'c':
      snprintf(s[0], 40,"You choose %s", p3_name);
  mvprintw(9,19, " %-40s ", s[0]);
  world.pc.pokemon_list.push_back(*p3);
  break;
  }
  }
  while(key!=97&&key!=98&&key!=99);
  snprintf(s[0], 40,"Hit esc to continue");
  mvprintw(10,19, " %-40s ", s[0]);
  while(getch()!=27);
  free(s);
}

void io_pokemon(){
  io_choose_pokemon(9);
  io_display();
}

void io_battle(Character *aggressor, Character *defender)
{
  Npc *npc;
  io_wildpokemon_battle(aggressor->pokemon_list.at(0),9);
  io_display();
  mvprintw(0, 0, "Aww, how'd you get so strong?  You and your pokemon must share a special bond!");
  refresh();
  getch();
  if (!(npc = dynamic_cast<Npc *>(aggressor))) {
    npc = dynamic_cast<Npc *>(defender);
  }
  
  npc->defeated = 1;
  if (npc->ctype == char_hiker || npc->ctype == char_rival) {
    npc->mtype = move_wander;
  }
}



uint32_t move_pc_dir(uint32_t input, pair_t dest)
{
  dest[dim_y] = world.pc.pos[dim_y];
  dest[dim_x] = world.pc.pos[dim_x];

  switch (input) {
  case 1:
  case 2:
  case 3:
    dest[dim_y]++;
    break;
  case 4:
  case 5:
  case 6:
    break;
  case 7:
  case 8:
  case 9:
    dest[dim_y]--;
    break;
  }
  switch (input) {
  case 1:
  case 4:
  case 7:
    dest[dim_x]--;
    break;
  case 2:
  case 5:
  case 8:
    break;
  case 3:
  case 6:
  case 9:
    dest[dim_x]++;
    break;
  case '>':
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_mart) {
      io_pokemart();
    }
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_center) {
      io_pokemon_center();
    }
    break;
  }

  if ((world.cur_map->map[dest[dim_y]][dest[dim_x]] == ter_exit) &&
      (input == 1 || input == 3 || input == 7 || input == 9)) {
    // Exiting diagonally leads to complicated entry into the new map
    // in order to avoid INT_MAX move costs in the destination.
    // Most easily solved by disallowing such entries here.
    return 1;
  }

  if (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) {
    if (dynamic_cast<Npc *>(world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) &&
        ((Npc *) world.cur_map->cmap[dest[dim_y]][dest[dim_x]])->defeated) {
      // Some kind of greeting here would be nice
      return 1;
    } else if (dynamic_cast<Npc *>
               (world.cur_map->cmap[dest[dim_y]][dest[dim_x]])) {
      io_battle(&world.pc, world.cur_map->cmap[dest[dim_y]][dest[dim_x]]);
      // Not actually moving, so set dest back to PC position
      dest[dim_x] = world.pc.pos[dim_x];
      dest[dim_y] = world.pc.pos[dim_y];
    }
  }
  
  if (move_cost[char_pc][world.cur_map->map[dest[dim_y]][dest[dim_x]]] ==
      INT_MAX) {
    return 1;
  }

  return 0;
}




int pokemon_move(Pokemon wild, Pokemon current, int move){
  int acc = rand()%100;
  if(acc<80){
    int damage = 2* wild.get_level();
    damage /= 5;
    damage +=2;
    damage *= wild.get_atk();
    damage /= current.get_def();
    damage /=50;
    damage +=2;
    damage *= 1.5;
    current.sub_hp(damage);
    return damage;
  }
  else{
    return 0;
  }
}


int  pokemon_move(Pokemon wild, Pokemon current){
  int acc = rand()%100;
  if(acc<80){
    int damage = 2* wild.get_level();
    damage /= 5;
    damage +=2;
    damage *= wild.get_atk();
    damage /= current.get_def();
    damage /=50;
    damage +=2;
    damage *= 1.5;
    current.sub_hp(damage);
    return damage;
  }
  else{
    return 0;
  }
}
void io_teleport_world(pair_t dest)
{
  int x, y;
  
  world.cur_map->cmap[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = NULL;

  mvprintw(0, 0, "Enter x [-200, 200]: ");
  refresh();
  echo();
  curs_set(1);
  mvscanw(0, 21, (char *) "%d", &x);
  mvprintw(0, 0, "Enter y [-200, 200]:          ");
  refresh();
  mvscanw(0, 21, (char *) "%d", &y);
  refresh();
  noecho();
  curs_set(0);

  if (x < -200) {
    x = -200;
  }
  if (x > 200) {
    x = 200;
  }
  if (y < -200) {
    y = -200;
  }
  if (y > 200) {
    y = 200;
  }
  
  x += 200;
  y += 200;

  world.cur_idx[dim_x] = x;
  world.cur_idx[dim_y] = y;

  new_map(1);
  io_teleport_pc(dest);
}

void io_encounter_pokemon()
{
  Pokemon *p;
  
  int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
            abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)));
  int minl, maxl;
  
  if (md <= 200) {
    minl = 1;
    maxl = md / 2;
  } else {
    minl = (md - 200) / 2;
    maxl = 100;
  }
  if (minl < 1) {
    minl = 1;
  }
  if (minl > 100) {
    minl = 100;
  }
  if (maxl < 1) {
    maxl = 1;
  }
  if (maxl > 100) {
    maxl = 100;
  }

  p = new Pokemon(rand() % (maxl - minl + 1) + minl);

  //  std::cerr << *p << std::endl << std::endl;

  io_queue_message("%s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d %s",
                   p->is_shiny() ? "*" : "", p->get_species(),
                   p->is_shiny() ? "*" : "", p->get_hp(), p->get_atk(),
                   p->get_def(), p->get_spatk(), p->get_spdef(),
                   p->get_speed(), p->get_gender_string());
  io_queue_message("%s's moves: %s %s", p->get_species(),
                   p->get_move(0), p->get_move(1));
  io_wildpokemon_battle(*p,9);
  io_display();
}

void io_wildpokemon_battle(Pokemon p, uint32_t count ){
   char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
  mvprintw(3, 19, " %-60s ", "");
  /* Borrow the first element of our array for this string: */
     snprintf(s[0], 60, "You are in a battle with a %s",p.get_species());
  mvprintw(4, 19, " %-60s ", s[0]);
  snprintf(s[0], 60,"%s%s%s: HP:%d ATK:%d DEF:%d",
                   p.is_shiny() ? "*" : "", p.get_species(),
                   p.is_shiny() ? "*" : "", p.get_hp(), p.get_atk(),
                   p.get_def());
  mvprintw(5,19, " %-60s ", s[0]);
  snprintf(s[0], 60, " SPATK:%d SPDEF:%d SPEED:%d %s", p.get_spatk(), p.get_spdef(),
	   p.get_speed(), p.get_gender_string());
  mvprintw(6, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "%s's moves: %s %s", p.get_species(),
	   p.get_move(0), p.get_move(1));
    mvprintw(7, 19, " %-60s ", s[0]);
    snprintf(s[0], 60, "\n");
    mvprintw(8, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "\n");
    mvprintw(9, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "\n");
    mvprintw(10, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "\n");
    mvprintw(11, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "\n");
    mvprintw(12, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "\n");
    mvprintw(13, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "Hit c to continue: ");
    mvprintw(14, 19, " %-60s ", s[0]);
    while(getch()!=99){};
  bool battleOver = false;
  Pokemon current = world.pc.pokemon_list.at(0);
  while(!battleOver){
    battleOver = move_progression(p, current);
    io_display();
  }
  free(s);
}

void display_bag(){

 uint32_t count=9;
    char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
  snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Here is the contents of you bag:");
  mvprintw(4, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Pokeballs: %d", world.pc.pokeballs);
  mvprintw(5, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Potions: %d",world.pc.potions);
  mvprintw(6, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Revives: %d",world.pc.revives);
  mvprintw(7, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Please enter esc to close");
  mvprintw(8, 19, " %-60s ", s[0]);
  int key = getch();
  while(key!=27){
    key= getch();
  }
  io_display();
  getch();
}

void bag_move(Pokemon p, Pokemon current){
  uint32_t count=9;
    char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
  snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Here is the contents of you bag:");
  mvprintw(4, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Pokeballs: %d", world.pc.pokeballs);
  mvprintw(5, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Potions: %d",world.pc.potions);
  mvprintw(6, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Revives: %d",world.pc.revives);
  mvprintw(7, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Please enter one of the following options: ");
  mvprintw(8, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "p: To use a potion on %s current hp is %d",current.get_species(), current.get_hp());
  mvprintw(9, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "r: To use a revive on one of your knocked out pokemon");
  mvprintw(10, 19, " %-60s ", s[0]);
  int key = getch();
  while(key!=112&&key!=114){
    key= getch();
  }
  if(key == 112 && world.pc.potions>0){
     current.sub_hp(-20);
     world.pc.potions--;
     snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
      int damage =  pokemon_move(p,current);
      if(damage ==0){
	snprintf(s[0], 60, " %s move missed",p.get_species());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage",p.get_species(),damage);
  mvprintw(4, 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
  mvprintw(5, 19, " %-60s ", s[0]);
  key=getch();
  while(key!= 99){
    key=getch();
  }
    }
  if(key == 114 && world.pc.revives > 0 && current.get_hp() == 0){
    current.sub_hp(-20);
     world.pc.revives--;
     snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
       int damage =  pokemon_move(p,current);
      if(damage ==0){
	snprintf(s[0], 60, " %s move missed",p.get_species());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage",p.get_species(),damage);
  mvprintw(4, 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
  mvprintw(5, 19, " %-60s ", s[0]);
  key=getch();
    while(key!= 99){key=getch();}
  
    }
  
}



void move(Pokemon p, Pokemon current){
    uint32_t count=9;
    char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
  snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Here is your pokemons different moves select with a or b:");
  mvprintw(4, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "a: %s", current.get_move(0));
  mvprintw(5, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "b: %s",current.get_move(1));
  mvprintw(6, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Please enter one of the following options: ");
  mvprintw(7, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "\n");
  mvprintw(8, 19, " %-60s ", s[0]);
  mvprintw(9, 19, " %-60s ", s[0]);
  mvprintw(10, 19, " %-60s ", s[0]);
  int key = getch();
  while(key!=98 && key!=97){
    key= getch();
  }
  if(key == 97){
     snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
      int damage =  pokemon_move(current,p,0);
      if(damage ==0){
	snprintf(s[0], 60, "your move %s missed",current.get_move(0));
  mvprintw(4, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage hp:%d",current.get_species(),damage,p.get_hp());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
   snprintf(s[0], 60, "\n");
    mvprintw(5, 19, " %-60s ", s[0]);
     mvprintw(6, 19, " %-60s ", s[0]);
      mvprintw(7, 19, " %-60s ", s[0]);


  damage =  pokemon_move(p,current);
      if(damage ==0){
	snprintf(s[0], 60, " %s move missed",p.get_species());
  mvprintw(8, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage hp:%d",p.get_species(),damage,current.get_hp());
  mvprintw(8, 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
  mvprintw(9, 19, " %-60s ", s[0]);
  key = getch();
  while(key!= 99){
    key = getch();
  }
    }
  
  else {
   snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
      int damage =  pokemon_move(current,p,1);
      if(damage ==0){
	snprintf(s[0], 60, "your move %s missed",current.get_move(1));
  mvprintw(4, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage hp: %d",current.get_species(),damage,p.get_hp());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
   snprintf(s[0], 60, "\n");
    mvprintw(5, 19, " %-60s ", s[0]);
     mvprintw(6, 19, " %-60s ", s[0]);
      mvprintw(7, 19, " %-60s ", s[0]);


  damage =  pokemon_move(p,current);
      if(damage ==0){
	snprintf(s[0], 60, " %s move missed",p.get_species());
  mvprintw(8, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage hp:%d",p.get_species(),damage,current.get_hp());
  mvprintw(8, 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
  mvprintw(9, 19, " %-60s ", s[0]);
  key = getch();
  while(key!= 99){
    key=getch();
  }
  
    }
  
}

void switch_pokemon(Pokemon wild, Pokemon current){
    uint32_t count=9;
    int key;
  char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
  int size = world.pc.pokemon_list.size();
    snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
  if(size >1){
     snprintf(s[0], 60, "Please select which pokemon you want to select");
      mvprintw(4, 19, " %-60s ", s[0]);
    for(int i=0;i<size;i++){
      snprintf(s[0], 60, "%d %s",i, world.pc.pokemon_list.at(i).get_species());
      mvprintw(5+i, 19, " %-60s ", s[0]);
    }
	 key = getch();
      while(key!=48&&key!=49){
	key = getch();
      }
      if(key==48){
	snprintf(s[0], 60, "Your new current pokemon is %s", world.pc.pokemon_list.at(0).get_species());
		  mvprintw(5+world.pc.pokemon_list.size(), 19, " %-60s ", s[0]);
      }else if(key == 49){
	snprintf(s[0], 60, "Your new current pokemon is %s", world.pc.pokemon_list.at(1).get_species());
		  mvprintw(5+world.pc.pokemon_list.size(), 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
	  mvprintw(6+world.pc.pokemon_list.size(), 19, " %-60s ", s[0]);
	  key=getch();
	  while(key!= 99){key=getch();}
  }
	 else{
	    snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
      snprintf(s[0], 60, "You only have one pokemon in you arsenal %s",current.get_species());
      mvprintw(4,19, " %-60s ", s[0]);
	        snprintf(s[0], 60, "You wasted your turn");
		mvprintw(5,19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Please enter c to continue to the next move");
	       mvprintw(6, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "/n");
	       mvprintw(7, 19, " %-60s ", s[0]);
	       mvprintw(8, 19, " %-60s ", s[0]);
	       mvprintw(9, 19, " %-60s ", s[0]);
	       key=getch();
	       while(key!= 99){ key = getch();}
	       
	 }
	       snprintf(s[0],60,"\n");
	       mvprintw(1,19," %-60s",s[0]);
	         mvprintw(2,19," %-60s",s[0]);
	         mvprintw(3,19," %-60s",s[0]);
		 
      int damage =  pokemon_move(wild,current);
      if(damage ==0){
	snprintf(s[0], 60, " %s move missed",wild.get_species());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage hp:%d",wild.get_species(),damage, current.get_hp());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
  mvprintw(5, 19, " %-60s ", s[0]);
    snprintf(s[0],60,"\n");
	       mvprintw(6,19," %-60s",s[0]);
	         mvprintw(7,19," %-60s",s[0]);
	         mvprintw(8,19," %-60s",s[0]);
		 key = getch();
		 while(key!= 99){key=getch();}
  
    }

	bool escape_attempt(Pokemon wild, Pokemon current){
	    uint32_t count=9;
	    int key;
	   char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
	       snprintf(s[0], 60, "\n");
    mvprintw(1, 19, " %-60s ", s[0]);
     mvprintw(2, 19, " %-60s ", s[0]);
      mvprintw(3, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "You are attempting to escape");
  mvprintw(4, 19, " %-60s ", s[0]);
     snprintf(s[0], 60, "\n");
    mvprintw(5, 19, " %-60s ", s[0]);
     mvprintw(6, 19, " %-60s ", s[0]);
      mvprintw(7, 19, " %-60s ", s[0]);
      int escape = rand()%256;
      if(escape<165){
  snprintf(s[0], 60, "You successfully escaped congragulations");
  mvprintw(8, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Please enter c to continue");
  mvprintw(9, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "\n");
  mvprintw(10, 19, " %-60s ", s[0]);
  key=getch();
  while(key!= 99){key=getch();}
  return true;
      }
      else{
  snprintf(s[0], 60, "You failed at escaping");
  mvprintw(8, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Please enter c to continue");
  mvprintw(9, 19, " %-60s ", s[0]);
  
  snprintf(s[0], 60, "\n");
  mvprintw(10, 19, " %-60s ", s[0]);
  key=getch();
  while(key!= 99){key=getch();}
  	       snprintf(s[0],60,"\n");
	       mvprintw(1,19," %-60s",s[0]);
	         mvprintw(2,19," %-60s",s[0]);
	         mvprintw(3,19," %-60s",s[0]);
		 
      int damage =  pokemon_move(wild,current);
      if(damage ==0){
	snprintf(s[0], 60, " %s move missed",wild.get_species());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
      else{
	snprintf(s[0], 60, " %s move hit for %d damage hp: %d",wild.get_species(),damage, current.get_hp());
  mvprintw(4, 19, " %-60s ", s[0]);
      }
  snprintf(s[0], 60, "Please enter c to continue to the next move");
  mvprintw(5, 19, " %-60s ", s[0]);
    snprintf(s[0],60,"\n");
	       mvprintw(6,19," %-60s",s[0]);
	         mvprintw(7,19," %-60s",s[0]);
	         mvprintw(8,19," %-60s",s[0]);
		 key=getch();	 
		 while(key!= 99){key=getch();}
  
  return false;
      }
	}


bool move_progression(Pokemon wild, Pokemon current){
    uint32_t count=9;
   char (*s)[60]; /* pointer to array of 40 char */
  s = (char (*)[60]) malloc(count * sizeof (*s));
 mvprintw(4, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Enter one of the following commands:");
  mvprintw(5, 19, " %-60s ", s[0]);
    snprintf(s[0], 60, "b for bag:");
  mvprintw(6, 19, " %-60s ", s[0]);
    snprintf(s[0], 60, "m for move with a current pokemon:");
  mvprintw(7, 19, " %-60s ", s[0]);
    snprintf(s[0], 60, "p to switch pokemon:");
  mvprintw(8, 19, " %-60s ", s[0]);
   snprintf(s[0], 60, "esc to attempt to escape the battle:");
  mvprintw(9, 19, " %-60s ", s[0]);
  snprintf(s[0], 60, "Your current pokemon is: %s", current.get_species());
  mvprintw(10, 19, " %-60s ", s[0]);
  int key= getch();
  while(key!=98 && key!= 109 && key!= 112 && key!= 27){
    key = getch();
  }
  if(key ==98){
    bag_move(wild, current);
    if(wild.get_hp()<0 || current.get_hp()<0)return true;
    else return false;
  }
  else if(key == 109){
    move(wild,current);
    if(wild.get_hp()<0){
      wild.sub_hp(-10);
      world.pc.pokemon_list.push_back(wild);
      snprintf(s[0], 60, "You captured the wild pokemon %s",wild.get_species());
      mvprintw(1,19, " %-60s ", s[0]);
         snprintf(s[0], 60, "Please enter c to continue");
      mvprintw(2,19, " %-60s ", s[0]);
         snprintf(s[0], 60, "\n");
      mvprintw(3,19, " %-60s ", s[0]);
      
      mvprintw(4,19, " %-60s ", s[0]);
      
      mvprintw(5,19, " %-60s ", s[0]);
      
      mvprintw(6,19, " %-60s ", s[0]);
      
      mvprintw(7,19, " %-60s ", s[0]);
      
      mvprintw(8,19, " %-60s ", s[0]);
      
      mvprintw(9,19, " %-60s ", s[0]);
      key = getch();
      while(key!=99){
	key=getch();
      }
      return true;
    }
    else if(current.get_hp()<0){
      return true;
    }
    else return false;
  }
  else if(key == 112){
    switch_pokemon(wild, current);
      if(wild.get_hp()<0 || current.get_hp()<0)return true;
    else return false;
  }
  else {
    bool escaped = escape_attempt(wild, current);
        if(escaped || current.get_hp()<0)return true;
	else return false;
  }
}




void io_handle_input(pair_t dest)
{
  uint32_t turn_not_consumed;
  int key;

  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      turn_not_consumed = move_pc_dir(7, dest);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      turn_not_consumed = move_pc_dir(8, dest);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      turn_not_consumed = move_pc_dir(9, dest);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      turn_not_consumed = move_pc_dir(6, dest);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      turn_not_consumed = move_pc_dir(3, dest);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      turn_not_consumed = move_pc_dir(2, dest);
      break;
    case '1':
    case 'b':
    case KEY_END:
      turn_not_consumed = move_pc_dir(1, dest);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      turn_not_consumed = move_pc_dir(4, dest);
      break;
    case '5':
    case ' ':
    case '.':
    case KEY_B2:
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case '>':
      turn_not_consumed = move_pc_dir('>', dest);
      break;
    case 'Q':
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      world.quit = 1;
      turn_not_consumed = 0;
      break;
      break;
    case 't':
      /* Teleport the PC to a random place in the map.              */
      io_teleport_pc(dest);
      turn_not_consumed = 0;
      break;
    case 'T':
      /* Teleport the PC to any map in the world.                   */
      io_teleport_world(dest);
      turn_not_consumed = 0;
      break;
    case 'm':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
      // case 'x':
      //io_pokemon();
      // refresh();
      // getch();
   break;
    case 'B':
      display_bag();
      break;
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matters, but using this command will   *
       * waste a turn.  Set turn_not_consumed to 1 and you should be *
       * able to figure out why I did it that way.                   */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      io_queue_message("Oh!  And use 'Q' to quit!");

      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      turn_not_consumed = 1;
    }
    refresh();
  } while (turn_not_consumed);
}
