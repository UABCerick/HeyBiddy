#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "engine.h"
uint8_t platform_random_byte(void){ return (uint8_t)(rand() & 0xFF); }
static void pr(bool ok, const char *n, const char *d){ printf("%s %s%s%s\n", ok?"OK  ":"FAIL", n, d[0]?" · ":"", d); }
static int rnd(int n){ return rand()%n; }
int main(void){
  srand((unsigned)time(NULL));
  int tot, pass;
  eng_set_lang(LANG_ES); pass = run_tests(pr,&tot); printf("ES %d/%d\n",pass,tot);
  eng_set_lang(LANG_EN); pass = run_tests(NULL,&tot); printf("EN %d/%d\n",pass,tot);
  /* partidas aleatorias */
  static game_t g; int over=0; char buf[160]; int powers=0;
  for(int k=0;k<500;k++){
    army_t a[2]={{0},{0}}; for(int p=0;p<2;p++) for(int t=0;t<40;t++) army_add(&a[p], rnd(C_COUNT));
    game_new(&g,0,&a[0],&a[1]); auto_place_rest(&g);
    for(int steps=0; g.phase==PH_BATTLE && steps<5000; steps++){
      if(g.active_id<0){
        power_state_t ps[P_COUNT]; powers_for(&g,ps);
        int en[P_COUNT],ne=0; for(int i=0;i<P_COUNT;i++) if(ps[i].enabled) en[ne++]=i;
        if(ne && rnd(2)){ int id=en[rnd(ne)]; target_t t[MAX_TARGETS]; bool ok;
          if(id==P_HARDEN){ int n=power_targets(&g,P_HARDEN,t); ok=use_power(&g,P_HARDEN,t[rnd(n)].unit_id,0,0,NULL); }
          else if(id==P_RESUMMON){ int n=power_targets(&g,P_RESUMMON,t); int pick=-1; for(int i=0;i<n;i++) if(t[i].affordable) pick=i; if(pick<0) continue; target_t c[MAX_TARGETS]; resummon_cells(&g,c); ok=use_power(&g,P_RESUMMON,t[pick].unit_id,c[0].x,c[0].y,NULL); }
          else ok=use_power(&g,id,-1,0,0,NULL);
          if(!ok){ printf("power fail %d\n",id); return 1;} powers++; continue; }
        int ids[MAX_UNITS]; int n=selectable_units(&g,ids); activate(&g, ids[rnd(n)]); continue; }
      action_t acts[8]; int na=actions_for(&g,acts); int en[8],ne=0; for(int i=0;i<na;i++) if(acts[i].enabled && acts[i].id!=A_INSPECT) en[ne++]=i;
      action_t a=acts[en[rnd(ne)]];
      if(a.id==A_END){ end_activation(&g); continue; } if(a.id==A_DEFEND){ do_defend(&g); continue; }
      target_t t[MAX_TARGETS]; int nt=targets_for(&g,a.id,t); target_t tt=nt?t[rnd(nt)]:t[0];
      if(a.id==A_ABILITY && !special_is_attack(active_unit(&g)->cid)){ int x=nt?tt.x:0, y=nt?tt.y:0; if(!do_special(&g,x,y)){printf("special fail\n");return 1;} continue; }
      if(a.id==A_MOVE||a.id==A_RETREAT){ if(!do_move(&g,tt.x,tt.y,a.id==A_RETREAT)){printf("move fail\n");return 1;} continue; }
      attack_t p; if(!begin_attack(&g,a.id,tt.unit_id,&p)){printf("attack fail\n");return 1;}
      resolve_skill(&g,&p,d6_hw()); if(p.stage==1){ int r[2]={d6_hw(),d6_hw()}; resolve_damage(&g,&p,r); }
    }
    if(g.phase==PH_OVER) over++;
  }
  for(uint16_t s=g.seq-5; s<=g.seq; s++){ const event_t*e=game_event(&g,s); if(e){ event_text(e,buf,sizeof buf); printf("  ev: %s\n",buf);} }
  printf("partidas terminadas %d/500, poderes usados %d, sizeof(game_t)=%zu\n",over,powers,sizeof(game_t));
  return 0;
}
