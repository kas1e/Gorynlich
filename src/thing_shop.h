/*
 * Copyright (C) 2011-2015 goblinhack@gmail.com
 *
 * See the LICENSE file for license.
 */

#include <SDL.h>
#include <math.h>

#include "main.h"
#include "thing.h"
#include "wid_game_map_server.h"
#include "map.h"
#include "level.h"
#include "time_util.h"

void shop_enter(thingp t, thingp floor);
void shop_leave(thingp t, thingp floor);
void shop_collect_message(thingp t, thingp item);
void shop_pay_for_items(thingp t);
void shop_deposit_message(thingp t, thingp item);
void shop_steal_message(thingp t);
void shop_attack_message(thingp t);
void shop_fixup(void);
void thing_shop_item_tick(thingp t);
void shop_break_message(thingp t, thingp shopkeeper);
void shop_whodunnit_break_message(thingp t, thingp shopkeeper);
int shop_inside(thingp t);
void shop_on_level(Double x, Double y);
