#include "startup.h"
#include "file.h"
#include "hardware.h"
#include "memory.h"
#include "ahx/ahx.h"
#include "interrupts.h"

static APTR module;

static void Load() {
  module = ReadFile("data/jazzcat-electric_city.ahx", MEMF_PUBLIC);
}

static void UnLoad() {
  MemFreeAuto(module);
}

__interrupt_handler void IntLevel2Handler() {
  if (custom->intreqr & INTF_PORTS) {
    /* Handle CIA Timer A interrupt. */
    if (ciaa->ciaicr & CIAICRF_TA) {
      custom->color[0] = 0xaaa;
      AhxInterrupt();
      custom->color[0] = 0;
    }
  }

  custom->intreq = INTF_PORTS;
}

static void AhxSetTempo(UWORD tempo asm("d0")) {
  ciaa->ciatalo = tempo & 0xff;
  ciaa->ciatahi = tempo >> 8;
  ciaa->ciacra |= CIACRAF_START;
}

static void Init() {
  /* Enable only CIA Timer A interrupt. */
  ciaa->ciaicr = (UBYTE)(~CIAICRF_SETCLR);
  ciaa->ciaicr = CIAICRF_SETCLR | CIAICRF_TA;
  /* Run CIA Timer A in continuous / normal mode, increment every 10 cycles. */
  ciaa->ciacra &= (UBYTE)(~CIACRAF_RUNMODE & ~CIACRAF_INMODE & ~CIACRAF_PBON);

  InterruptVector->IntLevel2 = IntLevel2Handler;
  custom->intena = INTF_SETCLR | INTF_PORTS;

  /* Use AHX_EXPLICIT_WAVES_PRECALCING flag,
   * because dos.library is not usable at this point. */

  if (AhxInitCIA((APTR)AhxSetTempo, AHX_KILL_SYSTEM) == 0)
    if (AhxInitPlayer(AHX_EXPLICIT_WAVES_PRECALCING, AHX_FILTERS) == 0)
      if (AhxInitModule(module) == 0)
        AhxInitSubSong(0, 0);
}

static void Kill() {
  AhxStopSong();
  AhxKillPlayer();
  AhxKillCIA();
}

EffectT Effect = { Load, UnLoad, Init, Kill, NULL };
