/*
 * Copyright (C) 2011-2015 goblinhack@gmail.com
 *
 * See the LICENSE file for license.
 */

#include "main.h"
#include "thing.h"
#include "tile.h"
#include "math_util.h"
#include "thing_shop.h"

typedef struct {
    thingp target;
    const char *reason;
    uint16_t priority;
    uint8_t hitter_killed_on_hitting:1;
    uint8_t hitter_killed_on_hit_or_miss:1;
} thing_possible_hit;

#define MAX_THING_POSSIBLE_HIT 16

static thing_possible_hit thing_possible_hits[MAX_THING_POSSIBLE_HIT];
static uint32_t thing_possible_hit_size;
static int collision_radius = 2;

/*
 * Add a thing to the list of things that could be hit on this attack.
 */
static void 
thing_possible_hit_add_hitter_killed_on_hitting_ (thingp target,
                                                  const char *reason,
                                                  int hitter_killed_on_hitting,
                                                  int hitter_killed_on_hit_or_miss)
{
    if (thing_possible_hit_size >= MAX_THING_POSSIBLE_HIT) {
        return;
    }

    thing_possible_hit *h = &thing_possible_hits[thing_possible_hit_size++];
    memset(h, 0, sizeof(*h));
    h->target = target;
    h->priority = tp_get_hit_priority(target->tp);
    h->hitter_killed_on_hitting = hitter_killed_on_hitting;
    h->hitter_killed_on_hit_or_miss = hitter_killed_on_hit_or_miss;
}

static void 
thing_possible_hit_add (thingp target, const char *reason)
{
    thing_possible_hit_add_hitter_killed_on_hitting_(target,
                                                     reason,
                                                     false,
                                                     false);
}

static void 
thing_possible_hit_add_hitter_killed_on_hitting (thingp target,
                                                 const char *reason)
{
    thing_possible_hit_add_hitter_killed_on_hitting_(target,
                                                     reason,
                                                     true,
                                                     false);
}

static void 
thing_possible_hit_add_hitter_killed_on_hit_or_miss (thingp target,
                                                 const char *reason)
{
    thing_possible_hit_add_hitter_killed_on_hitting_(target,
                                                     reason,
                                                     false,
                                                     true);
}

/*
 * Reset the list of things we can possibly hit.
 */
static void thing_possible_init (void)
{
    thing_possible_hit_size = 0;
}

/*
 * Find the thing with the highest priority to hit.
 */
static void thing_possible_hit_do (thingp hitter)
{
    thing_possible_hit *best = 0;
    uint32_t i;

    for (i = 0; i < thing_possible_hit_size; i++) {
        thing_possible_hit *cand = &thing_possible_hits[i];

        /*
         * Don't be silly and hit yourself.
         */
        if (cand->target == hitter) {
            continue;
        }

        /*
         * Skip things that aren't really hitable.
         */
        if (thing_is_animation(cand->target)            ||
            thing_is_cloud_effect(cand->target)         ||
            thing_is_weapon_carry_anim(cand->target)) {
            continue;
        }

        if (!best) {
            best = cand;
            continue;
        }

        if (cand->priority > best->priority) {
            /*
             * If this target is higher prio, prefer it.
             */
            best = cand;
        } else if (cand->priority == best->priority) {
            /*
             * If this target is closer, prefer it.
             */
            Double dist_best = DISTANCE(hitter->x, hitter->y,
                                        best->target->x, best->target->y);
            Double dist_cand = DISTANCE(hitter->x, hitter->y,
                                        cand->target->x, cand->target->y);

            if (dist_cand < dist_best) {
                best = cand;
            }
        }
//CON("hitter %s target 
//%s",thing_logname(hitter),thing_logname(cand->target));
    }

    if (best) {
//CON("hitter %s best %s and hitter_killed_on_hitting 
//%d",thing_logname(hitter),thing_logname(best->target), 
//best->hitter_killed_on_hitting);
        if (thing_hit(best->target, hitter, 0)) {
            if (best->hitter_killed_on_hitting) {
                thing_dead(hitter, 0, "hit");
            }
        } else if (best->hitter_killed_on_hit_or_miss) {
            thing_dead(hitter, 0, "hit");
        }
    }

    thing_possible_init();
}

/*
 * On the server, things move in jumps. Find the real position the client
 * will see so collisions look more accurate.
 */
void FASTMATH
thingp_get_interpolated_position (const thingp t, Double *x, Double *y)
{
    widp w = thing_wid(t);

    if (!wid_is_moving(w)) {
        *x = t->x;
        *y = t->y;

        thing_round(t, x, y);
        return;
    }

    Double wdx, wdy;
    Double dx = t->x - t->last_x;
    Double dy = t->y - t->last_y;

    wid_get_move_interpolated_progress(thing_wid(t), &wdx, &wdy);

    *x = t->last_x + (dx * wdx);
    *y = t->last_y + (dy * wdy);

    thing_round(t, x, y);
}

static FASTMATH uint8_t things_overlap (const thingp A, 
                               Double nx,
                               Double ny,
                               const thingp B)
{
    static tilep wall;
    static Double collision_map_large_x1;
    static Double collision_map_large_x2;
    static Double collision_map_large_y1;
    static Double collision_map_large_y2;
    static Double collision_map_player_sized_x1;
    static Double collision_map_player_sized_x2;
    static Double collision_map_player_sized_y1;
    static Double collision_map_player_sized_y2;

    /*
     * Find out the position of the thing on the client. On the server we move 
     * in jumps, but on the server we want the collision to be more accurate
     * so we use the amount of time passed to interpolate the thing position.
     */
    Double Ax, Ay;
    Double Bx, By;

    /*
     * If -1, -1 then we are looking at the current position.
     *
     * If not then we are just checking out a future position.
     */
    if ((nx == -1.0) && (ny == -1.0)) {
        thingp_get_interpolated_position(A, &Ax, &Ay);
        thingp_get_interpolated_position(B, &Bx, &By);
    } else {
        Ax = nx;
        Ay = ny;
        Bx = B->x;
        By = B->y;
    }

    /*
     * The tiles are considered to be 1 unit wide. However the actual pixels
     * of each tile include shadows. px1/px2 are the bounds and exclude the
     * shadows. So we need to scale up the px1/px2 bounds to 1 as if the 
     * shadow pixels were not there.
     */
    if (!wall) {
        wall = tile_find("wall1_0_0");
        if (!wall) {
            ERR("no wall for collisions");
        }

        tilep tile = tile_find("large-collision-map");
        if (!tile) {
            ERR("no tile for collisions");
        }

        collision_map_large_x1 = tile->px1;
        collision_map_large_x2 = tile->px2;
        collision_map_large_y1 = tile->py1;
        collision_map_large_y2 = tile->py2;

        tile = tile_find("medium-collision-map");
        if (!tile) {
            ERR("no tile for collisions");
        }

        collision_map_player_sized_x1 = tile->px1;
        collision_map_player_sized_x2 = tile->px2;
        collision_map_player_sized_y1 = tile->py1;
        collision_map_player_sized_y2 = tile->py2;
    }

    Double Apx1;
    Double Apx2;
    Double Apy1;
    Double Apy2;

    Double Bpx1;
    Double Bpx2;
    Double Bpy1;
    Double Bpy2;

    widp Aw = thing_wid(A);
    widp Bw = thing_wid(B);

    if (thing_is_wall(A) || thing_is_door(A)) {
        tilep tileA = wid_get_tile(Aw);

        Apx1 = tileA->px1;
        Apx2 = tileA->px2;
        Apy1 = tileA->py1;
        Apy2 = tileA->py2;

        Double tile_width = (Apx2 - Apx1);
        Double tile_height = (Apy2 - Apy1);

        Apy1 -= tile_height / 3.0;
        Apx2 += tile_width / 3.0;

    } else if (thing_is_collision_map_large(A)) {
        Apx1 = collision_map_large_x1;
        Apx2 = collision_map_large_x2;
        Apy1 = collision_map_large_y1;
        Apy2 = collision_map_large_y2;
    } else if (thing_is_collision_map_player_sized(A)) {
        Apx1 = collision_map_player_sized_x1;
        Apx2 = collision_map_player_sized_x2;
        Apy1 = collision_map_player_sized_y1;
        Apy2 = collision_map_player_sized_y2;
    } else {
        /*
         * Just use pixel and alpha values.
         */
        tilep tileA = wid_get_tile(Aw);

        Apx1 = tileA->px1;
        Apx2 = tileA->px2;
        Apy1 = tileA->py1;
        Apy2 = tileA->py2;
    }

    if (thing_is_wall(B) || thing_is_door(B)) {
        tilep tileB = wid_get_tile(Bw);

        Bpx1 = tileB->px1;
        Bpx2 = tileB->px2;
        Bpy1 = tileB->py1;
        Bpy2 = tileB->py2;

        Double tile_width = (Bpx2 - Bpx1);
        Double tile_height = (Bpy2 - Bpy1);

        Bpy1 -= tile_height / 3.0;
        Bpx2 += tile_width / 3.0;

    } else if (thing_is_collision_map_large(B)) {
        Bpx1 = collision_map_large_x1;
        Bpx2 = collision_map_large_x2;
        Bpy1 = collision_map_large_y1;
        Bpy2 = collision_map_large_y2;
    } else if (thing_is_collision_map_player_sized(B)) {
        Bpx1 = collision_map_player_sized_x1;
        Bpx2 = collision_map_player_sized_x2;
        Bpy1 = collision_map_player_sized_y1;
        Bpy2 = collision_map_player_sized_y2;
    } else {
        /*
         * Just use pixel and alpha values.
         */
        tilep tileB = wid_get_tile(Bw);

        Bpx1 = tileB->px1;
        Bpx2 = tileB->px2;
        Bpy1 = tileB->py1;
        Bpy2 = tileB->py2;
    }

    /*
     * Bit of a hack. We need bonepiles to be passable by ghosts, but easy to 
     * hit by missiles.
     */
    if (thing_is_projectile(A) &&
        (thing_is_monst(B)          ||
         thing_is_trap(B)           ||
         thing_is_shield(B)         ||
         thing_is_mob_spawner(B))) {

        Bpx1 = collision_map_large_x1;
        Bpx2 = collision_map_large_x2;
        Bpy1 = collision_map_large_y1;
        Bpy2 = collision_map_large_y2;
    }

    /*
     * Similar thing for sword things, we want them to hit targets easily.
     */
    if (thing_is_weapon_swing_effect(A) &&
        (thing_is_monst(B)          ||
         thing_is_player(B)         ||
         thing_is_shield(B)         ||
         thing_is_mob_spawner(B))) {

        Bpx1 = collision_map_large_x1;
        Bpx2 = collision_map_large_x2;
        Bpy1 = collision_map_large_y1;
        Bpy2 = collision_map_large_y2;
    }

    /*
     * We really only care about collision radius for large objects like 
     * sawblades. If we use walls here then because of the granularity of
     * movement of one block at a time, the blade detects a collision too
     * far from the wall. The solution would be to use smaller next hop
     * walks but that might cause problems in the client with speed 
     * calculations. Easier just to save radial collisions for monsters and 
     * the like.
     */
    if ((thing_collision_radius(A) > 0.0) || (thing_collision_radius(B) > 0.0)) {

        if (!thing_is_wall(A) && !thing_is_door(A) && !thing_is_action(A) && !thing_is_action_trigger(A) &&
            !thing_is_wall(B) && !thing_is_door(B) && !thing_is_action(B) && !thing_is_action_trigger(B)) { 

            if (Aw->first_tile) {
                Ax += ((Aw->first_tile->px1 + Aw->first_tile->px2) / 2.0);
                Ay += ((Aw->first_tile->py1 + Aw->first_tile->py2) / 2.0);
            }

            if (Bw->first_tile) {
                Bx += ((Bw->first_tile->px1 + Bw->first_tile->px2) / 2.0);
                By += ((Bw->first_tile->py1 + Bw->first_tile->py2) / 2.0);
            }

            Double dist = DISTANCE(Ax, Ay, Bx, By);
            if (dist < max(thing_collision_radius(A), thing_collision_radius(B))) {
                return (true);
            } else {
                return (false);
            }
        }
    }

    /*
     * Find the start of pixels in the tile.
     */
    Double Atlx = Ax + Apx1;
    Double Abrx = Ax + Apx2;
    Double Atly = Ay + Apy1;
    Double Abry = Ay + Apy2;

    Double Btlx = Bx + Bpx1;
    Double Bbrx = Bx + Bpx2;
    Double Btly = By + Bpy1;
    Double Bbry = By + Bpy2;

    /*
     * The rectangles don't overlap if one rectangle's minimum in some 
     * dimension is greater than the other's maximum in that dimension.
     */
    if ((Atlx < Bbrx) && 
        (Abrx > Btlx) &&
        (Atly < Bbry) && 
        (Abry > Btly)) {

#if 0
    if ((thing_is_projectile(A) &&
         thing_is_wall(B))) {
CON("    A %s %f %f %f %f",thing_logname(A),Atlx,Atly,Abrx,Abry);
CON("      %f %f",Ax,Ay);
CON("      %f %f %f %f",Apx1,Apy1,Apx2,Apy2);
CON("    B %s %f %f %f %f",thing_logname(B),Btlx,Btly,Bbrx,Bbry);
CON("      %f %f",Bx,By);
CON("      %f %f %f %f",Bpx1,Bpy1,Bpx2,Bpy2);
    }
CON("%s  thing_collision_radius %f",thing_logname(A),thing_collision_radius(A));
CON("%s  thing_collision_radius %f",thing_logname(B),thing_collision_radius(B));
#endif

        return (true);
    }

    return (false);
}

/*
 * handle a single collision between two things
 */
static void thing_handle_collision (thingp me, thingp it, 
                                    int32_t x, int32_t y,
                                    int32_t dx, int32_t dy)
{
    /*
     * Filter out boring things.
     */
    if (thing_is_dungeon_floor(it)          ||
        thing_is_world_floor(it)            ||
        thing_is_weapon_carry_anim(it)      ||
        thing_is_animation(it)) {

        if ((dx == 0) && (dy == 0)) {
            if (thing_is_player(me)) {
                if (thing_is_shop_floor(it)) {
                    /*
                     * Going into a shop.
                     */
                    shop_enter(me, it);
                } else if (me->in_shop_owned_by_thing_id) {
                    /*
                     * Still inside the shop?
                     */
                    if (!shop_inside(me)) {
                        shop_leave(me, it);
                    }
                }
            }
        }

        return;
    }

    /*
     * Weapon swing of monster should not hit other monsters.
     */
    thingp owner_it = thing_owner(it);
    thingp owner_me = thing_owner(me);

    if (owner_me) {
        if (thing_is_monst(owner_me) && thing_is_monst(it)) {
            if (thing_is_jesus(owner_me)) {
                /*
                * Let jesus get rid of monsters
                */
            } else if (!thing_is_shop_floor(owner_me)) {
                /*
                 * Allow shopkeepers to shoot monsters.
                 */
                return;
            }
        }
    }

    if (thing_is_dead_or_dying(it)) {
#if 0
if (debug) {
LOG("  dead or dying");
}
#endif
        return;
    }

    if (thing_has_left_level(it)) {
#if 0
if (debug) {
LOG("  no on lev");
}
#endif
        return;
    }

    /*
     * Need this or shields attack the player.
     */
    if ((owner_it == me) || (owner_me == it)) {
#if 0
if (debug) {
LOG("  owner");
}
#endif
        return;
    }

    /*
     * Do we overlap with something?
     */
    if (!things_overlap(me, -1.0, -1.0, it)) {
#if 0
CON("  no overlap %s vs %s",thing_logname(me), thing_logname(it));
#endif
        return;
    }

#if 0
CON("  overlap %s vs %s",thing_logname(me), thing_logname(it));
#endif

    if (!thing_is_teleport(me)        &&
        !thing_is_world_floor(me)     &&
        !thing_is_dungeon_floor(me)) {

        if (thing_is_teleport(it)) {
            #ifndef __amigaos4__
            //crash ! todo to fix!
            thing_reached_teleport(me, it);
            #endif
            return;
        }
    }

    /*
     * Lava does not attack lava
     */
    if (thing_is_lava(me) || thing_is_acid(me)) {
        if (thing_is_lava(it) || thing_is_acid(it)) {
            return;
        }

        /*
         * Allow weapon blasts to sail over lava
         */
        if (thing_is_projectile(it)) {
            return;
        }

        if (thing_is_levitating(it)) {
            return;
        }

        if (thing_is_fragile(it)         ||
            thing_is_combustable(it)) {
//CON("  %d",__LINE__);
            thing_possible_hit_add(it, "monst hit thing");
            return;
        }
//CON("%s vs %s",thing_logname(me), thing_logname(it));

        if (thing_is_player(it) ||
            thing_is_monst(it)) {
            thing_possible_hit_add(it, "burn attack");
            return;
        }

        return;
    }

    /*
     * Monster friendly fire?
     */
    if (thing_is_monst(me) && thing_is_monst(it)) {
        if (thing_is_jesus(me)) {
            /*
             * Let jesus get rid of monsters
             */
        } else if (!thing_is_shop_floor(me)) {
            /*
             * Allow shopkeepers to shoot monsters.
             */
            return;
        }
    }

    if (thing_is_player(me)) {
        /*
         * Players don't sin
         */
        if (thing_is_jesus(it) || 
            (owner_it && thing_is_jesus(owner_it))) {
            return;
        }

        /*
         * Player collects keys and other items
         */
        if (thing_is_treasure(it)               ||
            thing_is_weapon(it)                 ||
            thing_is_carryable(it)              ||
            thing_is_potion(it)                 ||
            thing_is_food(it)) {

            if (thing_is_bomb(it) && thing_is_awake(it)) {
                /*
                 * Don't collect ticking bombs.
                 */
            } else {
                thing_item_collect(me, it, thing_tp(it));

                thing_dead(it, me, "collected");
                return;
            }
        }

        /*
         * Open doors if you have a key.
         */
        if (thing_is_door(it)) {
            tpp tp;
            tp = thing_is_carrying_thing(me, tp_is_key);
            if (tp) {
                thing_used(me, tp);
                level_open_door(server_level, x, y);
                return;
            }
        }

        /*
         * Player bumped into something.
         */
        if (thing_is_shield(it)                ||
            thing_is_cloud_effect(it)) {
            /*
             * I'm hit!
             */
#if 0
if (debug) {
LOG("add poss me %s hitter %s",thing_logname(me), thing_logname(it));
}
#endif
//CON("  %d",__LINE__);
            thing_possible_hit_add(it, "player hit thing");
            return;
        }

        if (thing_is_exit(it)) {
            thing_reached_exit(me, it);
            return;
        }

        /*
         * An action trigger is only ever used to start an object off as the 
         * initiator of a collision.
         */
        if (thing_is_action_trigger_on_hero(it)) {
            /*
             * Giant sawblades should only activate on the center tile of 
             * collisions.
             */
            level_trigger_activate(server_level,
                                   it->data ? it->data->col_name: 0);
        }
    }

    if (thing_is_treasure_eater(me)) {
        if (thing_is_treasure(it)) {
//CON("  %d",__LINE__);
            thing_possible_hit_add(it, "monst ate thing");
            return;
        }
    }

    if (thing_is_jesus(me)) {
        if (thing_is_monst(it)) {
            thing_possible_hit_add(it, "monst hit thing");
            return;
        }

        return;
    }

    if (thing_is_monst(me)) {
        /*
         * Monster bumped into something.
         */
        if (thing_is_player(it)                ||
            thing_is_shield(it)                ||
            thing_is_cloud_effect(it)) {
            /*
             * I'm hit!
             */
#if 0
if (debug) {
LOG("add poss me %s hitter %s",thing_logname(me), thing_logname(it));
}
#endif
//CON("%d %s %s",__LINE__,thing_logname(me), thing_logname(it));
//CON("  %d",__LINE__);
            thing_possible_hit_add(it, "monst hit thing");
            return;
        }

        if (thing_is_action_trigger_on_monst(it)) {
            /*
             * Giant sawblades should only activate on the center tile of 
             * collisions.
             */
            if ((dx == 0) && (dy == 0)) {
                level_trigger_activate(server_level, 
                                    it->data ? it->data->col_name: 0);
            }
        }
    }

    /*
     * If spinning blades or moving wall hit something?
     */
    if ((thing_is_wall(me) || thing_is_sawblade(me)) && me->on_active_list) {

        if (thing_is_sawblade(me)) {
            /*
             * Sawblades are tricky. We want them to be covered in blood and 
             * to do that they need to polymorph and essentially die on the 
             * first hit.
             */
            if (thing_is_warm_blooded(it)) {
                uint16_t id = tp_to_id(me->tp);
                switch (id) {
                case THING_SAWBLADE1:
                case THING_SAWBLADE2:
                case THING_SAWBLADE3:
                case THING_SAWBLADE4:
                    thing_possible_hit_add_hitter_killed_on_hitting(
                                                    it, "projection hit thing");
                    return;

                default:
                    break;
                }
            }

        }

        /*
         * Allow things like death to walk unharmed through walls.
         */
        if (thing_is_ethereal(it)) {
            return;
        }

        /*
         * Wall is crushing something
         */
        if (thing_is_player(it)                 ||
            thing_is_treasure(it)               ||
            thing_is_potion(it)                 ||
            thing_is_food(it)                   ||
            thing_is_door(it)                   ||
            thing_is_cobweb(it)                 ||
            thing_is_stickyslime(it)            ||
            thing_is_mob_spawner(it)            ||
            thing_is_shield(it)                 ||
            thing_is_monst(it)) {
            /*
             * I'm hit!
             */
#if 0
if (debug) {
LOG("add poss me %s hitter %s",thing_logname(me), thing_logname(it));
}
#endif
//CON("%d %s %s",__LINE__,thing_logname(me), thing_logname(it));
//CON("  %d",__LINE__);
            thing_possible_hit_add(it, "object hit thing");
            return;
        }

        if (thing_is_action_trigger_on_wall(it)) {
            /*
             * Giant sawblades should only activate on the center tile of 
             * collisions.
             */
            level_trigger_activate(server_level, 
                                    it->data ? it->data->col_name: 0);
        }

        /*
         * Initially we have a wall sitting on a trigger. The wall is inactive
         * and doesn't do collision tests. The trigger might no be active 
         * either and so we n
         */
        if (thing_is_action_left(it)                ||
            thing_is_action_right(it)               ||
            thing_is_action_up(it)                  ||
            thing_is_action_down(it)) {

            if (level_trigger_is_activated(server_level, 
                                        it->data ? it->data->col_name : 0)) {
                level_trigger_move_thing(thing_tp(it), me);
            }
        }
    }

    /*
     * Explosion hit something?
     */
    if (thing_is_projectile(me)                 || 
        thing_is_explosion(me)) {

        /*
         * Don't let shopkeepers shoot their own wares when defending a shop
         */
        thingp owner_proj = thing_owner(me);
        if (owner_it && (owner_it == owner_proj)) {
            return;
        }

        if (owner_proj) {
            /*
             * Don't let jesus hit players
             */
            if (thing_is_jesus(owner_proj)) {
                if (thing_is_player(it)) {
                    return;
                }
            }

            /*
             * Don't let monsters shoot their own mob spawners.
             */
            if (thing_is_mob_spawner(it) ||
                thing_is_cobweb(it)      ||
                thing_is_stickyslime(it)) {
                if (thing_is_monst(me)) {
                    return;
                }
            }
        }

        if (thing_is_lava(it)) {
            /*
             * No hitting this.
             */
            return;
        } else if (thing_is_monst(it)           || 
                   thing_is_fragile(it)         ||
                   thing_is_trap(it)            ||
                   thing_is_shield(it)          ||
                   thing_is_combustable(it)     ||
                   thing_is_cobweb(it)          ||
                   thing_is_stickyslime(it)     ||
                   thing_is_mob_spawner(it)) {
            /*
             * Can projectiles hit these?
             */
//CON("%d %s %s",__LINE__,thing_logname(me), thing_logname(it));
            if (owner_me == it) {
                /*
                 * Don't hit your owner.
                 */
            } else {
                /*
                 * Weapon hits monster or generator
                 */
                if (thing_stats_get_hp(it) < thing_stats_get_total_damage(me)) {

                    thing_stats_modify_hp(me, -thing_stats_get_hp(it));

//CON("  %d",__LINE__);
                    thing_possible_hit_add(it, "projectile hit");
                } else {
//CON("  %d",__LINE__);
                    thing_possible_hit_add_hitter_killed_on_hit_or_miss(
                                                    it, "projectile hit");
                }
                return;
            }
        }

        if (thing_is_door(it)                   ||
            thing_is_player(it)                 ||
            thing_is_rock(it)                   ||
            thing_is_wall(it)                   ||
            thing_is_sawblade(it)) {
            if (owner_me == it) {
                /*
                 * Don't hit your owner.
                 */
                return;
            } else {
                /*
                 * Weapon hits monster or generator
                 */
//CON("  %d",__LINE__);
                thing_possible_hit_add_hitter_killed_on_hit_or_miss(
                                                it, "projectile hit");
                return;
            }
        }
    }

    /*
     * Poison cloud hit something?
     */
    if (thing_is_non_explosive_gas_cloud(me)) {

        if (thing_is_monst(it)                  || 
            thing_is_player(it)                 ||
            thing_is_mob_spawner(it)) {
            /*
             * Weapon hits monster or generator
             */
//CON("%d %s %s",__LINE__,thing_logname(me), thing_logname(it));
//CON("  %d",__LINE__);
            thing_possible_hit_add_hitter_killed_on_hit_or_miss(
                                            it, "projection hit thing");
            return;
        }
    }

    /*
     * Sword swing hits?
     */
    if (thing_is_weapon_swing_effect(me)) {

        if (thing_is_monst(it)                  || 
            thing_is_door(it)                   ||
            thing_is_bomb(it)                   ||
            thing_is_player(it)                 ||
            thing_is_rock(it)                   ||
            thing_is_wall(it)                   ||
            /*
             * Don't hit walls. It's daft.
             */
            thing_is_cobweb(it)                 ||
            thing_is_stickyslime(it)            ||
            thing_is_mob_spawner(it)) {
            /*
             * Weapon hits monster or generator.
             */
//CON("%d %s %s",__LINE__,thing_logname(me), thing_logname(it));
//CON("  %d",__LINE__);
            thing_possible_hit_add_hitter_killed_on_hitting(
                                            it, "sword hit thing");
            return;
        }
    }

    /*
     * Shield hits a bad guy?
     */
    if (thing_is_shield(me)) {
        if (thing_is_monst(it) ||
            thing_is_mob_spawner(it)) {
            /*
             * Weapon hits monster or generator.
             */
#if 0
if (debug) {
LOG("add poss me %s hitter %s",thing_logname(me), thing_logname(it));
}
#endif
//CON("%d %s %s",__LINE__,thing_logname(me), thing_logname(it));
//CON("  %d",__LINE__);
            thing_possible_hit_add(it, "shield hit thing");
            return;
        }
    }
}

/*
 * Have we hit anything?
 */
void thing_handle_collisions (widp grid, thingp me)
{
    int32_t dx, dy;

    if (thing_has_left_level(me)) {
        return;
    }

#if 0
if (thing_is_powerup(me)) {
debug = 1;
LOG("  ");
LOG("  ");
LOG("--");
LOG("  ");
LOG("shield coll");
}
#endif
    thing_map *map = thing_get_map(me);

    for (dx = -collision_radius; dx <= collision_radius; dx++) 
    for (dy = -collision_radius; dy <= collision_radius; dy++) {
        int32_t x = (int32_t)me->x + dx;
        int32_t y = (int32_t)me->y + dy;

        if ((x < 0) || (y < 0) || (x >= MAP_WIDTH) || (y >= MAP_HEIGHT)) {
            continue;
        }

        thing_map_cell *cell = &map->cells[x][y];

        uint32_t i;
        for (i = 0; i < cell->count; i++) {
            thingp it;
            
            if (me->on_server) {
                it = thing_server_id(cell->id[i]);
            } else {
                it = thing_client_id(cell->id[i]);
            }
#if 0
if (debug) {
LOG("%d %d [%d] %s",x,y,i, thing_logname(it));
}
#endif

            if (me == it) {
                continue;
            }

            thing_handle_collision(me, it, x, y, dx, dy);
        }
    }

    thing_possible_hit_do(me);
}

/*
 * Have we hit anything?
 *
 * No opening of doors in here or other actions. This is just a check.
 */
uint8_t thing_hit_solid_obstacle (widp grid, thingp t, Double nx, Double ny)
{
    thingp me;
    widp wid_me;

    verify(t);
    wid_me = thing_wid(t);
    verify(wid_me);

    int32_t dx, dy;
    me = wid_get_thing(wid_me);
    thing_map *map = thing_get_map(t);

    /*
     * Death knows no bounds!
     */
    if (thing_is_death(t)) {
        return (false);
    }

    /*
     * Allow things like death to walk unharmed through walls.
     */
    if (thing_is_ethereal(t)) {
        return (false);
    }

    for (dx = -collision_radius; dx <= collision_radius; dx++) 
    for (dy = -collision_radius; dy <= collision_radius; dy++) {
        int32_t x = (int32_t)nx + dx;
        int32_t y = (int32_t)ny + dy;

        if ((x < 0) || (y < 0) || (x >= MAP_WIDTH) || (y >= MAP_HEIGHT)) {
            continue;
        }

        thing_map_cell *cell = &map->cells[x][y];

        uint32_t i;
        for (i = 0; i < cell->count; i++) {
            thingp it;
            
            if (t->on_server) {
                it = thing_server_id(cell->id[i]);
            } else {
                it = thing_client_id(cell->id[i]);
            }

            if (it == t) {
                continue;
            }

            verify(it);

            /*
             * No collisions with the floor!
             */
            if (thing_is_dungeon_floor(it)  ||
                thing_is_world_floor(it)    ||
                thing_is_action(it)         ||
                thing_is_animation(it)) {
                continue;
            }

            /*
             * Allow things like death to walk unharmed through walls.
             */
            if (thing_is_ethereal(it)) {
                continue;
            }

            /*
             * Allow dead ghosts to walk over each other!
             */
            if (thing_is_dead_or_dying(it)) {
                continue;
            }

            if (thing_is_monst(me)) {
                if (thing_is_player(it) &&
                    thing_get_weapon_carry_anim_wid(me)) {
                    /*
                     * If the monst has a weapon do not walk into the player 
                     * like a bite attack.
                     */
                    Double dist = DISTANCE(me->x, me->y, it->x, it->y);
                    if (dist > 0.5) {
                        /*
                         * Get close, enough to hit but not too close.
                         */
                        continue;
                    }

                } else if (thing_is_player(it)                 ||
                           thing_can_walk_through(it)          ||
                           thing_is_carryable(it)              ||
                           thing_is_weapon_swing_effect(it)    ||
                           thing_is_shield(it)                 ||
                           thing_is_explosion(it)              ||
                           thing_is_non_explosive_gas_cloud(it)||
                           thing_is_projectile(it)             ||
                           thing_is_treasure(it)               ||
                           thing_is_weapon(it)                 ||
                           thing_is_sawblade(it)               ||
                           thing_is_potion(it)                 ||
                           thing_is_teleport(it)               ||
                           thing_is_corpse(it)                 ||
                           thing_is_food(it)) {
                    /*
                     * Allow monsters to walk into these things:
                     */
                    continue;
                }

                if (thing_is_levitating(me)) {
                    if (thing_is_acid(it)    ||
                        thing_is_cobweb(it)  ||
                        thing_is_lava(it)    ||
                        thing_is_stickyslime(it)) {
                        /*
                         * Allow monsters to glide over these things:
                         */
                        continue;
                    }
                }
            }

            if (thing_is_projectile(me)                 ||
                thing_is_cloud_effect(me)               ||
                thing_is_shield(me)                     ||
                thing_is_weapon_swing_effect(me)) {
                /*
                 * Allow these to pass through anything.
                 */
                continue;
            }

            if (thing_is_player(me)) {
                /*
                 * Allow to walk through doors so we can open them later.
                 */
                if (thing_is_door(it)) {
                    if (thing_is_carrying_thing(me, tp_is_key)) {
                        continue;
                    } else {
                        if (!me->message_open_door_need_key) {
                            me->message_open_door_need_key = 1;
                            MSG_SERVER_SHOUT_AT(INFO, me,
                                                       0, 0,
                                                       "Collect keys to open doors");
                        }
                    }
                }

                /*
                 * Allow players to collect keys and other junk.
                 */
                if (thing_is_carryable(it)              ||
                    thing_can_walk_through(it)          ||
                    thing_is_potion(it)                 ||
                    thing_is_food(it)                   ||
                    thing_is_treasure(it)               ||
                    thing_is_weapon(it)                 ||
                    thing_is_exit(it)                   ||
                    thing_is_teleport(it)               ||
                    thing_is_lava(it)                   ||
                    thing_is_trap(it)                   ||
                    thing_is_monst(it)                  ||
                    /*
                     * Walk through friendly fire.
                     */
                    thing_is_projectile(it)             ||
                    thing_is_weapon_swing_effect(it)    ||
                    thing_is_shield_carry_anim(it)      ||
                    thing_is_weapon_carry_anim(it)      ||
                    thing_is_shield(it)                 ||
                    thing_is_explosion(it)              ||
                    thing_is_sawblade(it)               ||
                    thing_is_cloud_effect(it)) {
                    continue;
                }

                /*
                 * Allow players to walk through other players. Else thay
                 * can spawn on top of each other and get stuck.
                 */
                if (thing_is_player(it)) {
                    continue;
                }
            }

            if (thing_is_wall(me)     || 
                thing_is_sawblade(me) || 
                thing_is_juggernaut(me)) {
                /*
                 * Allow moving walls to crush most things except walls and 
                 * doors.
                 */
                if (!thing_is_wall(it) && 
                    !thing_is_rock(it) && 
                    !thing_is_door(it)) {
                    continue;
                }
            }

            if (!things_overlap(me, nx, ny, it)) {
                continue;
            }

            /*
             * You can walk closer to a cobweb, but not back out...
             */
            if (thing_is_cobweb(it) || thing_is_stickyslime(it)) {
                /*
                 * Got spider boots?
                 */
                tpp boots = thing_boots(me);
                if (boots) {
                    if (tp_is_spider_proof(boots)) {
                        continue;
                    }
                }

                Double dist_now = DISTANCE(t->x, t->y, it->x, it->y);
                Double dist_then = DISTANCE(nx, ny, it->x, it->y);

                /*
                 * No spiders stuck in their own web
                 */
                if ((tp_to_id(t->tp) == THING_SPIDER1) ||
                    (tp_to_id(t->tp) == THING_SPIDER2)) {
                    continue;
                }

                /*
                 * Allow floating things to glide over
                 */
                if (thing_is_levitating(t)) {
                    continue;
                }

                /*
                 * Else be sticky
                 */
                if (dist_then < dist_now) {
                    continue;
                } else {
                    return (true);
                }
            }

            return (true);
        }
    }

    return (false);
}

/*
 * Have we hit anything?
 *
 * Is there anything other than floor here
 */
uint8_t thing_hit_any_obstacle (widp grid, thingp t, Double nx, Double ny)
{
    thingp it;
    thingp me;
    widp wid_next;
    widp wid_me;
    widp wid_it;

    verify(t);
    wid_me = thing_wid(t);
    verify(wid_me);

    int32_t dx, dy;

    me = wid_get_thing(wid_me);

    uint8_t z;

    /*
     * Death knows no bounds!
     */
    if (thing_is_death(t)) {
        return (false);
    }

    for (dx = -collision_radius; dx <= collision_radius; dx++) 
    for (dy = -collision_radius; dy <= collision_radius; dy++)
    for (z = MAP_DEPTH_OBJ; z < MAP_DEPTH_MAX; z++) {
        int32_t x = (int32_t)nx + dx;
        int32_t y = (int32_t)ny + dy;

        if ((x < 0) || (y < 0) || (x >= MAP_WIDTH) || (y >= MAP_HEIGHT)) {
            continue;
        }

        wid_it = wid_grid_find_first(grid, x, y, z);
        while (wid_it) {
            verify(wid_it);

            wid_next = wid_grid_find_next(grid, wid_it, x, y, z);
            if (wid_me == wid_it) {
                wid_it = wid_next;
                continue;
            }

            it = wid_get_thing(wid_it);
            if (!it) {
                wid_it = wid_next;
                continue;
            }

            /*
             * No collisions with the floor!
             */
            if (thing_is_dungeon_floor(it) ||
                thing_is_world_floor(it)) {
                wid_it = wid_next;
                continue;
            }

            /*
             * Allow dead ghosts to walk over each other!
             */
            if (thing_is_dead_or_dying(it)) {
                wid_it = wid_next;
                continue;
            }

            if (!things_overlap(me, nx, ny, it)) {
                wid_it = wid_next;
                continue;
            }

            /*
             * You can walk closer to a cobweb, but not back out...
             */
            if (thing_is_cobweb(it) || thing_is_stickyslime(it)) {
                wid_it = wid_next;
                continue;
            }

            return (true);
        }
    }

    return (false);
}
