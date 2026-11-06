/* Textos de la interfaz en español e inglés. Los que llevan %d/%s son formatos de printf. */
#pragma once

typedef struct {
    const char *project, *clay, *working, *tagline, *tap_start;
    const char *menu[4], *menu_hint;
    const char *options, *lang, *lang_name, *haptic, *on, *off, *back, *opt_hint;
    const char *howto, *hold_exit;
    const char *team[2], *team_word[2];
    const char *priv_start[4], *priv_pass[4], *priv_reveal[4], *tap_ready;
    const char *army, *mana_left, *done, *army_summary, *army_empty, *seal, *turn_to_pick, *leftover;
    const char *role_cost, *stats[5], *passive, *ap, *builder_hint, *joins, *removed, *need_one;
    const char *reveal_mana, *tap_map;
    const char *pick_field, *soon, *play_here, *see_others, *later_map;
    const char *deploy, *places, *remaining, *deploy_hint1, *deploy_hint2;
    const char *turn, *round_of, *to_activate, *hold_pause;
    const char *summoner, *summoner_sub, *power_used, *boost_ready;
    const char *tap_choose, *tap_choose_other, *tap_confirm;
    const char *move_to, *retreat_to, *burns, *pick_mana, *n_of_m, *on_guard, *cover;
    const char *tap_back, *pause, *pause_items[2], *pause_note;
    const char *skill_die, *dmg_die, *vs, *no_mods, *crush_note;
    const char *charge, *tap_throw, *fair, *rolled, *next_critfail, *next_fail, *next_dmg;
    const char *hp, *no_damage, *minimum, *back_to_clay, *tap_continue;
    const char *wins, *draw, *r_elim, *r_points, *r_cost, *r_draw;
    const char *stat_rows[4], *rounds_rolls, *tap_menu;
    const char *power_title, *mana_cost, *mana_from, *power_hint;
    const char *choose_fallen, *returns_half, *pick_ally, *pick_cell;
    const char *f_captured, *f_contested, *f_mana, *f_atk;
    const char *rules[7][2];
    const char *tests_line;
    const char *power_cooling, *bd_hint, *bd_hint_back, *bd_skill, *bd_dmg, *bd_die, *bd_die_why, *bd_total, *bd_total_why, *bd_scale[6], *bd_num[6], *bd_eff[6];
    const char *odds_hit, *odds_dmg, *odds_kill, *max_charge, *map_size, *odds_chain, *chain_jumps, *chain_row, *passive_lbl, *hidden_lbl, *f_reveal, *f_wall, *tips[9];
} texts_t;

extern const texts_t TXT_ES, TXT_EN;
