// file: "intr.cpp"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 22 Sep 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "intr.h"
#include "asm.h"
#include "pic.h"
#include "apic.h"
#include "term.h"

//-----------------------------------------------------------------------------

//
// Interrupt handlers.
//

void setup_intr ()
{
#ifdef USE_APIC_FOR_TIMER

  // Make sure that the local APIC is mapped to the default memory
  // location and that it is enabled.

  uint32 dummy, features;

  cpuid (1, dummy, dummy, dummy, features);

  if (features & HAS_MSR)
    {
      uint64 x = rdmsr (MSR_APIC);
      x &= MSR_APIC_BSP;
      x |= MSR_APIC_BASE | MSR_APIC_E;
      wrmsr (MSR_APIC, x);
    }

  uint32 x;

  x = APIC_SVR;
  x |= APIC_SVR_SW_ENABLE; // Enable APIC
  x &= ~APIC_SVR_FPC_DISABLE; // Enable Focus Processor Checking
  x &= ~APIC_SVR_VECTOR_MASK;
  x |= 0xcf; // low 4 bits of vector should be ones
  APIC_SVR = x;

  x = APIC_TPR;
  x &= ~APIC_TPR_PRIO_MASK; // Accept all interrupts
  APIC_TPR = x;

  x = APIC_LDR;
  x &= ~APIC_LDR_LOGID_MASK; // Logical APIC ID = 0
  APIC_LDR = x;

  x = APIC_DFR;
  x |= APIC_DFR_CONFIG (0x0f); // Flat model
  APIC_DFR = x;

#ifdef USE_APIC_FOR_TIMER

  APIC_TIMER_DIVIDE_CONFIG = 0x0b; // divide by 1

  x = APIC_LVTT;
  x |= APIC_LVT_MASKED; // Mask timer interrupt
  x &= ~APIC_LVTT_PERIODIC; // One-shot mode
  x &= ~APIC_LVT_VECTOR_MASK;
  x |= 0xa0;
  APIC_LVTT = x;

#endif

  x = APIC_LVTTM;
  x |= APIC_LVT_MASKED; // Mask thermal sensor interrupt
  APIC_LVTTM = x;

  x = APIC_LVTPC;
  x |= APIC_LVT_MASKED; // Mask performance counter interrupt
  APIC_LVTPC = x;

  x = APIC_LVT0;
  x |= APIC_LVT_MASKED; // Mask local interrupt 0 interrupt
  x |= APIC_LVT_LTM; // Level trigger mode
  x |= APIC_LVT_RIRR; // Remote IRR
  x |= APIC_LVT_POL; // Interrupt input pin polarity = 1
  x &= ~APIC_LVT_DM_MASK;
  x |= APIC_LVT_DM_EXTINT; // Delivery mode = ExtINT
  APIC_LVT0 = x;

  x = APIC_LVT1;
  x |= APIC_LVT_MASKED; // Mask local interrupt 1 interrupt
  APIC_LVT1 = x;

  x = APIC_LVTE;
  x |= APIC_LVT_MASKED; // Mask error interrupt
  APIC_LVTE = x;

#endif

  // Initialize master and slave PICs.

  outb (PIC_ICW1(PIC_ICW1_ICW4), PIC_PORT_MASTER_ICW1);
  outb (PIC_ICW1(PIC_ICW1_ICW4), PIC_PORT_SLAVE_ICW1);

  outb (PIC_ICW2(0x16), PIC_PORT_MASTER_ICW2); // int. vectors 0xb0 .. 0xb7
  outb (PIC_ICW2(0x17), PIC_PORT_SLAVE_ICW2);  // int. vectors 0xb8 .. 0xbf

  outb (PIC_MASTER_ICW3(PIC_MASTER_ICW3_SLAVE(2)), PIC_PORT_MASTER_ICW3);
  outb (PIC_SLAVE_ICW3(2), PIC_PORT_SLAVE_ICW3);

  outb (PIC_ICW4(PIC_ICW4_8086|PIC_ICW4_MASTER), PIC_PORT_MASTER_ICW4);
  outb (PIC_ICW4(PIC_ICW4_8086), PIC_PORT_SLAVE_ICW4);

  // Disable interrupts on IRQ0, IRQ1, IRQ3 .. IRQ15

  // Note: one of these interrupts may have occurred between the
  // initialization of the PICs and the disabling of the PIC
  // interrupts, so it is possible (but very unlikely) that an
  // interrupt will occur when the CPU re-enables interrupts.  The CPU
  // should only re-enable interrupts when it is safe to call the
  // interrupt handlers (in particular the timer interrupt handler
  // which drives the thread scheduler).

  outb (PIC_OCW1_MASK(PIC_MASTER_IRQ0) |
        PIC_OCW1_MASK(PIC_MASTER_IRQ1) |
        PIC_OCW1_MASK(PIC_MASTER_IRQ3) |
        PIC_OCW1_MASK(PIC_MASTER_IRQ4) |
        PIC_OCW1_MASK(PIC_MASTER_IRQ5) |
        PIC_OCW1_MASK(PIC_MASTER_IRQ6) |
        PIC_OCW1_MASK(PIC_MASTER_IRQ7),
        PIC_PORT_MASTER_OCW1);

  outb (PIC_OCW1_MASK(PIC_SLAVE_IRQ8) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ9) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ10) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ11) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ12) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ13) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ14) |
        PIC_OCW1_MASK(PIC_SLAVE_IRQ15),
        PIC_PORT_SLAVE_OCW1);
}

#ifndef USE_PIT_FOR_TIMER

void irq0 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq0 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(0);
}

#endif

#ifndef USE_IRQ1_FOR_KEYBOARD

void irq1 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq1 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(1);
}

#endif

void irq2 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq2 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(2);
}

void irq3 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq3 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(3);
}

void irq4 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq4 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(4);
}

void irq5 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq5 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(5);
}

void irq6 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq6 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(6);
}

void irq7 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq7 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(7);
}

#ifndef USE_IRQ8_FOR_TIME

void irq8 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq8 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(8);
}

#endif

void irq9 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq9 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(9);
}

void irq10 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq10 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(10);
}

void irq11 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq11 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(11);
}

#ifndef USE_IRQ12_FOR_MOUSE

void irq12 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq12 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(12);
}

#endif

void irq13 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq13 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(13);
}

#ifndef USE_IRQ14_FOR_IDE0

void irq14 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq14 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(14);
}

#endif

#ifndef USE_IRQ15_FOR_IDE1

void irq15 ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m irq15 \033[0m";
#endif

  ACKNOWLEDGE_IRQ(15);
}

#endif

#ifndef USE_APIC_FOR_TIMER

void APIC_timer_irq ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m APIC timer irq \033[0m";
#endif

  APIC_EOI = 0;
}

#endif

void APIC_spurious_irq ()
{
#ifdef SHOW_INTERRUPTS
  cout << "\033[41m APIC spurious irq \033[0m";
#endif

  APIC_EOI = 0;
}

void unhandled_interrupt (int num)
{
  cout << "\033[41m unhandled interrupt " << num << " \033[0m";
}

//-----------------------------------------------------------------------------
